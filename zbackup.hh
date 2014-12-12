// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
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
#include "zbackup_base.hh"
#include "backup_exchanger.hh"

using std::string;
using std::vector;
using std::bitset;

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

class ZCollect: public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;
  size_t threads;

public:
  ZCollect( string const & storageDir, string const & password,
            size_t threads, size_t cacheSize );

  void gc();
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
