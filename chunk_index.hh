// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#ifndef CHUNK_INDEX_HH_INCLUDED__
#define CHUNK_INDEX_HH_INCLUDED__

// <ext/hash_map> is obsolete, but <unordered_map> requires C++11. Make up your
// mind, GNU people!
#undef __DEPRECATED

#include <stdint.h>
#include <exception>
#include <ext/hash_map>
#include <functional>
#include <string>
#include <vector>

#include "appendallocator.hh"
#include "bundle.hh"
#include "chunk_id.hh"
#include "dir.hh"
#include "encryption_key.hh"
#include "endian.hh"
#include "ex.hh"
#include "index_file.hh"
#include "nocopy.hh"
#include "rolling_hash.hh"
#include "tmp_mgr.hh"

using std::vector;

// 32-bit specific hash function for unsigned long long which is what
// uint64_t is on 32-bit platforms. Also, on Mac, uint64_t is defined
// as 'unsigned long long'.
#if (SIZE_MAX == UINT32_MAX || defined __APPLE__)
namespace __gnu_cxx
{
  template<>
  struct hash< unsigned long long >
  {
    size_t operator()( unsigned long long v ) const
    { return v ^ ( v >> 32 ); }
  };
}
#endif

/// Maintains an in-memory hash table allowing to check whether we have a
/// specific chunk or not, and if we do, get the bundle id it's in
class ChunkIndex: NoCopy
{
  struct Chain
  {
    ChunkId::CryptoHashPart cryptoHash;
    Chain * next;
    Bundle::Id const * bundleId;

    Chain( ChunkId const &, Bundle::Id const * bundleId );

    bool equalsTo( ChunkId const & id );
  };

  /// This hash map stores all known chunk ids
  /// TODO: implement a custom hash table for better performance
  typedef __gnu_cxx::hash_map< RollingHash::Digest, Chain * > HashTable;

  EncryptionKey const & key;
  TmpMgr & tmpMgr;
  string indexPath;
  AppendAllocator storage;

  HashTable hashTable;

  /// Stores the last used bundle id, which can be re-used
  Bundle::Id const * lastBundleId;

public:
  DEF_EX( Ex, "Chunk index exception", std::exception )
  DEF_EX( exIncorrectChunkIdSize, "Incorrect chunk id size encountered", Ex )

  ChunkIndex( EncryptionKey const &, TmpMgr &, string const & indexPath );

  struct ChunkInfoInterface
  {
    /// Returns the full id of the chunk. This function is only called if that
    /// full id is actually needed, as its generation requires the expensive
    /// calculation of the full hash
    virtual ChunkId const & getChunkId()=0;
    virtual ~ChunkInfoInterface() {}
  };

  /// If the given chunk exists, its bundle id is returned, otherwise NULL
  Bundle::Id const * findChunk( ChunkId::RollingHashPart,
                                ChunkInfoInterface & );

  /// If the given chunk exists, its bundle id is returned, otherwise NULL
  Bundle::Id const * findChunk( ChunkId const & );

  /// Adds a new chunk to the index if it did not exist already. Returns true
  /// if added, false if existed already
  bool addChunk( ChunkId const &, Bundle::Id const & );

private:
  void loadIndex();

  /// Inserts new chunk id into the in-memory hash table. Returns the created
  /// Chain if it was inserted, NULL if it existed before
  Chain * registerNewChunkId( ChunkId const & id, Bundle::Id const * );
};

#endif
