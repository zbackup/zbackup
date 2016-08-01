// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zutils.hh"
#include "debug.hh"
#include "version.hh"
#include "utils.hh"

DEF_EX( exSpecifyTwoKeys, "Specify password flag (--non-encrypted or --password-file)"
  " for import/export/passwd operation twice (first for source and second for destination)", std::exception )
DEF_EX( exNonEncryptedWithKey, "--non-encrypted and --password-file are incompatible", std::exception )
DEF_EX( exSpecifyEncryptionOptions, "Specify either --password-file or --non-encrypted", std::exception )
DEF_EX( exSourceInaccessible, "Backup source file/directory is inaccessible", std::exception )

int main( int argc, char *argv[] )
{
  try
  {
    dPrintf( "ZBackup version %s\n", zbackup_version.c_str() );

    bool printHelp = false;
    vector< char const * > args;
    vector< string > passwords;
    Config config;

    for( int x = 1; x < argc; ++x )
    {
      string option;
      Config::OptionType optionType = Config::Runtime;

      if ( strcmp( argv[ x ], "--password-file" ) == 0 && x + 1 < argc )
      {
        // Read the password
        char const * passwordFile = argv[ x + 1 ];
        string passwordData;
        if ( passwordFile )
        {
          File f( passwordFile, File::ReadOnly );
          passwordData.resize( f.size() );
          f.read( &passwordData[ 0 ], passwordData.size() );

          // If the password ends with \n, remove that last \n. Many editors will
          // add \n there even if a user doesn't want them to
          if ( !passwordData.empty() &&
               passwordData[ passwordData.size() - 1 ] == '\n' )
            passwordData.resize( passwordData.size() - 1 );
          passwords.push_back( passwordData );
        }
        ++x;
      }
      else
      if ( strcmp( argv[ x ], "--non-encrypted" ) == 0 )
      {
          passwords.push_back( "" );
      }
      else
      if ( strcmp( argv[ x ], "--silent" ) == 0 )
        verboseMode = false;
      else
      if ( strcmp( argv[ x ], "--exchange" ) == 0 && x + 1 < argc )
      {
        fprintf( stderr, "%s is deprecated, use -O exchange instead\n", argv[ x ] );
        option = argv[ x ] + 2;//; + "=" + argv[ x + 1 ];
        option += "=";
        option += argv[ x + 1 ];
        goto parse_option;
      }
      else
      if ( strcmp( argv[ x ], "--threads" ) == 0 && x + 1 < argc )
      {
        fprintf( stderr, "%s is deprecated, use -O threads instead\n", argv[ x ] );
        option = argv[ x ] + 2;
        option += "=";
        option += argv[ x + 1 ];
        goto parse_option;
      }
      else
      if ( strcmp( argv[ x ], "--cache-size" ) == 0 && x + 1 < argc )
      {
        fprintf( stderr, "%s is deprecated, use -O cache-size instead\n", argv[ x ] );
        size_t cacheSizeMb;
        char suffix[ 16 ];
        int n;
        if ( sscanf( argv[ x + 1 ], "%zu %15s %n",
              &cacheSizeMb, suffix, &n ) == 2 && !argv[ x + 1][ n ] )
        {
          option = argv[ x ] + 2;
          option += "=" + Utils::numberToString( cacheSizeMb ) + "MiB";
          goto parse_option;
        }
      }
      else
      if ( strcmp( argv[ x ], "--compression" ) == 0 && x + 1 < argc )
      {
        fprintf( stderr, "%s is deprecated, use -o bundle.compression_method instead\n", argv[ x ] );
        option = argv[ x ] + 2;
        option += "=";
        option += argv[ x + 1 ];
        optionType = Config::Storable;
        goto parse_option;
      }
      else
      if ( strcmp( argv[ x ], "--help" ) == 0 || strcmp( argv[ x ], "-h" ) == 0 )
      {
        printHelp = true;
      }
      else
      if ( ( strcmp( argv[ x ], "-o" ) == 0 || strcmp( argv[ x ], "-O" ) == 0 )
          && x + 1 < argc )
      {
        option = argv[ x + 1 ];
        if ( !option.empty() )
        {
          if ( strcmp( argv[ x ], "-O" ) == 0 )
            optionType = Config::Runtime;
          else
          if ( strcmp( argv[ x ], "-o" ) == 0 )
            optionType = Config::Storable;

          if ( strcmp( option.c_str(), "help" ) == 0 )
          {
            config.showHelp( optionType );
            return EXIT_SUCCESS;
          }
          else
          {
parse_option:
            if ( !config.parseOrValidate( option, optionType ) )
              goto invalid_option;
          }
        }
        else
        {
invalid_option:
          fprintf( stderr, "Invalid option specified: %s\n",
                   option.c_str() );
          return EXIT_FAILURE;
        }
        ++x;
      }
      else
        args.push_back( argv[ x ] );
    }

    if ( args.size() < 1 || printHelp )
    {
      fprintf( stderr,
"ZBackup, a versatile deduplicating backup tool, version %s\n"
"Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and\n"
"ZBackup contributors.\n"
"Comes with no warranty. Licensed under GNU GPLv2 or later + OpenSSL.\n"
"Visit the project's home page at http://zbackup.org/\n\n"

"Usage: %s [flags] <command [action]> [command args]\n"
"\n"
"  Flags: --non-encrypted|--password-file <file>\n"
"          password flag should be specified twice if\n"
"          import/export/passwd command specified\n"
"         --silent (default is verbose)\n"
"         --help|-h show this message\n"
"         -O <option[=value]> (overrides runtime configuration,\n"
"          can be specified multiple times,\n"
"          for detailed runtime options overview run with -O help)\n"
"         -o <option[=value]> (overrides storable repository\n"
"          configuration, can be specified multiple times,\n"
"          for detailed storable options overview run with -o help)\n"
"\n"
"  Commands:\n"
"    init <storage path> - initializes new storage\n"
"    backup <backup file name> - performs a backup from stdin\n"
"    backup <input file> <backup file name> - performs a backup from file\n"
"    backup <input dir> <backup dir> - performs a backup from directory\n"
"    restore <backup file name> - restores a backup to stdout\n"
"    restore <backup file name> <output file name> - restores\n"
"            a backup to file using two-pass \"cacheless\" process\n"
"    restore-partial <backup file name> - restores a backup partial to stdout\n"
"    nbd <backup file name> /dev/nbd0\n"
"            start NBD server that will serve backup data as block device\n"
"    export <source storage path> <destination storage path> -\n"
"            performs export from source to destination storage\n"
"    import <source storage path> <destination storage path> -\n"
"            performs import from source to destination storage,\n"
"            for export/import storage path must point to\n"
"            a valid (initialized) storage\n"
"    inspect [fast|deep] <backup file name> - inspect backup (default\n"
"            is fast)\n"
"    gc [fast|deep] <storage path> - performs garbage\n"
"            collection (default is fast)\n"
"    passwd <storage path> - changes repo info file passphrase\n"
"    config [show|edit|set|reset] <storage path> - performs\n"
"            configuration manipulations (default is show)\n"
"", zbackup_version.c_str(), *argv );
      return EXIT_FAILURE;
    }

    if ( passwords.size() > 1 &&
        ( ( passwords[ 0 ].empty() && !passwords[ 1 ].empty() ) ||
          ( !passwords[ 0 ].empty() && passwords[ 1 ].empty() ) ) &&
        ( strcmp( args[ 0 ], "export" ) != 0 &&
	  strcmp( args[ 0 ], "import" ) != 0 &&
	  strcmp( args[ 0 ], "passwd" )	) )
      throw exNonEncryptedWithKey();
    else
      if ( passwords.size() != 2 &&
          ( strcmp( args[ 0 ], "export" ) == 0 ||
	    strcmp( args[ 0 ], "import" ) == 0 ||
	    strcmp( args[ 0 ], "passwd" ) == 0 ) )
        throw exSpecifyTwoKeys();
    else
      if ( passwords.size() < 1 )
        throw exSpecifyEncryptionOptions();

    if ( strcmp( args[ 0 ], "init" ) == 0 )
    {
      // Perform the init
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s %s <storage path>\n", *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      ZBackup::initStorage( args[ 1 ],
          passwords[ 0 ], !passwords[ 0 ].empty(), config );
    }
    else
    if ( strcmp( args[ 0 ], "backup" ) == 0 )
    {
      // Perform the backup
      if ( args.size() < 2 || args.size() > 3)
      {
        fprintf( stderr, "Usage 1: your_backup_command | %s %s <backup file name>\n"
                         "Usage 2: %s %s <file> <backup file name>\n"
                         "Usage 3: %s %s <directory> <backup subdirectory>\n",
                 *argv, args[ 0 ], *argv, args[ 0 ], *argv, args[ 0 ]);
        return EXIT_FAILURE;
      }

      string backupSource, backupsDest;
      bool dirBackupMode = false;
      if ( args.size() == 2 )
        backupsDest = args[ 1 ];
      else
      {
        backupSource = args[ 1 ];
        if ( Dir::exists( backupSource ) )
        {
          dirBackupMode = true;
          backupsDest = Dir::addPath( args[ 2 ], Dir::getBaseName( Dir::getRealPath( backupSource ) ));
        }
        else
          backupsDest = args[ 2 ];
      }

      ZBackup zb( ZBackup::deriveStorageDirFromBackupsFile( backupsDest ),
                  passwords[ 0 ], config );
      if ( args.size() == 2 )
        zb.backupFromStdin( backupsDest );
      else
      {
        if ( dirBackupMode )
          zb.backupFromDirectory( args[ 1 ], backupsDest );
        else
          zb.backupFromFile( args[ 1 ], backupsDest );
      }
    }
    else
    if ( strcmp( args[ 0 ], "restore" ) == 0 )
    {
      // Perform the restore
      if ( args.size() != 2 && args.size() != 3 )
      {
        fprintf( stderr, "Usage: %s %s <backup file name> [output file name]\n",
                 *argv , args[ 0 ] );
        return EXIT_FAILURE;
      }
      ZRestore zr( ZRestore::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                   passwords[ 0 ], config );
      if ( args.size() == 3 )
        zr.restoreToFile( args[ 1 ], args[ 2 ] );
      else
        zr.restoreToStdin( args[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "restore-partial" ) == 0 )
    {
      // Perform the restore
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s %s <backup file name>\n",
                 *argv , args[ 0 ] );
        return EXIT_FAILURE;
      }
      ZRestore zr( ZRestore::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                   passwords[ 0 ], config );
      zr.restorePartialToStdout( args[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "nbd" ) == 0 )
    {
      // Start NBD server
      if ( args.size() != 3 )
      {
        fprintf( stderr, "Usage: %s %s <backup file name> /dev/nbd0\n",
                 *argv , args[ 0 ] );
        return EXIT_FAILURE;
      }
      ZRestore zr( ZRestore::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                   passwords[ 0 ], config );
      zr.startNBDServer( args[ 1 ], args[ 2 ] );
    }
    else
    if ( strcmp( args[ 0 ], "export" ) == 0 || strcmp( args[ 0 ], "import" ) == 0 )
    {
      if ( args.size() != 3 )
      {
        fprintf( stderr, "Usage: %s %s <source storage path> <destination storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }
      if ( config.runtime.exchange.none() )
      {
        fprintf( stderr, "Specify any --exchange flag\n" );
        return EXIT_FAILURE;
      }

      int src = 1, dst = 2;
      if ( strcmp( args[ 0 ], "export" ) == 0 )
      {
        src = 1, dst = 2;
      }
      else
      if ( strcmp( args[ 0 ], "import" ) == 0 )
      {
        src = 2, dst = 1;
      }
      dPrintf( "%s src: %s\n", args[ 0 ], args[ src ] );
      dPrintf( "%s dst: %s\n", args[ 0 ], args[ dst ] );

      ZExchange ze( ZBackupBase::deriveStorageDirFromBackupsFile( args[ src ], true ),
                    passwords[ src - 1 ],
                    ZBackupBase::deriveStorageDirFromBackupsFile( args[ dst ], true ),
                    passwords[ dst - 1 ],
                    config );
      ze.exchange();
    }
    else
    if ( strcmp( args[ 0 ], "gc" ) == 0 )
    {
      // Perform the garbage collection
      if ( args.size() < 2 || args.size() > 3 )
      {
        fprintf( stderr, "Usage: %s %s [fast|deep] <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      int fieldStorage = 1, fieldAction = 2;

      if ( args.size() == 3 )
      {
        fieldStorage = 2, fieldAction = 1;
      }

      if ( args.size() > 2 && strcmp( args[ fieldAction ], "fast" ) == 0 )
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        zc.gc( false );
      }
      else
      if ( args.size() > 2 && strcmp( args[ fieldAction ], "deep" ) == 0 )
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        zc.gc( true );
      }
      else
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        zc.gc( false );
      }
    }
    else
    if ( strcmp( args[ 0 ], "passwd" ) == 0 )
    {
      // Perform the password change
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s %s <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ 1 ], true ),
          passwords[ 0 ], true );

      if ( passwords[ 0 ].empty() != passwords[ 1 ].empty() )
      {
        fprintf( stderr,
"Changing repo encryption type (non-encrypted to encrypted and vice versa) is possible "
"only via import/export operations.\n"
"Current repo type: %s.\n", zbb.encryptionkey.hasKey() ? "encrypted" : "non-encrypted" );
        return EXIT_FAILURE;
      }
      zbb.setPassword( passwords[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "inspect" ) == 0 )
    {
      if ( args.size() < 2 || args.size() > 3 )
      {
        fprintf( stderr, "Usage: %s %s [full] <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      int fieldStorage = 1, fieldAction = 2;

      if ( args.size() == 3 )
      {
        fieldStorage = 2, fieldAction = 1;
      }

      if ( args.size() > 2 && strcmp( args[ fieldAction ], "deep" ) == 0 )
      {
        ZInspect zi( ZRestore::deriveStorageDirFromBackupsFile( args[ fieldStorage ] ),
                     passwords[ 0 ], config, true );
        zi.inspect( args[ fieldStorage ] );
      }
      else
      {
        ZInspect zi( ZRestore::deriveStorageDirFromBackupsFile( args[ fieldStorage ] ),
                     passwords[ 0 ], config, false );
        zi.inspect( args[ fieldStorage ] );
      }
    }
    else
    if ( strcmp( args[ 0 ], "config" ) == 0 )
    {
      if ( args.size() < 2 || args.size() > 3 )
      {
        fprintf( stderr, "Usage: %s %s [show|edit|set|reset] <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      int fieldStorage = 1, fieldAction = 2;

      if ( args.size() == 3 )
      {
        fieldStorage = 2, fieldAction = 1;
      }

      if ( args.size() > 2 && strcmp( args[ fieldAction ], "edit" ) == 0 )
      {
        ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], true );
        if ( zbb.editConfigInteractively() )
          zbb.saveExtendedStorageInfo();
      }
      else
      if ( args.size() > 2 && strcmp( args[ fieldAction ], "set" ) == 0 )
      {
        ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config, true );
        zbb.config.show();
        zbb.saveExtendedStorageInfo();
      }
      else
      if ( args.size() > 2 && strcmp( args[ fieldAction ], "reset" ) == 0 )
      {
        ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], true );
        zbb.config.reset_storable();
        zbb.config.show();
        zbb.saveExtendedStorageInfo();
      }
      else
      {
        ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], true );
        zbb.config.show();
      }
    }
    else
    {
      fprintf( stderr, "Error: unknown command line option: %s\n", args[ 0 ] );
      return EXIT_FAILURE;
    }
  }
  catch( std::exception & e )
  {
    fprintf( stderr, "%s\n", e.what() );
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    fprintf( stderr, "Unknown exception!\n" );
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
