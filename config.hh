// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED__
#define CONFIG_HH_INCLUDED__

#include <string>
#include <bitset>
#include <google/protobuf/text_format.h>
#include "zbackup.pb.h"
#include "mt.hh"
#include "backup_exchanger.hh"

// TODO: make *_storable to be variadic
#define SET_STORABLE( storage, property, value ) (\
{\
  dPrintf( "storable->mutable_"#storage"()->set_"#property"( "#value" )\n" ); \
  storable->mutable_##storage()->set_##property( value ); \
})

#define GET_STORABLE( storage, property ) storable->storage().property()

using std::string;
using std::bitset;

class ZBackupBase;

class Config
{
public:
  struct RuntimeConfig
  {
    size_t threads;
    size_t cacheSize;
    bitset< BackupExchanger::Flags > exchange;

    // Default runtime config
    RuntimeConfig():
      threads( getNumberOfCpus() ),
      cacheSize( 40 * 1024 * 1024 ) // 40 MB
    {
    }
  };

  enum OptionType
  {
    Runtime,
    Storable,
    None
  };

  /* Keyword tokens. */
  typedef enum
  {
    oBadOption,

    oChunk_max_size,
    oBundle_max_payload_size,
    oBundle_compression_method,

    oRuntime_threads,
    oRuntime_cacheSize,
    oRuntime_exchange,

    oDeprecated, oUnsupported
  } OpCodes;

  // Validator for user-supplied storable configuration
  static bool validateProto( const string &, const string & );

  static bool parseProto( const string &, google::protobuf::Message * );

  static void showHelp( const OptionType );

  static string toString( google::protobuf::Message const & );

  // Edit current configuration
  // returns true if configuration is changed
  static bool editInteractively( ZBackupBase * );

  // Print configuration to screen
  static void show( const ConfigInfo & );
  void show();

  OpCodes parseToken( const char *, const OptionType );
  bool parseOrValidate( const char *, const OptionType, bool validate = false );

  Config( const Config &, ConfigInfo * );
  Config( ConfigInfo * );
  Config();
  ~Config();

  void reset_storable();

  RuntimeConfig runtime;
  ConfigInfo * storable;
private:
  bool want_cleanup;
};

#include "zbackup_base.hh"

#endif
