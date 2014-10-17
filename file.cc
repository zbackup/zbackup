// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <fcntl.h>

#include "file.hh"

enum
{
  // We employ a writing buffer to considerably speed up file operations when
  // they consists of many small writes. The default size for the buffer is 64k
  WriteBufferSize = 65536
};

bool File::exists( char const * filename ) throw()
{
#ifdef __WIN32
  struct _stat buf;
  return _stat( filename, &buf ) == 0;
#else
  struct stat buf;

  // EOVERFLOW rationale: if the file is too large, it still does exist
  return stat( filename, &buf ) == 0 || errno == EOVERFLOW;
#endif
}

void File::erase( std::string const & filename ) throw( exCantErase )
{
  if ( remove( filename.c_str() ) != 0 )
    throw exCantErase( filename );
}

void File::rename( std::string const & from,
                   std::string const & to ) throw( exCantRename )
{
  int res = 0;
  res = ::rename( from.c_str(), to.c_str() );
  if ( 0 != res )
  {
    if ( EXDEV == errno )
    {
      int read_fd;
      int write_fd;
      struct stat stat_buf;
      off_t offset = 0;

      /* Open the input file. */
      read_fd = ::open( from.c_str(), O_RDONLY );
      /* Stat the input file to obtain its size. */
      fstat( read_fd, &stat_buf );
      /* Open the output file for writing, with the same permissions as the
       source file. */
      write_fd = ::open( to.c_str(), O_WRONLY | O_CREAT, stat_buf.st_mode );
      /* Blast the bytes from one file to the other. */
      if ( -1 == sendfile(write_fd, read_fd, &offset, stat_buf.st_size) )
             throw exCantRename( from + " to " + to );

      /* Close up. */
      ::close( read_fd );
      ::close( write_fd );
      File::erase ( from );
    }
    else
      throw exCantRename( from + " to " + to );
  }
}

void File::open( char const * filename, OpenMode mode ) throw( exCantOpen )
{
  char const * m;

  switch( mode )
  {
    case Update:
      m = "r+b";
      break;
    case WriteOnly:
      m = "wb";
      break;
    default:
      m = "rb";
  }

  f = fopen( filename, m );

  if ( !f )
    throw exCantOpen( std::string( filename ) + ": " + strerror( errno ) );
}

File::File( char const * filename, OpenMode mode ) throw( exCantOpen ):
  writeBuffer( 0 )
{
  open( filename, mode );
}

File::File( std::string const & filename, OpenMode mode )
  throw( exCantOpen ): writeBuffer( 0 )
{
  open( filename.c_str(), mode );
}

void File::read( void * buf, size_t size ) throw( exReadError, exWriteError )
{
  if ( !size )
    return;

  if ( writeBuffer )
    flushWriteBuffer();

  size_t result = fread( buf, size, 1, f );

  if ( result != 1 )
  {
    if ( !ferror( f ) )
      throw exShortRead();
    else
      throw exReadErrorDetailed( f );
  }
}

size_t File::readRecords( void * buf, size_t size, size_t count ) throw( exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  return fread( buf, size, count, f );
}

void File::write( void const * buf, size_t size ) throw( exWriteError )
{
  if ( !size )
    return;

  if ( size >= WriteBufferSize )
  {
    // If the write is large, there's not much point in buffering
    flushWriteBuffer();

    size_t result = fwrite( buf, size, 1, f );

    if ( result != 1 )
      throw exWriteError();

    return;
  }

  if ( !writeBuffer )
  {
    // Allocate the writing buffer since we don't have any yet
    writeBuffer = new char[ WriteBufferSize ];
    writeBufferLeft = WriteBufferSize;
  }

  size_t toAdd = size < writeBufferLeft ? size : writeBufferLeft;

  memcpy( writeBuffer + ( WriteBufferSize - writeBufferLeft ),
          buf, toAdd );

  size -= toAdd;
  writeBufferLeft -= toAdd;

  if ( !writeBufferLeft ) // Out of buffer? Flush it
  {
    flushWriteBuffer();

    if ( size ) // Something's still left? Add to buffer
    {
      memcpy( writeBuffer, (char const *)buf + toAdd, size );
      writeBufferLeft -= size;
    }
  }
}

