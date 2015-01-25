// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "config.hh"
#include "ex.hh"
#include "debug.hh"
#include "utils.hh"
#include "compression.hh"

#define VALID_SUFFIXES "Valid suffixes:\n" \
                       "B - multiply by 1 (bytes)\n" \
                       "KiB - multiply by 1024 (kibibytes)\n" \
                       "MiB - multiply by 1024*1024 (mebibytes)\n" \
                       "GiB - multiply by 1024*1024*1024 (gibibytes)\n" \
                       "KB - multiply by 1000 (kilobytes)\n" \
                       "MB - multiply by 1000*1000 (megabytes)\n" \
                       "GB - multiply by 1000*1000*1000 (gigabytes)\n" \

#define SKIP_ON_VALIDATION \
{ \
  if ( validate ) \
    return true; \
}

// Some configurables could be just a switch
// So we introducing a macros that would indicate
// that this configurable is not a switch
#define REQUIRE_VALUE \
{ \
  if ( !hasValue && !validate ) \
    return false; \
}

#define PARSE_OR_VALIDATE( parse_src, validate_src ) \
  ( !validate && ( parse_src ) ) || ( validate && ( validate_src ) )

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
    //"Default is %s",
    //Utils::numberToString( defaultConfig.GET_STORABLE( chunk, max_size ) )
  },
  {
    "bundle.max_payload_size",
    Config::oBundle_max_payload_size,
    Config::Storable,
    "Maximum number of bytes a bundle can hold. Only real chunk bytes are\n"
    "counted, not metadata. Any bundle should be able to contain at least\n"
    "one arbitrary single chunk, so this should not be smaller than\n"
    "chunk.max_size"
    //"Default is %s",
    //Utils::numberToString( defaultConfig.GET_STORABLE( bundle, max_payload_size ) )
  },
  {
    "bundle.compression_method",
    Config::oBundle_compression_method,
    Config::Storable,
    "Compression method for new bundles"
    //"Default is %s",
    //defaultConfig.GET_STORABLE( bundle, compression_method )
  },
  {
    "lzma.compression_level",
    Config::oLZMA_compression_level,
    Config::Storable,
    "Compression level for new LZMA-compressed files\n"
    "Valid values: 0-19 (values over 9 enables extreme mode)"
    //"Default is %s",
    //Utils::numberToString( defaultConfig.GET_STORABLE( lzma, compression_level ) )
  },

  // Shortcuts for storable options
  {
    "compression",
    Config::oBundle_compression_method,
    Config::Storable,
    "Shortcut for bundle.compression_method"
    //"Default is %s",
    //defaultConfig.GET_STORABLE( bundle, compression_method )
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
  {
    "exchange",
    Config::oRuntime_exchange,
    Config::Runtime,
    "Data to exchange between repositories in import/export process\n"
    "Can be specified multiple times\n"
    "Valid values:\n"
    "backups - exchange backup instructions (files in backups/ directory)\n"
    "bundles - exchange bundles with data (files in bunles/ directory)\n"
    "index - exchange indicies of chunks (files in index/ directory)\n"
    "No default value, you should specify it explicitly"
  },

  { NULL, Config::oBadOption, Config::None }
};

}

Config::~Config()
{
  // prevent memleak
  if ( want_cleanup )
    delete storable;
}

Config::Config():
  want_cleanup( false )
{
  ConfigInfo * configInfo = new ConfigInfo;
  storable = configInfo;
  dPrintf( "%s is instantiated and initialized with default values\n",
      __CLASS );
}

Config::Config( ConfigInfo * configInfo ):
  want_cleanup( false )
{
  storable = configInfo;
  dPrintf( "%s is instantiated and initialized with supplied ConfigInfo\n",
      __CLASS );
}

