// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_CREATOR_HH_INCLUDED__
#define BACKUP_CREATOR_HH_INCLUDED__

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "chunk_id.hh"
#include "chunk_index.hh"
#include "chunk_storage.hh"
#include "file.hh"
#include "nocopy.hh"
#include "rolling_hash.hh"
#include "sptr.hh"
#include "zbackup.pb.h"

using std::vector;
using std::string;

/// Creates a backup by processing input data and matching/writing chunks
class BackupCreator: ChunkIndex::ChunkInfoInterface, NoCopy
{
  unsigned chunkMaxSize;
  ChunkIndex & chunkIndex;
  ChunkStorage::Writer & chunkStorageWriter;
  vector< char > ringBuffer;
  // Ring buffer vars
  char * begin;
  char * end;
  char * head;
  char * tail;
  unsigned ringBufferFill;

  /// In this buffer we assemble the next chunk to be eventually stored. We
  /// copy the bytes from the ring buffer. While the copying may be avoided in
  /// some cases, the plan is to move to multi-threaded chunk storage in the
  /// future, where it would be necessary in any case
  vector< char > chunkToSave;
  unsigned chunkToSaveFill; /// Number of bytes accumulated in chunkToSave
  /// When we have data in chunkToSave, this points to the record in backupData
  /// which should store it
  unsigned recordIndexToSaveDataInto;

  RollingHash rollingHash;

  string backupData;
  sptr< google::protobuf::io::StringOutputStream > backupDataStream;

  /// Sees if the current block in the ring buffer exists in the chunk store.
  /// If it does, the reference is emitted and the ring buffer is cleared
  void addChunkIfMatched();

  /// Outputs data contained in chunkToSave as a new chunk
  void saveChunkToSave();

  /// Move the given amount of bytes from the ring buffer to the chunk to save.
  /// Ring buffer must have at least that many bytes
  void moveFromRingBufferToChunkToSave( unsigned bytes );

  /// Outputs the given instruction to the backup stream
  void outputInstruction( BackupInstruction const & );

  bool chunkIdGenerated;
  ChunkId generatedChunkId;
  virtual ChunkId const & getChunkId();

public:
  BackupCreator( StorageInfo const &, ChunkIndex &, ChunkStorage::Writer & );

  /// The data is fed the following way: the user fills getInputBuffer() with
  /// up to getInputBufferSize() bytes, then calls handleMoreData() with the
  /// number of bytes written
  void * getInputBuffer();
  size_t getInputBufferSize();

  void handleMoreData( unsigned );

  /// Flushes any remaining data and finishes the process. No additional data
  /// may be added after this call is made
  void finish();

  /// Returns the result of the backup creation. Can only be called once the
  /// finish() was called and the backup is complete
  void getBackupData( string & );
};

#endif
