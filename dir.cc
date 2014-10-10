// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "dir.hh"

DIR * dir;

namespace Dir {
bool exists( string const & name )
{
  struct stat buf;

  return stat( name.c_str(), &buf ) == 0 && S_ISDIR( buf.st_mode );
}

void create( string const & name )
{
  if ( mkdir( name.c_str(), 0777 ) != 0 )
    throw exCantCreate( name );
}

void remove( string const & name )
{
  if ( rmdir( name.c_str() ) != 0 )
    throw exCantRemove( name );
}

string addPath( string const & first, string const & second )
{
  if ( first.empty() )
    return second;

  if ( second.empty() )
    return first;

  if ( first[ first.size() - 1 ] == separator() )
    return first + second;
  else
    return first + separator() + second;
}

string getRealPath( string const & path )
{
  if ( char * r = realpath( path.c_str(), NULL ) )
  {
    string result( r );
    free( r );
    return result;
  }
  else
    throw exCantGetRealPath( path );
}

string getDirName( string const & path )
{
  char const * c = path.c_str();
  std::vector< char > copy( c, c + path.size() + 1 );

  return dirname( copy.data() );
}

Listing::Listing( string const & dirName ): dirName( dirName )
{
  dir = opendir( dirName.c_str() );

  if ( !dir )
    throw exCantList( dirName );

}

Listing::~Listing()
{
  closedir( dir );
}

bool Listing::getNext( Entry & result )
{
  dirent entry;

  dirent * entryPtr;

  struct stat entryStats;

  for ( ; ; )
  {
    if ( readdir_r( dir, &entry, &entryPtr ) != 0 )
      throw exCantList( dirName );

    if ( !entryPtr )
      return false;

#ifndef __APPLE__
    if ( fstatat( dirfd( dir ), entry.d_name, &entryStats,
                  AT_SYMLINK_NOFOLLOW ) != 0 )
#else
    if ( lstat( addPath( dirName, entry.d_name ).c_str(),
                &entryStats ) != 0)
#endif
      throw exCantList( dirName );

    bool isDir = S_ISDIR( entryStats.st_mode );
    bool isSymLink = S_ISLNK( entryStats.st_mode );

    if ( isDir &&
         ( entry.d_name[ 0 ] == '.' &&
           ( !entry.d_name[ 1 ] || entry.d_name[ 1 ] == '.' ) ) )
    {
      // Skip the . or .. entries
      continue;
    }

    result = Entry( entry.d_name, isDir, isSymLink );
    return true;
  }
}

}
