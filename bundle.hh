// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BUNDLE_HH_INCLUDED__
#define BUNDLE_HH_INCLUDED__

#include <stddef.h>
#include <string.h>
#include <exception>
#include <map>
#include <string>
#include <utility>

#include "encryption_key.hh"
#include "ex.hh"
#include "nocopy.hh"
#include "static_assert.hh"
#include "zbackup.pb.h"
#include "encrypted_file.hh"

namespace Bundle {

using std::string;
using std::pair;
using std::map;

enum
{
  /// The number of bytes the bundle id has. We chose 192-bit just to be on
  /// the safer side. It is also a multiple of 8 bytes, which is good for
  /// alignment
  IdSize = 24
};

/// Id of the bundle is IdSize bytes. Can and should be used as a POD type
struct Id
{
  char blob[ IdSize ];

  bool operator == ( Id const & other ) const
  { return memcmp( blob, other.blob, sizeof( blob ) ) == 0; }
  bool operator != ( Id const & other ) const
  { return ! operator == ( other ); }
};

STATIC_ASSERT( sizeof( Id ) == IdSize );

/// Reads the bundle and allows accessing chunks
class Reader: NoCopy
{
  BundleInfo info;
  /// Unpacked payload
  string payload;
  /// Maps chunk id blob to its contents and size
  typedef map< string, pair< char const *, size_t > > Chunks;
  Chunks chunks;

public:
  DEF_EX( Ex, "Bundle reader exception", std::exception )
  DEF_EX( exBundleReadFailed, "Bundle read failed", Ex )
  DEF_EX( exUnsupportedVersion, "Unsupported version of the index file format", Ex )
  DEF_EX( exTooMuchData, "More data than expected in a bundle", Ex )
  DEF_EX( exDuplicateChunks, "Chunks with the same id found in a bundle", Ex )

  Reader( string const & fileName, EncryptionKey const & key,
      bool prohibitProcessing = false );

  /// Reads the chunk into chunkData and returns true, or returns false if there
  /// was no such chunk in the bundle. chunkData may be enlarged but won't
  /// be shrunk. The size of the actual chunk would be stored in chunkDataSize
  bool get( string const & chunkId, string & chunkData, size_t & chunkDataSize );
  BundleInfo getBundleInfo()
  { return info; }
  string getPayload()
  { return payload; }

  EncryptedFile::InputStream is;
};

/// Creates a bundle by adding chunks to it until it's full, then compressing
/// it and writing out to disk
class Creator
{
  BundleInfo info;
  string payload;

public:
  DEF_EX( Ex, "Bundle creator exception", std::exception )
  DEF_EX( exBundleWriteFailed, "Bundle write failed", Ex )

  /// Adds a chunk with the given id
  void addChunk( string const & chunkId, void const * data, size_t size );

  /// Returns the number of bytes comprising all chunk bodies so far
  size_t getPayloadSize() const
  { return payload.size(); }

  /// Compresses and writes the bundle to the given file. The operation is
  /// time-consuming - calling this function from a worker thread could be
  /// warranted
  void write( string const & fileName, EncryptionKey const & );
  void write( string const & fileName, EncryptionKey const &,
      Bundle::Reader & reader );

  /// Returns the current BundleInfo record - this is used for index files
  BundleInfo const & getCurrentBundleInfo() const
  { return info; }
};

/// Generates a full file name for a bundle with the given id. If createDirs
/// is true, any intermediate directories will be created if they don't exist
/// already
string generateFileName( Id const &, string const & bundlesDir,
                         bool createDirs );
}

#endif
