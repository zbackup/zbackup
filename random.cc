// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include "random.hh"
#include <openssl/rand.h>

namespace Random {
void genarateTrue( void * buf, unsigned size )
{
  if ( RAND_bytes( (unsigned char *) buf, size ) != 1 )
    throw exCantGenerate();
}

void genaratePseudo( void * buf, unsigned size )
{
  if ( RAND_pseudo_bytes( (unsigned char *) buf, size ) < 0 )
    throw exCantGenerate();
}
}
