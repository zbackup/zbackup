// Part of ZBackup. Licensed under GNU GPLv2 or later

#ifndef BACKUP_EXCHANGER_HH_INCLUDED__
#define BACKUP_EXCHANGER_HH_INCLUDED__

#include <bitset>
#include <exception>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace BackupExchanger {

enum {
  backups,
  bundles,
  index,
  Flags
};

/// Recreate source directory structure in destination
vector< string > recreateDirectories( string const & src, string const & dst, string const & relaPath = std::string() );
}

#endif
