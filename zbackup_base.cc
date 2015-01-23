// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <sys/wait.h>
#include <cerrno>

#include "zbackup_base.hh"

#include "storage_info_file.hh"
#include "compression.hh"
#include "debug.hh"

// TODO: make configurable by cmake
#if defined(_PATH_VI)
# define EDITOR _PATH_VI
#else
# define EDITOR "/bin/vi"
#endif

#ifndef _PATH_BSHELL
# define _PATH_BSHELL "/bin/sh"
#endif

using std::string;

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

string Paths::getExtendedStorageInfoPath()
{
  return string( Dir::addPath( storageDir, "info_extended" ) );
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
  extendedStorageInfo( loadExtendedStorageInfo( encryptionkey ) ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), false ),
  config( extendedStorageInfo.mutable_config() )
{
  propagateUpdate();
  dPrintf("%s for %s is instantiated and initialized\n", __CLASS,
      storageDir.c_str() );
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password,
                          Config & configIn ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  extendedStorageInfo( loadExtendedStorageInfo( encryptionkey ) ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), false ),
  config( configIn, extendedStorageInfo.mutable_config() )
{
  propagateUpdate();
  dPrintf("%s for %s is instantiated and initialized\n", __CLASS,
      storageDir.c_str() );
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password,
                          bool prohibitChunkIndexLoading ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  extendedStorageInfo( loadExtendedStorageInfo( encryptionkey ) ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), prohibitChunkIndexLoading ),
  config( extendedStorageInfo.mutable_config() )
{
  propagateUpdate();
  dPrintf("%s for %s is instantiated and initialized\n", __CLASS,
      storageDir.c_str() );
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password,
                          Config & configIn, bool prohibitChunkIndexLoading ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  extendedStorageInfo( loadExtendedStorageInfo( encryptionkey ) ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), prohibitChunkIndexLoading ),
  config( configIn, extendedStorageInfo.mutable_config() )
{
  propagateUpdate();
  dPrintf("%s for %s is instantiated and initialized\n", __CLASS,
      storageDir.c_str() );
}

// Update all internal variables according to real configuration
// Dunno why someone need to store duplicate information
// in deduplication utility
void ZBackupBase::propagateUpdate()
{
  const_sptr< Compression::CompressionMethod > compression =
    Compression::CompressionMethod::findCompression(
      config.GET_STORABLE( bundle, compression_method ) );
  Compression::CompressionMethod::selectedCompression = compression;
}

StorageInfo ZBackupBase::loadStorageInfo()
{
  StorageInfo storageInfo;

  StorageInfoFile::load( getStorageInfoPath(), storageInfo );

  return storageInfo;
}

ExtendedStorageInfo ZBackupBase::loadExtendedStorageInfo(
    EncryptionKey const & encryptionkey )
{
  try
  {
    ExtendedStorageInfo extendedStorageInfo;

    ExtendedStorageInfoFile::load( getExtendedStorageInfoPath(), encryptionkey,
        extendedStorageInfo );

    return extendedStorageInfo;
  }
  catch ( UnbufferedFile::exCantOpen & ex )
  {
    verbosePrintf( "Can't open extended storage info (info_extended)!\n"
                   "Attempting to start repo migration.\n" );

    if ( !File::exists( getExtendedStorageInfoPath() ) )
    {
      ExtendedStorageInfo extendedStorageInfo;
      Config config( extendedStorageInfo.mutable_config() );
      config.SET_STORABLE( chunk, max_size, storageInfo.chunk_max_size() );
      config.SET_STORABLE( bundle, max_payload_size,
          storageInfo.bundle_max_payload_size() );
      config.SET_STORABLE( bundle, compression_method,
          storageInfo.default_compression_method() );

      ExtendedStorageInfoFile::save( getExtendedStorageInfoPath(), encryptionkey,
          extendedStorageInfo );

      verbosePrintf( "Done.\n" );

      return loadExtendedStorageInfo( encryptionkey );
    }
    else
    {
      fprintf( stderr, "info_extended exists but can't be opened!\n"
                       "Please check file permissions.\n" );
    }
  }
}

void ZBackupBase::initStorage( string const & storageDir,
                               string const & password,
                               bool isEncrypted )
{
  StorageInfo storageInfo;
  ExtendedStorageInfo extendedStorageInfo;
  Config config( extendedStorageInfo.mutable_config() );
  config.reset_storable();

  EncryptionKey encryptionkey = EncryptionKey::noKey();

  if ( isEncrypted )
    EncryptionKey::generate( password,
                             *storageInfo.mutable_encryption_key(),
                             encryptionkey );

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
  string extendedStorageInfoPath( paths.getExtendedStorageInfoPath() );

  if ( File::exists( storageInfoPath ) )
    throw exWontOverwrite( storageInfoPath );

  encryptionkey = EncryptionKey( password, storageInfo.has_encryption_key() ?
      &storageInfo.encryption_key() : 0 );

  StorageInfoFile::save( storageInfoPath, storageInfo );
  ExtendedStorageInfoFile::save( extendedStorageInfoPath, encryptionkey, extendedStorageInfo );
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

void ZBackupBase::setPassword( string const & password )
{
  EncryptionKey::generate( password,
                         *storageInfo.mutable_encryption_key(), encryptionkey );

  StorageInfoFile::save( getStorageInfoPath(), storageInfo );

  EncryptionKey encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 );
}

void ZBackupBase::saveExtendedStorageInfo()
{
  ExtendedStorageInfoFile::save( getExtendedStorageInfoPath(), encryptionkey,
      extendedStorageInfo );
}

