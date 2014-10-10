// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef APPENDALLOCATOR_HH_INCLUDED__
#define APPENDALLOCATOR_HH_INCLUDED__

#include <stdlib.h>
#include <limits>
#include <new>
#include <vector>

/// A simple "add-only" memory allocation mechanism.
class AppendAllocator
{
  unsigned alignMask;
  unsigned blockSize;

  struct Record
  {
    char * data;
    char * prevNextAvailable;
    int prevLeftInBlock;

    Record( char * data_, char * prevNextAvailable_, int prevLeftInBlock_ ):
      data( data_ ), prevNextAvailable( prevNextAvailable_ ),
      prevLeftInBlock( prevLeftInBlock_ ) {}
  };

  std::vector< Record > blocks;
  char * nextAvailable;
  int leftInBlock; // Can become < 0 due to added alignment

public:

  /// blockSize is the amount of bytes allocated for each of the underlying
  /// storage blocks. granularity makes sure you allocate objects with
  /// the proper alignment. It must be a power of 2
  AppendAllocator( unsigned blockSize, unsigned granularity );
  ~AppendAllocator();

  /// Removes all data from the append allocator.
  void clear();

  /// Allocates a size-sized memory block. The only way to free it is to
  /// destroy the whole AppendAllocator. Can throw bad_alloc in an out-of-
  /// memory situation
  char * allocateBytes( unsigned size );

  /// Returns the allocated bytes back. The size must match the size passed
  /// to allocateBytes() on the last invocation. Calls to allocateBytes()/
  /// returnBytes() must follow the stack order - returnBytes() should undo
  /// the previous allocateBytes()
  void returnBytes( unsigned size );

  /// Allocates memory to hold 'count' objects of T. In essense, it just does
  /// multiplication and type casting
  template< typename T >
  T * allocateObjects( unsigned count )
  { return (T *) allocateBytes( count * sizeof( T ) ); }

  /// Returns the allocated objects back
  template< typename T >
  void returnObjects( unsigned count )
  { returnBytes( count * sizeof( T ) ); }
};

#endif
