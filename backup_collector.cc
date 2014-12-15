// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "backup_collector.hh"

#include <string>
#include <vector>

#include "bundle.hh"
#include "chunk_index.hh"
#include "backup_restorer.hh"
#include "backup_file.hh"

#include "debug.hh"

using std::string;

namespace {

class BundleCollector: public IndexProcessor
{
private:
  Bundle::Id savedId;
  int totalChunks, usedChunks, indexTotalChunks, indexUsedChunks;
  int indexModifiedBundles, indexKeptBundles, indexRemovedBundles;
  bool indexModified;
  vector< string > filesToUnlink;
  
public:
  string bundlesPath;
  bool verbose;
  ChunkStorage::Reader *chunkStorageReader;
  ChunkStorage::Writer *chunkStorageWriter;
  BackupRestorer::ChunkSet usedChunkSet;

  void startIndex( string const & indexFn )
  {
    indexModified = false;
    indexTotalChunks = indexUsedChunks = 0;
    indexModifiedBundles = indexKeptBundles = indexRemovedBundles = 0;
  }

  void finishIndex( string const & indexFn )
  {
    if ( indexModified )
    {
      verbosePrintf( "Chunks: %d used / %d total, bundles: %d kept / %d modified / %d removed\n",
                     indexUsedChunks, indexTotalChunks, indexKeptBundles, indexModifiedBundles, indexRemovedBundles);
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
    totalChunks++;
    if ( usedChunkSet.find( chunkId ) != usedChunkSet.end() )
    {
      usedChunks++;
    }
  }

  void finishBundle( Bundle::Id const & bundleId, BundleInfo const & info )
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

  void commit()
  {
    for ( int i = filesToUnlink.size(); i--; )
    {
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

void ZCollector::gc()
{
  ChunkIndex chunkReindex( encryptionkey, tmpMgr, getIndexPath(), true );

  ChunkStorage::Writer chunkStorageWriter( storageInfo, encryptionkey, tmpMgr, chunkReindex,
                      getBundlesPath(), getIndexPath(), threads );

  string fileName;
  string backupsPath = getBackupsPath();

  Dir::Listing lst( backupsPath );

  Dir::Entry entry;

  BundleCollector collector;
  collector.bundlesPath = getBundlesPath();
  collector.chunkStorageReader = &this->chunkStorageReader;
  collector.chunkStorageWriter = &chunkStorageWriter;
  collector.verbose = false;

  verbosePrintf( "Checking used chunks...\n" );

  while( lst.getNext( entry ) )
  {
    verbosePrintf( "Checking backup %s...\n", entry.getFileName().c_str() );

    BackupInfo backupInfo;

    BackupFile::load( Dir::addPath( backupsPath, entry.getFileName() ), encryptionkey, backupInfo );

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
    if (entry.isDir() && Dir::isDirEmpty(dirPath)) {
      Dir::remove(dirPath);
    }
  }
  
  verbosePrintf( "Garbage collection complete\n" );
}
