// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <string.h>
#include <algorithm>

#include "check.hh"
#include "encrypted_file.hh"
#include "endian.hh"
#include "page_size.hh"
#include "random.hh"

namespace EncryptedFile {

using Encryption::BlockSize;

InputStream::InputStream( char const * fileName, EncryptionKey const & key,
                          void const * iv_ ):
  file( fileName, UnbufferedFile::ReadOnly ), filePos( 0 ), key( key ),
  // Our buffer must be larger than BlockSize, as otherwise we won't be able
  // to handle PKCS#7 padding properly
  buffer( std::max( getPageSize(), ( unsigned ) BlockSize * 2 ) ),
  fill( 0 ), remainder( 0 ), backedUp( false )
{
  if ( key.hasKey() )
  {
    memcpy( iv, iv_, sizeof( iv ) );
    // Since we use padding, file size should be evenly dividable by the cipher
    // block size, and we should have at least one block
    UnbufferedFile::Offset size = file.size();
    if ( !size || size % BlockSize )
      throw exIncorrectFileSize();
  }
}

bool InputStream::Next( void const ** data, int * size )
{
  // If we backed up, return the unconsumed data
  if ( backedUp )
    backedUp = false;
  else
  {
    try
    {
      // Update adler32 for the previous block
      adler32.add( start, fill );

      // Read more data
      if ( filePos && !remainder )
      {
        // Once we're read a full block, we always have a remainder. If not,
        // this means we've hit the end of file already
        fill = 0;
        return false;
      }

      // If we have a remainder, move it to the beginning of buffer and make
      // it start the next block
      memmove( buffer.data(), start + fill, remainder );
      start = buffer.data();
      fill = file.read( start + remainder, buffer.size() - remainder ) +
             remainder;
      // remainder should techically be 0 now, but decrypt() will update it
      // anyway
      // remainder = 0;
      decrypt();
    }
    catch( UnbufferedFile::exReadError & )
    {
      fill = 0; // To make sure state is remaining consistent
      return false;
    }
  }
  *data = start;
  *size = fill;
  filePos += fill;
  return *size;
}

void InputStream::BackUp( int count )
{
  CHECK( count >= 0, "count is negative" );
  if ( !backedUp )
  {
    CHECK( (size_t) count <= fill, "Backing up too much" );
    size_t consumed = fill - count;
    adler32.add( start, consumed );
    start += consumed;
    fill = count;
    filePos -= count;
    backedUp = fill; // Don't make the next Next() return 0 bytes
  }
  else
  {
    CHECK( count == 0, "backing up after being backed up already" );
  }
}

bool InputStream::Skip( int count )
{
  CHECK( count >= 0, "count is negative" );

  // We always need to read and decrypt data, as otherwise both the state of
  // CBC and adler32 would be incorrect
  void const * data;
  int size;
  while( count )
  {
    if ( !Next( &data, &size ) )
      return false;
    else
    if ( size > count )
    {
      BackUp( size - count );
      break;
    }
    else
      count -= size;
  }
  return true;
}

int64_t InputStream::ByteCount() const
{
  return filePos;
}

Adler32::Value InputStream::getAdler32()
{
  // This makes all data consumed, if not already
  BackUp( 0 );
  return adler32.result();
}

void InputStream::read( void * buf, size_t size )
{
  void const * data;
  int avail;
  char * n = ( char * ) buf;
  while( size )
  {
    if ( !Next( &data, &avail ) )
      throw exReadFailed();
    else
    if ( avail > ( ssize_t ) size )
    {
      memcpy( n, data, size );
      BackUp( avail - size );
      break;
    }
    else
    {
      memcpy( n, data, avail );
      n += avail;
      size -= avail;
    }
  }
}

void InputStream::checkAdler32()
{
  Adler32::Value ours = getAdler32();
  Adler32::Value r;
  read( &r, sizeof( r ) );
  if ( ours != fromLittleEndian( r ) )
    throw exAdlerMismatch();
}

void InputStream::consumeRandomIv()
{
  if ( key.hasKey() )
  {
    char iv[ Encryption::IvSize ];
    read( iv, sizeof( iv ) ); // read() can throw exceptions, Skip() can't
  }
}

void InputStream::decrypt()
{
  if ( fill == buffer.size() )
  {
    // When we have the full buffer, we set the last block of it aside and
    // treat the rest as the normal CBC sequence. The last block in the buffer
    // may be the last block of file, in which case we would need to handle
    // padding. That may happen the next time the function is called
    remainder = BlockSize;
    fill -= BlockSize;
    doDecrypt();
  }
  else
  {
    // This is an end of file. Decrypt data treating the last block being
    // padded

    // Since we always have padding in the file and the last block is always
    // set apart when reading full buffers, we must have at least one block
    // to decrypt here
    doDecrypt();

    // Unpad the last block
    if ( key.hasKey() )
      fill -= BlockSize - Encryption::unpad( start + fill - BlockSize );

    // We have not left any remainder this time
    remainder = 0;
  }
}

void InputStream::doDecrypt()
{
  if ( !key.hasKey() )
    return;

  // Since we use padding, file size should be evenly dividable by the cipher's
  // block size, and we should always have at least one block. When we get here,
  // we would always get the proper fill value unless those characteristics are
  // not met. We check for the same condition on construction, but the file
  // size can change while we are reading it

  // We don't throw an exception here as the interface we implement doesn't
  // support them
  CHECK( fill > 0 && !( fill % BlockSize ), "incorrect size of the encrypted "
         "file - must be non-zero and in multiples of %u",
         ( unsigned ) BlockSize );

  // Copy the next iv prior to decrypting the data in place, as it will
  // not be available afterwards
  char newIv[ Encryption::IvSize ];
  memcpy( newIv, Encryption::getNextDecryptionIv( start, fill ),
          sizeof( newIv ) );
  // Decrypt the data
  Encryption::decrypt( iv, key.getKey(), start, start, fill );
  // Copy the new iv
  memcpy( iv, newIv, sizeof( iv ) );
}

OutputStream::OutputStream( char const * fileName, EncryptionKey const & key,
                            void const * iv_ ):
  file( fileName, UnbufferedFile::WriteOnly ), filePos( 0 ), key( key ),
  buffer( getPageSize() ), start( buffer.data() ), avail( 0 ), backedUp( false )
{
  if ( key.hasKey() )
    memcpy( iv, iv_, sizeof( iv ) );
}

bool OutputStream::Next( void ** data, int * size )
{
  // If we backed up, return the unconsumed data
  if ( backedUp )
    backedUp = false;
  else
  {
    try
    {
      // Update adler32 for the previous block
      adler32.add( start, avail );

      // Encrypt and write the buffer if it had data
      if ( filePos )
        encryptAndWrite( buffer.size() );

      start = buffer.data();
      avail = buffer.size();
    }
    catch( UnbufferedFile::exWriteError & )
    {
      avail = 0; // To make sure state is remaining consistent
      return false;
    }
  }
  *data = start;
  *size = avail;
  filePos += avail;
  return *size;
}

void OutputStream::BackUp( int count )
{
  CHECK( count >= 0, "count is negative" );
  if ( !backedUp )
  {
    CHECK( (size_t) count <= avail, "Backing up too much" );
    size_t consumed = avail - count;
    adler32.add( start, consumed );
    start += consumed;
    avail = count;
    filePos -= count;
    backedUp = avail; // Don't make the next Next() return 0 bytes
  }
  else
  {
    CHECK( count == 0, "backing up after being backed up already" );
  }
}

int64_t OutputStream::ByteCount() const
{
  return filePos;
}

Adler32::Value OutputStream::getAdler32()
{
  // This makes all data consumed, if not already
  BackUp( 0 );
  return adler32.result();
}

void OutputStream::write( void const * buf, size_t size )
{
  void * data;
  int avail;
  char const * n = ( char const * ) buf;
  while( size )
  {
    if ( !Next( &data, &avail ) )
      throw exReadFailed();
    else
    if ( avail > ( ssize_t ) size )
    {
      memcpy( data, n, size );
      BackUp( avail - size );
      break;
    }
    else
    {
      memcpy( data, n, avail );
      n += avail;
      size -= avail;
    }
  }
}

void OutputStream::writeAdler32()
{
  Adler32::Value v = toLittleEndian( getAdler32() );
  write( &v, sizeof( v ) );
}

void OutputStream::writeRandomIv()
{
  if ( key.hasKey() )
  {
    char iv[ Encryption::IvSize ];
    Random::genaratePseudo( iv, sizeof( iv ) );
    write( iv, sizeof( iv ) );
  }
}

void OutputStream::encryptAndWrite( size_t bytes )
{
  if ( key.hasKey() )
  {
    CHECK( bytes > 0 && !( bytes % BlockSize ), "incorrect number of bytes to "
           "encrypt and write - must be non-zero and in multiples of %u",
           ( unsigned ) BlockSize );

    void const * nextIv = Encryption::encrypt( iv, key.getKey(), buffer.data(),
                                               buffer.data(), bytes );
    memcpy( iv, nextIv, sizeof( iv ) );
  }

  file.write( buffer.data(), bytes );
}

OutputStream::~OutputStream()
{
  // This makes all data consumed, if not already
  BackUp( 0 );

  // If we have the full buffer, write it first
  if ( start == buffer.data() + buffer.size() )
  {
    encryptAndWrite( buffer.size() );
    start = buffer.data();
  }

  size_t bytesToWrite = start - buffer.data();

  if ( key.hasKey() )
  {
    // Perform padding
    size_t remainderSize = bytesToWrite % BlockSize;

    Encryption::pad( start - remainderSize, remainderSize );
    bytesToWrite += BlockSize - remainderSize;
  }

  encryptAndWrite( bytesToWrite );
}

}
