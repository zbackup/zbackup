// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>

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

using std::vector;

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
  chunkIndex( encryptionkey, tmpMgr, getIndexPath() )
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
                               bool isEncrypted,
                               size_t chunkMaxSize,
                               size_t bundleMaxSize )
{
  StorageInfo storageInfo;
  storageInfo.set_chunk_max_size( chunkMaxSize );
  storageInfo.set_bundle_max_payload_size( bundleMaxSize );

  verbosePrintf( "Backup repository parameters: chunk_max[%u], bundle_max[%u]\n", storageInfo.chunk_max_size(), storageInfo.bundle_max_payload_size() );

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
                                                     backupsFile )
{
  // TODO: handle cases when there's a backup/ folder within the backup/ folder
  // correctly
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
                  size_t threads, size_t compressionLevel ):
  ZBackupBase( storageDir, password ),
  chunkStorageWriter( storageInfo, encryptionkey, tmpMgr, chunkIndex,
                      getBundlesPath(), getIndexPath(), threads, compressionLevel )
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

  verbosePrintf( "Backup repository parameters: chunk_max[%u], bundle_max[%u]\n", storageInfo.chunk_max_size(), storageInfo.bundle_max_payload_size() );

  time_t startTime = time( 0 );
  uint64_t totalDataSize = 0;

  for ( ; ; )
  {
    size_t toRead = backupCreator.getInputBufferSize();
    //dPrintf( "Reading up to %u bytes from stdin\n", toRead );

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
                    size_t cacheSize ):
  ZBackupBase( storageDir, password ),
  chunkStorageReader( storageInfo, encryptionkey, chunkIndex, getBundlesPath(),
                      cacheSize )
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

      BackupRestorer::restore( chunkStorageReader, backupData, stringWriter );
      backupInfo.mutable_backup_data()->swap( stringWriter.result );
      backupInfo.set_iterations( backupInfo.iterations() - 1 );
    }
    else
      break;
  }

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

  BackupRestorer::restore( chunkStorageReader, backupData, stdoutWriter );

  if ( stdoutWriter.sha256.finish() != backupInfo.sha256() )
    throw exChecksumError();
}

DEF_EX( exNonEncryptedWithKey, "--non-encrypted and --password-file are incompatible", std::exception )
DEF_EX( exSpecifyEncryptionOptions, "Specify either --password-file or --non-encrypted", std::exception )
DEF_EX_STR( exInvalidThreadsValue, "Invalid threads value specified:", std::exception )
DEF_EX_STR( exInvalidCompressionValue, "Invalid compression level value specified:", std::exception )

