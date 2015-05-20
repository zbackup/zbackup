// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_COLLECTOR_HH_INCLUDED
#define BACKUP_COLLECTOR_HH_INCLUDED

#include <string>
#include <vector>

#include "backup_restorer.hh"
#include "backup_file.hh"

#include "debug.hh"

using std::string;

class BundleCollector: public IndexProcessor
{
private:
  Bundle::Id savedId;
  int totalChunks, usedChunks, indexTotalChunks, indexUsedChunks;
  int indexModifiedBundles, indexKeptBundles, indexRemovedBundles;
  bool indexModified, indexNecessary;
  vector< string > filesToUnlink;
  BackupRestorer::ChunkSet overallChunkSet;
  std::set< Bundle::Id > overallBundleSet;

  void copyUsedChunks( BundleInfo const & info );

public:
  string bundlesPath;
  ChunkStorage::Reader *chunkStorageReader;
  ChunkStorage::Writer *chunkStorageWriter;
  BackupRestorer::ChunkSet usedChunkSet;
  bool gcRepack, gcDeep;

  void startIndex( string const & indexFn );

  void finishIndex( string const & indexFn );

  void startBundle( Bundle::Id const & bundleId );

  void processChunk( ChunkId const & chunkId, uint32_t size );

  void finishBundle( Bundle::Id const & bundleId, BundleInfo const & info );

  void commit();
};

#endif
