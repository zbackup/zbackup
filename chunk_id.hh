// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CHUNK_ID_HH_INCLUDED__
#define CHUNK_ID_HH_INCLUDED__

#include <string>
#include "rolling_hash.hh"

using std::string;

/// Chunk is identified by its crypto hash concatenated with its rolling hash
struct ChunkId
{
  typedef char CryptoHashPart[ 16 ];
  CryptoHashPart cryptoHash;

  typedef RollingHash::Digest RollingHashPart;
  RollingHashPart rollingHash;

  enum
  {
    BlobSize = sizeof( CryptoHashPart ) + sizeof( RollingHashPart )
  };

  string toBlob() const;

  /// Faster version - should point to a buffer with at least BlobSize bytes
  void toBlob( void * ) const;

  /// Set the chunk id data reading from the given blob
  void setFromBlob( void const * );

  ChunkId() {}
  ChunkId( string const & blob );
};

#endif
