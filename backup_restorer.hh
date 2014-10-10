// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_RESTORER_HH_INCLUDED__
#define BACKUP_RESTORER_HH_INCLUDED__

#include <stddef.h>
#include <exception>
#include <string>

#include "chunk_storage.hh"
#include "ex.hh"

/// Generic interface to stream data out
class DataSink
{
public:
  virtual void saveData( void const * data, size_t size )=0;
  virtual ~DataSink() {}
};

/// Restores the backup
namespace BackupRestorer {

DEF_EX( Ex, "Backup restorer exception", std::exception )
DEF_EX( exTooManyBytesToEmit, "A backup record asks to emit too many bytes", Ex )

/// Restores the given backup
void restore( ChunkStorage::Reader &, std::string const & backupData,
              DataSink & );
}

#endif