int main( int argc, char *argv[] )
{
  try
  {
    char const * passwordFile = 0;
    bool nonEncrypted = false;

    size_t const defaultThreads = getNumberOfCpus();
    size_t threads = defaultThreads;

    size_t const defaultCompression = 6;
    size_t compressionLevel = defaultCompression;

    size_t const defaultCacheSize = 40 * 1024 * 1024;
    size_t cacheSize = defaultCacheSize;

    size_t const defaultChunkMaxSize = 64 * 1024;
    size_t chunkMaxSize = defaultChunkMaxSize;

    size_t const defaultBundleMaxSize = 2 * 1024 * 1024;
    size_t bundleMaxSize = defaultBundleMaxSize;

    vector< char const * > args;

    for( int x = 1; x < argc; ++x )
    {
      if ( strcmp( argv[ x ], "--password-file" ) == 0 && x + 1 < argc )
      {
        passwordFile = argv[ x + 1 ];
        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--non-encrypted" ) == 0 )
        nonEncrypted = true;
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
      if ( strcmp( argv[ x ], "--lzma-compression-level" ) == 0 && x + 1 < argc )
      {
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %n", &compressionLevel, &n ) != 1 ||
             argv[ x + 1 ][ n ] || ( compressionLevel > 19 ) )
          throw exInvalidCompressionValue( argv[ x + 1 ] );
        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--cache-size" ) == 0 && x + 1 < argc )
      {
        char suffix[ 16 ];
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %15s %n",
                     &cacheSize, suffix, &n ) == 2 && !argv[ x + 1 ][ n ] )
        {
          // Convert to bytes
          cacheSize *= 1024 * 1024;
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
      if ( strcmp( argv[ x ], "--chunk-max-size" ) == 0 && x + 1 < argc )
      {
        char suffix[ 16 ];
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %15s %n",
                     &chunkMaxSize, suffix, &n ) == 2 && !argv[ x + 1 ][ n ] )
        {
          // Convert to bytes
          chunkMaxSize *= 1024;

          // Check the suffix
          for ( char * c = suffix; *c; ++c )
            *c = tolower( *c );

          if ( strcmp( suffix, "kb" ) != 0 )
          {
            fprintf( stderr, "Invalid suffix specified in chunk max size: %s. "
                     "The only supported suffix is 'kb' for kilobytes\n",
                     argv[ x + 1 ] );
            return EXIT_FAILURE;
          }

          ++x;
        }
        else
        {
          fprintf( stderr, "Invalid chunk max size value specified: %s. "
                   "Must be a number with the 'kb' suffix, e.g. '16kb'\n",
                   argv[ x + 1 ] );
          return EXIT_FAILURE;
        }
      }
      else
      if ( strcmp( argv[ x ], "--bundle-max-size" ) == 0 && x + 1 < argc )
      {
        char suffix[ 16 ];
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %15s %n",
                     &bundleMaxSize, suffix, &n ) == 2 && !argv[ x + 1 ][ n ] )
        {
          // Convert to bytes
          bundleMaxSize *= 1024 * 1024;

          // Check the suffix
          for ( char * c = suffix; *c; ++c )
            *c = tolower( *c );

          if ( strcmp( suffix, "mb" ) != 0 )
          {
            fprintf( stderr, "Invalid suffix specified in bundle max size: %s. "
                     "The only supported suffix is 'mb' for megabytes\n",
                     argv[ x + 1 ] );
            return EXIT_FAILURE;
          }

          ++x;
        }
        else
        {
          fprintf( stderr, "Invalid bundle max size value specified: %s. "
                   "Must be a number with the 'mb' suffix, e.g. '128mb'\n",
                   argv[ x + 1 ] );
          return EXIT_FAILURE;
        }
      }
      else
        args.push_back( argv[ x ] );
    }

    if ( nonEncrypted && passwordFile )
      throw exNonEncryptedWithKey();

    if ( args.size() < 1 )
    {
      fprintf( stderr,
"ZBackup, a versatile deduplicating backup tool, version 1.2\n"
"Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>\n"
"Comes with no warranty. Licensed under GNU GPLv2 or later.\n"
"Visit the project's home page at http://zbackup.org/\n\n"

"Usage: %s [flags] <command> [command args]\n"
"  Flags: --non-encrypted|--password-file <file>\n"
"         --silent (default is verbose)\n"
"         --threads <number> (default is %zu on your system)\n"
"         --lzma-compression-level <number> (default is %zu, 0-9 normal and 10-19 are extra preset)\n"
"         --cache-size <number> MB (default is %zu)\n"
"         --chunk-max-size <number> KB (default is %zu)\n"
"         --bundle-max-size <number> MB (default is %zu)\n"
"  Commands:\n"
"    init <storage path> - initializes new storage;\n"
"    backup <backup file name> - performs a backup from stdin;\n"
"    restore <backup file name> - restores a backup to stdout.\n", *argv,
               defaultThreads, defaultCompression, defaultCacheSize, defaultChunkMaxSize, defaultBundleMaxSize );
      return EXIT_FAILURE;
    }

    // Read the password
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
    }

    if ( strcmp( args[ 0 ], "init" ) == 0 )
    {
      // Perform the init
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s init <storage path>\n", *argv );
        return EXIT_FAILURE;
      }
      if ( !nonEncrypted && !passwordFile )
          throw exSpecifyEncryptionOptions();

      ZBackup::initStorage( args[ 1 ], passwordData, !nonEncrypted, chunkMaxSize, bundleMaxSize );
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
                  passwordData, threads, compressionLevel );
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
                   passwordData, cacheSize );
      zr.restoreToStdin( args[ 1 ] );
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
