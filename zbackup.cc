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
"ZBackup contributors\n"
"Comes with no warranty. Licensed under GNU GPLv2 or later + OpenSSL.\n"
"Visit the project's home page at http://zbackup.org/\n\n"

"Usage: %s [flags] <command [action]> [command args]\n"
"  Flags: --non-encrypted|--password-file <file>\n"
"          password flag should be specified twice if\n"
"          import/export/passwd command specified\n"
"         --silent (default is verbose)\n"
"         --help|-h show this message\n"
"         -O <Option[=Value]> (overrides runtime configuration,\n"
"          can be specified multiple times,\n"
"          for detailed runtime options overview run with -O help)\n"
"         -o <Option[=Value]> (overrides storable repository\n"
"          configuration, can be specified multiple times,\n"
"          for detailed storable options overview run with -o help)\n"
"  Commands:\n"
"    init <storage path> - initializes new storage\n"
"    backup <backup file name> - performs a backup from stdin\n"
"    restore <backup file name> - restores a backup to stdout\n"
"    export <source storage path> <destination storage path> -\n"
"            performs export from source to destination storage\n"
"    import <source storage path> <destination storage path> -\n"
"            performs import from source to destination storage,\n"
"            for export/import storage path must be\n"
"            a valid (initialized) storage\n"
"    gc [chunks|indexes] <storage path> - performs garbage\n"
"            collection (default is chunks)\n"
"    passwd <storage path> - changes repo info file passphrase\n"
//"    info <storage path> - shows repo information\n"
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
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s %s <backup file name>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }
      ZBackup zb( ZBackup::deriveStorageDirFromBackupsFile( args[ 1 ] ),
                  passwords[ 0 ], config );
      zb.backupFromStdin( args[ 1 ] );
    }
    else
    if ( strcmp( args[ 0 ], "restore" ) == 0 )
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
      zr.restoreToStdin( args[ 1 ] );
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

      int src, dst;
      if ( strcmp( args[ 0 ], "export" ) == 0 )
      {
        src = 1;
        dst = 2;
      }
      else
      if ( strcmp( args[ 0 ], "import" ) == 0 )
      {
        src = 2;
        dst = 1;
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
        fprintf( stderr, "Usage: %s %s [chunks|indexes] <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      int fieldStorage = 1;
      int fieldAction = 2;

      if ( args.size() == 3 )
      {
        fieldStorage = 2;
        fieldAction = 1;
      }

      if ( args.size() > 2 && strcmp( args[ fieldAction ], "chunks" ) == 0 )
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        zc.gcChunks();
      }
      else
      if ( args.size() > 2 && strcmp( args[ fieldAction ], "indexes" ) == 0 )
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        fprintf( stderr, "NOT IMPLEMENTED YET!\n" );
        zc.gcIndexes();
      }
      else
      {
        ZCollector zc( ZBackupBase::deriveStorageDirFromBackupsFile( args[ fieldStorage ], true ),
            passwords[ 0 ], config );
        zc.gcChunks();
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
    if ( strcmp( args[ 0 ], "info" ) == 0 )
    {
      // Show repo info
      if ( args.size() != 2 )
      {
        fprintf( stderr, "Usage: %s %s <storage path>\n",
                 *argv, args[ 0 ] );
        return EXIT_FAILURE;
      }

      // TODO: implementation in ZBackupBase
      ZBackupBase zbb( ZBackupBase::deriveStorageDirFromBackupsFile( args[ 1 ], true ),
          passwords[ 0 ], true );
      fprintf( stderr, "NOT IMPLEMENTED YET!\n" );
      return EXIT_FAILURE;
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

      int fieldStorage = 1;
      int fieldAction = 2;

      if ( args.size() == 3 )
      {
        fieldStorage = 2;
        fieldAction = 1;
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
