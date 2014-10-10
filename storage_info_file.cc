// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stddef.h>

#include "encrypted_file.hh"
#include "message.hh"
#include "storage_info_file.hh"

namespace StorageInfoFile {

enum
{
  FileFormatVersion = 1
};

void save( string const & fileName, StorageInfo const & storageInfo )
{
  EncryptedFile::OutputStream os( fileName.c_str(), EncryptionKey::noKey(),
                                  NULL );
  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, os );

  Message::serialize( storageInfo, os );
  os.writeAdler32();
}

void load( string const & fileName, StorageInfo & storageInfo )
{
  EncryptedFile::InputStream is( fileName.c_str(), EncryptionKey::noKey(),
                                 NULL );
  FileHeader header;
  Message::parse( header, is );
  if ( header.version() != FileFormatVersion )
    throw exUnsupportedVersion();

  Message::parse( storageInfo, is );
  is.checkAdler32();
}

}
