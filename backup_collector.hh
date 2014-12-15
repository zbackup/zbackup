// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_COLLECTOR_HH_INCLUDED__
#define BACKUP_COLLECTOR_HH_INCLUDED__

#include "zbackup_base.hh"
#include "chunk_storage.hh"

class ZCollector : public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;
  size_t threads;

public:
  ZCollector( std::string const & storageDir, std::string const & password,
              size_t threads, size_t cacheSize );

  void gc();
};

#endif
