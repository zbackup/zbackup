// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <iomanip>
#include <sstream>
#include "sha256.hh"

using namespace std;

Sha256::Sha256()
{
  SHA256_Init( &ctx );
}

void Sha256::add( void const * data, size_t size )
{
  SHA256_Update( &ctx, data, size );
}

void Sha256::finish( void * result )
{
  SHA256_Final( ( unsigned char * ) result, &ctx );
}

string Sha256::finish()
{
  char buf[ Size ];
  finish( buf );

  return string( buf, buf + sizeof( buf ) );
}

string Sha256::getAsHex( string const & input )
{
    stringstream ss;
    for( int i = 0; i < SHA256_DIGEST_LENGTH; i++ )
        ss << hex << setw(2) << setfill('0') << ( unsigned )( unsigned char )input[ i ];
    return ss.str();
}

