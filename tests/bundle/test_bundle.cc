// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <stdlib.h>
//TODO don't use printf and sprintf...
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

char tmpbuf[100];

void readAndWrite( EncryptionKey const & key,
  const Compression* compression1, const Compression* compression2 )
{
  // temporary file for the bundle
  TmpMgr tmpMgr( "/dev/shm" );
  sptr< TemporaryFile > tempFile = tmpMgr.makeTemporaryFile();

  // some chunk data
  int     chunk_count = rand() % 30;
  size_t  chunk_size  = rand() % 20 ? 64*1024 : 10;
  char**  chunks      = new char*[chunk_count];
  string* chunkIds    = new string[chunk_count];

  Compression::default_compression = compression1;

  // write bundle
  {
    Bundle::Creator bundle;

    for (int i=0;i<chunk_count;i++) {
      chunks[i] = new char[chunk_size];
      Random::genaratePseudo( chunks[i], chunk_size );

      //TODO make it look like a real Id (or even let it match the data)
      //TODO make sure we don't have any duplicate Ids
      sprintf(tmpbuf, "0x%08x", rand());
      chunkIds[i] = string(tmpbuf);

      bundle.addChunk( chunkIds[i], chunks[i], chunk_size );
    }

    bundle.write( tempFile->getFileName().c_str(), key );
  }

  Compression::default_compression = compression2;

  // read it and compare
  {
    Bundle::Reader bundle( tempFile->getFileName().c_str(), key );

    for (int i=0;i<chunk_count;i++) {
      string data;
      size_t size;
      bool ret = bundle.get( chunkIds[i], data, size );
      CHECK( ret, "bundle.get returned false for chunk %d (%s)", i, chunkIds[i].c_str() );
      CHECK( size == chunk_size, "wrong chunk size for chunk %d (%s)", i, chunkIds[i].c_str() );
      CHECK( memcmp(data.c_str(), chunks[i], chunk_size) == 0, "wrong chunk data for chunk %d (%s)", i, chunkIds[i].c_str() );
    }
  }

  // clean up
  for (int i=0;i<chunk_count;i++)
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

  std::vector<const Compression*> compressions;
  for ( Compression::iterator it = Compression::begin(); it!=Compression::end(); ++it ) {
    printf( "supported compression: %s\n", (*it)->getName().c_str() );
    compressions.push_back( *it );
  }

  for ( size_t iteration = 100; iteration--; ) {
    // default compression while writing the file
    const Compression* compression1 = compressions[ rand() % compressions.size() ];
    // default compression while reading the file
    // The reader should ignore it and always use the compression that was used for the file.
    const Compression* compression2 = compressions[ rand() % compressions.size() ];

    readAndWrite( ( rand() & 1 ) ? key : noKey, compression1, compression2 );
  }

  printf("\n");

  return 0;
}
