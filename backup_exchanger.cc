// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "backup_exchanger.hh"
#include "dir.hh"
#include "debug.hh"

namespace BackupExchanger {

vector< string > recreateDirectories( string const & src, string const & dst, string const & relativePath )
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
      string srcFullPath ( Dir::addPath( src, currentRelativePath ) );
      string dstFullPath ( Dir::addPath( dst, currentRelativePath ) );
      if ( !Dir::exists( dstFullPath.c_str() ) )
      {
        verbosePrintf( "Directory %s not found in destination, creating...\n",
            currentRelativePath.c_str() );
        Dir::create( dstFullPath.c_str() );
      }
      vector< string > subFiles ( recreateDirectories( src, dst, currentRelativePath ) );
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
}
