// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <vector>
#include <algorithm>

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
  cis.SetTotalBytesLimit( backupData.size() );

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

// TODO: This iterator can be used in restore() function.
/// Iterator over BackupInstructions in stream
class BackupInstructionsIterator : NoCopy
{
public:
  BackupInstructionsIterator( std::string const & backupData )
    : is( backupData.data(), backupData.size() )
    , cis( &is )

  {
    limit = cis.PushLimit( backupData.size() );
    // The following line prevents it from barfing on large backupData.
    // TODO: this disables size checks for each separate message. Figure a better
    // way to do this while keeping them enabled. It seems we need to create an
    // instance of CodedInputStream for each message, but it might be expensive
    cis.SetTotalBytesLimit( backupData.size() );
  }

  ~BackupInstructionsIterator()
  {
    cis.PopLimit( limit );
  }

  /// Read next BackupInstruction instruction in stream.
  /// Returns true if there is any, and false if end of stream reached.
  bool readNext( BackupInstruction & instr )
  {
    if ( cis.BytesUntilLimit() > 0 )
    {
      Message::parse( instr, cis );
      return true;
    }
    else
    {
      return false;
    }
  }

private:
  google::protobuf::io::ArrayInputStream is;
  CodedInputStream cis;
  CodedInputStream::Limit limit;
};

IndexedRestorer::IndexedRestorer( ChunkStorage::Reader & chunkStorageReader,
                                  std::string const & backupData )
   : chunkStorageReader( chunkStorageReader )
{
  BackupInstructionsIterator instructionIter( backupData );

  BackupInstruction instr;
  int64_t position = 0;
  while ( instructionIter.readNext( instr ) )
  {
    instructions.push_back( std::make_pair( position, instr ) );

    if ( instr.has_chunk_to_emit() )
    {
      ChunkId id( instr.chunk_to_emit() );
      size_t chunkSize;
      chunkStorageReader.getBundleId( id, chunkSize );

      position += chunkSize;
    }

    if ( instr.has_bytes_to_emit() )
    {
      string const & bytes = instr.bytes_to_emit();
      position += bytes.size();
    }
  }

  totalSize = position;
}

int64_t IndexedRestorer::size() const
{
  return totalSize;
}

template<class PairType>
class PairFirstLess
{
public:
  bool operator()( PairType const & left, PairType const & right )
  {
    return left.first < right.first;
  }
};

void IndexedRestorer::saveData( int64_t offset, void * data, size_t size ) const
{
  if ( offset < 0 || offset + size > totalSize )
    throw exOutOfRange();

  // Find first instruction which generates output range that starts after offset
  Instructions::const_iterator it =
      std::upper_bound( instructions.begin(), instructions.end(),
                        std::make_pair(offset, BackupInstruction()),
                        PairFirstLess<InstructionAtPos>() );
  assert(it != instructions.begin());
  // Iterator will point on instruction, which range will include byte at offset
  --it;

  struct Outputer
  {
    Outputer( int64_t offset, char * data, size_t size )
      : offset(offset)
      , data(data)
      , size(size)
    {
    }

    bool operator()( int64_t chunkOffset, char const * chunk, size_t chunkSize )
    {
      size_t start = 0;
      if ( chunkOffset < offset )
      {
        // First chunk which begins before offset
        start = offset - chunkOffset;
      }

      size_t end = chunkSize;
      if ( chunkOffset + chunkSize > offset + size )
      {
        // Chunk ends beyond requested range
        end = offset + size - chunkOffset;
      }

      size_t partSize = end - start;
      memcpy( data, chunk + start, partSize );

      offset += partSize;
      data += partSize;

      assert( size >= partSize );
      size -= partSize;

      return size != 0;
    }

    int64_t offset;
    char * data;
    size_t size;
  };

  Outputer out( offset, static_cast<char *>( data ), size );
  string chunk;

  int64_t position = it->first;
  for ( ; it != instructions.end(); ++it)
  {
    assert( position == it->first );
    BackupInstruction const & instr = it->second;

    if ( instr.has_chunk_to_emit() )
    {
      ChunkId id( instr.chunk_to_emit() );
      size_t chunkSize;
      chunkStorageReader.get( id, chunk, chunkSize );

      if ( !out( position, chunk.data(), chunkSize ) )
      {
        break;
      }
      position += chunkSize;
    }

    if ( instr.has_bytes_to_emit() )
    {
      string const & bytes = instr.bytes_to_emit();
      if ( !out( position, bytes.data(), bytes.size() ) )
      {
        break;
      }
      position += bytes.size();
    }
  }
}

}
