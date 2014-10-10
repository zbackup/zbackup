// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <openssl/sha.h>
#include <string.h>

#include "backup_creator.hh"
#include "check.hh"
#include "debug.hh"
#include "message.hh"
#include "page_size.hh"
#include "static_assert.hh"

namespace {
  unsigned const MinChunkSize = 256;
}

BackupCreator::BackupCreator( StorageInfo const & info,
                              ChunkIndex & chunkIndex,
                              ChunkStorage::Writer & chunkStorageWriter ):
  chunkMaxSize( info.chunk_max_size() ),
  chunkIndex( chunkIndex ), chunkStorageWriter( chunkStorageWriter ),
  ringBufferFill( 0 ),
  chunkToSaveFill( 0 ),
  backupDataStream( new google::protobuf::io::StringOutputStream( &backupData ) ),
  chunkIdGenerated( false )
{
  // In our ring buffer we have enough space to store one chunk plus an extra
  // page for buffering the input
  ringBuffer.resize( chunkMaxSize + getPageSize() );

  begin = ringBuffer.data();
  end = &ringBuffer.back() + 1;
  head = begin;
  tail = head;

  chunkToSave.resize( chunkMaxSize );
}

void * BackupCreator::getInputBuffer()
{
  return head;
}

size_t BackupCreator::getInputBufferSize()
{
  if ( tail > head )
    return tail - head;
  else
  if ( tail == head && ringBufferFill )
    return 0;
  else
    return end - head;
}

void BackupCreator::handleMoreData( unsigned added )
{
  // Note: head is never supposed to wrap around in the middle of the operation,
  // as getInputBufferSize() never returns a value which could result in a
  // wrap-around
  while( added )
  {
    // If we don't have a full chunk, we need to consume data until we have
    // one
    if ( ringBufferFill < chunkMaxSize )
    {
      unsigned left = chunkMaxSize - ringBufferFill;
      bool canFullyFill = added >= left;

      unsigned toFill = canFullyFill ? left : added;

      added -= toFill;
      ringBufferFill += toFill;

      while ( toFill-- )
        rollingHash.rollIn( *head++ );

      if ( head == end )
        head = begin;

      // If we've managed to fill in the complete chunk, attempt matching it
      if ( canFullyFill )
        addChunkIfMatched();
    }
    else
    {
      // At this point we have a full chunk in the ring buffer, so we can rotate
      // over a byte
      chunkToSave[ chunkToSaveFill++ ] = *tail;

      if ( chunkToSaveFill == chunkMaxSize )
        // Got the full chunk - save it
        saveChunkToSave();

      rollingHash.rotate( *head++, *tail++ );

      if ( head == end )
        head = begin;

      if ( tail == end )
        tail = begin;

      addChunkIfMatched();

      --added; // A byte was consumed
    }
  }
}

void BackupCreator::saveChunkToSave()
{
  CHECK( chunkToSaveFill > 0, "chunk to save is empty" );

  if ( chunkToSaveFill < 128 ) // TODO: make this value configurable
  {
    // The amount of data is too small - emit without creating a new chunk
    BackupInstruction instr;
    instr.set_bytes_to_emit( chunkToSave.data(), chunkToSaveFill );
    outputInstruction( instr );
  }
  else
  {
    // Output as a chunk

    ChunkId id;

    id.rollingHash = RollingHash::digest( chunkToSave.data(),
                                          chunkToSaveFill );
    unsigned char sha1Value[ SHA_DIGEST_LENGTH ];
    SHA1( (unsigned char const *) chunkToSave.data(), chunkToSaveFill,
          sha1Value );

    STATIC_ASSERT( sizeof( id.cryptoHash ) <= sizeof( sha1Value ) );
    memcpy( id.cryptoHash, sha1Value, sizeof( id.cryptoHash ) );

    // Save it to the store if it's not there already
    chunkStorageWriter.add( id, chunkToSave.data(), chunkToSaveFill );

    BackupInstruction instr;
    instr.set_chunk_to_emit( id.toBlob() );
    outputInstruction( instr );
  }

  chunkToSaveFill = 0;
}

