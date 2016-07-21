// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zutils.hh"
#include "backup_creator.hh"
#include "sha256.hh"
#include "backup_collector.hh"
#include "utils.hh"
#include "buse.h"
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
  backupFromFileHandle( "stdin", stdin, outputFileName );
}

/// Backs up the data from a file
void ZBackup::backupFromFile( string const & inputFileName, string const & outputFileName,
                              bool checkFileSize )
{
  File inputFile( inputFileName, File::ReadOnly );
  if ( checkFileSize && inputFile.size() < config.runtime.backupMinimalSize )
    fprintf( stderr, "WARNING: skipping file %s because its size (use -O backup.minimalSize to adjust)\n",
        inputFileName.c_str() );
  else
    backupFromFileHandle( inputFileName, inputFile.file(), outputFileName );
}

/// Backs up the data from a directory
void ZBackup::backupFromDirectory( string const & inputDirectoryName, string const & outputDirectoryName )
{
  std::list< string > dirs;
  dirs.push_front( inputDirectoryName );
  
  while ( !dirs.empty() )
  {
    string dir = dirs.front();
    dirs.pop_front();
    
    Dir::Listing list( dir );
    Dir::Entry e;
    while ( list.getNext( e ) )
    {
      string srcPath = Dir::addPath( dir, e.getFileName() );
      string relativePath = srcPath.substr( inputDirectoryName.size() );

      // Chop leading slash, which may be present depending on whether the 
      // command line arg had a trailing slash or not
      while ( !relativePath.empty() && Dir::separator() == relativePath[ 0 ] )
        relativePath = relativePath.substr( 1 );

      // Calculate output path as function of path relative to input dir
      string outputPath = Dir::addPath( outputDirectoryName, relativePath );

      // Make sure directory structure for destination file exists
      string destDir = Dir::getDirName( outputPath );
      if ( ! Dir::exists( destDir ) )
        Dir::create( destDir );

      if ( e.isDir() ) // recurse dir tree
      {
        if ( ! Dir::exists( outputPath ) )
          Dir::create( outputPath );
        dirs.push_front( srcPath );
      }
      else if ( File::special( srcPath ) )
        fprintf( stderr, "WARNING: ignoring special file: %s\n", srcPath.c_str() );
      else 
        backupFromFile( srcPath, outputPath, true );
    }
  }
}

