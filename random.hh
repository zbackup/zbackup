// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef RANDOM_HH_INCLUDED__
#define RANDOM_HH_INCLUDED__

#include <exception>

#include "ex.hh"

namespace Random {
DEF_EX( exCantGenerate, "Error generating random sequence, try later", std::exception )

/// This one fills the buffer with true randomness, suitable for a key
void genarateTrue( void * buf, unsigned size );
/// This one fills the buffer with pseudo randomness, suitable for salts but not
/// keys
void genaratePseudo( void * buf, unsigned size );
}

#endif
