// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_EXCHANGER_HH_INCLUDED
#define BACKUP_EXCHANGER_HH_INCLUDED

#include <string>
#include <vector>
#include "tmp_mgr.hh"

namespace BackupExchanger {

using std::string;
using std::vector;
using std::pair;

enum {
  backups,
  bundles,
  indexes,
  Flags
};

/// Recreate source directory structure in destination
vector< string > findOrRebuild( string const & src,
    string const & dst = std::string(),
    string const & relativePath = std::string() );
typedef pair< sptr< TemporaryFile >, string > PendingExchangeRename;
}

#endif
