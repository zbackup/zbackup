// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#define _LARGEFILE64_SOURCE

#include <fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "check.hh"
#include "unbuffered_file.hh"


#ifdef __APPLE__
#define lseek64 lseek
#endif


UnbufferedFile::UnbufferedFile( char const * fileName, Mode mode )
  throw( exCantOpen )
{

  int flags = ( mode == WriteOnly ? ( O_WRONLY | O_CREAT | O_TRUNC ) :
                                    O_RDONLY );
#ifndef __APPLE__
  flags |= O_LARGEFILE;
#endif
  fd = open( fileName, flags, 0666 );
  if ( fd < 0 )
    throw exCantOpen( fileName );
}

size_t UnbufferedFile::read( void * buf, size_t size )
  throw( exReadError )
{
  char * next = ( char * ) buf;
  size_t left = size;

  while( left )
  {
    ssize_t rd = ::read( fd, next, left );
    if ( rd < 0 )
    {
      if ( errno != EINTR )
        throw exReadError();
    }
    else
    if ( rd > 0 )
    {
      CHECK( ( size_t ) rd <= left, "read too many bytes from a file" );
      next += rd;
      left -= rd;
    }
    else
      break;
  }

  return size - left;
}

void UnbufferedFile::write( void const * buf, size_t size )
  throw( exWriteError )
{
  char const * next = ( char const * ) buf;
  size_t left = size;

  while( left )
  {
    ssize_t written = ::write( fd, next, left );
    if ( written < 0 )
    {
      if ( errno != EINTR )
        throw exWriteError();
    }
    else
    {
      CHECK( ( size_t ) written <= left, "wrote too many bytes to a file" );
      next += written;
      left -= written;
    }
  }
}

UnbufferedFile::Offset UnbufferedFile::size() throw( exSeekError )
{
  Offset cur = lseek64( fd, 0, SEEK_CUR );
  if ( cur < 0 )
    throw exSeekError();
  Offset result = lseek64( fd, 0, SEEK_END );
  if ( result < 0 || lseek64( fd, cur, SEEK_SET ) < 0 )
    throw exSeekError();
  return result;
}

void UnbufferedFile::seekCur( Offset offset ) throw( exSeekError )
{
  if ( lseek64( fd, offset, SEEK_CUR ) < 0 )
    throw exSeekError();
}

UnbufferedFile::~UnbufferedFile() throw()
{
  close( fd );
}
