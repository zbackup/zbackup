// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "config.hh"
#include "debug.hh"

/* Keyword tokens. */

typedef enum
{
  oBadOption,

  oChunk_max_size,
  oBundle_max_payload_size,
  oBundle_compression_method,

  oDeprecated, oUnsupported
} OpCodes;

/* Textual representations of the tokens. */

static struct
{
  const char * name;
  OpCodes opcode;
  const char * description;
} keywords[] = {
  {
    "chunk.max_size",
    oChunk_max_size,
    "Maximum chunk size used when storing chunks\n"
    "Directly affects deduplication ratio"
  },
  {
    "bundle.max_payload_size",
    oBundle_max_payload_size,
    "Maximum number of bytes a bundle can hold. Only real chunk bytes are\n"
    "counted, not metadata. Any bundle should be able to contain at least\n"
    "one arbitrary single chunk, so this should not be smaller than\n"
    "chunk.max_size" },
  {
    "bundle.compression_method",
    oBundle_compression_method,
    "Compression method for new bundles"
  },
  {
    "compression",
    oBundle_compression_method,
    "Shortcut for bundle.compression_method"
  },

  { NULL, oBadOption }
};

bool Config::parseOption( const char * option )
{
  dPrintf( "Parsing option \"%s\"...\n", option );
  return true;
}

void Config::showHelp()
{
  fprintf( stderr,
"Available options overview:\n\n"
"== help ==\n"
"shows this message\n"
"");

  u_int i;
  for ( i = 0; keywords[ i ].name; i++ )
  {
    fprintf( stderr, "\n== %s ==\n%s\n", keywords[ i ].name, keywords[ i ].description );
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
