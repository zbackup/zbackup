// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_FILE_HH_INCLUDED__
#define BACKUP_FILE_HH_INCLUDED__

#include <exception>
#include <string>

#include "encryption_key.hh"
#include "ex.hh"
#include "zbackup.pb.h"

namespace BackupFile {

using std::string;

DEF_EX( Ex, "Backup file exception", std::exception )
DEF_EX( exUnsupportedVersion, "Unsupported version of the backup file format", Ex )

/// Saves the given BackupInfo data into the given file
void save( string const & fileName, EncryptionKey const &, BackupInfo const & );

/// Loads the given BackupInfo data from the given file
void load( string const & fileName, EncryptionKey const &, BackupInfo & );
}

#endif
