// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ZBACKUP_HH_INCLUDED__
#define ZBACKUP_HH_INCLUDED__

#include "chunk_storage.hh"
#include "zbackup_base.hh"

using std::string;
using std::vector;
using std::bitset;

class ZBackup: public ZBackupBase
{
  ChunkStorage::Writer chunkStorageWriter;

public:
  ZBackup( string const & storageDir, string const & password,
           Config & configIn );

  /// Backs up the data from stdin
  void backupFromStdin( string const & outputFileName );
};

class ZRestore: public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZRestore( string const & storageDir, string const & password,
            Config & configIn );

  /// Restores the data to stdin
  void restoreToStdin( string const & inputFileName );
};

class ZCollect: public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZCollect( string const & storageDir, string const & password,
            Config & configIn );

  void gc();
};

class ZExchange
{
  ZBackupBase srcZBackupBase;
  ZBackupBase dstZBackupBase;

public:
  ZExchange( string const & srcStorageDir, string const & srcPassword,
             string const & dstStorageDir, string const & dstPassword,
             Config & configIn );

  Config config;

  /// Exchanges the data between storages
  void exchange();
};

#endif
