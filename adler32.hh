// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ADLER32_HH_INCLUDED__
#define ADLER32_HH_INCLUDED__

#include <zlib.h>
#include <stdint.h>
#include <stddef.h>

/// A simple wrapper to calculate adler32
class Adler32
{
public:
  typedef uint32_t Value;

  Adler32(): value( ( Value ) adler32( 0, 0, 0 ) ) {}

  void add( void const * data, size_t size )
  {
    // When size is 0, we assume a no-op was requested and 'data' should be
    // ignored. However, adler32() has a special semantic for NULL 'data'.
    // Therefore we check the size before calling it
    if ( size )
      value = ( Value ) adler32( value, ( Bytef const * ) data, size );
  }

  Value result() const
  { return value; }

private:
  Value value;
};

#endif
