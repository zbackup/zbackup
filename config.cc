// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <sstream>
#include "config.hh"
#include "ex.hh"
#include "debug.hh"
#include "utils.hh"

#define VALID_SUFFIXES "Valid suffixes:\n" \
                       "B - multiply by 1 (bytes)\n" \
                       "KiB - multiply by 1024 (kibibytes)\n" \
                       "MiB - multiply by 1024*1024 (mebibytes)\n" \
                       "GiB - multiply by 1024*1024*1024 (gibibytes)\n" \
                       "KB - multiply by 1000 (kilobytes)\n" \
                       "MB - multiply by 1000*1000 (megabytes)\n" \
                       "GB - multiply by 1000*1000*1000 (gigabytes)\n" \

DEF_EX_STR( exInvalidThreadsValue, "Invalid threads value specified:", std::exception )

namespace ConfigHelper {
Config defaultConfig;

/* Textual representations of the tokens. */

static struct
{
  const char * name;
  const Config::OpCodes opcode;
  const Config::OptionType type;
  const char * description;
  const string defaultValue;
} keywords[] = {
  // Storable options
  {
    "chunk.max_size",
    Config::oChunk_max_size,
    Config::Storable,
    "Maximum chunk size used when storing chunks\n"
    "Affects deduplication ratio directly"
  },
  {
    "bundle.max_payload_size",
    Config::oBundle_max_payload_size,
    Config::Storable,
    "Maximum number of bytes a bundle can hold. Only real chunk bytes are\n"
    "counted, not metadata. Any bundle should be able to contain at least\n"
    "one arbitrary single chunk, so this should not be smaller than\n"
    "chunk.max_size" },
  {
    "bundle.compression_method",
    Config::oBundle_compression_method,
    Config::Storable,
    "Compression method for new bundles"
  },

  // Shortcuts for storable options
  {
    "compression",
    Config::oBundle_compression_method,
    Config::Storable,
    "Shortcut for bundle.compression_method"
  },

  // Runtime options
  {
    "threads",
    Config::oRuntime_threads,
    Config::Runtime,
    "Maximum number of compressor threads to use in backup process\n"
    "Default is %s on your system",
    Utils::numberToString( defaultConfig.runtime.threads )
  },
  {
    "cache-size",
    Config::oRuntime_cacheSize,
    Config::Runtime,
    "Cache size to use in restore process\n"
    "Affects restore process speed directly\n"
    VALID_SUFFIXES
    "Default is %sMiB",
    Utils::numberToString( defaultConfig.runtime.cacheSize / 1024 / 1024 )
  },

  { NULL, Config::oBadOption, Config::None }
};

}

Config::OpCodes Config::parseToken( const char * option, const OptionType type )
{
  for ( u_int i = 0; ConfigHelper::keywords[ i ].name; i++ )
  {
    if ( strcasecmp( option, ConfigHelper::keywords[ i ].name ) == 0 )
    {
      if ( ConfigHelper::keywords[ i ].type != type )
      {
        fprintf( stderr, "Invalid option type specified for %s\n", option );
        break;
      }

      return ConfigHelper::keywords[ i ].opcode;
    }
  }

  return Config::oBadOption;
}

