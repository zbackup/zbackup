// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "rolling_hash.hh"

RollingHash::RollingHash()
{
  reset();
}

void RollingHash::reset()
{
  count = 0;
  factor = 0;
  nextFactor = 1;
  value = 0;
}

RollingHash::Digest RollingHash::digest( void const * buf, unsigned size )
{
  // TODO: this can be optimized, as in this case there's no need to calculate
  // factor values.
  RollingHash hash;

  for ( char const * p = ( char const * )buf; size--; )
    hash.rollIn( *p++ );

  return hash.digest();
}
