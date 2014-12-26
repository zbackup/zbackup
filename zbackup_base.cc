// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zbackup_base.hh"

#include "storage_info_file.hh"
#include "compression.hh"
#include "debug.hh"

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
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), false )
{
  dPrintf("%s repo instantiated and initialized\n", storageDir.c_str() );
}

ZBackupBase::ZBackupBase( string const & storageDir, string const & password,
                          bool prohibitChunkIndexLoading ):
  Paths( storageDir ), storageInfo( loadStorageInfo() ),
  encryptionkey( password, storageInfo.has_encryption_key() ?
                   &storageInfo.encryption_key() : 0 ),
  extendedStorageInfo( loadExtendedStorageInfo( encryptionkey ) ),
  tmpMgr( getTmpPath() ),
  chunkIndex( encryptionkey, tmpMgr, getIndexPath(), prohibitChunkIndexLoading )
{
  dPrintf("%s repo instantiated and initialized\n", storageDir.c_str() );
}

StorageInfo ZBackupBase::loadStorageInfo()
{
  StorageInfo storageInfo;

  StorageInfoFile::load( getStorageInfoPath(), storageInfo );

  return storageInfo;
}

ExtendedStorageInfo ZBackupBase::loadExtendedStorageInfo( EncryptionKey const & encryptionkey )
{
  ExtendedStorageInfo extendedStorageInfo;

  ExtendedStorageInfoFile::load( getExtendedStorageInfoPath(), encryptionkey, extendedStorageInfo );

  return extendedStorageInfo;
}

void ZBackupBase::initStorage( string const & storageDir,
                               string const & password,
                               bool isEncrypted )
{
  StorageInfo storageInfo;
  ExtendedStorageInfo extendedStorageInfo;

  // TODO: Make a proper setup of initial values
  storageInfo.set_chunk_max_size( 65536 );
  storageInfo.set_bundle_max_payload_size( 0x200000 );
  storageInfo.set_default_compression_method(
      Compression::CompressionMethod::defaultCompression->getName() );

  extendedStorageInfo.mutable_config()->mutable_chunk()->set_max_size(
      extendedStorageInfo.config().chunk().max_size() );
  extendedStorageInfo.mutable_config()->mutable_bundle()->set_max_payload_size(
      extendedStorageInfo.config().bundle().max_payload_size() );
  extendedStorageInfo.mutable_config()->mutable_bundle()->set_default_compression_method(
      extendedStorageInfo.config().bundle().default_compression_method() );

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

void ZBackupBase::useDefaultCompressionMethod()
{
  std::string compression_method_name = storageInfo.default_compression_method();
  const_sptr<Compression::CompressionMethod> compression
      = Compression::CompressionMethod::findCompression( compression_method_name );
  Compression::CompressionMethod::defaultCompression = compression;
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
  ExtendedStorageInfoFile::save( getExtendedStorageInfoPath(), encryptionkey, extendedStorageInfo );
}
