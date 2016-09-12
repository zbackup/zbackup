// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef UTILS_HH_INCLUDED
#define UTILS_HH_INCLUDED

#include <sstream>
#include <string>
#include <vector>

#define VALID_SUFFIXES "Valid suffixes:\n" \
"|--------|----------------|----------|\n" \
"| suffix | multiplier     | name     |\n" \
"|--------|----------------|----------|\n" \
"| B      | 1              | byte     |\n" \
"| KiB    | 1024           | kibibyte |\n" \
"| MiB    | 1024*1024      | mebibyte |\n" \
"| GiB    | 1024*1024*1024 | gibibyte |\n" \
"| KB     | 1000           | kilobyte |\n" \
"| MB     | 1000*1000      | megabyte |\n" \
"| GB     | 1000*1000*1000 | gigabyte |\n" \
"|--------|----------------|----------|\n"

namespace Utils {
using std::string;
using std::vector;

/// Recreate source directory structure in destination
vector< string > findOrRebuild( string const & src,
    string const & dst = std::string(),
    string const & relativePath = std::string() );

unsigned int getScale( char * );

/// Converts 'size' bytes pointed to by 'in' into a hex string
std::string toHex( unsigned char const * in, unsigned size );

std::string toHex( string const & );

string fromHex( string const & in );

template <typename T>
string numberToString( T pNumber )
{
  std::ostringstream oOStrStream;
  oOStrStream << pNumber;
  return oOStrStream.str();
}

}

#endif
