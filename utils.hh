// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef UTILS_HH_INCLUDED
#define UTILS_HH_INCLUDED

#include <sstream>

namespace Utils {

template <typename T>
std::string numberToString( T pNumber )
{
  std::ostringstream oOStrStream;
  oOStrStream << pNumber;
  return oOStrStream.str();
}

}

#endif
