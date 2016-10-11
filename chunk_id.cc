// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "chunk_id.hh"

#include <string.h>
#include "endian.hh"
#include "check.hh"

string ChunkId::toBlob() const
{
  string out( BlobSize, 0 );

  toBlob( &out[ 0 ] );

  return out;
}

void ChunkId::toBlob( void * outPtr ) const
{
  char * out = ( char * ) outPtr;

  RollingHash::Digest v = toLittleEndian( rollingHash );

  memcpy( out, cryptoHash, sizeof( cryptoHash ) );
  memcpy( out + sizeof( cryptoHash ), &v, sizeof( v ) );
}

void ChunkId::setFromBlob( void const * data )
{
  char const * blob = ( char const * ) data;

  RollingHash::Digest v;

  memcpy( cryptoHash, blob, sizeof( cryptoHash ) );
  memcpy( &v, blob + sizeof( cryptoHash ), sizeof( v ) );

  rollingHash = fromLittleEndian( v );
}

bool operator <( const ChunkId &lhs, const ChunkId &rhs )
{
  int r = memcmp( &lhs.cryptoHash, &rhs.cryptoHash, sizeof( lhs.cryptoHash ) );
  if ( r != 0 )
    return r < 0;
  return memcmp( &lhs.rollingHash, &rhs.rollingHash, sizeof( lhs.rollingHash ) ) < 0;
}

ChunkId::ChunkId( string const & blob )
{
  ZBACKUP_CHECK( blob.size() == BlobSize, "incorrect blob size: %zu", blob.size() );

  setFromBlob( blob.data() );
}
