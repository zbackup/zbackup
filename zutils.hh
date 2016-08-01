// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ZUTILS_HH_INCLUDED
#define ZUTILS_HH_INCLUDED

#include "chunk_storage.hh"
#include "zbackup_base.hh"

class ZBackup: public ZBackupBase
{
  ChunkStorage::Writer chunkStorageWriter;

public:
  ZBackup( string const & storageDir, string const & password,
           Config & configIn );

  /// Backs up the data from stdin
  void backupFromStdin( string const & outputFileName );

  /// Backs up the data from a file
  void backupFromFile( string const & inputFileName,
      string const & outputFileName,
      bool checkFileSize = false );

  /// Backs up the data from a directory
  void backupFromDirectory( string const & inputDirectoryName,
      string const & outputDirectoryName );

  /// Backs up the data from a stdio FILE handle
  void backupFromFileHandle( string const & inputName, FILE* inputFileHandle,
      string const & outputFileName );
};

class ZRestore: public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZRestore( string const & storageDir, string const & password,
            Config & configIn );

  /// Restores the data to file
  void restoreToFile( string const & inputFileName, string const & outputFileName );

  /// Restores the data to stdout
  void restoreToStdin( string const & inputFileName );
  
  /// Restores only parts of the data to stdout
  void restorePartialToStdout( string const & inputFileName );

  /// Starts NBD server that serves backup data as block device with random access
  void startNBDServer( string const & inputFileName, string const & nbdDevice );
};

class ZExchange
{
  ZBackupBase srcZBackupBase;
  ZBackupBase dstZBackupBase;

public:
  ZExchange( string const & srcStorageDir, string const & srcPassword,
             string const & dstStorageDir, string const & dstPassword,
             Config & configIn );

  Config config;

  /// Exchanges the data between storages
  void exchange();
};

class ZCollector : public ZBackupBase
{
  ChunkStorage::Reader chunkStorageReader;

public:
  ZCollector( std::string const & storageDir, std::string const & password,
              Config & configIn );

  void gc( bool );
};

class ZInspect : public ZBackupBase
{
public:
  ZInspect( std::string const & storageDir, std::string const & password,
              Config & configIn );
  ZInspect( std::string const & storageDir, std::string const & password,
              Config & configIn, bool deep );

  void inspect( string const & inputFileName );
};

#endif
