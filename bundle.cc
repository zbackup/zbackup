// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdint.h>

#include "bundle.hh"
#include "check.hh"
#include "dir.hh"
#include "encryption.hh"
#include "hex.hh"
#include "message.hh"
#include "adler32.hh"
#include "compression.hh"

namespace Bundle {

enum
{
  FileFormatVersion = 1,

  // This means, we don't use LZMA in this file.
  FileFormatVersionNotLZMA,

  // <- add more versions here

  // This is the first version, we do not support.
  FileFormatVersionFirstUnsupported
};

void Creator::addChunk( string const & id, void const * data, size_t size )
{
  BundleInfo_ChunkRecord * record = info.add_chunk_record();
  record->set_id( id );
  record->set_size( size );
  payload.append( ( char const * ) data, size );
}

void Creator::write( std::string const & fileName, EncryptionKey const & key,
    Reader & reader )
{
  EncryptedFile::OutputStream os( fileName.c_str(), key, Encryption::ZeroIv );

  os.writeRandomIv();

  Message::serialize( reader.getBundleHeader(), os );

  Message::serialize( reader.getBundleInfo(), os );
  os.writeAdler32();

  void * bufPrev = NULL;
  const void * bufCurr = NULL;
  int sizePrev = 0, sizeCurr = 0;
  bool readPrev = false, readCurr = false;

  for ( ; ; )
  {
    bool readCurr = reader.is->Next( &bufCurr, &sizeCurr );

    if ( readCurr )
    {
      if ( readPrev )
      {
        os.write( bufPrev, sizePrev );

        readPrev = readCurr;
        free( bufPrev );
        bufPrev = malloc( sizeCurr );
        memcpy( bufPrev, bufCurr, sizeCurr );
        sizePrev = sizeCurr;
      }
      else
      {
        readPrev = readCurr;
        bufPrev = malloc( sizeCurr );
        memcpy( bufPrev, bufCurr, sizeCurr );
        sizePrev = sizeCurr;
      }
    }
    else
    {
      if ( readPrev )
      {
        sizePrev -= sizeof( Adler32::Value );
        os.write( bufPrev, sizePrev );
        os.writeAdler32();
        free ( bufPrev );
        break;
      }
    }
  }

  if ( reader.is.get() )
    reader.is.reset();
}

void Creator::write( std::string const & fileName, EncryptionKey const & key )
{
  EncryptedFile::OutputStream os( fileName.c_str(), key, Encryption::ZeroIv );

  os.writeRandomIv();

  BundleFileHeader header;

  const_sptr<Compression::CompressionMethod> compression = Compression::CompressionMethod::defaultCompression;
  header.set_compression_method( compression->getName() );

  // The old code only support lzma, so we will bump up the version, if we're
  // using lzma. This will make it fail cleanly.
  if ( compression->getName() == "lzma" )
    header.set_version( FileFormatVersion );
  else
    header.set_version( FileFormatVersionNotLZMA );

  Message::serialize( header, os );

  Message::serialize( info, os );
  os.writeAdler32();

  // Compress

  sptr<Compression::EnDecoder> encoder = compression->createEncoder();

  encoder->setInput( payload.data(), payload.size() );

  for ( ; ; )
  {
    {
      void * data;
      int size;
      if ( !os.Next( &data, &size ) )
      {
        encoder.reset();
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

  encoder.reset();

  os.writeAdler32();
}

Reader::Reader( string const & fileName, EncryptionKey const & key, bool keepStream )
{
  is = new EncryptedFile::InputStream( fileName.c_str(), key, Encryption::ZeroIv );
  is->consumeRandomIv();

  Message::parse( header, *is );

  if ( header.version() >= FileFormatVersionFirstUnsupported )
    throw exUnsupportedVersion();

  Message::parse( info, *is );
  is->checkAdler32();

  size_t payloadSize = 0;
  for ( int x = info.chunk_record_size(); x--; )
    payloadSize += info.chunk_record( x ).size();

  payload.resize( payloadSize );

  if ( keepStream )
    return;

  sptr<Compression::EnDecoder> decoder = Compression::CompressionMethod::findCompression(
                                           header.compression_method() )->createDecoder();

  decoder->setOutput( &payload[ 0 ], payload.size() );

  for ( ; ; )
  {
    {
      void const * data;
      int size;
      if ( !is->Next( &data, &size ) )
      {
        decoder.reset();
        throw exBundleReadFailed();
      }
      if ( !size )
        continue;
      decoder->setInput( data, size );
    }

    if ( decoder->process(false) ) {
      if ( decoder->getAvailableInput() )
        is->BackUp( decoder->getAvailableInput() );
      break;
    }

    if ( !decoder->getAvailableOutput() && decoder->getAvailableInput() )
    {
      // Apparently we have more data than we were expecting
      decoder.reset();
      throw exTooMuchData();
    }
  }

  decoder.reset();

  is->checkAdler32();
  if ( is.get() )
    is.reset();

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
