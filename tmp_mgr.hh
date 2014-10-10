// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef TMP_MGR_HH_INCLUDED__
#define TMP_MGR_HH_INCLUDED__

#include <exception>
#include <string>

#include "dir.hh"
#include "ex.hh"
#include "file.hh"
#include "nocopy.hh"
#include "sptr.hh"

/// A temporary file
class TemporaryFile: NoCopy
{
public:
  /// Returns the temporary file's file name. The file may already be existent -
  /// it is supposed to be overwritten then
  string const & getFileName() const;
  /// Renames this temporary file over the given file name. If the destination
  /// file exists already, it gets replaced if mayOverwrite is true, or throws
  /// an exception otherwise
  void moveOverTo( string const & destinationFileName, bool mayOverwrite = false );
  /// Removes the file from the disk, unless moveOverTo() was called previously
  ~TemporaryFile();

private:
  /// Use TmpMgr::makeTemporaryFile() instead of this constructor
  TemporaryFile( string const & fileName );

  string fileName;

  friend class TmpMgr;
};

/// Allows creating temporary files and later either removing them or moving
/// them over to the target ones
class TmpMgr: NoCopy
{
  string path;
public:

  DEF_EX( Ex, "Temporary file manager exception", std::exception )
  DEF_EX_STR( exCantCreate, "Can't create a temporary file in dir", Ex )
  DEF_EX_STR( exWontOverwrite, "Won't overwrite existing file", Ex )

  /// Creates the given directory if it doesn't exist already and uses it to
  /// store temporary files.
  TmpMgr( string const & path );

  /// Creates an new empty temporary file and returns its full file name,
  /// including the path. The file is then supposed to be overwritten
  sptr< TemporaryFile > makeTemporaryFile();

  /// Removes the temporary directory, if possible
  ~TmpMgr();
};

#endif