bool ZBackupBase::spawnEditor( string & data, bool( * validator )
    ( string const &, string const & ) )
{
  // Based on ideas found in cronie-1.4.4-12.el6
  // Initially it was just a copy-paste from edit_cmd (crontab.c)

  /* Turn off signals. */
  (void) signal( SIGHUP, SIG_IGN );
  (void) signal( SIGINT, SIG_IGN );
  (void) signal( SIGQUIT, SIG_IGN );

  sptr< TemporaryFile > tmpFile = tmpMgr.makeTemporaryFile();
  const char * tmpFileName = tmpFile->getFileName().c_str();

  sptr< File> tmpDataFile = new File( tmpFileName, File::WriteOnly );
  tmpDataFile->writeRecords( data.c_str(), data.size(), 1 );

again:
  tmpDataFile->rewind();
  if ( tmpDataFile->error() )
  {
    verbosePrintf( "Error while writing data to %s\n", tmpFileName );
fatal:
    tmpFile.reset();
    exit( EXIT_FAILURE );
  }

  char * editorEnv;
  string editor;
  if ( ( ( editorEnv = getenv( "VISUAL" ) ) == NULL || *editorEnv == '\0' ) &&
      ( ( editorEnv = getenv( "EDITOR" ) ) == NULL || *editorEnv == '\0' ) )
    editor.assign( EDITOR );
  else
    editor.assign( editorEnv );

  /* we still have the file open.  editors will generally rewrite the
   * original file rather than renaming/unlinking it and starting a
   * new one; even backup files are supposed to be made by copying
   * rather than by renaming.  if some editor does not support this,
   * then don't use it.  the security problems are more severe if we
   * close and reopen the file around the edit.
   */

  string shellArgs;
  shellArgs += editor;
  shellArgs += " ";
  shellArgs += tmpFileName;

  pid_t pid, xpid;

  switch ( pid = fork() )
  {
    case -1:
      perror( "fork" );
      goto fatal;
    case 0:
      /* child */
      dPrintf( "Spawning editor: %s %s %s %s\n", _PATH_BSHELL, _PATH_BSHELL,
          "-c", shellArgs.c_str() );
      execlp( _PATH_BSHELL, _PATH_BSHELL, "-c", shellArgs.c_str(), (char *) 0 );
      perror( editor.c_str() );
      exit( EXIT_FAILURE );
    /*NOTREACHED*/
    default:
      /* parent */
      break;
  }

  /* parent */
  int waiter;
  for ( ; ; )
  {
    xpid = waitpid( pid, &waiter, 0 );
    if ( xpid == -1 )
    {
      if ( errno != EINTR )
        verbosePrintf( "waitpid() failed waiting for PID %ld from \"%s\": %s\n",
            (long) pid, editor.c_str(), strerror( errno ) );
    }
    else
    if (xpid != pid)
    {
      verbosePrintf( "wrong PID (%ld != %ld) from \"%s\"\n",
          (long) xpid, (long) pid, editor.c_str() );
      goto fatal;
    }
    else
    if ( WIFEXITED( waiter ) && WEXITSTATUS( waiter ) )
    {
      verbosePrintf( "\"%s\" exited with status %d\n",
          editor.c_str(), WEXITSTATUS( waiter ) );
      goto fatal;
    }
    else
    if ( WIFSIGNALED( waiter ) )
    {
      verbosePrintf( "\"%s\" killed; signal %d (%score dumped)\n",
          editor.c_str(), WTERMSIG( waiter ),
          WCOREDUMP( waiter ) ? "" : "no ");
      goto fatal;
    }
    else
      break;
  }
  (void) signal( SIGHUP, SIG_DFL );
  (void) signal( SIGINT, SIG_DFL );
  (void) signal( SIGQUIT, SIG_DFL );

  tmpDataFile->close();
  tmpDataFile = new File( tmpFileName, File::ReadOnly );

  string newData;
  newData.resize( tmpDataFile->size() );
  tmpDataFile->read( &newData[ 0 ], newData.size() );
  bool isChanged = false;

  bool valid = validator( data, newData );

  switch ( valid )
  {
    case true:
      goto success;
    case false:
      for ( ; ; )
      {
        fprintf( stderr, "Supplied data is not valid\n" );
        fflush( stderr );
        printf( "Do you want to retry the same edit? " );
        fflush( stdout );

        string input;
        input.resize( 131072 ); // Should I choose another magic value?
        if ( fgets( &input[ 0 ], input.size(), stdin ) == 0L )
          continue;

        switch ( input[ 0 ] )
        {
          case 'y':
          case 'Y':
            goto again;
          case 'n':
          case 'N':
            verbosePrintf( "Data is kept intact\n" );
            goto end;
          default:
            fprintf( stderr, "Enter Y or N\n" );
        }
      }
  }

success:
  isChanged = true;
  data.assign( newData );

end:
  tmpDataFile.reset();
  tmpFile.reset();

  return isChanged;
}

bool ZBackupBase::editConfigInteractively()
{
  string configData( Config::toString( *config.storable ) );

  if ( !spawnEditor( configData, &Config::validateProto ) )
    return false;

  ConfigInfo newConfig;
  Config::parseProto( configData, &newConfig );
  if ( Config::toString( *config.storable ) ==
      Config::toString( newConfig ) )
  {
    verbosePrintf( "No changes made to config\n" );
    return false;
  }

  verbosePrintf( "Updating configuration...\n" );
  config.storable->MergeFrom( newConfig );
  verbosePrintf(
"Configuration successfully updated!\n"
"Updated configuration:\n%s", Config::toString( *config.storable ).c_str() );

  return true;
}
