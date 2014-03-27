// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <lzma.h>
#include <stdint.h>

#include "bundle.hh"
#include "check.hh"
#include "debug.hh"
#include "dir.hh"
#include "encrypted_file.hh"
#include "encryption.hh"
#include "hex.hh"
#include "message.hh"

namespace Bundle {

enum
{
  FileFormatVersion = 1
};

void Creator::addChunk( string const & id, void const * data, size_t size )
{
  BundleInfo_ChunkRecord * record = info.add_chunk_record();
  record->set_id( id );
  record->set_size( size );
  payload.append( ( char const * ) data, size );
}

void Creator::write( std::string const & fileName, EncryptionKey const & key, size_t compressionLevel )
{
  EncryptedFile::OutputStream os( fileName.c_str(), key, Encryption::ZeroIv );

  os.writeRandomIv();

  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, os );

  Message::serialize( info, os );
  os.writeAdler32();

  // Compress

  uint32_t preset = ( compressionLevel > 9 ) ? ( compressionLevel - 10 ) | LZMA_PRESET_EXTREME : compressionLevel;
  dPrintf( "LZMA Compression level: %lu, preset %u\n", compressionLevel, preset );

  lzma_stream strm = LZMA_STREAM_INIT;
  lzma_ret ret;

  ret = lzma_easy_encoder( &strm, preset, LZMA_CHECK_CRC64 );
  CHECK( ret == LZMA_OK, "lzma_easy_encoder error: %d", (int) ret );

  strm.next_in = ( uint8_t const * ) payload.data();
  strm.avail_in = payload.size();

  for ( ; ; )
  {
    {
      void * data;
      int size;
      if ( !os.Next( &data, &size ) )
      {
        lzma_end( &strm );
        throw exBundleWriteFailed();
      }
      if ( !size )
        continue;
      strm.next_out = ( uint8_t * ) data;
      strm.avail_out = size;
    }

    // Perform the compression
    ret = lzma_code( &strm, LZMA_FINISH );

    if ( ret == LZMA_STREAM_END )
    {
      if ( strm.avail_out )
        os.BackUp( strm.avail_out );
      break;
    }

    CHECK( ret == LZMA_OK, "lzma_code error: %d", (int) ret );
  }

  dPrintf( "Waiting for compression to finish\n" );

  lzma_end( &strm );

  os.writeAdler32();
}

Reader::Reader( string const & fileName, EncryptionKey const & key )
{
  EncryptedFile::InputStream is( fileName.c_str(), key, Encryption::ZeroIv );

  is.consumeRandomIv();

  FileHeader header;
  Message::parse( header, is );

  if ( header.version() != FileFormatVersion )
    throw exUnsupportedVersion();

  BundleInfo info;
  Message::parse( info, is );
  is.checkAdler32();

  size_t payloadSize = 0;
  for ( int x = info.chunk_record_size(); x--; )
    payloadSize += info.chunk_record( x ).size();

  payload.resize( payloadSize );

  lzma_stream strm = LZMA_STREAM_INIT;

  lzma_ret ret;

  ret = lzma_stream_decoder( &strm, UINT64_MAX, 0 );
  CHECK( ret == LZMA_OK,"lzma_stream_decoder error: %d", (int) ret );

  strm.next_out = ( uint8_t * ) &payload[ 0 ];
  strm.avail_out = payload.size();

  for ( ; ; )
  {
    {
      void const * data;
      int size;
      if ( !is.Next( &data, &size ) )
      {
        lzma_end( &strm );
        throw exBundleReadFailed();
      }
      if ( !size )
        continue;
      strm.next_in = ( uint8_t const * ) data;
      strm.avail_in = size;
    }

    ret = lzma_code( &strm, LZMA_RUN );

    if ( ret == LZMA_STREAM_END )
    {
      if ( strm.avail_in )
        is.BackUp( strm.avail_in );
      break;
    }

    CHECK( ret == LZMA_OK, "lzma_code error: %d", (int) ret );

    if ( !strm.avail_out && strm.avail_in )
    {
      // Apparently we have more data than we were expecting
      lzma_end( &strm );
      throw exTooMuchData();
    }
  }

  dPrintf( "Waiting for de-compression to finish\n" );

  lzma_end( &strm );

  is.checkAdler32();

  // Populate the map
  char const * next = payload.data();
  for ( int x = 0, count = info.chunk_record_size(); x < count; ++x )
  {
    BundleInfo_ChunkRecord const & record = info.chunk_record( x );
    pair< Chunks::iterator, bool > res =
      chunks.insert( Chunks::value_type( record.id(),
                                         Chunks::mapped_type( next,
                                                              record.size() ) ) );
    if ( !res.second )
      throw exDuplicateChunks(); // Duplicate key encountered
    next += record.size();
  }
}

bool Reader::get( string const & chunkId, string & chunkData,
                  size_t & chunkDataSize )
{
  Chunks::iterator i = chunks.find( chunkId );
  if ( i != chunks.end() )
  {
    size_t sz = i->second.second;
    if ( chunkData.size() < sz )
      chunkData.resize( sz );
    memcpy( &chunkData[ 0 ], i->second.first, sz );

    chunkDataSize = sz;
    return true;
  }
  else
    return false;
}

string generateFileName( Id const & id, string const & bundlesDir,
                         bool createDirs )
{
  string hex( toHex( ( unsigned char * ) &id, sizeof( id ) ) );

  // TODO: make this scheme more flexible and allow it to scale, or at least
  // be configurable
  string level1( Dir::addPath( bundlesDir, hex.substr( 0, 2 ) ) );

  if ( createDirs && !Dir::exists( level1 ) )
      Dir::create( level1 );

  return string( Dir::addPath( level1, hex ) );
}
}
