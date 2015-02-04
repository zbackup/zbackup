// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "backup_collector.hh"

#include <string>
#include <vector>

#include "bundle.hh"
#include "chunk_index.hh"
#include "backup_restorer.hh"
#include "backup_file.hh"
#include "backup_exchanger.hh"

#include "debug.hh"

using std::string;

void BundleCollector::startIndex( string const & indexFn )
{
  indexModified = false;
  indexTotalChunks = indexUsedChunks = 0;
  indexModifiedBundles = indexKeptBundles = indexRemovedBundles = 0;
}

void BundleCollector::finishIndex( string const & indexFn )
{
  if ( indexModified )
  {
    verbosePrintf( "Chunks: %d used / %d total, bundles: %d kept / %d modified / %d removed\n",
                   indexUsedChunks, indexTotalChunks, indexKeptBundles, indexModifiedBundles, indexRemovedBundles);
    filesToUnlink.push_back( indexFn );
  }
}

void BundleCollector::startBundle( Bundle::Id const & bundleId )
{
  savedId = bundleId;
  totalChunks = 0;
  usedChunks = 0;
}

void BundleCollector::processChunk( ChunkId const & chunkId )
{
  totalChunks++;
  if ( usedChunkSet.find( chunkId ) != usedChunkSet.end() )
  {
    usedChunks++;
  }
}

void BundleCollector::finishBundle( Bundle::Id const & bundleId, BundleInfo const & info )
{
  string i = Bundle::generateFileName( savedId, "", false );
  indexTotalChunks += totalChunks;
  indexUsedChunks += usedChunks;
  if ( usedChunks == 0 )
  {
    if ( verbose )
      printf( "delete %s\n", i.c_str() );
    filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
    indexModified = true;
    indexRemovedBundles++;
  }
  else if ( usedChunks < totalChunks )
  {
    if ( verbose )
      printf( "%s: used %d/%d\n", i.c_str(), usedChunks, totalChunks );
    filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
    indexModified = true;
    // Copy used chunks to the new index
    string chunk;
    size_t chunkSize;
    for ( int x = info.chunk_record_size(); x--; )
    {
      BundleInfo_ChunkRecord const & record = info.chunk_record( x );
      ChunkId id( record.id() );
      if ( usedChunkSet.find( id ) != usedChunkSet.end() )
      {
        chunkStorageReader->get( id, chunk, chunkSize );
        chunkStorageWriter->add( id, chunk.data(), chunkSize );
      }
    }
    indexModifiedBundles++;
  }
  else
  {
    chunkStorageWriter->addBundle( info, savedId );
    if ( verbose )
      printf( "keep %s\n", i.c_str() );
    indexKeptBundles++;
  }
}

void BundleCollector::commit()
{
  for ( int i = filesToUnlink.size(); i--; )
  {
    unlink( filesToUnlink[i].c_str() );
  }
  filesToUnlink.clear();
  chunkStorageWriter->commit();
}
