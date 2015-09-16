// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zcollector.hh"

#include "backup_restorer.hh"
#include "backup_file.hh"
#include "backup_exchanger.hh"

#include "debug.hh"

using std::string;
using std::iterator;

namespace {

class BundleCollector: public IndexProcessor
{
private:
  Bundle::Id savedId;
  int totalChunks, usedChunks, indexTotalChunks, indexUsedChunks;
  int indexModifiedBundles, indexKeptBundles, indexRemovedBundles;
  bool indexModified, indexNecessary;
  vector< string > filesToUnlink;
  BackupRestorer::ChunkSet overallChunkSet;
  std::set< Bundle::Id > overallBundleSet;

  void copyUsedChunks( BundleInfo const & info )
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

public:
  string bundlesPath;
  ChunkStorage::Reader *chunkStorageReader;
  ChunkStorage::Writer *chunkStorageWriter;
  BackupRestorer::ChunkSet usedChunkSet;
  bool gcRepack, gcDeep;

  void startIndex( string const & indexFn )
  {
    indexModified = indexNecessary = false;
    indexTotalChunks = indexUsedChunks = 0;
    indexModifiedBundles = indexKeptBundles = indexRemovedBundles = 0;
  }

  void finishIndex( string const & indexFn )
  {
    verbosePrintf( "Chunks used: %d/%d, bundles: %d kept, %d modified, %d removed\n",
                   indexUsedChunks, indexTotalChunks, indexKeptBundles,
                   indexModifiedBundles, indexRemovedBundles);
    if ( indexModified )
    {
      filesToUnlink.push_back( indexFn );
      commit();
    }
    else
    {
      chunkStorageWriter->reset();
      if ( gcDeep && !indexNecessary )
        // this index was a complete copy so we don't need it
        filesToUnlink.push_back( indexFn );
    }
  }

  void startBundle( Bundle::Id const & bundleId )
  {
    savedId = bundleId;
    totalChunks = 0;
    usedChunks = 0;
  }

  void processChunk( ChunkId const & chunkId )
  {
    if ( gcDeep )
    {
      if ( overallChunkSet.find ( chunkId ) == overallChunkSet.end() )
        overallChunkSet.insert( chunkId );
      else
        return;
    }

    totalChunks++;
    if ( usedChunkSet.find( chunkId ) != usedChunkSet.end() )
    {
      usedChunks++;
      indexNecessary = true;
    }
  }

  void finishBundle( Bundle::Id const & bundleId, BundleInfo const & info )
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
        if ( gcDeep && 0 == totalChunks )
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
          if ( gcDeep && overallBundleSet.find ( bundleId ) == overallBundleSet.end() )
            overallBundleSet.insert( bundleId );

          chunkStorageWriter->addBundle( info, savedId );
          dPrintf( "Keeping %s bundle\n", i.c_str() );
          indexKeptBundles++;
        }
      }
    }
  }

  void commit()
  {
    for ( int i = filesToUnlink.size(); i--; )
    {
      dPrintf( "Unlinking %s\n", filesToUnlink[i].c_str() );
      unlink( filesToUnlink[i].c_str() );
    }
    filesToUnlink.clear();
    chunkStorageWriter->commit();
  }
};

}

ZCollector::ZCollector( string const & storageDir, string const & password,
                    size_t threads, size_t cacheSize ):
  ZBackupBase( storageDir, password ),
  chunkStorageReader( storageInfo, encryptionkey, chunkIndex, getBundlesPath(),
                      cacheSize )
{
  this->threads = threads;
}

void ZCollector::gc( bool gcDeep )
{
  ChunkIndex chunkReindex( encryptionkey, tmpMgr, getIndexPath(), true );

  ChunkStorage::Writer chunkStorageWriter( storageInfo, encryptionkey, tmpMgr, chunkReindex,
                      getBundlesPath(), getIndexPath(), threads );

  string fileName;

  Dir::Entry entry;

  string backupsPath = getBackupsPath();

  BundleCollector collector;
  collector.bundlesPath = getBundlesPath();
  collector.chunkStorageReader = &this->chunkStorageReader;
  collector.chunkStorageWriter = &chunkStorageWriter;
  collector.gcRepack = false;
  collector.gcDeep = gcDeep;

  verbosePrintf( "Performing garbage collection...\n" );

  verbosePrintf( "Searching for backups...\n" );
  vector< string > backups = BackupExchanger::findOrRebuild( getBackupsPath() );

  for ( std::vector< string >::iterator it = backups.begin(); it != backups.end(); ++it )
  {
    string backup( Dir::addPath( getBackupsPath(), *it ) );

    verbosePrintf( "Checking backup %s...\n", backup.c_str() );

    BackupInfo backupInfo;

    BackupFile::load( backup , encryptionkey, backupInfo );

    string backupData;

    BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, &collector.usedChunkSet );

    BackupRestorer::restore( chunkStorageReader, backupData, NULL, &collector.usedChunkSet );
  }

  verbosePrintf( "Checking bundles...\n" );

  chunkIndex.loadIndex( collector );

  collector.commit();

  verbosePrintf( "Cleaning up...\n" );

  string bundlesPath = getBundlesPath();
  Dir::Listing bundleLst( bundlesPath );
  while( bundleLst.getNext( entry ) )
  {
    const string dirPath = Dir::addPath( bundlesPath, entry.getFileName());
    if ( entry.isDir() && Dir::isDirEmpty( dirPath ) )
    {
      Dir::remove(dirPath);
    }
  }

  verbosePrintf( "Garbage collection complete\n" );
}
