// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zutils.hh"
#include "backup_creator.hh"
#include "sha256.hh"
#include "backup_collector.hh"
#include <unistd.h>

using std::vector;
using std::bitset;
using std::iterator;

ZBackup::ZBackup( string const & storageDir, string const & password,
                  Config & configIn ):
  ZBackupBase( storageDir, password, configIn ),
  chunkStorageWriter( config, encryptionkey, tmpMgr, chunkIndex,
                      getBundlesPath(), getIndexPath(), config.runtime.threads )
{
}

void ZBackup::backupFromStdin( string const & outputFileName )
{
  if ( isatty( fileno( stdin ) ) )
    throw exWontReadFromTerminal();

  if ( File::exists( outputFileName ) )
    throw exWontOverwrite( outputFileName );

  Sha256 sha256;
  BackupCreator backupCreator( config, chunkIndex, chunkStorageWriter );

  time_t startTime = time( 0 );
  uint64_t totalDataSize = 0;

  for ( ; ; )
  {
    size_t toRead = backupCreator.getInputBufferSize();
//    dPrintf( "Reading up to %u bytes from stdin\n", toRead );

    void * inputBuffer = backupCreator.getInputBuffer();
    size_t rd = fread( inputBuffer, 1, toRead, stdin );

    if ( !rd )
    {
      if ( feof( stdin ) )
      {
        dPrintf( "No more input on stdin\n" );
        break;
      }
      else
        throw exStdinError();
    }

    sha256.add( inputBuffer, rd );

    backupCreator.handleMoreData( rd );

    totalDataSize += rd;
  }

  // Finish up with the creator
  backupCreator.finish();

  string serialized;
  backupCreator.getBackupData( serialized );

  BackupInfo info;

  info.set_sha256( sha256.finish() );
  info.set_size( totalDataSize );

  // Shrink the serialized data iteratively until it wouldn't shrink anymore
  for ( ; ; )
  {
    BackupCreator backupCreator( config, chunkIndex, chunkStorageWriter );
    char const * ptr = serialized.data();
    size_t left = serialized.size();
    while( left )
    {
      size_t bufferSize = backupCreator.getInputBufferSize();
      size_t toCopy = bufferSize > left ? left : bufferSize;

      memcpy( backupCreator.getInputBuffer(), ptr, toCopy );
      backupCreator.handleMoreData( toCopy );
      ptr += toCopy;
      left -= toCopy;
    }

    backupCreator.finish();

    string newGen;
    backupCreator.getBackupData( newGen );

    if ( newGen.size() < serialized.size() )
    {
      serialized.swap( newGen );
      info.set_iterations( info.iterations() + 1 );
    }
    else
      break;
  }

  dPrintf( "Iterations: %u\n", info.iterations() );

  info.mutable_backup_data()->swap( serialized );

  info.set_time( time( 0 ) - startTime );

  // Commit the bundles to the disk before creating the final output file
  chunkStorageWriter.commit();

  // Now save the resulting BackupInfo

  sptr< TemporaryFile > tmpFile = tmpMgr.makeTemporaryFile();
  BackupFile::save( tmpFile->getFileName(), encryptionkey, info );
  tmpFile->moveOverTo( outputFileName );
}

ZRestore::ZRestore( string const & storageDir, string const & password,
                    Config & configIn ):
  ZBackupBase( storageDir, password, configIn ),
  chunkStorageReader( config, encryptionkey, chunkIndex, getBundlesPath(),
                      config.runtime.cacheSize )
{
}

