// Part of ZBackup. Licensed under GNU GPLv2 or later

#ifndef EXCHANGE_HH_INCLUDED__
#define EXCHANGE_HH_INCLUDED__

#include <bitset>
#include <exception>
#include <string>

using std::vector;
using std::string;

namespace BackupExchanger {

enum {
  backups,
  bundles,
  index,
  Flags
};

}

#endif
