// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "backup_file.hh"

#include "encrypted_file.hh"
#include "encryption.hh"
#include "message.hh"

namespace BackupFile {

enum
{
  FileFormatVersion = 1
};

void save( string const & fileName, EncryptionKey const & encryptionKey,
           BackupInfo const & backupInfo )
{
  EncryptedFile::OutputStream os( fileName.c_str(), encryptionKey,
                                  Encryption::ZeroIv );
  os.writeRandomIv();

  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, os );

  Message::serialize( backupInfo, os );
  os.writeAdler32();
}

void load( string const & fileName, EncryptionKey const & encryptionKey,
           BackupInfo & backupInfo )
{
  EncryptedFile::InputStream is( fileName.c_str(), encryptionKey,
                                 Encryption::ZeroIv );
  is.consumeRandomIv();

  FileHeader header;
  Message::parse( header, is );
  if ( header.version() != FileFormatVersion )
    throw exUnsupportedVersion();

  Message::parse( backupInfo, is );
  is.checkAdler32();
}

}
