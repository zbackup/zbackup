// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <string.h>

#include "utils.hh"
#include "dir.hh"
#include "debug.hh"

namespace Utils {

vector< string > findOrRebuild( string const & src, string const & dst, string const & relativePath )
{
  vector< string > files;

  Dir::Listing lst ( Dir::addPath( src, relativePath ) );

  Dir::Entry entry;

  while ( lst.getNext( entry ) )
  {
    string currentRelativePath ( relativePath );
    if ( currentRelativePath.empty() )
      currentRelativePath.assign( entry.getFileName() );
    else
      currentRelativePath.assign( Dir::addPath( relativePath, entry.getFileName() ) );

    if ( entry.isDir() )
    {
      verbosePrintf( "Found directory %s...\n", currentRelativePath.c_str() );
      string srcFullPath( Dir::addPath( src, currentRelativePath ) );
      string dstFullPath( Dir::addPath( dst, currentRelativePath ) );
      if ( !dst.empty() && !Dir::exists( dstFullPath.c_str() ) )
      {
        verbosePrintf( "Directory %s not found in destination, creating...\n",
            currentRelativePath.c_str() );
        Dir::create( dstFullPath.c_str() );
      }
      vector< string > subFiles( findOrRebuild( src, dst, currentRelativePath ) );
      files.insert( files.end(), subFiles.begin(), subFiles.end() );
    }
    else
    {
      verbosePrintf( "Found file %s...\n", currentRelativePath.c_str() );
      files.push_back( currentRelativePath );
    }
  }

  return files;
}

unsigned int getScale( char * suffix )
{
  unsigned int scale, scaleBase = 1;

  // Check the suffix
  for ( char * c = suffix; *c; ++c )
    *c = tolower( *c );

  if ( strcmp( suffix, "b" ) == 0 )
  {
    scale = 1;
  }
  else
  if ( strcmp( suffix, "kib" ) == 0 )
  {
    scaleBase = 1024;
    scale = scaleBase;
  }
  else
  if ( strcmp( suffix, "mib" ) == 0 )
  {
    scaleBase = 1024;
    scale = scaleBase * scaleBase;
  }
  else
  if ( strcmp( suffix, "gib" ) == 0 )
  {
    scaleBase = 1024;
    scale = scaleBase * scaleBase * scaleBase;
  }
  else
  if ( strcmp( suffix, "kb" ) == 0 )
  {
    scaleBase = 1000;
    scale = scaleBase;
  }
  else
  if ( strcmp( suffix, "mb" ) == 0 )
  {
    scaleBase = 1000;
    scale = scaleBase * scaleBase;
  }
  else
  if ( strcmp( suffix, "gb" ) == 0 )
  {
    scaleBase = 1000;
    scale = scaleBase * scaleBase * scaleBase;
  }
  else
  {
    // SI or IEC
    fprintf( stderr, "Invalid suffix specified: %s.\n"
             VALID_SUFFIXES, suffix );
    return 0;
  }

  return scale;
}

/// Converts 'size' bytes pointed to by 'in' into a hex string pointed to by
/// 'out'. It should have at least size * 2 bytes. No trailing zero is added
void hexify( unsigned char const * in, unsigned size, char * out )
{
  while( size-- )
  {
    unsigned char v = *in++;

    *out++ = ( v >> 4 < 10 ) ? '0' + ( v >> 4 ) : 'a' + ( v >> 4 ) - 10;
    *out++ = ( ( v & 0xF ) < 10 ) ? '0' + ( v & 0xF ) : 'a' + ( v & 0xF ) - 10;
  }
}

string toHex( string const & in )
{
  return toHex( ( unsigned char const * )in.c_str(), in.size() );
}

string toHex( unsigned char const * in, unsigned size )
{
  string result( size * 2, 0 );
  hexify( in, size, &result[ 0 ] );

  return result;
}

}
