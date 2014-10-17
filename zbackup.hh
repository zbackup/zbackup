// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ZBACKUP_HH_INCLUDED__
#define ZBACKUP_HH_INCLUDED__

#include <stddef.h>
#include <exception>
#include <string>
#include <vector>
#include <bitset>

#include "chunk_id.hh"
#include "chunk_index.hh"
#include "chunk_storage.hh"
#include "encryption_key.hh"
#include "ex.hh"
#include "tmp_mgr.hh"
#include "zbackup.pb.h"
#include "backup_exchanger.hh"

using std::string;
using std::vector;
using std::bitset;

struct Paths
{
  string storageDir;

  Paths( string const & storageDir );

  string getTmpPath();
  string getRestorePath();
  string getCreatePath();
  string getBundlesPath();
  string getStorageInfoPath();
  string getIndexPath();
  string getBackupsPath();
};

class ZBackupBase: public Paths
{
public:
  DEF_EX( Ex, "ZBackup exception", std::exception )
  DEF_EX_STR( exWontOverwrite, "Won't overwrite existing file", Ex )
  DEF_EX( exStdinError, "Error reading from standard input", Ex )
  DEF_EX( exWontReadFromTerminal, "Won't read data from a terminal", exStdinError )
  DEF_EX( exStdoutError, "Error writing to standard output", Ex )
  DEF_EX( exWontWriteToTerminal, "Won't write data to a terminal", exStdoutError )
  DEF_EX( exSerializeError, "Failed to serialize data", Ex )
  DEF_EX( exParseError, "Failed to parse data", Ex )
  DEF_EX( exChecksumError, "Checksum error", Ex )
  DEF_EX_STR( exCantDeriveStorageDir, "The path must be within the backups/ dir:", Ex )

  /// Opens the storage
  ZBackupBase( string const & storageDir, string const & password );
  ZBackupBase( string const & storageDir, string const & password, bool prohibitChunkIndexLoading );

  /// Creates new storage
  static void initStorage( string const & storageDir, string const & password,
                           bool isEncrypted );

  /// For a given file within the backups/ dir in the storage, returns its
  /// storage dir or throws an exception
  static string deriveStorageDirFromBackupsFile( string const & backupsFile, bool allowOutside = false );

  StorageInfo storageInfo;
  EncryptionKey encryptionkey;
  TmpMgr tmpMgr;
  ChunkIndex chunkIndex;
protected:

private:
  StorageInfo loadStorageInfo();
};

class ZBackup: public ZBackupBase
{
  ChunkStorage::Writer chunkStorageWriter;

public:
  ZBackup( string const & storageDir, string const & password,
           size_t threads );

  /// Backs up the data from stdin
  void backupFromStdin( string const & outputFileName );
};

class ZRestore: public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZRestore( string const & storageDir, string const & password,
            size_t cacheSize );

  /// Restores the data to stdin
  void restoreToStdin( string const & inputFileName );
};

class ZExchange
{
  ZBackupBase srcZBackupBase;
  ZBackupBase dstZBackupBase;

public:
  ZExchange( string const & srcStorageDir, string const & srcPassword,
            string const & dstStorageDir, string const & dstPassword,
            bool prohibitChunkIndexLoading );

  /// Exchanges the data between storages
  void exchange( string const & srcFileName, string const & dstFileName,
      bitset< BackupExchanger::Flags > const & exchange );
};

#endif
