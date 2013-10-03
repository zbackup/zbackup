// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <stdlib.h>
//TODO don't use printf and sprintf...
#include <stdio.h>
#include "../../encrypted_file.hh"
#include "../../encryption_key.hh"
#include "../../random.hh"
#include "../../tmp_mgr.hh"
#include "../../check.hh"
#include "../../adler32.hh"
#include "../../bundle.hh"

char tmpbuf[100];

void readAndWrite( EncryptionKey const & key )
{
  // temporary file for the bundle
  TmpMgr tmpMgr( "/dev/shm" );
  sptr< TemporaryFile > tempFile = tmpMgr.makeTemporaryFile();

  // some chunk data
  int     chunk_count = rand() % 30;
  size_t  chunk_size  = 64*1024;
  char**  chunks      = new char*[chunk_count];
  string* chunkIds    = new string[chunk_count];

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

  for ( size_t iteration = 100; iteration--; )
    readAndWrite( ( rand() & 1 ) ? key : noKey );

  printf("\n");
}
