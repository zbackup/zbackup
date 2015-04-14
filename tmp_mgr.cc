// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "tmp_mgr.hh"

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "dir.hh"
#include "file.hh"

TemporaryFile::TemporaryFile( string const & fileName ): fileName( fileName )
{
}

void TemporaryFile::moveOverTo( string const & destinationFileName,
                                bool mayOverwrite )
{
  if ( !mayOverwrite && File::exists( destinationFileName ) )
    throw TmpMgr::exWontOverwrite( destinationFileName );

  File::rename( fileName, destinationFileName );
  fileName.clear();
}

TemporaryFile::~TemporaryFile()
{
  if ( !fileName.empty() )
    File::erase( fileName );
}

string const & TemporaryFile::getFileName() const
{
  return fileName;
}

TmpMgr::TmpMgr( string const & path ): path( path )
{
  if ( !Dir::exists( path ) )
    Dir::create( path );
}

sptr< TemporaryFile > TmpMgr::makeTemporaryFile()
{
  string name( Dir::addPath( path, "XXXXXX") );

  // Set the umask to remove permisions of grp and other
  // in case an insecure (older) mkstemp is used.
  mode_t old_mask = umask( S_IRWXG | S_IRWXO );
  // Create the temporary file.
  int fd = mkstemp( &name[ 0 ] );
  // Reset the umask to the previous setting
  umask(old_mask);

  if ( fd == -1 || close( fd ) != 0 )
    throw exCantCreate( path );

  return new TemporaryFile( name );
}

TmpMgr::~TmpMgr()
{
  try
  {
    Dir::remove( path );
  }
  catch( Dir::exCantRemove & )
  {
  }
}