bool Config::parseOption( const char * option, const OptionType type )
{
  string prefix;
  if ( type == Runtime )
    prefix.assign( "runtime" );
  else
  if ( type == Storable )
    prefix.assign( "storable" );
  dPrintf( "Parsing %s option \"%s\"...\n", prefix.c_str(), option );

  bool hasValue = false;
  size_t optionLength = strlen( option );
  char optionName[ optionLength ], optionValue[ optionLength ];

  if ( sscanf( option, "%[^=]=%s", optionName, optionValue ) == 2 )
  {
    dPrintf( "%s option name: %s, value: %s\n", prefix.c_str(),
        optionName, optionValue );
    hasValue = true;
  }
  else
    dPrintf( "%s option name: %s\n", prefix.c_str(), option );

  int opcode = parseToken( hasValue ? optionName : option, type );

  size_t sizeValue;
  char suffix[ 16 ];
  int n;
  unsigned int scale, scaleBase = 1;

  switch ( opcode )
  {
    case oRuntime_threads:
      if ( !hasValue )
        return false;

      sizeValue = runtime.threads;
      if ( sscanf( optionValue, "%zu %n", &sizeValue, &n ) != 1 ||
           optionValue[ n ] || sizeValue < 1 )
        throw exInvalidThreadsValue( optionValue );
      runtime.threads = sizeValue;

      dPrintf( "runtime[threads]: %zu\n", runtime.threads );

      return true;
      /* NOTREACHED */
      break;

    case oRuntime_cacheSize:
      if ( !hasValue )
        return false;

      sizeValue = runtime.cacheSize;
      if ( sscanf( optionValue, "%zu %15s %n",
                   &sizeValue, suffix, &n ) == 2 && !optionValue[ n ] )
      {
        // Check the suffix
        for ( char * c = suffix; *c; ++c )
          *c = tolower( *c );

        if ( strcmp( suffix, "b" ) == 0 )
        {
          scale = 1;
        }
        else
        if ( strcmp( suffix, "kib" ) == 0 )
        {
          scaleBase = 1024;
          scale = scaleBase;
        }
        else
        if ( strcmp( suffix, "mib" ) == 0 )
        {
          scaleBase = 1024;
          scale = scaleBase * scaleBase;
        }
        else
        if ( strcmp( suffix, "gib" ) == 0 )
        {
          scaleBase = 1024;
          scale = scaleBase * scaleBase * scaleBase;
        }
        else
        if ( strcmp( suffix, "kb" ) == 0 )
        {
          scaleBase = 1000;
          scale = scaleBase;
        }
        else
        if ( strcmp( suffix, "mb" ) == 0 )
        {
          scaleBase = 1000;
          scale = scaleBase * scaleBase;
        }
        else
        if ( strcmp( suffix, "gb" ) == 0 )
        {
          scaleBase = 1000;
          scale = scaleBase * scaleBase * scaleBase;
        }
        else
        {
          // SI or IEC
          fprintf( stderr, "Invalid suffix specified in cache size (%s): %s. "
                   VALID_SUFFIXES,
                   optionValue, suffix );
          return false;
        }
        runtime.cacheSize = sizeValue * scale;

        dPrintf( "runtime[cacheSize]: %zu\n", runtime.cacheSize );

        return true;
      }
      return false;
      /* NOTREACHED */
      break;

    case oBadOption:
    default:
      return false;
      /* NOTREACHED */
      break;
  }

  /* NOTREACHED */
  return false;
}

void Config::showHelp( const OptionType type )
{
  string prefix;
  if ( type == Runtime )
    prefix.assign( "runtime" );
  else
  if ( type == Storable )
    prefix.assign( "storable" );
  fprintf( stderr,
"Available %s options overview:\n\n"
"== help ==\n"
"shows this message\n"
"", prefix.c_str() );

  for ( u_int i = 0; ConfigHelper::keywords[ i ].name; i++ )
  {
    if ( ConfigHelper::keywords[ i ].type != type )
      continue;

    fprintf( stderr, "\n== %s ==\n", ConfigHelper::keywords[ i ].name );
    fprintf( stderr, ConfigHelper::keywords[ i ].description,
       ConfigHelper::keywords[ i ].defaultValue.c_str() );
    fprintf( stderr, "\n" );
  }
}

bool Config::parse( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}

string Config::toString( google::protobuf::Message const & message )
{
  std::string str;
  google::protobuf::TextFormat::PrintToString( message, &str );

  return str;
}

bool Config::validate( const string & configData, const string & newConfigData )
{
  ConfigInfo newConfig;
  return parse( newConfigData, &newConfig );
}
