// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED__
#define CONFIG_HH_INCLUDED__

#include <string>
#include <google/protobuf/text_format.h>

using std::string;

class ZConfig: public ZBackupBase
{
public:
  ZConfig( string const & storageDir, string const & password );

  // Print current configuration to screen
  void show();

  // Edit current configuration in default editor
  // returns true if configuration is changed
  bool edit();

private:
  string toString( google::protobuf::Message const & message );
  bool parse( const string & str, google::protobuf::Message * mutable_message );
};

#endif
