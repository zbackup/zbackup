// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <stdint.h>

#include "bundle.hh"
#include "check.hh"
#include "dir.hh"
#include "encrypted_file.hh"
#include "encryption.hh"
#include "hex.hh"
#include "message.hh"
#include "compression.hh"

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

void Creator::write( std::string const & fileName, EncryptionKey const & key )
{
  EncryptedFile::OutputStream os( fileName.c_str(), key, Encryption::ZeroIv );

  os.writeRandomIv();

  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, os );

  const Compression* compression = Compression::default_compression;
  info.set_compression_method( compression->getName() );

  Message::serialize( info, os );
  os.writeAdler32();

  // Compress

  EnDecoder* encoder = compression->getEncoder();

  encoder->setInput( payload.data(), payload.size() );

  // deliberately break the test: ((uint8_t*)strm.next_in)[10] = 7;

  for ( ; ; )
  {
    {
      void * data;
      int size;
      if ( !os.Next( &data, &size ) )
      {
        delete encoder;
        throw exBundleWriteFailed();
      }
      if ( !size )
        continue;
      encoder->setOutput( data, size );
    }

    // Perform the compression
    if ( encoder->process(true) )
    {
      if ( encoder->getAvailableOutput() )
        os.BackUp( encoder->getAvailableOutput() );
      break;
    }
  }

	delete encoder;

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

  EnDecoder* decoder = Compression::findCompression( info.compression_method() )->getDecoder();

  decoder->setOutput( &payload[ 0 ], payload.size() );

  for ( ; ; )
  {
    {
      void const * data;
      int size;
      if ( !is.Next( &data, &size ) )
      {
        delete decoder;
        throw exBundleReadFailed();
      }
      if ( !size )
        continue;
      decoder->setInput( data, size );
    }

    if ( decoder->process(false) ) {
      if ( decoder->getAvailableInput() )
        is.BackUp( decoder->getAvailableInput() );
      break;
    }

    if ( !decoder->getAvailableOutput() && decoder->getAvailableInput() )
    {
      // Apparently we have more data than we were expecting
      delete decoder;
      throw exTooMuchData();
    }
  }

  delete decoder;

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
