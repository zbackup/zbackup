// Part of ZBackup. Licensed under GNU GPLv2 or later

#include "backup_exchanger.hh"
#include "dir.hh"
#include "debug.hh"

namespace BackupExchanger {

vector< string > recreateDirectories( string const & src, string const & dst, string const & relaPath )
{
  vector< string > files;

  Dir::Listing lst ( Dir::addPath( src, relaPath ) );

  Dir::Entry entry;

  while ( lst.getNext( entry ) )
  {
    string curRelaPath ( relaPath );
    if ( curRelaPath.empty() )
      curRelaPath.assign( entry.getFileName() );
    else
      curRelaPath.assign( Dir::addPath( relaPath, entry.getFileName() ) );

    if ( entry.isDir() )
    {
      verbosePrintf( "Found directory %s...\n", curRelaPath.c_str() );
      string srcFullPath ( Dir::addPath( src, curRelaPath ) );
      string dstFullPath ( Dir::addPath( dst, curRelaPath ) );
      if ( !Dir::exists( dstFullPath.c_str() ) )
      {
        verbosePrintf( "Directory %s not found in destination, creating...\n",
            curRelaPath.c_str() );
        Dir::create( dstFullPath.c_str() );
      }
      vector< string > subFiles ( recreateDirectories( src, dst, curRelaPath ) );
      files.insert( files.end(), subFiles.begin(), subFiles.end() );
    }
    else
    {
      verbosePrintf( "Found file %s...\n", curRelaPath.c_str() );
      files.push_back( curRelaPath );
    }
  }

  return files;
}
}