Config::Config( const Config & configIn, ConfigInfo * configInfo )
{
  configInfo->MergeFrom( *configIn.storable );
  *this = configIn;
  storable = configInfo;
  want_cleanup = false;
  dPrintf( "%s is instantiated and initialized with supplied values\n",
      __CLASS );
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

bool Config::parseOrValidate( const char * option, const OptionType type,
   bool validate )
{
  string prefix;
  if ( type == Runtime )
    prefix.assign( "runtime" );
  else
  if ( type == Storable )
    prefix.assign( "storable" );

  dPrintf( "%s %s option \"%s\"...\n", ( validate ? "Validating" : "Parsing" ),
      prefix.c_str(), option );

  bool hasValue = false;
  size_t optionLength = strlen( option );
  char optionName[ optionLength ], optionValue[ optionLength ];

  if ( sscanf( option, "%[^=]=%s", optionName, optionValue ) == 2 )
  {
    dPrintf( "%s option %s: %s\n", prefix.c_str(), optionName, optionValue );
    hasValue = true;
  }
  else
    dPrintf( "%s option %s\n", prefix.c_str(), option );

  int opcode = parseToken( hasValue ? optionName : option, type );

  size_t sizeValue;
  char suffix[ 16 ];
  int n;
  unsigned int scale, scaleBase = 1;
  uint32_t uint32Value;

  switch ( opcode )
  {
    case oChunk_max_size:
      SKIP_ON_VALIDATION;
      REQUIRE_VALUE;

      if ( sscanf( optionValue, "%zu %n", &uint32Value, &n ) == 1
          && !optionValue[ n ] )
      {
        SET_STORABLE( chunk, max_size, uint32Value );
        dPrintf( "storable[chunk][max_size] = %zu\n",
            GET_STORABLE( chunk, max_size ) );

        return true;
      }

      return false;
      /* NOTREACHED */
      break;

    case oBundle_max_payload_size:
      SKIP_ON_VALIDATION;
      REQUIRE_VALUE;

      if ( sscanf( optionValue, "%zu %n", &uint32Value, &n ) == 1
          && !optionValue[ n ] )
      {
        SET_STORABLE( bundle, max_payload_size, uint32Value );
        dPrintf( "storable[bundle][max_payload_size] = %zu\n",
            GET_STORABLE( bundle, max_payload_size ) );

        return true;
      }

      return false;
      /* NOTREACHED */
      break;

    case oLZMA_compression_level:
      REQUIRE_VALUE;

      if ( PARSE_OR_VALIDATE(
            sscanf( optionValue, "%zu %n", &uint32Value, &n ) != 1 ||
            optionValue[ n ] || uint32Value > 19,
            GET_STORABLE( lzma, compression_level ) > 19 )
         )
        return false;

      SKIP_ON_VALIDATION;
      SET_STORABLE( lzma, compression_level, uint32Value );
      dPrintf( "storable[lzma][compression_level] = %zu\n",
          GET_STORABLE( lzma, compression_level ) );

      return true;
      /* NOTREACHED */
      break;

    case oBundle_compression_method:
      REQUIRE_VALUE;

      if ( PARSE_OR_VALIDATE( strcmp( optionValue, "lzma" ) == 0,
           GET_STORABLE( bundle, compression_method ) == "lzma" ) )
      {
        const_sptr< Compression::CompressionMethod > lzma =
          Compression::CompressionMethod::findCompression( "lzma" );
        if ( !lzma )
        {
          fprintf( stderr, "zbackup is compiled without LZMA support, but the code "
            "would support it. If you install liblzma (including development files) "
            "and recompile zbackup, you can use LZMA.\n" );
          return false;
        }
        Compression::CompressionMethod::selectedCompression = lzma;
      }
      else
      if ( PARSE_OR_VALIDATE(
            strcmp( optionValue, "lzo1x_1" ) == 0 || strcmp( optionValue, "lzo" ) == 0,
            GET_STORABLE( bundle, compression_method ) == "lzo1x_1" ) )
      {
        const_sptr< Compression::CompressionMethod > lzo =
          Compression::CompressionMethod::findCompression( "lzo1x_1" );
        if ( !lzo )
        {
          fprintf( stderr, "zbackup is compiled without LZO support, but the code "
            "would support it. If you install liblzo2 (including development files) "
            "and recompile zbackup, you can use LZO.\n" );
          return false;
        }
        Compression::CompressionMethod::selectedCompression = lzo;
      }
      else
      {
        fprintf( stderr,
            "ZBackup doesn't support %s compression.\n"
            "You probably need a newer version.\n", validate ?
            GET_STORABLE( bundle, compression_method ).c_str() : optionValue );
        fprintf( stderr, "Supported compression methods:\n" );
        for ( const const_sptr< Compression::CompressionMethod > * c =
            Compression::CompressionMethod::compressions; *c; ++c )
        {
          fprintf( stderr, "%s\n", (*c)->getName().c_str() );
        }
        fprintf( stderr, "\n" );

        return false;
      }

      SKIP_ON_VALIDATION;
      SET_STORABLE( bundle, compression_method,
          Compression::CompressionMethod::selectedCompression->getName() );
      dPrintf( "storable[bundle][compression_method] = %s\n",
          GET_STORABLE( bundle, compression_method ).c_str() );

      return true;
      /* NOTREACHED */
      break;

    case oRuntime_threads:
      REQUIRE_VALUE;

      sizeValue = runtime.threads;
      if ( sscanf( optionValue, "%zu %n", &sizeValue, &n ) != 1 ||
           optionValue[ n ] || sizeValue < 1 )
        throw exInvalidThreadsValue( optionValue );
      runtime.threads = sizeValue;

      dPrintf( "runtime[threads] = %zu\n", runtime.threads );

      return true;
      /* NOTREACHED */
      break;

    case oRuntime_cacheSize:
      REQUIRE_VALUE;

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
          fprintf( stderr, "Invalid suffix specified in cache size (%s): %s.\n"
                   VALID_SUFFIXES, optionValue, suffix );
          return false;
        }
        runtime.cacheSize = sizeValue * scale;

        dPrintf( "runtime[cacheSize] = %zu\n", runtime.cacheSize );

        return true;
      }
      return false;
      /* NOTREACHED */
      break;

    case oRuntime_exchange:
      REQUIRE_VALUE;

      if ( strcmp( optionValue, "backups" ) == 0 )
        runtime.exchange.set( BackupExchanger::backups );
      else
      if ( strcmp( optionValue, "bundles" ) == 0 )
        runtime.exchange.set( BackupExchanger::bundles );
      else
      if ( strcmp( optionValue, "index" ) == 0 )
        runtime.exchange.set( BackupExchanger::index );
      else
      {
        fprintf( stderr, "Invalid exchange value specified: %s\n"
                 "Must be one of the following: backups, bundles, index.\n",
                 optionValue );
        return false;
      }

      dPrintf( "runtime[exchange] = %s\n", runtime.exchange.to_string().c_str() );

      return true;
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
  fprintf( stderr,
"Available %s options overview:\n\n"
"== help ==\n"
"show this message\n"
"", ( type == Runtime ? "runtime" : ( type == Storable ? "storable" : "" ) ) );

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

bool Config::parseProto( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}

string Config::toString( google::protobuf::Message const & message )
{
  std::string str;
  google::protobuf::TextFormat::PrintToString( message, &str );

  return str;
}

bool Config::validateProto( const string & oldConfigData, const string & configData )
{
  Config config;
  dPrintf( "Validating proto...\n" );
  if ( !parseProto( configData, config.storable ) )
    return false;

  const ::google::protobuf::Descriptor * configDescriptor =
    config.storable->descriptor();
  for ( int i = 0; i < configDescriptor->field_count(); i++ )
  {
    const ::google::protobuf::FieldDescriptor * storage =
      configDescriptor->field( i );
    dPrintf( "Storage: %s - %d - %d\n", storage->name().c_str(),
        storage->label(), storage->type());

    // TODO: support for top-level fields
    if ( storage->type() == ::google::protobuf::FieldDescriptor::TYPE_MESSAGE )
    {
      const ::google::protobuf::Descriptor * storageDescriptor =
        storage->message_type();

      for ( int j = 0; j < storageDescriptor->field_count(); j++ )
      {
        const ::google::protobuf::FieldDescriptor * field =
          storageDescriptor->field( j );

        dPrintf( "Field: %s - %d - %d\n", field->name().c_str(),
            field->label(), field->type());

        string option = storage->name() + "." + field->name();

        if ( !config.parseOrValidate( option.c_str(), Storable, true ) )
        {
          fprintf( stderr, "Invalid option specified: %s\n",
                   option.c_str() );
          return false;
        }
      }
    }
  }

  return true;
}

void Config::reset_storable()
{
  // TODO: Use protobuf introspection
  // to fill messages in loop with default values
  // without explicit declaration
  SET_STORABLE( chunk, max_size, GET_STORABLE( chunk, max_size ) );
  SET_STORABLE( bundle, max_payload_size, GET_STORABLE( bundle, max_payload_size ) );
  SET_STORABLE( bundle, compression_method, GET_STORABLE( bundle, compression_method ) );
  SET_STORABLE( lzma, compression_level, GET_STORABLE( lzma, compression_level ) );
}

void Config::show()
{
  printf( "%s", toString( *storable ).c_str() );
}

void Config::show( const ConfigInfo & config )
{
  printf( "%s", toString( config ).c_str() );
}
