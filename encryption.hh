// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ENCRYPTION_HH_INCLUDED__
#define ENCRYPTION_HH_INCLUDED__

#include <stddef.h>
#include <exception>

#include "ex.hh"

/// What we implement right now is AES-128 in CBC mode with PKCS#7 padding
namespace Encryption {

enum
{
  KeySize = 16, /// Size of the key in bytes
  IvSize = 16, /// Size of the IV data in bytes
  BlockSize = 16 /// Cipher block size in bytes
};

DEF_EX( exBadPadding, "Bad padding encountered", std::exception )

/// Encrypts 'size' bytes of the data pointed to by 'in', outputting 'size'
/// bytes to 'out'. 'key' points to KeySize bytes of the key data. 'iv' points
/// to IvSize bytes used as an initialization vector. 'in' and 'out' can be the
/// same. 'size' must be a multiple of BlockSize. Returns a pointer to the
/// IV which should be used to continue encrypting, which in CBC is the last
/// encrypted block
void const * encrypt( void const * iv, void const * key, void const * in,
                      void * out, size_t size );

/// Returns a pointer to the IV which should be used to decrypt the block next
/// to the given one, which in CBC is the last encrypted block. Note that if an
/// in-place decryption is performed, this IV should be saved first, as it will
/// be overwritten with the decrypted data. For size == 0, the returned pointer
/// is invalid
void const * getNextDecryptionIv( void const * in, size_t size );

/// The reverse of encrypt()
void decrypt( void const * iv, void const * key, void const * in, void * out,
              size_t size );

/// Pads the last block to be encrypted, pointed to by 'data', 'size' bytes,
/// which should be less than BlockSize, to occupy BlockSize bytes
void pad( void * data, size_t size );

/// Returns the size of the padded data. The data itself is unchanged - use the
/// first bytes of 'data'. Can throw exBadPadding
size_t unpad( void const * data );

/// The IV consisting of zero bytes. Use it when there is no IV
extern char const ZeroIv[ IvSize ];
}

#endif
