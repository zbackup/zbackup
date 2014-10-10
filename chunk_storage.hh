// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CHUNK_STORAGE_HH_INCLUDED__
#define CHUNK_STORAGE_HH_INCLUDED__

#include <stddef.h>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "bundle.hh"
#include "chunk_id.hh"
#include "chunk_index.hh"
#include "encryption_key.hh"
#include "ex.hh"
#include "file.hh"
#include "index_file.hh"
#include "mt.hh"
#include "nocopy.hh"
#include "objectcache.hh"
#include "sptr.hh"
#include "tmp_mgr.hh"
#include "zbackup.pb.h"

namespace ChunkStorage {

using std::string;
using std::vector;
using std::pair;

DEF_EX( Ex, "Chunk storage exception", std::exception )

/// Allows adding new chunks to the storage by filling up new bundles with them
/// and writing new index files
class Writer: NoCopy
{
public:
  /// All new bundles and index files are created as temp files. Call commit()
  /// to move them to their permanent locations. commit() is never called
  /// automatically!
  Writer( StorageInfo const &, EncryptionKey const &,
          TmpMgr &, ChunkIndex & index, string const & bundlesDir,
          string const & indexDir, size_t maxCompressorsToRun );

  /// Adds the given chunk to the store. If such a chunk has already existed
  /// in the index, does nothing and returns false
  bool add( ChunkId const &, void const * data, size_t size );

  /// Commits all newly created bundles. Must be called before destroying the
  /// object -- otherwise all work will be removed from the temp dir and lost
  void commit();

  ~Writer();

private:
  /// Performs the compression in a separate thread. Destroys itself once done
  class Compressor: public Thread
  {
    Writer & writer;
    sptr< Bundle::Creator > bundleCreator;
    string fileName;
  public:
    Compressor( Writer &, sptr< Bundle::Creator > const &,
                string const & fileName );
  protected:
    virtual void * threadFunction() throw();
  };

  friend class Compressor;

  /// Returns the id of the currently written bundle. If there's none, generates
  /// one. If a bundle hasn't yet started, still generates it - once the bundle
  /// is started, it will be used then
  Bundle::Id const & getCurrentBundleId();

  /// Returns *currentBundle or creates a new one
  Bundle::Creator & getCurrentBundle();

  /// Writes the current bundle and deallocates it
  void finishCurrentBundle();

  /// Wait for all compressors to finish
  void waitForAllCompressorsToFinish();

  StorageInfo const & storageInfo;
  EncryptionKey const & encryptionKey;
  TmpMgr & tmpMgr;
  ChunkIndex & index;
  string bundlesDir, indexDir;
  sptr< TemporaryFile > indexTempFile;
  sptr< IndexFile::Writer > indexFile;

  sptr< Bundle::Creator > currentBundle;
  Bundle::Id currentBundleId;
  bool hasCurrentBundleId;

  size_t maxCompressorsToRun;
  Mutex runningCompressorsMutex;
  Condition runningCompressorsCondition;
  size_t runningCompressors;

  /// Maps temp file of the bundle to its id blob
  typedef pair< sptr< TemporaryFile >, Bundle::Id > PendingBundleRename;
  vector< PendingBundleRename > pendingBundleRenames;
};

/// Allows retrieving existing chunks by extracting them from the bundles with
/// the help of an Index object
class Reader: NoCopy
{
public:
  DEF_EX_STR( exNoSuchChunk, "no such chunk found:", Ex )

  Reader( StorageInfo const &, EncryptionKey const &, ChunkIndex & index,
          string const & bundlesDir, size_t maxCacheSizeBytes );

  /// Loads the given chunk from the store into the given buffer. May throw file
  /// and decompression exceptions. 'data' may be enlarged but won't be shrunk.
  /// The size of the actual chunk would be stored in 'size'
  void get( ChunkId const &, string & data, size_t & size );

  /// Retrieves the reader for the given bundle id. May employ caching
  Bundle::Reader & getReaderFor( Bundle::Id const & );

private:
  StorageInfo const & storageInfo;
  EncryptionKey const & encryptionKey;
  ChunkIndex & index;
  string bundlesDir;
  ObjectCache cachedReaders;
};

}

#endif
