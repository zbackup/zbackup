// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_RESTORER_HH_INCLUDED
#define BACKUP_RESTORER_HH_INCLUDED

#include <stddef.h>
#include <exception>
#include <string>
#include <set>

#undef __DEPRECATED
#include <ext/hash_map>

#include "chunk_storage.hh"
#include "ex.hh"

/// Generic interface to stream data out
class DataSink
{
public:
  virtual void saveData( void const * data, size_t size )=0;
  virtual ~DataSink() {}
};

/// Generic interface to seekable data output
class SeekableSink
{
public:
  virtual void saveData( int64_t position, void const * data, size_t size )=0;
};

namespace __gnu_cxx
{
  template<>
  struct hash< Bundle::Id >
  {
    size_t operator()( Bundle::Id v ) const
    { return *((size_t*)(v.blob)); }
  };
}

/// Restores the backup
namespace BackupRestorer {

DEF_EX( Ex, "Backup restorer exception", std::exception )
DEF_EX( exTooManyBytesToEmit, "A backup record asks to emit too many bytes", Ex )
DEF_EX( exBytesToMap, "Can't restore bytes to ChunkMap", Ex )
DEF_EX( exOutOfRange, "Requested data block is out of backup data range", Ex )

typedef std::set< ChunkId > ChunkSet;
typedef std::vector< std::pair < ChunkId, int64_t > > ChunkPosition;
typedef __gnu_cxx::hash_map< Bundle::Id, ChunkPosition > ChunkMap;

/// Restores the given backup
void restore( ChunkStorage::Reader &, std::string const & backupData,
              DataSink *, ChunkSet *, ChunkMap *, SeekableSink * );

/// Restores ChunkMap using seekable output
void restoreMap( ChunkStorage::Reader & chunkStorageReader,
              ChunkMap const * chunkMap, SeekableSink *output );

/// Performs restore iterations on backupData
void restoreIterations( ChunkStorage::Reader &, BackupInfo &, std::string &, ChunkSet * );

/// Reader class that loads information about all backup chunks and provides
/// fast way of retrieving data from arbitrary offset
class IndexedRestorer : NoCopy
{
public:
  IndexedRestorer( ChunkStorage::Reader & chunkStorageReader, std::string const & backupData );

  /// Returns total size of the backup
  int64_t size() const;

  /// Restore "size" bytes of data from specified offset into "data" buffer
  void saveData( int64_t offset, void * data, size_t size ) const;

private:
  ChunkStorage::Reader & chunkStorageReader;
  int64_t totalSize;

  typedef std::pair<int64_t, BackupInstruction> InstructionAtPos;
  typedef std::vector<InstructionAtPos> Instructions;
  Instructions instructions;
};
}

#endif