void BackupCreator::finish()
{
  dPrintf( "At finish: %u, %u\n", chunkToSaveFill, ringBufferFill );

  // At this point we may have some bytes in chunkToSave, and some in the ring
  // buffer. We need to save both
  if ( chunkToSaveFill + ringBufferFill > chunkMaxSize )
  {
    // We have more than a full chunk in chunkToSave and ringBuffer together, so
    // save the first part as a full chunk first

    // Move data from ring buffer to have full chunk in chunkToSave.
    moveFromRingBufferToChunkToSave( chunkMaxSize - chunkToSaveFill );
    saveChunkToSave();
  }

  // Concatenate the rest of data and save it too

  CHECK( chunkToSaveFill + ringBufferFill <= chunkMaxSize, "had more than two "
         "full chunks at backup finish" );

  moveFromRingBufferToChunkToSave( ringBufferFill );

  if ( chunkToSaveFill )
    saveChunkToSave();
}

void BackupCreator::moveFromRingBufferToChunkToSave( unsigned toMove )
{
  // If tail is before head, all data in the ring buffer is in one contiguous
  // piece. If not, it's in two pieces
  if ( tail < head )
  {
    memcpy( chunkToSave.data() + chunkToSaveFill, tail, toMove );
    tail += toMove;
  }
  else
  {
    unsigned toEnd = end - tail;

    unsigned firstPart = toEnd < toMove ? toEnd : toMove;
    memcpy( chunkToSave.data() + chunkToSaveFill, tail, firstPart );

    tail += firstPart;

    if ( toMove > firstPart )
    {
      unsigned secondPart = toMove - firstPart;
      memcpy( chunkToSave.data() + chunkToSaveFill + firstPart, begin,
              secondPart );
      tail = begin + secondPart;
    }
  }

  if ( tail == end )
    tail = begin;

  chunkToSaveFill += toMove;
  ringBufferFill -= toMove;
}

ChunkId const & BackupCreator::getChunkId()
{
  if ( !chunkIdGenerated )
  {
    // Calculate SHA1
    SHA_CTX ctx;
    SHA1_Init( &ctx );

    if ( tail < head )
    {
      // Tail is before head - all the block is in one contiguous piece
      SHA1_Update( &ctx, tail, head - tail );
    }
    else
    {
      // Tail is after head - the block consists of two pieces
      SHA1_Update( &ctx, tail, end - tail );
      SHA1_Update( &ctx, begin, head - begin );
    }

    unsigned char sha1Value[ SHA_DIGEST_LENGTH ];
    SHA1_Final( sha1Value, &ctx );

    generatedChunkId.rollingHash = rollingHash.digest();

    memcpy( generatedChunkId.cryptoHash, sha1Value,
            sizeof( generatedChunkId.cryptoHash ) );

    chunkIdGenerated = true;
  }

  return generatedChunkId;
}

void BackupCreator::addChunkIfMatched()
{
  chunkIdGenerated = false;

  if ( chunkIndex.findChunk( rollingHash.digest(), *this ) )
  {
//    verbosePrintf( "Reuse of chunk %lu\n", rollingHash.digest() );

    // Before emitting the matched chunk, we need to make sure any bytes
    // which came before it are saved first
    if ( chunkToSaveFill )
      saveChunkToSave();

    // Add the record
    BackupInstruction instr;
    instr.set_chunk_to_emit( getChunkId().toBlob() );
    outputInstruction( instr );

    // The block was consumed from the ring buffer - remove the block from it
    tail = head;
    ringBufferFill = 0;
    rollingHash.reset();
  }
}

void BackupCreator::outputInstruction( BackupInstruction const & instr )
{
  // TODO: once backupData becomes large enough, spawn another BackupCreator and
  // feed data to it. This way we wouldn't have to store the entire backupData
  // in RAM
  Message::serialize( instr, *backupDataStream );
}

void BackupCreator::getBackupData( string & str )
{
  CHECK( backupDataStream.get(), "getBackupData() called twice" );
  backupDataStream.reset();
  str.swap( backupData );
}
