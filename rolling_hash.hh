// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ROLLING_HASH_HH_INCLUDED__
#define ROLLING_HASH_HH_INCLUDED__

#include <stdint.h>
#include <stddef.h>

// Modified Rabin-Karp rolling hash with the base of 257 and the modulo of 2^64.

// The canonical RK hash calculates the following value (e.g. for 4 bytes):

// hash = ( v1*b^3 + v2*b^2 + v3*b + v4 ) % m
//   where v1, v2, v3 and v4 are the sequence of bytes, b is the base and m
//   is the modulo.

// We add b^4 in the mix:

// hash = ( b^4 + v1*b^3 + v2*b^2 + v3*b + v4 ) % m

// This fixes collisions where sequences only differ in the amount of zero
// bytes in the beginning (those amount to zero in the canonical RK), since the
// power of b in the first member depends on the total number of bytes in the
// sequence.

// The choice of base: 257 is easy to multiply by (only two bits are set), and
// is the first prime larger than the value of any byte. It's easy to create
// collisions with the smaller primes: two-byte sequences '1, 0' and '0, base'
// would collide, for example.

// The choice of modulo: 32-bit is impractical due to birthday paradox -- you
// get a collision with the 50% probability having only 77000 hashes. With
// 64-bit, the number of hashes to have the same probability would be 5.1
// billion. With the block size of 64k, that would amount to 303 terabytes of
// data stored, which should be enough for our purposes.

// Note: ( a = ( a << 8 ) + a ) is equivalent to ( a *= 257 )

class RollingHash
{
  uint64_t factor;
  uint64_t nextFactor;
  uint64_t value;
  size_t count;

public:
  typedef uint64_t Digest;

  RollingHash();

  void reset();

  void rollIn( char c )
  {
    factor = nextFactor;
    nextFactor = ( nextFactor << 8 ) + nextFactor; // nextFactor *= 257
    value = ( value << 8 ) + value;
    value += ( unsigned char ) c;
    ++count;
  }

  void rotate( char in, char out )
  {
    value -= uint64_t( ( unsigned char ) out ) * factor;
    value = ( value << 8 ) + value; // value *= 257
    value += ( unsigned char ) in;
  }

  Digest digest() const
  {
    return value + nextFactor;
  }

  size_t size() const
  { return count; }

  static Digest digest( void const * buf, unsigned size );
};

#endif