void ZRestore::restoreToStdin( string const & inputFileName )
{
  if ( isatty( fileno( stdout ) ) )
    throw exWontWriteToTerminal();

  BackupInfo backupInfo;

  BackupFile::load( inputFileName, encryptionkey, backupInfo );

  string backupData;

  // Perform the iterations needed to get to the actual user backup data
  BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, NULL );

  struct StdoutWriter: public DataSink
  {
    Sha256 sha256;

    virtual void saveData( void const * data, size_t size )
    {
      sha256.add( data, size );
      if ( fwrite( data, size, 1, stdout ) != 1 )
        throw exStdoutError();
    }
  } stdoutWriter;

  BackupRestorer::restore( chunkStorageReader, backupData, &stdoutWriter, NULL );

  if ( stdoutWriter.sha256.finish() != backupInfo.sha256() )
    throw exChecksumError();
}

ZExchange::ZExchange( string const & srcStorageDir, string const & srcPassword,
                      string const & dstStorageDir, string const & dstPassword,
                      Config & configIn ):
  srcZBackupBase( srcStorageDir, srcPassword, configIn, true ),
  dstZBackupBase( dstStorageDir, dstPassword, configIn, true ),
  config( configIn )
{
}

void ZExchange::exchange()
{
  vector< BackupExchanger::PendingExchangeRename > pendingExchangeRenames;

  if ( config.runtime.exchange.test( BackupExchanger::bundles ) )
  {
    verbosePrintf( "Searching for bundles...\n" );

    vector< string > bundles = BackupExchanger::findOrRebuild(
        srcZBackupBase.getBundlesPath(), dstZBackupBase.getBundlesPath() );

    for ( std::vector< string >::iterator it = bundles.begin(); it != bundles.end(); ++it )
    {
      verbosePrintf( "Processing bundle file %s... ", it->c_str() );
      string outputFileName ( Dir::addPath( dstZBackupBase.getBundlesPath(), *it ) );
      if ( !File::exists( outputFileName ) )
      {
        sptr< Bundle::Reader > reader = new Bundle::Reader( Dir::addPath (
              srcZBackupBase.getBundlesPath(), *it ), srcZBackupBase.encryptionkey, true );
        sptr< Bundle::Creator > creator = new Bundle::Creator;
        sptr< TemporaryFile > bundleTempFile = dstZBackupBase.tmpMgr.makeTemporaryFile();
        creator->write( bundleTempFile->getFileName(), dstZBackupBase.encryptionkey, *reader );

        if ( creator.get() && reader.get() )
        {
          creator.reset();
          reader.reset();
          pendingExchangeRenames.push_back( BackupExchanger::PendingExchangeRename(
                bundleTempFile, outputFileName ) );
          verbosePrintf( "done.\n" );
        }
      }
      else
      {
        verbosePrintf( "file exists - skipped.\n" );
      }
    }

    verbosePrintf( "Bundle exchange completed.\n" );
  }

  if ( config.runtime.exchange.test( BackupExchanger::indexes ) )
  {
    verbosePrintf( "Searching for indexes...\n" );
    vector< string > indexes = BackupExchanger::findOrRebuild(
        srcZBackupBase.getIndexPath(), dstZBackupBase.getIndexPath() );

    for ( std::vector< string >::iterator it = indexes.begin(); it != indexes.end(); ++it )
    {
      verbosePrintf( "Processing index file %s... ", it->c_str() );
      string outputFileName ( Dir::addPath( dstZBackupBase.getIndexPath(), *it ) );
      if ( !File::exists( outputFileName ) )
      {
        sptr< IndexFile::Reader > reader = new IndexFile::Reader( srcZBackupBase.encryptionkey,
                                 Dir::addPath( srcZBackupBase.getIndexPath(), *it ) );
        sptr< TemporaryFile > indexTempFile = dstZBackupBase.tmpMgr.makeTemporaryFile();
        sptr< IndexFile::Writer > writer = new IndexFile::Writer( dstZBackupBase.encryptionkey,
            indexTempFile->getFileName() );

        BundleInfo bundleInfo;
        Bundle::Id bundleId;
        while( reader->readNextRecord( bundleInfo, bundleId ) )
        {
          writer->add( bundleInfo, bundleId );
        }

        if ( writer.get() && reader.get() )
        {
          writer.reset();
          reader.reset();
          pendingExchangeRenames.push_back( BackupExchanger::PendingExchangeRename(
                indexTempFile, outputFileName ) );
          verbosePrintf( "done.\n" );
        }
      }
      else
      {
        verbosePrintf( "file exists - skipped.\n" );
      }
    }

    verbosePrintf( "Index exchange completed.\n" );
  }

  if ( config.runtime.exchange.test( BackupExchanger::backups ) )
  {
    BackupInfo backupInfo;

    verbosePrintf( "Searching for backups...\n" );
    vector< string > backups = BackupExchanger::findOrRebuild(
        srcZBackupBase.getBackupsPath(), dstZBackupBase.getBackupsPath() );

    for ( std::vector< string >::iterator it = backups.begin(); it != backups.end(); ++it )
    {
      verbosePrintf( "Processing backup file %s... ", it->c_str() );
      string outputFileName ( Dir::addPath( dstZBackupBase.getBackupsPath(), *it ) );
      if ( !File::exists( outputFileName ) )
      {
        BackupFile::load( Dir::addPath( srcZBackupBase.getBackupsPath(), *it ),
            srcZBackupBase.encryptionkey, backupInfo );
        sptr< TemporaryFile > tmpFile = dstZBackupBase.tmpMgr.makeTemporaryFile();
        BackupFile::save( tmpFile->getFileName(), dstZBackupBase.encryptionkey,
            backupInfo );
        pendingExchangeRenames.push_back( BackupExchanger::PendingExchangeRename(
                tmpFile, outputFileName ) );
        verbosePrintf( "done.\n" );
      }
      else
      {
        verbosePrintf( "file exists - skipped.\n" );
      }
    }

    verbosePrintf( "Backup exchange completed.\n" );
  }

  if ( pendingExchangeRenames.size() > 0 )
  {
    verbosePrintf( "Moving files from temp directory to appropriate places... " );
    for ( size_t x = pendingExchangeRenames.size(); x--; )
    {
      BackupExchanger::PendingExchangeRename & r = pendingExchangeRenames[ x ];
      r.first->moveOverTo( r.second );
      if ( r.first.get() )
      {
        r.first.reset();
      }
    }
    pendingExchangeRenames.clear();
    verbosePrintf( "done.\n" );
  }
}

