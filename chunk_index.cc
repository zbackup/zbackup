// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdio.h>
#include <string.h>
#include <new>
#include <utility>

#include "chunk_index.hh"
#include "debug.hh"
#include "dir.hh"
#include "index_file.hh"
#include "zbackup.pb.h"

ChunkIndex::Chain::Chain( ChunkId const & id, Bundle::Id const * bundleId ):
  next( 0 ), bundleId( bundleId )
{
  memcpy( cryptoHash, id.cryptoHash, sizeof( cryptoHash ) );
}

bool ChunkIndex::Chain::equalsTo( ChunkId const & id )
{
  return memcmp( cryptoHash, id.cryptoHash, sizeof ( cryptoHash ) ) == 0;
}

void ChunkIndex::loadIndex()
{
  Dir::Listing lst( indexPath );

  Dir::Entry entry;

  verbosePrintf( "Loading index...\n" );

  while( lst.getNext( entry ) )
  {
    verbosePrintf( "Loading index file %s...\n", entry.getFileName().c_str() );

    IndexFile::Reader reader( key,
                             Dir::addPath( indexPath, entry.getFileName() ) );

    BundleInfo info;
    Bundle::Id bundleId;
    while( reader.readNextRecord( info, bundleId ) )
    {
      Bundle::Id * savedId = storage.allocateObjects< Bundle::Id >( 1 );
      memcpy( savedId, &bundleId, sizeof( bundleId ) );

      lastBundleId = savedId;

      ChunkId id;

      for ( int x = info.chunk_record_size(); x--; )
      {
        BundleInfo_ChunkRecord const & record = info.chunk_record( x );

        if ( record.id().size() != ChunkId::BlobSize )
          throw exIncorrectChunkIdSize();

        id.setFromBlob( record.id().data() );
        registerNewChunkId( id, savedId );
      }
    }
  }

  verbosePrintf( "Index loaded.\n" );
}

ChunkIndex::ChunkIndex( EncryptionKey const & key, TmpMgr & tmpMgr,
                        string const & indexPath, bool prohibitChunkIndexLoading ):
  key( key ), tmpMgr( tmpMgr ), indexPath( indexPath ), storage( 65536, 1 ),
  lastBundleId( NULL )
{
  if ( !prohibitChunkIndexLoading )
    loadIndex();
}

Bundle::Id const * ChunkIndex::findChunk( ChunkId::RollingHashPart rollingHash,
                                          ChunkInfoInterface & chunkInfo )
{
  HashTable::iterator i = hashTable.find( rollingHash );

  ChunkId const * id = 0;

  if ( i != hashTable.end() )
  {
    if ( !id )
      id = &chunkInfo.getChunkId();
    // Check the chains
    for ( Chain * chain = i->second; chain; chain = chain->next )
      if ( chain->equalsTo( *id ) )
        return chain->bundleId;
  }

  return NULL;
}

namespace {
struct ChunkInfoImmediate: public ChunkIndex::ChunkInfoInterface
{
  ChunkId const & id;

  ChunkInfoImmediate( ChunkId const & id ): id( id ) {}

  virtual ChunkId const & getChunkId()
  { return id; }
};
}

Bundle::Id const * ChunkIndex::findChunk( ChunkId const & chunkId )
{
  ChunkInfoImmediate chunkInfo( chunkId );
  return findChunk( chunkId.rollingHash, chunkInfo );
}

ChunkIndex::Chain * ChunkIndex::registerNewChunkId( ChunkId const & id,
                                                    Bundle::Id const * bundleId )
{
  HashTable::iterator i =
    hashTable.insert( std::make_pair( id.rollingHash, ( Chain *) 0 ) ).first;

  Chain ** chain = &i->second;

  // Check the chains
  for ( ; *chain; chain = &( ( *chain )->next ) )
    if ( ( *chain )->equalsTo( id ) )
    {
      return NULL; // The entry existed already
    }

  // Create a new chain
  *chain = new ( storage.allocateObjects< Chain >( 1 ) ) Chain( id, bundleId );

  return *chain;
}


bool ChunkIndex::addChunk( ChunkId const & id, Bundle::Id const & bundleId )
{
  if ( Chain * chain = registerNewChunkId( id, NULL ) )
  {
    // Allocate or re-use bundle id
    if ( !lastBundleId || *lastBundleId != bundleId )
    {
      Bundle::Id * allocatedId  = storage.allocateObjects< Bundle::Id >( 1 );
      memcpy( allocatedId, &bundleId, Bundle::IdSize );
      lastBundleId = allocatedId;
    }
    chain->bundleId = lastBundleId;

    return true;
  }
  else
    return false;
}
