// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdlib.h>
#include <new>

#include "appendallocator.hh"

AppendAllocator::AppendAllocator( unsigned blockSize_, unsigned granularity ):
  alignMask( granularity - 1 ),
  // We may decide to enlarge the block to make sure it is a multiple of
  // granularity. An improperly sized block would just waste the leftover
  // bytes
  blockSize( ( blockSize_ + alignMask ) & ~alignMask ), leftInBlock( -1 )
{
}

char * AppendAllocator::allocateBytes( unsigned size )
{
  // For zero-sized allocations, we always return a non-zero pointer. To do
  // that, we need to make sure we have it
  if ( !size && !blocks.empty() )
    return nextAvailable;

  if ( leftInBlock < (int) size )
  {
    unsigned toAllocate = ( size <= blockSize ? blockSize : size );

    // Need a new block
    char * p = (char *) malloc( toAllocate );

    if ( !p )
      throw std::bad_alloc();

    blocks.push_back( Record( p, nextAvailable, leftInBlock ) );

    leftInBlock = (int) toAllocate;
    nextAvailable = p;
  }

  // We may need to allocate more than was asked to preserve granularity
  int toTake = (int) ( ( size + alignMask ) & ~alignMask );

  char * result = nextAvailable;

  nextAvailable += toTake;

  leftInBlock -= toTake; // leftInBlock can become negative here, as toTake can
  // actually be larger than the space left due to an added alignment

  return result;
}

void AppendAllocator::returnBytes( unsigned size )
{
  if ( !size )
    return;

  // If we are pointing to the start of the block, we need to free it and go
  // back to the previous one
  if ( nextAvailable == blocks.back().data )
  {
    if ( blocks.size() == 1 )
      throw std::bad_alloc();

    free( blocks.back().data );
    leftInBlock = blocks.back().prevLeftInBlock;
    nextAvailable = blocks.back().prevNextAvailable;
    blocks.pop_back();
  }

  unsigned toTake = ( size + alignMask ) & ~alignMask;

  // There must be enough used bytes in the block
  if ( nextAvailable - blocks.back().data < (int) toTake )
    throw std::bad_alloc();

  nextAvailable -= toTake;
  leftInBlock += toTake;
}

void AppendAllocator::clear()
{
  for ( unsigned x = blocks.size(); x--; )
    free( blocks[ x ].data );
  blocks.clear();

  leftInBlock = -1;
}

AppendAllocator::~AppendAllocator()
{
  clear();
}
