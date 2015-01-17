// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED__
#define CONFIG_HH_INCLUDED__

#include <string>
#include <google/protobuf/text_format.h>
#include "zbackup.pb.h"
#include "mt.hh"

using std::string;

class Config
{
public:
  struct RuntimeConfig
  {
    size_t threads;
    size_t cacheSize;

    // Default runtime config
    RuntimeConfig():
      threads( getNumberOfCpus() ),
      cacheSize( 40 * 1024 * 1024 ) // 40 MB
    {
    }
  };

  RuntimeConfig runtime;

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

    oDeprecated, oUnsupported
  } OpCodes;

  // Validator for user-supplied configuration
  static bool validate( const string &, const string & );

  static bool parse( const string & str, google::protobuf::Message * mutable_message );

  static void showHelp( const OptionType );

  OpCodes parseToken( const char * option, const OptionType );
  bool parseOption( const char * option, const OptionType );

  string toString( google::protobuf::Message const & message );
private:
  ConfigInfo storableConfig;
};

#endif
