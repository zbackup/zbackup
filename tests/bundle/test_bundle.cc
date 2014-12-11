// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "../../encrypted_file.hh"
#include "../../encryption_key.hh"
#include "../../random.hh"
#include "../../tmp_mgr.hh"
#include "../../check.hh"
#include "../../adler32.hh"
#include "../../bundle.hh"
#include "../../compression.hh"
#include "../../message.hh"

using namespace Compression;


char tmpbuf[100];

void testCompatibility()
{
  // The LZO code uses a different file header than the previous code
  // because it adds the compression_method field. Nevertheless, it
  // must be compatible with previous code.

  TmpMgr tmpMgr( "/dev/shm" );
  sptr< TemporaryFile > tempFile = tmpMgr.makeTemporaryFile();
  std::string fileName = tempFile->getFileName();

  EncryptionKey noKey( std::string(), NULL );


  // Write old header, read as new header
  {
    {
      EncryptedFile::OutputStream os( fileName.c_str(), noKey, Encryption::ZeroIv );

      FileHeader header;
      header.set_version( 42 );
      Message::serialize( header, os );
    }

    {
      EncryptedFile::InputStream is( fileName.c_str(), noKey, Encryption::ZeroIv );

      BundleFileHeader header;
      Message::parse( header, is );

      CHECK( header.version()            == 42,     "version is wrong when reading old header with new program" );
      CHECK( header.compression_method() == "lzma", "compression_method is wrong when reading old header with new program" );
    }
  }

  // Write new header, read as old header
  //NOTE In the real code, this will only work, if the file uses LZMA. If it doesn't, the version
  //     field is increased and the old code will refuse to read the file.
  {
    {
      EncryptedFile::OutputStream os( fileName.c_str(), noKey, Encryption::ZeroIv );

      BundleFileHeader header;
      header.set_version( 42 );
      Message::serialize( header, os );
    }

    {
      EncryptedFile::InputStream is( fileName.c_str(), noKey, Encryption::ZeroIv );

      FileHeader header;
      Message::parse( header, is );

      CHECK( header.version() == 42,     "version is wrong when reading new header with old program" );
      // cannot check compression_method because the field doesn't exist
    }
  }

  printf("compatibility test successful.\n");
}

void readAndWrite( EncryptionKey const & key,
  const_sptr<CompressionMethod> compression1, const_sptr<CompressionMethod> compression2 )
{
  // temporary file for the bundle
  TmpMgr tmpMgr( "/dev/shm" );
  sptr< TemporaryFile > tempFile = tmpMgr.makeTemporaryFile();

  // some chunk data
  int     chunkCount = rand() % 30;
  size_t  chunkSize  = rand() % 20 ? 64*1024 : 10;
  char**  chunks      = new char*[chunkCount];
  string* chunkIds    = new string[chunkCount];

  CompressionMethod::defaultCompression = compression1;

  // write bundle
  {
    Bundle::Creator bundle;

    for (int i=0;i<chunkCount;i++) {
      chunks[i] = new char[chunkSize];
      Random::genaratePseudo( chunks[i], chunkSize );

      //TODO make it look like a real Id (or even let it match the data)
      //TODO make sure we don't have any duplicate Ids
      sprintf(tmpbuf, "0x%08x", rand());
      chunkIds[i] = string(tmpbuf);

      bundle.addChunk( chunkIds[i], chunks[i], chunkSize );
    }

    bundle.write( tempFile->getFileName().c_str(), key );
  }

  CompressionMethod::defaultCompression = compression2;

  // read it and compare
  {
    Bundle::Reader bundle( tempFile->getFileName().c_str(), key );

    for (int i=0;i<chunkCount;i++) {
      string data;
      size_t size;
      bool ret = bundle.get( chunkIds[i], data, size );
      CHECK( ret, "bundle.get returned false for chunk %d (%s)", i, chunkIds[i].c_str() );
      CHECK( size == chunkSize, "wrong chunk size for chunk %d (%s)", i, chunkIds[i].c_str() );
      CHECK( memcmp(data.c_str(), chunks[i], chunkSize) == 0, "wrong chunk data for chunk %d (%s)", i, chunkIds[i].c_str() );
    }
  }

  // clean up
  for (int i=0;i<chunkCount;i++)
    delete[] chunks[i];
  delete[] chunks;
  //TODO does that call the destructors?
  delete[] chunkIds;

  printf(".");
  fflush(stdout);
}

int main()
{
  EncryptionKeyInfo keyInfo;
  EncryptionKey::generate( "blah", keyInfo );
  EncryptionKey key( "blah", &keyInfo );
  EncryptionKey noKey( std::string(), NULL );

  testCompatibility();

  std::vector< const_sptr<CompressionMethod> > compressions;
  for ( CompressionMethod::iterator it = CompressionMethod::begin(); it!=CompressionMethod::end(); ++it ) {
    printf( "supported compression: %s\n", (*it)->getName().c_str() );
    compressions.push_back( *it );
  }

  for ( size_t iteration = 100; iteration--; ) {
    // default compression while writing the file
    const_sptr<CompressionMethod> compression1 = compressions[ rand() % compressions.size() ];
    // default compression while reading the file
    // The reader should ignore it and always use the compression that was used for the file.
    const_sptr<CompressionMethod> compression2 = compressions[ rand() % compressions.size() ];

    readAndWrite( ( rand() & 1 ) ? key : noKey, compression1, compression2 );
  }

  printf("\n");

  return 0;
}
