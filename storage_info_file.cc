// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stddef.h>

#include "encrypted_file.hh"
#include "message.hh"
#include "storage_info_file.hh"
#include "debug.hh"

namespace StorageInfoFile {

enum
{
  FileFormatVersion = 1
};

void save( string const & fileName, StorageInfo const & storageInfo )
{
  dPrintf( "Saving storage info...\n" );
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
  dPrintf( "Loading storage info...\n" );
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

namespace ExtendedStorageInfoFile {

enum
{
  FileFormatVersion = 1
};

void save( string const & fileName, EncryptionKey const & encryptionKey,
           ExtendedStorageInfo const & extendedStorageInfo )
{
  dPrintf( "Saving extended storage info, hasKey: %s\n",
      encryptionKey.hasKey() ? "true" : "false" );
  EncryptedFile::OutputStream os( fileName.c_str(), encryptionKey,
                                  Encryption::ZeroIv );
  os.writeRandomIv();

  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, os );

  Message::serialize( extendedStorageInfo, os );
  os.writeAdler32();
}

void load( string const & fileName, EncryptionKey const & encryptionKey,
           ExtendedStorageInfo & extendedStorageInfo )
{
  dPrintf( "Loading extended storage info, hasKey: %s\n",
      encryptionKey.hasKey() ? "true" : "false" );
  EncryptedFile::InputStream is( fileName.c_str(), encryptionKey,
                                 Encryption::ZeroIv );
  is.consumeRandomIv();

  FileHeader header;
  Message::parse( header, is );
  if ( header.version() != FileFormatVersion )
    throw exUnsupportedVersion();

  Message::parse( extendedStorageInfo, is );
  is.checkAdler32();
}

}