ZCollector::ZCollector( string const & storageDir, string const & password,
                    Config & configIn ):
  ZBackupBase( storageDir, password, configIn ),
  chunkStorageReader( config, encryptionkey, chunkIndex, getBundlesPath(),
                      config.runtime.cacheSize )
{
}

void ZCollector::gcChunks()
{
  ChunkIndex chunkReindex( encryptionkey, tmpMgr, getIndexPath(), true );

  ChunkStorage::Writer chunkStorageWriter( config, encryptionkey, tmpMgr,
      chunkReindex, getBundlesPath(), getIndexPath(), config.runtime.threads );

  string fileName;

  Dir::Entry entry;

  BundleCollector collector;
  collector.bundlesPath = getBundlesPath();
  collector.chunkStorageReader = &this->chunkStorageReader;
  collector.chunkStorageWriter = &chunkStorageWriter;

  verbosePrintf( "Checking used chunks...\n" );

  verbosePrintf( "Searching for backups...\n" );
  vector< string > backups = BackupExchanger::findOrRebuild( getBackupsPath() );

  for ( std::vector< string >::iterator it = backups.begin(); it != backups.end(); ++it )
  {
    string backup( Dir::addPath( getBackupsPath(), *it ) );

    verbosePrintf( "Checking backup %s...\n", backup.c_str() );

    BackupInfo backupInfo;

    BackupFile::load( backup, encryptionkey, backupInfo );

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
      Dir::remove( dirPath );
    }
  }

  verbosePrintf( "Garbage collection complete\n" );
}

void ZCollector::gcIndexes()
{
}
