// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <string.h>

#include "bundle.hh"
#include "encryption.hh"
#include "index_file.hh"
#include "message.hh"

namespace IndexFile {

enum
{
  FileFormatVersion = 1
};

Writer::Writer( EncryptionKey const & key, string const & fileName ):
  stream( fileName.c_str(), key, Encryption::ZeroIv )
{
  stream.writeRandomIv();
  FileHeader header;
  header.set_version( FileFormatVersion );
  Message::serialize( header, stream );
}

void Writer::add( BundleInfo const & info, Bundle::Id const & bundleId )
{
  IndexBundleHeader header;
  header.set_id( &bundleId, sizeof( bundleId ) );

  Message::serialize( header, stream );
  Message::serialize( info, stream );
}

Writer::~Writer()
{
  // Final record which does not have a bundle id
  IndexBundleHeader header;
  Message::serialize( header, stream );
  stream.writeAdler32();
}

Reader::Reader( EncryptionKey const & key, string const & fileName ):
  stream( fileName.c_str(), key, Encryption::ZeroIv )
{
  stream.consumeRandomIv();

  FileHeader header;
  Message::parse( header, stream );

  if ( header.version() != FileFormatVersion )
    throw exUnsupportedVersion();
}

bool Reader::readNextRecord( BundleInfo & info, Bundle::Id & bundleId )
{
  IndexBundleHeader header;
  Message::parse( header, stream );

  if ( header.has_id() )
  {
    if ( header.id().size() != sizeof( bundleId ) )
      throw exIncorrectBundleIdSize();

    memcpy( &bundleId, header.id().data(), sizeof( bundleId ) );

    Message::parse( info, stream );
    return true;
  }
  else
  {
    stream.checkAdler32();
    return false;
  }
}

}
