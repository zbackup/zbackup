// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdlib.h>
#include "../../encrypted_file.hh"
#include "../../encryption_key.hh"
#include "../../random.hh"
#include "../../tmp_mgr.hh"
#include "../../check.hh"
#include "../../adler32.hh"

char rnd[ 16384 ];

Adler32::Value adler( int sz )
{
  Adler32 a;
  a.add( rnd, sz );
  return a.result();
}

void readAndWrite( EncryptionKey const & key, bool writeBackups,
                   bool readBackups, bool readSkips )
{
  TmpMgr tmpMgr( "/dev/shm" );

  sptr< TemporaryFile > tempFile = tmpMgr.makeTemporaryFile();

  int fileSize = rand() % ( sizeof( rnd ) + 1 );

  fprintf( stderr, "Run with %d bytes, %s%s%s%sfile %s...\n", fileSize,
           key.hasKey() ? "" : "no encryption, ",
           writeBackups ? "write backups, " : "",
           readBackups ? "read backups, " : "",
           readSkips ? "read skips, " : "",
           tempFile->getFileName().c_str() );

  char iv[ Encryption::IvSize ];

  Random::genaratePseudo( iv, sizeof( iv ) );

  // Write
  {
    EncryptedFile::OutputStream out( tempFile->getFileName().c_str(), key, iv );

    char const * next = rnd;

    int avail = 0;
    for ( int left = fileSize; left; )
    {
      CHECK( out.ByteCount() == fileSize - left, "Incorrect bytecount in the "
             "middle of writing" );
      void * data;
      CHECK( out.Next( &data, &avail ), "out.Next() returned false" );
      CHECK( avail > 0, "out.Next() returned zero size" );

      bool doBackup = writeBackups && ( rand() & 1 );
      int backup;
      if ( doBackup )
      {
        backup = rand() % ( avail + 1 );
        // Make sure we don't back up and then need to back up again to finish
        // the write
        if ( avail > left )
          backup = avail - left;
        avail -= backup;
      }

      int toWrite = avail > left ? left : avail;
      memcpy( data, next, toWrite );

      if ( doBackup )
        out.BackUp( backup );

      next += toWrite;
      left -= toWrite;
      avail -= toWrite;

      if ( !avail && ( rand() & 1 ) )
      {
        CHECK( adler( next - rnd ) == out.getAdler32(),
               "bad adler32 in the middle of writing" );
      }
    }

    if ( avail || ( rand() & 1 ) )
      out.BackUp( avail );

    CHECK( out.ByteCount() == fileSize, "Incorrect bytecount after writing" );

    if ( rand() & 1 )
    {
      CHECK( adler( fileSize ) == out.getAdler32(),
             "bad adler32 of the written file" );
    }
  }

  // Read back
  {
    EncryptedFile::InputStream in( tempFile->getFileName().c_str(), key, iv );

    char const * next = rnd;

    void const * data;
    int avail = 0;
    for ( int left = fileSize; left; )
    {
      if ( readSkips && ( rand() & 1 ) )
      {
        int toSkip = rand() % ( left + 1 );
        in.Skip( toSkip );
        next += toSkip;
        left -= toSkip;
        avail = 0;
        continue;
      }

      CHECK( in.ByteCount() == fileSize - left, "Incorrect bytecount in the "
             "middle of reading" );
      CHECK( in.Next( &data, &avail ), "file ended while %d were still left",
             left );
      CHECK( avail > 0, "in.Next() returned zero size" );

      bool doBackup = readBackups && ( rand() & 1 );
      int backup;
      if ( doBackup )
      {
        backup = rand() % ( avail + 1 );
        avail -= backup;
      }

      int toRead = avail > left ? left : avail;

      CHECK( memcmp( next, data, toRead ) == 0, "Different bytes read than "
             "expected at offset %d", int( next - rnd ) );

      if ( doBackup )
        in.BackUp( backup );

      next += toRead;
      left -= toRead;
      avail -= toRead;

      if ( !avail && ( rand() & 1 ) )
      {
        CHECK( adler( next - rnd ) == in.getAdler32(),
               "bad adler32 in the middle of the reading" );
      }
    }

    CHECK( in.ByteCount() == fileSize, "Incorrect bytecount after reading" );

    CHECK( !avail, "at least %d bytes still available", avail );
    CHECK( !in.Next( &data, &avail ), "file should have ended but resulted in "
           "%d more bytes", avail );
    if ( rand() & 1 )
    {
      CHECK( adler( fileSize ) == in.getAdler32(),
             "bad adler32 of the read file" );
    }
  }
}

int main()
{
  Random::genaratePseudo( rnd, sizeof( rnd ) );
  EncryptionKeyInfo keyInfo;
  EncryptionKey::generate( "blah", keyInfo );
  EncryptionKey key( "blah", &keyInfo );
  EncryptionKey noKey( std::string(), NULL );

  for ( size_t iteration = 100000; iteration--; )
    readAndWrite( ( rand() & 1 ) ? key : noKey, rand() & 1, rand() & 1,
                  rand() & 1 );
}
