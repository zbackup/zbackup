// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CONFIG_HH_INCLUDED__
#define CONFIG_HH_INCLUDED__

#include <string>
#include <google/protobuf/text_format.h>

using std::string;

/* This class is intentended only for use with storable options */
class ZConfig: public ZBackupBase
{
public:
  typedef struct
  {
    uint32_t max_size;
  } ChunkConfig;

  typedef struct {
    uint32_t max_payload_size;
    string default_compression_method;
  } BundleConfig;

  typedef struct {
    ChunkConfig chunkConfig;
    BundleConfig bundleConfig;
  } Config;

  // Print current configuration to screen
  void show();

  // Edit current configuration
  // returns true if configuration is changed
  bool editInteractively();

  // Validator for user-supplied configuration
  static bool validate( const string &, const string & );

  static bool parse( const string & str, google::protobuf::Message * mutable_message );

  static bool parseOption( Config & config, const char * option );

  static void showHelp();

  ZConfig( string const & storageDir, string const & password );
  ZConfig( string const & storageDir, string const & password, Config & );
private:
  string toString( google::protobuf::Message const & message );

  Config config;
};

#endif
