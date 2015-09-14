// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ZBACKUP_BASE_HH_INCLUDED
#define ZBACKUP_BASE_HH_INCLUDED

#include <exception>
#include <string>

#include "ex.hh"
#include "chunk_index.hh"
#include "config.hh"

struct Paths
{
  Config config;
  std::string storageDir;

  Paths( std::string const & storageDir );
  Paths( std::string const & storageDir, Config const & );

  std::string getTmpPath();
  std::string getRestorePath();
  std::string getCreatePath();
  std::string getBundlesPath();
  std::string getStorageInfoPath();
  std::string getExtendedStorageInfoPath();
  std::string getIndexPath();
  std::string getBackupsPath();
};

class ZBackupBase: public Paths
{
public:
  DEF_EX( Ex, "ZBackup exception", std::exception )
  DEF_EX_STR( exWontOverwrite, "Won't overwrite existing file", Ex )
  DEF_EX_STR( exInputError, "Error reading from input:", Ex )
  DEF_EX( exWontReadFromTerminal, "Won't read data from a terminal", Ex )
  DEF_EX( exStdoutError, "Error writing to standard output", Ex )
  DEF_EX( exWontWriteToTerminal, "Won't write data to a terminal", exStdoutError )
  DEF_EX( exSerializeError, "Failed to serialize data", Ex )
  DEF_EX( exParseError, "Failed to parse data", Ex )
  DEF_EX( exChecksumError, "Checksum error", Ex )
  DEF_EX_STR( exCantDeriveStorageDir, "The path must be within the backups/ dir:", Ex )

  /// Opens the storage
  ZBackupBase( std::string const & storageDir, std::string const & password );
  ZBackupBase( std::string const & storageDir, std::string const & password, Config & configIn );
  ZBackupBase( std::string const & storageDir, std::string const & password,
      bool prohibitChunkIndexLoading );
  ZBackupBase( std::string const & storageDir, std::string const & password, Config & configIn,
      bool prohibitChunkIndexLoading );

  /// Creates new storage
  static void initStorage( std::string const & storageDir, std::string const & password,
                           bool isEncrypted, Config const & );

  /// For a given file within the backups/ dir in the storage, returns its
  /// storage dir or throws an exception
  static std::string deriveStorageDirFromBackupsFile( std::string const & backupsFile, bool allowOutside = false );

  void propagateUpdate();

  void saveExtendedStorageInfo();

  void setPassword( std::string const & password );

  // returns true if data is changed
  bool spawnEditor( std::string & data, bool( * validator )
      ( string const &, string const & ) );

  // Edit current configuration
  // returns true if configuration is changed
  bool editConfigInteractively();

  StorageInfo storageInfo;
  EncryptionKey encryptionkey;
  ExtendedStorageInfo extendedStorageInfo;
  TmpMgr tmpMgr;
  ChunkIndex chunkIndex;
  Config config;

private:
  StorageInfo loadStorageInfo();
  ExtendedStorageInfo loadExtendedStorageInfo( EncryptionKey const & );
};

#endif
