// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <vector>

#include "backup_restorer.hh"
#include "chunk_id.hh"
#include "message.hh"
#include "zbackup.pb.h"

namespace BackupRestorer {

using std::vector;
using google::protobuf::io::CodedInputStream;

void restoreMap( ChunkStorage::Reader & chunkStorageReader,
              ChunkMap const * chunkMap, SeekableSink *output )
{
  string chunk;
  size_t chunkSize;
  for ( ChunkMap::const_iterator it = chunkMap->begin(); it != chunkMap->end(); it++ )
  {
    for ( ChunkPosition::const_iterator pi = (*it).second.begin(); pi != (*it).second.end(); pi++ )
    {
      if ( output )
      {
        // Need to emit a chunk, reading it from the store
        chunkStorageReader.get( (*pi).first, chunk, chunkSize );
        output->saveData( (*pi).second, chunk.data(), chunkSize );
      }
    }
  }
}

void restore( ChunkStorage::Reader & chunkStorageReader,
              std::string const & backupData,
              DataSink * output, ChunkSet * chunkSet,
              ChunkMap * chunkMap, SeekableSink * seekOut )
{
  google::protobuf::io::ArrayInputStream is( backupData.data(),
                                             backupData.size() );
  CodedInputStream cis( &is );
  CodedInputStream::Limit limit = cis.PushLimit( backupData.size() );
  // The following line prevents it from barfing on large backupData.
  // TODO: this disables size checks for each separate message. Figure a better
  // way to do this while keeping them enabled. It seems we need to create an
  // instance of CodedInputStream for each message, but it might be expensive
  cis.SetTotalBytesLimit( backupData.size(), -1 );

  // Used when emitting chunks
  string chunk;

  BackupInstruction instr;
  int64_t position = 0;
  while ( cis.BytesUntilLimit() > 0 )
  {
    Message::parse( instr, cis );

    if ( instr.has_chunk_to_emit() )
    {
      ChunkId id( instr.chunk_to_emit() );
      size_t chunkSize;
      if ( output )
      {
        // Need to emit a chunk, reading it from the store
        chunkStorageReader.get( id, chunk, chunkSize );
        output->saveData( chunk.data(), chunkSize );
      }
      if ( chunkMap )
      {
        Bundle::Id const *bundleId = chunkStorageReader.getBundleId( id, chunkSize );
        ChunkMap::iterator it = chunkMap->find( *bundleId );
        if ( it == chunkMap->end() )
        {
          ChunkPosition v;
          std::pair< ChunkMap::iterator, bool > r = chunkMap->insert( std::make_pair( *bundleId, v ) );
          it = r.first;
        }
        (*it).second.push_back( std::make_pair( id, position ) );
        position += chunkSize;
      }
      if ( chunkSet )
      {
        chunkSet->insert( id );
      }
    }

    if ( ( output || chunkMap ) && instr.has_bytes_to_emit() )
    {
      // Need to emit the bytes directly
      string const & bytes = instr.bytes_to_emit();
      if ( output )
        output->saveData( bytes.data(), bytes.size() );
      if ( chunkMap )
      {
        if ( seekOut )
          seekOut->saveData( position, bytes.data(), bytes.size() );
        position += bytes.size();
      }
    }
  }

  cis.PopLimit( limit );
}

void restoreIterations( ChunkStorage::Reader & chunkStorageReader,
  BackupInfo & backupInfo, std::string & backupData, ChunkSet * chunkSet )
{
  // Perform the iterations needed to get to the actual user backup data
  for ( ; ; )
  {
    backupData.swap( *backupInfo.mutable_backup_data() );

    if ( backupInfo.iterations() )
    {
      struct StringWriter: public DataSink
      {
        string result;

        virtual void saveData( void const * data, size_t size )
        {
          result.append( ( char const * ) data, size );
        }
      } stringWriter;

      restore( chunkStorageReader, backupData, &stringWriter, chunkSet, NULL, NULL );
      backupInfo.mutable_backup_data()->swap( stringWriter.result );
      backupInfo.set_iterations( backupInfo.iterations() - 1 );
    }
    else
      break;
  }
}

}
