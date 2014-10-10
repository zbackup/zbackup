// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef STORAGE_INFO_FILE_HH_INCLUDED__
#define STORAGE_INFO_FILE_HH_INCLUDED__

#include <exception>
#include <string>

#include "encryption_key.hh"
#include "ex.hh"
#include "zbackup.pb.h"

namespace StorageInfoFile {

using std::string;

DEF_EX( Ex, "Storage info file exception", std::exception )
DEF_EX( exUnsupportedVersion, "Unsupported version of the storage info file format", Ex )

/// Saves the given StorageInfo data into the given file
void save( string const & fileName, StorageInfo const & );

/// Loads the given StorageInfo data from the given file
void load( string const & fileName, StorageInfo & );
}

#endif
