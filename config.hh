// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED
#define CONFIG_HH_INCLUDED

#include <string>
#include <bitset>
#include <google/protobuf/text_format.h>
#include "zbackup.pb.h"
#include "mt.hh"
#include "backup_exchanger.hh"

// TODO: make *_storable to be variadic
#define SET_STORABLE( storage, property, value ) \
  storable->mutable_##storage()->set_##property( value )

#define GET_STORABLE( storage, property ) \
  storable->storage().property()

using std::string;
using std::bitset;

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
    oLZMA_compression_level,

    oRuntime_threads,
    oRuntime_cacheSize,
    oRuntime_exchange,

    oDeprecated, oUnsupported
  } OpCodes;

  // Validator for user-supplied storable configuration
  static bool validateProto( const string &, const string & );

  static bool parseProto( const string &, google::protobuf::Message * );

  static string toString( google::protobuf::Message const & );

  // Print configuration to screen
  static void show( const ConfigInfo & );
  void show();

  void showHelp( const OptionType );

  OpCodes parseToken( const char *, const OptionType );
  bool parseOrValidate( const string &, const OptionType, bool validate = false );

  Config( const Config & );
  Config( const Config &, ConfigInfo * );
  Config( ConfigInfo * );
  Config();
  ~Config();

  void reset_storable();

  RuntimeConfig runtime;
  ConfigInfo * storable;
private:
  struct Keyword
  {
    string name;
    Config::OpCodes opcode;
    Config::OptionType type;
    string description;
    string defaultValue;
  };

  Keyword * keywords;

  bool cleanup_storable;
  bool cleanup_keywords;

  void prefillKeywords();
};

#endif
