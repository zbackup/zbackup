// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>

#include "../dir.hh"
#include "../file.hh"

using std::string;
using std::vector;
using std::map;

void mention( File & file, string const & path )
{
  file.write( path.data(), path.size() );
  file.write( '\n' );
}

bool startsWith( string const & s, char const * prefix )
{
  for ( char const * sPtr = s.c_str(), * pPtr = prefix; *pPtr; ++sPtr, ++pPtr )
    if ( *sPtr != *pPtr )
      return false;

  return true;
}

void scanDirIgnoringErrors( string const & path, File & includes, File & excludes,
                            bool currentlyIncluded );

void scanDir( string const & path, File & includes, File & excludes,
              bool currentlyIncluded )
{
  Dir::Entry entry;

  vector< string > subdirs;
  vector< string > namedIncludes, namedExcludes;
  typedef map< string, bool > FileList;
  FileList fileList;
  bool doBackup = false;
  bool dontBackup = false;

  for ( Dir::Listing dir( path ); dir.getNext( entry ); )
  {
    string const & fileName = entry.getFileName();

    if ( entry.isDir() )
    {
      if ( !entry.isSymLink() )
        subdirs.push_back( fileName );
    }
    else
    if ( fileName == ".backup" )
      doBackup = true;
    if ( fileName == ".no-backup" )
      dontBackup = true;
    else
    if ( startsWith( fileName, ".backup-" ) )
      namedIncludes.push_back( fileName.substr( 8 ) );
    else
    if ( startsWith( fileName, ".no-backup-" ) )
      namedExcludes.push_back( fileName.substr( 11 ) );
  }

  // If both are mentioned, backup
  if ( doBackup )
    dontBackup = false;

  if ( doBackup && !currentlyIncluded )
  {
    mention( includes, path );
    currentlyIncluded = true;
  }

  if ( dontBackup && currentlyIncluded )
  {
    mention( excludes, path );
    currentlyIncluded = false;
  }

  // If we have any effective named lists, build the fileList map and process
  // them.
  if ( ( !currentlyIncluded && !namedIncludes.empty() ) ||
       ( currentlyIncluded && !namedExcludes.empty() ) )
  {
    for ( Dir::Listing dir( path ); dir.getNext( entry ); )
      fileList[ entry.getFileName() ] = entry.isDir() && !entry.isSymLink();

    if ( !currentlyIncluded )
    {
      for ( vector< string > :: const_iterator i = namedIncludes.begin();
            i != namedIncludes.end(); ++i )
      {
        FileList::iterator entry = fileList.find( *i );

        if ( entry != fileList.end() )
        {
          mention( includes, Dir::addPath( path, *i ) );

          if ( entry->second ) // Is it a dir? Scan it then.
            scanDir( Dir::addPath( path, entry->first ), includes, excludes,
                     true );

          // Make sure we don't process it twice.
          fileList.erase( entry );
        }
        else
          fprintf( stderr, "Warning: named include %s does not exist in %s\n",
                   i->c_str(), path.c_str() );
      }
    }
    else
    {
      for ( vector< string > :: const_iterator i = namedExcludes.begin();
            i != namedExcludes.end(); ++i )
      {
        FileList::iterator entry = fileList.find( *i );

        if ( entry != fileList.end() )
        {
          mention( excludes, Dir::addPath( path, *i ) );

          if ( entry->second ) // Is it a dir? Scan it then.
            scanDir( Dir::addPath( path, entry->first ), includes, excludes,
                     false );

          // Make sure we don't process it twice.
          fileList.erase( entry );
        }
        else
          fprintf( stderr, "Warning: named exclude %s does not exist in %s\n",
                   i->c_str(), path.c_str() );
      }
    }

    // Scan the rest of dirs
    for ( FileList::const_iterator i = fileList.begin(); i != fileList.end();
          ++i )
      if ( i->second )
        scanDirIgnoringErrors( Dir::addPath( path, i->first ), includes,
                               excludes, currentlyIncluded );
  }
  else
  {
    // No named lists -- just process all the dirs
    for ( size_t x = 0; x < subdirs.size(); ++x )
      scanDirIgnoringErrors( Dir::addPath( path, subdirs[ x ] ), includes,
                             excludes, currentlyIncluded );
  }
}

void scanDirIgnoringErrors( string const & path, File & includes, File & excludes,
                            bool currentlyIncluded )
{
  try
  {
    scanDir( path, includes, excludes, currentlyIncluded );
  }
  catch( Dir::exCantList & e )
  {
    fprintf( stderr, "Warning: %s\n", e.what() );
  }
}

int main( int argc, char *argv[] )
{
  if ( argc != 4 )
  {
    fprintf( stderr, "Usage: %s <root dir> <includes out file> <excludes out file>\n", *argv );
    return EXIT_FAILURE;
  }

  try
  {
    File includes( argv[ 2 ], File::WriteOnly );
    File excludes( argv[ 3 ], File::WriteOnly );

    scanDir( argv[ 1 ], includes, excludes, false );

    return EXIT_SUCCESS;
  }
  catch( std::exception & e )
  {
    fprintf( stderr, "Error: %s\n", e.what() );

    return EXIT_FAILURE;
  }
}
