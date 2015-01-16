// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED__
#define CONFIG_HH_INCLUDED__

#include <string>
#include <vector>
#include <google/protobuf/text_format.h>
#include "zbackup.pb.h"

using std::string;
using std::vector;

class Config
{
public:
  typedef vector< string > Options;

  // Validator for user-supplied configuration
  static bool validate( const string &, const string & );

  static bool parse( const string & str, google::protobuf::Message * mutable_message );

  static void showHelp();

  bool parseOption( const char * option );

  string toString( google::protobuf::Message const & message );
private:
  Options options;
};

#endif
