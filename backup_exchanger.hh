// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_EXCHANGER_HH_INCLUDED__
#define BACKUP_EXCHANGER_HH_INCLUDED__

#include <string>
#include <vector>
#include "sptr.hh"
#include "tmp_mgr.hh"

namespace BackupExchanger {

using std::string;
using std::vector;
using std::pair;

enum {
  backups,
  bundles,
  index,
  Flags
};

/// Recreate source directory structure in destination
vector< string > recreateDirectories( string const & src, string const & dst, string const & relativePath = std::string() );
typedef pair< sptr< TemporaryFile >, string > PendingExchangeRename;
}

#endif
