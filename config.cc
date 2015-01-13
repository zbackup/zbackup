// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "zbackup_base.hh"
#include "zbackup.pb.h"
#include "debug.hh"

#include "config.hh"

bool ZConfig::parseOption( Config & config, const char * option )
{
  dPrintf( "Parsing option \"%s\"...\n", option );
  return false;
}

void ZConfig::showHelp()
{
  fprintf( stderr,
"Available options overview:\n"
" help - show this message\n"
"");
}

ZConfig::ZConfig( string const & storageDir, string const & password ):
  ZBackupBase( storageDir, password, true )
{
}

ZConfig::ZConfig( string const & storageDir, string const & password, Config & configIn ):
  ZBackupBase( storageDir, password, true )
{
}

bool ZConfig::parse( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}

string ZConfig::toString( google::protobuf::Message const & message )
{
  std::string str;
  google::protobuf::TextFormat::PrintToString( message, &str );

  return str;
}

void ZConfig::show()
{
  printf( "%s", toString( extendedStorageInfo.config() ).c_str() );
}

bool ZConfig::validate( const string & configData, const string & newConfigData )
{
  ConfigInfo newConfig;
  return parse( newConfigData, &newConfig );
}

bool ZConfig::editInteractively()
{
  string configData( toString( extendedStorageInfo.config() ) );
  string newConfigData( configData );

  if ( !spawnEditor( newConfigData, &validate ) )
    return false;
  ConfigInfo newConfig;
  if ( !parse( newConfigData, &newConfig ) )
    return false;
  if ( toString( extendedStorageInfo.config() ) == toString( newConfig ) )
  {
    verbosePrintf( "No changes made to config\n" );
    return false;
  }

  verbosePrintf( "Updating configuration...\n" );

  extendedStorageInfo.mutable_config()->CopyFrom( newConfig );
  verbosePrintf(
"Configuration successfully updated!\n"
"Updated configuration:\n\n%s", toString( extendedStorageInfo.config() ).c_str() );

  return true;
}
