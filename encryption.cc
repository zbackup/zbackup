// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <openssl/aes.h>

#include "check.hh"
#include "encryption.hh"
#include "static_assert.hh"

namespace Encryption {

char const ZeroIv[ IvSize ] = { 0, 0, 0, 0,
                                0, 0, 0, 0,
                                0, 0, 0, 0,
                                0, 0, 0, 0 };

void const * encrypt( void const * iv, void const * keyData,
                      void const * inData, void * outData, size_t size )
{
  unsigned char block[ BlockSize ];

  CHECK( !( size % BlockSize ), "size of data to encrypt is not a multiple of "
         "block size" );

  AES_KEY key;
  AES_set_encrypt_key( ( unsigned char const * ) keyData, KeySize * 8, &key );

  void const * prev = iv;

  // We do the operation in block size multiples. We do XOR in size_t
  // multiples. The operation is endian-neutral

  // Make sure that BlockSize is a multiple of the size of size_t
  STATIC_ASSERT( !( BlockSize % sizeof( size_t ) ) );

  size_t const * inS = ( size_t const * ) inData;
  unsigned char * out = ( unsigned char * ) outData;

  for ( size_t count = size / BlockSize; count--; )
  {
    size_t const * prevS = ( size_t const * ) prev;
    size_t * blockS = ( size_t * ) block;

    for ( size_t x = BlockSize / sizeof( size_t ); x--; )
      *blockS++ = *inS++ ^ *prevS++;

    AES_encrypt( block, out, &key );

    prev = out;
    out += BlockSize;
  }

  return prev;
}

void const * getNextDecryptionIv( void const * in, size_t size )
{
  CHECK( !( size % BlockSize ), "size of data to decrypt is not a multiple of "
         "block size" );
  return ( char const * ) in + size - BlockSize;
}

void decrypt( void const * iv, void const * keyData, void const * inData,
              void * outData, size_t size )
{
  CHECK( !( size % BlockSize ), "size of data to decrypt is not a multiple of "
         "block size" );

  AES_KEY key;
  AES_set_decrypt_key( ( unsigned char const * ) keyData, KeySize * 8, &key );

  // We decrypt from the end to the beginning

  unsigned char const * in = ( unsigned char const * ) inData + size;
  unsigned char * out = ( unsigned char * ) outData + size;

  size_t count = size / BlockSize;

  size_t const * prevS = ( size_t const * )( in - BlockSize );

  size_t * outS = ( size_t * ) out;

  while( count-- )
  {
    if ( prevS == inData )
      prevS = ( size_t const * )( ( unsigned char const * ) iv + BlockSize );

    in -= BlockSize;

    AES_decrypt( in, ( unsigned char * ) outS - BlockSize, &key );

    for ( size_t x = BlockSize / sizeof( size_t ); x--; )
      *--outS ^= *--prevS;
  }
}

void pad( void * data, size_t size )
{
  CHECK( size < BlockSize, "size to pad is too large: %zu bytes", size );
  unsigned char * p = ( unsigned char * ) data + size;
  unsigned char v = BlockSize - size;
  for ( size_t count = v; count--; )
    *p++ = v;
}

size_t unpad( void const * data )
{
  unsigned char const * p = ( unsigned char const * ) data + BlockSize - 1;
  unsigned char v = *p;
  if ( !v || v > BlockSize )
    throw exBadPadding();

  // Check the rest of the padding
  for ( size_t count = v - 1; count--; )
    if ( *--p != v )
      throw exBadPadding();

  return BlockSize - v;
}
}
