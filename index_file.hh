// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef INDEX_FILE_HH_INCLUDED__
#define INDEX_FILE_HH_INCLUDED__

#include <exception>
#include <string>

#include "adler32.hh"
#include "bundle.hh"
#include "encrypted_file.hh"
#include "encryption_key.hh"
#include "ex.hh"
#include "file.hh"
#include "nocopy.hh"
#include "zbackup.pb.h"

/// Index files store all existing chunk ids and their bundle ids. This
/// information can also be retrieved by scanning all bundle files, but that
/// would incur a lot of disk seeks which we want to minimize here
namespace IndexFile {

using std::string;

/// Creates index files
class Writer: NoCopy
{
  EncryptedFile::OutputStream stream;

public:
  /// Creates a new chunk log. Initially it is stored in a temporary file
  Writer( EncryptionKey const &, string const & fileName );

  /// Adds a bundle info to the log
  void add( BundleInfo const &, Bundle::Id const & bundleId );

  /// Finalizes the file
  ~Writer();
};

/// Reads index files
class Reader: NoCopy
{
  EncryptedFile::InputStream stream;

public:
  DEF_EX( Ex, "Index file reader exception", std::exception )
  DEF_EX( exUnsupportedVersion, "Unsupported version of the index file format", Ex )
  DEF_EX( exIncorrectBundleIdSize, "Incorrect bundle id size encountered", Ex )

  Reader( EncryptionKey const &, string const & fileName );

  /// Reads the next record from the file. Returns false if no more records can
  /// be found
  bool readNextRecord( BundleInfo &, Bundle::Id & bundleId );
};
}

#endif
