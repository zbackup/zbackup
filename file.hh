// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef FILE_HH_INCLUDED__
#define FILE_HH_INCLUDED__

#include <stddef.h>
#include <cstdio>
#include <exception>
#include <string>

#include "ex.hh"

using std::string;

/// A simple wrapper over FILE * operations with added write-buffering
class File
{
  FILE * f;
  char * writeBuffer;
  size_t writeBufferLeft;

public:
  DEF_EX( Ex, "File exception", std::exception )
  DEF_EX_STR( exCantOpen, "Can't open", Ex )
  DEF_EX( exReadError, "Error reading from file", Ex )
  DEF_EX( exShortRead, "Short read from the file", exReadError )
  DEF_EX( exWriteError, "Error writing to the file", Ex )
  DEF_EX( exSeekError, "File seek error", Ex )
  DEF_EX_STR( exCantErase, "Can't erase file", Ex )
  DEF_EX_STR( exCantRename, "Can't rename file", Ex )

  enum OpenMode
  {
    ReadOnly,
    WriteOnly,
    Update
  };

  typedef long Offset;

  File( char const * filename, OpenMode )
    throw( exCantOpen );

  File( std::string const & filename, OpenMode )
    throw( exCantOpen );

  /// Reads the number of bytes to the buffer, throws an error if it
  /// failed to fill the whole buffer (short read, i/o error etc)
  void read( void * buf, size_t size ) throw( exReadError, exWriteError );

  template< typename T >
  void read( T & value ) throw( exReadError, exWriteError )
  { read( &value, sizeof( value ) ); }

  template< typename T >
  T read() throw( exReadError, exWriteError )
  { T value; read( value ); return value; }

  /// Attempts reading at most 'count' records sized 'size'. Returns
  /// the number of records it managed to read, up to 'count'
  size_t readRecords( void * buf, size_t size, size_t count ) throw( exWriteError );

  /// Writes the number of bytes from the buffer, throws an error if it
  /// failed to write the whole buffer (short write, i/o error etc).
  /// This function employs write buffering, and as such, writes may not
  /// end up on disk immediately, or a short write may occur later
  /// than it really did. If you don't want write buffering, use
  /// writeRecords() function instead
  void write( void const * buf, size_t size ) throw( exWriteError );

  template< typename T >
  void write( T const & value ) throw( exWriteError )
  { write( &value, sizeof( value ) ); }

  /// Attempts writing at most 'count' records sized 'size'. Returns
  /// the number of records it managed to write, up to 'count'.
  /// This function does not employ buffering, but flushes the buffer if it
  /// was used before
  size_t writeRecords( void const * buf, size_t size, size_t count )
    throw( exWriteError );

  /// Reads a string from the file. Unlike the normal fgets(), this one
  /// can strip the trailing newline character, if this was requested.
  /// Returns either s or 0 if no characters were read
  char * gets( char * s, int size, bool stripNl = false ) throw( exWriteError );

  /// Like the above, but uses its own local internal buffer (1024 bytes
  /// currently), and strips newlines by default
  std::string gets( bool stripNl = true ) throw( exReadError, exWriteError );

  /// Seeks in the file, relative to its beginning
  void seek( long offset ) throw( exSeekError, exWriteError );
  /// Seeks in the file, relative to the current position
  void seekCur( long offset ) throw( exSeekError, exWriteError );
  /// Seeks in the file, relative to the end of file
  void seekEnd( long offset = 0 ) throw( exSeekError, exWriteError );

  /// Seeks to the beginning of file
  void rewind() throw( exSeekError, exWriteError );

  /// Tells the current position within the file, relative to its beginning
  size_t tell() throw( exSeekError );

  /// Returns file size
  size_t size() throw( exSeekError, exWriteError );

  /// Returns true if end-of-file condition is set
  bool eof() throw( exWriteError );

  /// Returns the underlying FILE * record, so other operations can be
  /// performed on it
  FILE * file() throw( exWriteError );

  /// Releases the file handle out of the control of the class. No further
  /// operations are valid. The file will not be closed on destruction
  FILE * release() throw( exWriteError );

  /// Closes the file. No further operations are valid
  void close() throw( exWriteError );

  /// Checks if the file exists or not
  static bool exists( char const * filename ) throw();

  static bool exists( std::string const & filename ) throw()
  { return exists( filename.c_str() ); }

  ~File() throw();

  /// Erases the given file
  static void erase( std::string const & ) throw( exCantErase );

  /// Renames the given file
  static void rename( std::string const & from,
                      std::string const & to ) throw( exCantRename );

  /// Throwing this class instead of exReadError will make the description
  /// include the file name
  class exReadErrorDetailed: public exReadError
  {
    string description;

  public:
    exReadErrorDetailed( int fd );
    exReadErrorDetailed( FILE * f );
    virtual const char * what() const throw();
    virtual ~exReadErrorDetailed() throw ();

  private:
    void buildDescription( int fd );
  };

private:

  void open( char const * filename, OpenMode ) throw( exCantOpen );
  void flushWriteBuffer() throw( exWriteError );
  void releaseWriteBuffer() throw( exWriteError );
};

#endif
