// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <bitset>

#include "backup_creator.hh"
#include "backup_file.hh"
#include "backup_restorer.hh"
#include "debug.hh"
#include "dir.hh"
#include "encryption_key.hh"
#include "ex.hh"
#include "file.hh"
#include "mt.hh"
#include "sha256.hh"
#include "sptr.hh"
#include "storage_info_file.hh"
#include "zbackup.hh"
#include "index_file.hh"
#include "bundle.hh"

using std::vector;
using std::bitset;
using std::iterator;

Paths::Paths( string const & storageDir ): storageDir( storageDir )
{
}

string Paths::getTmpPath()
{
  return string( Dir::addPath( storageDir, "tmp" ) );
}

string Paths::getBundlesPath()
{
  return string( Dir::addPath( storageDir, "bundles" ) );
}

string Paths::getStorageInfoPath()
{
  return string( Dir::addPath( storageDir, "info" ) );
}

string Paths::getIndexPath()
{
  return string( Dir::addPath( storageDir, "index" ) );
}

string Paths::getBackupsPath()
{
  return string( Dir::addPath( storageDir, "backups" ) );
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), false )
{
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password,
                          bool prohibitChunkIndexLoading ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), prohibitChunkIndexLoading )
{
}

StorageInfo ZBackupBase::loadStorageInfo()
{
  StorageInfo storageInfo;

  StorageInfoFile::load( getStorageInfoPath(), storageInfo );

  return storageInfo;
}

void ZBackupBase::initStorage( string const & storageDir,
                               string const & password,
                               bool isEncrypted )
{
  StorageInfo storageInfo;
  // TODO: make the following configurable
  storageInfo.set_chunk_max_size( 65536 );
  storageInfo.set_bundle_max_payload_size( 0x200000 );

  if ( isEncrypted )
    EncryptionKey::generate( password,
                             *storageInfo.mutable_encryption_key() );

  Paths paths( storageDir );

  if ( !Dir::exists( storageDir ) )
    Dir::create( storageDir );

  if ( !Dir::exists( paths.getBundlesPath() ) )
    Dir::create( paths.getBundlesPath() );

  if ( !Dir::exists( paths.getBackupsPath() ) )
    Dir::create( paths.getBackupsPath() );

  if ( !Dir::exists( paths.getIndexPath() ) )
    Dir::create( paths.getIndexPath() );

  string storageInfoPath( paths.getStorageInfoPath() );

  if ( File::exists( storageInfoPath ) )
    throw exWontOverwrite( storageInfoPath );

  StorageInfoFile::save( storageInfoPath, storageInfo );
}

string ZBackupBase::deriveStorageDirFromBackupsFile( string const &
                                                     backupsFile, bool allowOutside )
{
  // TODO: handle cases when there's a backup/ folder within the backup/ folder
  // correctly
  if ( allowOutside )
    return Dir::getRealPath( backupsFile );

  string realPath = Dir::getRealPath( Dir::getDirName( backupsFile ) );
  size_t pos;
  if ( realPath.size() >= 8 && strcmp( realPath.c_str() + realPath.size() - 8,
                                       "/backups") == 0 )
    pos = realPath.size() - 8;
  else
    pos = realPath.rfind( "/backups/" );
  if ( pos == string::npos )
    throw exCantDeriveStorageDir( backupsFile );
  else
    return realPath.substr( 0, pos );
}

ZBackup::ZBackup( string const & storageDir, string const & password,
                  size_t threads ):
  ZBackupBase( storageDir, password ),
  chunkStorageWriter( storageInfo, encryptionkey, tmpMgr, chunkIndex,
                      getBundlesPath(), getIndexPath(), threads )
{
}

