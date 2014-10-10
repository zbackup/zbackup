// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef HEX_HH_INCLUDED__
#define HEX_HH_INCLUDED__

#include <string>

/// Converts 'size' bytes pointed to by 'in' into a hex string
std::string toHex( unsigned char const * in, unsigned size );

#endif
