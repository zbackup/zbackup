// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "hex.hh"

using std::string;

namespace {
/// Converts 'size' bytes pointed to by 'in' into a hex string pointed to by
/// 'out'. It should have at least size * 2 bytes. No trailing zero is added
void hexify( unsigned char const * in, unsigned size, char * out )
{
  while( size-- )
  {
    unsigned char v = *in++;

    *out++ = ( v >> 4 < 10 ) ? '0' + ( v >> 4 ) : 'a' + ( v >> 4 ) - 10;
    *out++ = ( ( v & 0xF ) < 10 ) ? '0' + ( v & 0xF ) : 'a' + ( v & 0xF ) - 10;
  }
}
}

string toHex( unsigned char const * in, unsigned size )
{
  string result( size * 2, 0 );
  hexify( in, size, &result[ 0 ] );

  return result;
}