/// Backs up the data from a FILE handle
void ZBackup::backupFromFileHandle( string const & inputName, FILE* inputFileHandle, string const & outputFileName )
{
  if ( File::exists( outputFileName ) )
    throw exWontOverwrite( outputFileName );

  Sha256 sha256;
  BackupCreator backupCreator( config, chunkIndex, chunkStorageWriter );

  time_t startTime = time( 0 );
  uint64_t totalDataSize = 0;

  for ( ; ; )
  {
    size_t toRead = backupCreator.getInputBufferSize();
//    dPrintf( "Reading up to %u bytes on input\n", toRead );

    void * inputBuffer = backupCreator.getInputBuffer();
    size_t rd = fread( inputBuffer, 1, toRead, inputFileHandle );

    if ( !rd )
    {
      if ( feof( inputFileHandle ) )
      {
        dPrintf( "No more input from %s\n", inputName.c_str() );
        break;
      }
      else
        throw exInputError( inputName );
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

void ZRestore::restoreToFile( string const & inputFileName, string const & outputFileName )
{
  BackupInfo backupInfo;

  BackupFile::load( inputFileName, encryptionkey, backupInfo );

  string backupData;

  // Perform the iterations needed to get to the actual user backup data
  BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, NULL );

  UnbufferedFile f( outputFileName.data(), UnbufferedFile::ReadWrite );

  struct FileWriter: public SeekableSink
  {
    UnbufferedFile *f;

    FileWriter( UnbufferedFile *f ):
      f( f )
    {
    }

    virtual void saveData( int64_t position, void const * data, size_t size )
    {
      f->seek( position );
      f->write( data, size );
    }
  } seekWriter( &f );

  BackupRestorer::ChunkMap map;
  BackupRestorer::restore( chunkStorageReader, backupData, NULL, NULL, &map, &seekWriter );
  BackupRestorer::restoreMap( chunkStorageReader, &map, &seekWriter );

  Sha256 sha256;
  string buf;
  buf.resize( 0x100000 );
  size_t r;
  f.seek( 0 );
  while ( ( r = f.read( (void*)buf.data(), buf.size() ) ) > 0 )
    sha256.add( buf.data(), r );
  if ( sha256.finish() != backupInfo.sha256() )
    throw exChecksumError();
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

  BackupRestorer::restore( chunkStorageReader, backupData, &stdoutWriter, NULL, NULL, NULL );

  if ( stdoutWriter.sha256.finish() != backupInfo.sha256() )
    throw exChecksumError();
}

static int buse_read(void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
  dPrintf( "NBD read offset=%lu, size=%u\n", offset, len );

  BackupRestorer::IndexedRestorer & restorer = *(BackupRestorer::IndexedRestorer *)userdata;

  if ( offset > restorer.size() )
  {
     // Reading behind data end (block padding).
     return 0;
  }

  // Truncate read chunk by available data (for cases when reading data that is not padded to block size).
  u_int32_t clippedLen = std::min( int64_t(offset) + len, restorer.size() ) - offset;

  restorer.saveData( offset, buf, clippedLen );

  return 0;
}

void ZRestore::startNBDServer( string const & inputFileName, string const & nbdDevice )
{
  BackupInfo backupInfo;

  BackupFile::load( inputFileName, encryptionkey, backupInfo );

  string backupData;

  // Perform the iterations needed to get to the actual user backup data
  BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, NULL );

  BackupRestorer::IndexedRestorer restorer( chunkStorageReader, backupData );

  // TODO: should be configurable
  size_t const block_size = 1024;
  size_t num_blocks;
  if ( restorer.size() % block_size != 0 )
  {
    num_blocks = restorer.size() / block_size + 1;
    fprintf( stderr, "WARNING: data size %zi is not aligned with block size %zi, "
                     "device will be padded with zeros\n", restorer.size(), block_size );
  }
  else
  {
    num_blocks = restorer.size() / block_size;
  }

  static struct buse_operations aop;
  memset(&aop, 0, sizeof(aop));
  aop.read = buse_read;
  aop.block_size = block_size;
  aop.num_blocks = num_blocks;

  buse_main(nbdDevice.c_str(), &aop, (void *)&restorer);
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

    vector< string > bundles = Utils::findOrRebuild(
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
    vector< string > indexes = Utils::findOrRebuild(
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
    vector< string > backups = Utils::findOrRebuild(
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

void ZCollector::gc( bool gcDeep )
{
  ChunkIndex chunkReindex( encryptionkey, tmpMgr, getIndexPath(), true );

  ChunkStorage::Writer chunkStorageWriter( config, encryptionkey, tmpMgr,
      chunkReindex, getBundlesPath(), getIndexPath(), config.runtime.threads );

  string fileName;

  BundleCollector collector( getBundlesPath(), &chunkStorageReader, &chunkStorageWriter,
      gcDeep, config );

  verbosePrintf( "Performing garbage collection...\n" );

  verbosePrintf( "Searching for backups...\n" );
  vector< string > backups = Utils::findOrRebuild( getBackupsPath() );

  for ( std::vector< string >::iterator it = backups.begin(); it != backups.end(); ++it )
  {
    string backup( Dir::addPath( getBackupsPath(), *it ) );

    verbosePrintf( "Checking backup %s...\n", backup.c_str() );

    BackupInfo backupInfo;

    BackupFile::load( backup, encryptionkey, backupInfo );

    string backupData;

    BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, &collector.usedChunkSet );

    BackupRestorer::restore( chunkStorageReader, backupData, NULL, &collector.usedChunkSet, NULL, NULL );
  }

  verbosePrintf( "Checking bundles...\n" );

  chunkIndex.loadIndex( collector );

  collector.commit();

  verbosePrintf( "Cleaning up...\n" );

  string bundlesPath = getBundlesPath();
  Dir::Listing bundleLst( bundlesPath );
  Dir::Entry entry;
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

ZInspect::ZInspect( string const & storageDir, string const & password,
    Config & configIn ):
  ZBackupBase( storageDir, password, configIn, true )
{
}

ZInspect::ZInspect( string const & storageDir, string const & password,
    Config & configIn, bool deep ):
  ZBackupBase( storageDir, password, configIn, !deep )
{
}

void ZInspect::inspect( string const & inputFileName )
{
  BackupInfo backupInfo;

  BackupFile::load( inputFileName, encryptionkey, backupInfo );

  string out;
  out += "Backup file: ";
  out += inputFileName;

  out += "\nRestore iterations: ";
  out += Utils::numberToString( backupInfo.iterations() );

  out += "\nOriginal size: ";
  out += Utils::numberToString( backupInfo.size() );

  out += "\nDuration (seconds): ";
  out += Utils::numberToString( backupInfo.time() );

  out += "\nSHA256 sum of data: ";
  out += Utils::toHex( backupInfo.sha256() );

  // Index is loaded so mode is "deep", let's get chunk map
  if ( chunkIndex.size() )
  {
    out += "\nBundles containing backup chunks:\n";
    ChunkStorage::Reader chunkStorageReader( config, encryptionkey, chunkIndex, getBundlesPath(),
         config.runtime.cacheSize );
    string backupData;
    BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, NULL );
    BackupRestorer::ChunkMap map;
    BackupRestorer::restore( chunkStorageReader, backupData, NULL, NULL, &map, NULL );

    for ( BackupRestorer::ChunkMap::const_iterator it = map.begin(); it != map.end(); it++ )
    {
      out += Utils::toHex( string( (*it).first.blob, Bundle::IdSize ) );
      out += "\n";
    }
  }
  else
    out += "\n";

  fprintf( stderr, "%s", out.c_str() );
}
