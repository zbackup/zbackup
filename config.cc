// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "config.hh"
#include "debug.hh"

ZConfig::ZConfig( string const & storageDir, string const & password ):
  ZBackupBase( storageDir, password, true )
{
}

string ZConfig::toString( google::protobuf::Message const & message )
{
  std::string str;
  google::protobuf::TextFormat::PrintToString( message, &str );

  return str;
}

void ZConfig::print()
{
  printf( "%s", toString( extendedStorageInfo.config() ).c_str() );
}

bool ZConfig::parse( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}
