// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "backup_collector.hh"

using std::string;

void BundleCollector::startIndex( string const & indexFn )
{
  indexModified = indexNecessary = false;
  indexTotalChunks = indexUsedChunks = 0;
  indexModifiedBundles = indexKeptBundles = indexRemovedBundles = 0;
}

void BundleCollector::finishIndex( string const & indexFn )
{
  verbosePrintf( "Chunks used: %d/%d, bundles: %d kept, %d modified, %d removed\n",
                 indexUsedChunks, indexTotalChunks, indexKeptBundles,
                 indexModifiedBundles, indexRemovedBundles );
  if ( indexModified )
  {
    filesToUnlink.push_back( indexFn );
    commit();
  }
  else
  {
    chunkStorageWriter->reset();
    if ( !indexNecessary )
      // this index was a complete copy so we don't need it
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
  if ( overallChunkSet.find ( chunkId ) == overallChunkSet.end() )
    overallChunkSet.insert( chunkId );
  else
    return;

  totalChunks++;
  if ( usedChunkSet.find( chunkId ) != usedChunkSet.end() )
  {
    usedChunks++;
    indexNecessary = true;
  }
}

void BundleCollector::finishBundle( Bundle::Id const & bundleId, BundleInfo const & info )
{
  string i = Bundle::generateFileName( savedId, "", false );
  indexTotalChunks += totalChunks;
  indexUsedChunks += usedChunks;
  if ( 0 == usedChunks && 0 != totalChunks )
  {
    dPrintf( "Deleting %s bundle\n", i.c_str() );
    filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
    indexModified = true;
    indexRemovedBundles++;
  }
  else if ( usedChunks < totalChunks )
  {
    dPrintf( "%s: used %d/%d chunks\n", i.c_str(), usedChunks, totalChunks );
    filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
    indexModified = true;
    copyUsedChunks( info );
    indexModifiedBundles++;
  }
  else
  {
    if ( gcRepack )
    {
      filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
      indexModified = true;
      copyUsedChunks( info );
      indexModifiedBundles++;
    }
    else
    {
      if ( 0 == totalChunks )
      {
        if ( overallBundleSet.find ( bundleId ) == overallBundleSet.end() )
        {
          overallBundleSet.insert( bundleId );
          dPrintf( "Deleting %s bundle\n", i.c_str() );
          filesToUnlink.push_back( Dir::addPath( bundlesPath, i ) );
          indexModified = true;
          indexRemovedBundles++;
        }
        else
        {
          // trigger index update
          indexModified = true;
        }
      }
      else
      {
        if ( overallBundleSet.find ( bundleId ) == overallBundleSet.end() )
          overallBundleSet.insert( bundleId );
        chunkStorageWriter->addBundle( info, savedId );
        dPrintf( "Keeping %s bundle\n", i.c_str() );
        indexKeptBundles++;
      }
    }
  }
}

void BundleCollector::copyUsedChunks( BundleInfo const & info )
{
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
}

void BundleCollector::commit()
{
  for ( int i = filesToUnlink.size(); i--; )
  {
    dPrintf( "Unlinking %s\n", filesToUnlink[i].c_str() );
    unlink( filesToUnlink[i].c_str() );
  }
  filesToUnlink.clear();
  chunkStorageWriter->commit();
}