void ZBackup::backupFromStdin( string const & outputFileName )
{
  if ( isatty( fileno( stdin ) ) )
    throw exWontReadFromTerminal();

  if ( File::exists( outputFileName ) )
    throw exWontOverwrite( outputFileName );

  Sha256 sha256;
  BackupCreator backupCreator( storageInfo, chunkIndex, chunkStorageWriter );

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
    BackupCreator backupCreator( storageInfo, chunkIndex, chunkStorageWriter );
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
                    size_t threads, size_t cacheSize ):
  ZBackupBase( storageDir, password ),
  chunkStorageReader( storageInfo, encryptionkey, chunkIndex, getBundlesPath(),
                      cacheSize )
{
  this->threads = threads;
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

void ZRestore::gc()
{
  ChunkIndex chunkReindex( encryptionkey, tmpMgr, getIndexPath(), true );

  ChunkStorage::Writer chunkStorageWriter( storageInfo, encryptionkey, tmpMgr, chunkReindex,
                      getBundlesPath(), getIndexPath(), threads );

  string fileName;
  string backupsPath = getBackupsPath();

  Dir::Listing lst( backupsPath );

  Dir::Entry entry;

  class BundleChecker: public IndexProcessor
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
  } checker;

  checker.bundlesPath = getBundlesPath();
  checker.chunkStorageReader = &this->chunkStorageReader;
  checker.chunkStorageWriter = &chunkStorageWriter;
  checker.verbose = false;

  verbosePrintf( "Checking used chunks...\n" );

  while( lst.getNext( entry ) )
  {
    verbosePrintf( "Checking backup %s...\n", entry.getFileName().c_str() );

    BackupInfo backupInfo;

    BackupFile::load( Dir::addPath( backupsPath, entry.getFileName() ), encryptionkey, backupInfo );

    string backupData;

    BackupRestorer::restoreIterations( chunkStorageReader, backupInfo, backupData, &checker.usedChunkSet );

    BackupRestorer::restore( chunkStorageReader, backupData, NULL, &checker.usedChunkSet );
  }

  verbosePrintf( "Checking bundles...\n" );

  chunkIndex.loadIndex( checker );

  checker.commit();

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

ZExchange::ZExchange( string const & srcStorageDir, string const & srcPassword,
                    string const & dstStorageDir, string const & dstPassword,
                    bool prohibitChunkIndexLoading ):
  srcZBackupBase( srcStorageDir, srcPassword, prohibitChunkIndexLoading ),
  dstZBackupBase( dstStorageDir, dstPassword, prohibitChunkIndexLoading )
{
}

void ZExchange::exchange( string const & srcPath, string const & dstPath,
    bitset< BackupExchanger::Flags > const & exchange )
{
  vector< BackupExchanger::PendingExchangeRename > pendingExchangeRenames;

  if ( exchange.test( BackupExchanger::bundles ) )
  {
    verbosePrintf( "Searching for bundles...\n" );

    vector< string > bundles = BackupExchanger::recreateDirectories(
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

  if ( exchange.test( BackupExchanger::index ) )
  {
    verbosePrintf( "Searching for indicies...\n" );
    vector< string > indicies = BackupExchanger::recreateDirectories(
        srcZBackupBase.getIndexPath(), dstZBackupBase.getIndexPath() );

    for ( std::vector< string >::iterator it = indicies.begin(); it != indicies.end(); ++it )
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

  if ( exchange.test( BackupExchanger::backups ) )
  {
    BackupInfo backupInfo;

    verbosePrintf( "Searching for backups...\n" );
    vector< string > backups = BackupExchanger::recreateDirectories(
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

DEF_EX( exExchangeWithLessThanTwoKeys, "Specify password flag (--non-encrypted or --password-file)"
   " for import/export operation twice (first for source and second for destination)", std::exception )
DEF_EX( exNonEncryptedWithKey, "--non-encrypted and --password-file are incompatible", std::exception )
DEF_EX( exSpecifyEncryptionOptions, "Specify either --password-file or --non-encrypted", std::exception )
DEF_EX_STR( exInvalidThreadsValue, "Invalid threads value specified:", std::exception )

int main( int argc, char *argv[] )
{
  try
  {
    size_t const defaultThreads = getNumberOfCpus();
    size_t threads = defaultThreads;
    size_t const defaultCacheSizeMb = 40;
    size_t cacheSizeMb = defaultCacheSizeMb;
    vector< char const * > args;
    vector< string > passwords;
    bitset< BackupExchanger::Flags > exchange;

    for( int x = 1; x < argc; ++x )
    {
      if ( strcmp( argv[ x ], "--password-file" ) == 0 && x + 1 < argc )
      {
        // Read the password
        char const * passwordFile = argv[ x + 1 ];
        string passwordData;
        if ( passwordFile )
        {
          File f( passwordFile, File::ReadOnly );
          passwordData.resize( f.size() );
          f.read( &passwordData[ 0 ], passwordData.size() );

          // If the password ends with \n, remove that last \n. Many editors will
          // add \n there even if a user doesn't want them to
          if ( !passwordData.empty() &&
               passwordData[ passwordData.size() - 1 ] == '\n' )
            passwordData.resize( passwordData.size() - 1 );
          passwords.push_back( passwordData );
        }
        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--exchange" ) == 0 && x + 1 < argc )
      {
        char const * exchangeValue = argv[ x + 1 ];
        if ( strcmp( exchangeValue, "backups" ) == 0 )
          exchange.set( BackupExchanger::backups );
        else
        if ( strcmp( exchangeValue, "bundles" ) == 0 )
          exchange.set( BackupExchanger::bundles );
        else
        if ( strcmp( exchangeValue, "index" ) == 0 )
          exchange.set( BackupExchanger::index );
        else
        {
          fprintf( stderr, "Invalid exchange value specified: %s\n"
                   "Must be one of the following: backups, bundles, index\n",
                   exchangeValue );
          return EXIT_FAILURE;
        }

        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--non-encrypted" ) == 0 )
      {
          passwords.push_back( "" );
      }
      else
      if ( strcmp( argv[ x ], "--silent" ) == 0 )
        verboseMode = false;
      else
      if ( strcmp( argv[ x ], "--threads" ) == 0 && x + 1 < argc )
      {
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %n", &threads, &n ) != 1 ||
             argv[ x + 1 ][ n ] || threads < 1 )
          throw exInvalidThreadsValue( argv[ x + 1 ] );
        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--cache-size" ) == 0 && x + 1 < argc )
      {
        char suffix[ 16 ];
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %15s %n",
                     &cacheSizeMb, suffix, &n ) == 2 && !argv[ x + 1 ][ n ] )
        {
          // Check the suffix
          for ( char * c = suffix; *c; ++c )
            *c = tolower( *c );

          if ( strcmp( suffix, "mb" ) != 0 )
          {
            fprintf( stderr, "Invalid suffix specified in cache size: %s. "
                     "The only supported suffix is 'mb' for megabytes\n",
                     argv[ x + 1 ] );
            return EXIT_FAILURE;
          }

          ++x;
        }
        else
        {
          fprintf( stderr, "Invalid cache size value specified: %s. "
                   "Must be a number with the 'mb' suffix, e.g. '100mb'\n",
                   argv[ x + 1 ] );
          return EXIT_FAILURE;
        }
      }
      else
        args.push_back( argv[ x ] );
    }

    if ( args.size() < 1 ||
        ( args.size() == 1 &&
          ( strcmp( args[ 0 ], "-h" ) == 0 || strcmp( args[ 0 ], "--help" ) == 0 )
        )
       )
    {
      fprintf( stderr,
"ZBackup, a versatile deduplicating backup tool, version 1.3\n"
"Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>\n"
"Comes with no warranty. Licensed under GNU GPLv2 or later + OpenSSL.\n"
"Visit the project's home page at http://zbackup.org/\n\n"

"Usage: %s [flags] <command> [command args]\n"
"  Flags: --non-encrypted|--password-file <file>\n"
"          password flag should be specified twice if import/export\n"
"          command specified\n"
"         --silent (default is verbose)\n"
"         --threads <number> (default is %zu on your system)\n"
"         --cache-size <number> MB (default is %zu)\n"
"         --exchange [backups|bundles|index] (can be\n"
"          specified multiple times)\n"
"         --help|-h show this message\n"
"  Commands:\n"
"    init <storage path> - initializes new storage;\n"
"    backup <backup file name> - performs a backup from stdin;\n"
"    restore <backup file name> - restores a backup to stdout;\n"
"    export <source storage path> <destination storage path> -\n"
"            performs export from source to destination storage;\n"
"    import <source storage path> <destination storage path> -\n"
"            performs import from source to destination storage;\n"
"    gc <storage path> - performs chunk garbage collection.\n"
"  For export/import storage path must be valid (initialized) storage.\n"
"", *argv,
               defaultThreads, defaultCacheSizeMb );
      return EXIT_FAILURE;
    }

    if ( passwords.size() > 1 &&
        ( ( passwords[ 0 ].empty() && !passwords[ 1 ].empty() ) ||
          ( !passwords[ 0 ].empty() && passwords[ 1 ].empty() ) ) &&
        ( strcmp( args[ 0 ], "export" ) != 0 && strcmp( args[ 0 ], "import" ) != 0 ) )
      throw exNonEncryptedWithKey();
    else
      if ( passwords.size() < 2 &&
          ( strcmp( args[ 0 ], "export" ) == 0 || strcmp( args[ 0 ], "import" ) == 0 ) )
        throw exExchangeWithLessThanTwoKeys();
    else
      if ( passwords.size() < 1 )
        throw exSpecifyEncryptionOptions();

    if ( strcmp( args[ 0 ], "init" ) == 0 )
    {
      // Perform the init
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s init <storage path>\n", *argv );
        return EXIT_FAILURE;
      }

      ZBackup::initStorage( args[ 1 ], passwords[ 0 ], !passwords[ 0 ].empty() );
    }
    else
    if ( strcmp( args[ 0 ], "backup" ) == 0 )
    {
      // Perform the backup
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s backup <backup file name>\n",
                 *argv );
        return EXIT_FAILURE;
      }
      ZBackup zb( ZBackup::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                  passwords[ 0 ], threads );
      zb.backupFromStdin( args[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "restore" ) == 0 )
    {
      // Perform the restore
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s restore <backup file name>\n",
                 *argv );
        return EXIT_FAILURE;
      }
      ZRestore zr( ZRestore::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                   passwords[ 0 ], threads, cacheSizeMb * 1048576 );
      zr.restoreToStdin( args[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "export" ) == 0 || strcmp( args[ 0 ], "import" ) == 0 )
    {
      if ( args.size() != 3 )
      {
        fprintf( stderr, "Usage: %s %s <source storage path> <destination storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }
      if ( exchange.none() )
      {
        fprintf( stderr, "Specify any --exchange flag\n" );
        return EXIT_FAILURE;
      }

      int src, dst;
      if ( strcmp( args[ 0 ], "export" ) == 0 )
      {
        src = 1;
        dst = 2;
      }
      else
      if ( strcmp( args[ 0 ], "import" ) == 0 )
      {
        src = 2;
        dst = 1;
      }
      dPrintf( "%s src: %s\n", args[ 0 ], args[ src ] );
      dPrintf( "%s dst: %s\n", args[ 0 ], args[ dst ] );

      ZExchange ze( ZBackupBase::deriveStorageDirFromBackupsFile( args[ src ], true ),
                    passwords[ src - 1 ],
                    ZBackupBase::deriveStorageDirFromBackupsFile( args[ dst ], true ),
                    passwords[ dst - 1 ],
                    true );
      ze.exchange( args[ src ], args[ dst ], exchange );
    }
    else
    if ( strcmp( args[ 0 ], "gc" ) == 0 )
    {
      // Perform the restore
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s gc <backup directory>\n",
                 *argv );
        return EXIT_FAILURE;
      }
      ZRestore zr( args[ 1 ], passwords[ 0 ], threads, cacheSizeMb * 1048576 );
      zr.gc();
    }
    else
    {
      fprintf( stderr, "Error: unknown command line option: %s\n", args[ 0 ] );
      return EXIT_FAILURE;
    }
  }
  catch( std::exception & e )
  {
    fprintf( stderr, "%s\n", e.what() );
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
