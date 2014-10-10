// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef DIR_HH_INCLUDED__
#define DIR_HH_INCLUDED__

#include <dirent.h>
#include <sys/types.h>
#include <exception>
#include <string>

#include "ex.hh"
#include "nocopy.hh"

using std::string;

/// Directory-related operations
namespace Dir {

DEF_EX( Ex, "Directory exception", std::exception )
DEF_EX_STR( exCantCreate, "Can't create directory", Ex )
DEF_EX_STR( exCantRemove, "Can't remove directory", Ex )
DEF_EX_STR( exCantList, "Can't list directory", Ex )
DEF_EX_STR( exCantGetRealPath, "Can't real path of", Ex )

/// Checks whether the given dir exists or not
bool exists( string const & );

/// Creates the given directory
void create( string const & );

/// Removes the given directory. It must be empty to be removed
void remove( string const & );

/// Adds one path to another, e.g. for /hello/world and baz/bar, returns
/// /hello/world/baz/bar
string addPath( string const & first, string const & second );

/// Returns the canonicalized absolute pathname with symlinks resolved
string getRealPath( string const & );

/// Returns the directory part of the given path
string getDirName( string const & );

/// A separator used to separate names in the path.
inline char separator()
{ return '/'; }

class Entry
{
  string fileName;
  bool dir;
  bool symlink;

public:
  Entry() {}
  Entry( string const & fileName, bool dir, bool symlink ):
    fileName( fileName ), dir( dir ), symlink( symlink ) {}

  string const & getFileName() const
  { return fileName; }

  bool isDir() const
  { return dir; }

  bool isSymLink() const
  { return symlink; }
};

/// Allows listing the directory
class Listing: NoCopy
{
  string dirName;
  DIR * dir;
public:
  Listing( string const & dirName );
  ~Listing();

  /// Return true if entry was filled, false if end of dir was encountered
  bool getNext( Entry & );
};

}

#endif