size_t File::writeRecords( void const * buf, size_t size, size_t count )
  throw( exWriteError )
{
  flushWriteBuffer();

  return fwrite( buf, size, count, f );
}

char * File::gets( char * s, int size, bool stripNl )
  throw( exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  char * result = fgets( s, size, f );

  if ( result && stripNl )
  {
    size_t len = strlen( result );
    
    char * last = result + len;

    while( len-- )
    {
      --last;

      if ( *last == '\n' || *last == '\r' )
        *last = 0;
      else
        break;
    }
  }

  return result;
}

std::string File::gets( bool stripNl ) throw( exReadError, exWriteError )
{
  char buf[ 1024 ];

  if ( !gets( buf, sizeof( buf ), stripNl ) )
  {
    if ( !ferror( f ) )
      throw exShortRead();
    else
      throw exReadErrorDetailed( f );
  }

  return std::string( buf );
}

void File::seek( long offset ) throw( exSeekError, exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  if ( fseek( f, offset, SEEK_SET ) != 0 )
    throw exSeekError();
}

void File::seekCur( long offset ) throw( exSeekError, exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  if ( fseek( f, offset, SEEK_CUR ) != 0 )
    throw exSeekError();
}

void File::seekEnd( long offset ) throw( exSeekError, exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  if ( fseek( f, offset, SEEK_END ) != 0 )
    throw exSeekError();
}

void File::rewind() throw( exSeekError, exWriteError )
{
  seek( 0 );
}

size_t File::tell() throw( exSeekError )
{
  long result = ftell( f );

  if ( result == -1 )
    throw exSeekError();

  if ( writeBuffer )
    result += ( WriteBufferSize - writeBufferLeft );

  return ( size_t ) result;
}

size_t File::size() throw( exSeekError, exWriteError )
{
  size_t cur = tell();
  seekEnd( 0 );
  size_t result = tell();
  seek( cur );

  return result;
}

bool File::eof() throw( exWriteError )
{
  if ( writeBuffer )
    flushWriteBuffer();

  return feof( f );
}

FILE * File::file() throw( exWriteError )
{
  flushWriteBuffer();

  return f;
}

FILE * File::release() throw( exWriteError )
{
  releaseWriteBuffer();

  FILE * c = f;

  f = 0;

  return c;
}

void File::close() throw( exWriteError )
{
  fclose( release() );
}

File::~File() throw()
{
  if ( f )
  {
    try
    {
      releaseWriteBuffer();
    }
    catch( exWriteError & )
    {
    }
    fclose( f );
  }
}

void File::flushWriteBuffer() throw( exWriteError )
{
  if ( writeBuffer && writeBufferLeft != WriteBufferSize )
  {
    size_t result = fwrite( writeBuffer, WriteBufferSize - writeBufferLeft, 1, f );

    if ( result != 1 )
      throw exWriteError();

    writeBufferLeft = WriteBufferSize;
  }
}

void File::releaseWriteBuffer() throw( exWriteError )
{
  flushWriteBuffer();

  if ( writeBuffer )
  {
    delete [] writeBuffer;

    writeBuffer = 0;
  }
}

File::exReadErrorDetailed::exReadErrorDetailed( int fd )
{
  buildDescription( fd );
}

File::exReadErrorDetailed::exReadErrorDetailed( FILE * f )
{
  buildDescription( fileno( f ) );
}

void File::exReadErrorDetailed::buildDescription( int fd )
{
  description = "Error reading from file ";

  char path[ PATH_MAX ];
  char procFdLink[ 48 ];
  sprintf( procFdLink, "/proc/self/fd/%d", fd );

  int pathChars = readlink( procFdLink, path, sizeof( path ) );

  if ( pathChars < 0 )
    description += "(unknown)";
  else
    description.append( path, pathChars );
}

const char * File::exReadErrorDetailed::what() const throw()
{
  return description.c_str();
}

File::exReadErrorDetailed::~exReadErrorDetailed() throw ()
{
}
