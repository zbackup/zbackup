// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_COLLECTOR_HH_INCLUDED
#define BACKUP_COLLECTOR_HH_INCLUDED

#include "zbackup_base.hh"
#include "chunk_storage.hh"

class ZCollector : public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZCollector( std::string const & storageDir, std::string const & password,
              Config & configIn );

  void gc();
};

#endif
