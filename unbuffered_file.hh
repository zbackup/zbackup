// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef UNBUFFERED_FILE_HH_INCLUDED__
#define UNBUFFERED_FILE_HH_INCLUDED__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <exception>

#include "ex.hh"
#include "nocopy.hh"

/// A file which does not employ its own buffering.
/// TODO: add support for memory-mapped I/O, with the interface which would look
/// like that of a zero-copy stream. However, since we can do encryption in-
/// place, both interfaces should be available - when there's no memory-mapped
/// I/O available, the user should still provide its own buffer (and then do
/// in-place encryption in it).
class UnbufferedFile: NoCopy
{
public:

  DEF_EX( Ex, "Unbuffered file exception", std::exception )
  DEF_EX_STR( exCantOpen, "Can't open file", Ex )
  DEF_EX( exReadError, "File read error", Ex )
  DEF_EX( exWriteError, "File write error", Ex )
  DEF_EX( exSeekError, "File seek error", Ex )

  enum Mode
  {
    ReadOnly,
    WriteOnly
  };

  typedef int64_t Offset;

  /// Opens the given file
  UnbufferedFile( char const * fileName, Mode ) throw( exCantOpen );

  /// Reads up to 'size' bytes into the buffer. Returns the number of bytes
  /// read. If the value returned is less than the 'size' provided, the end of
  /// file was reached
  size_t read( void * buf, size_t size ) throw( exReadError );

  /// Writes 'size' bytes
  void write( void const * buf, size_t size ) throw( exWriteError );

  /// Returns file size
  Offset size() throw( exSeekError );

  /// Seeks to the given offset, relative to the current file offset
  void seekCur( Offset ) throw( exSeekError );

  ~UnbufferedFile() throw();

private:
  int fd;
};

#endif
