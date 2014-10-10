// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ENCRYPTION_KEY_HH_INCLUDED__
#define ENCRYPTION_KEY_HH_INCLUDED__

#include <exception>
#include <string>

#include "ex.hh"
#include "zbackup.pb.h"

using std::string;

class EncryptionKey
{
  bool isSet;
  unsigned const static KeySize = 16; // TODO: make this configurable
  char key[ KeySize ];

public:
  DEF_EX( exInvalidPassword, "Invalid password specified", std::exception )

  /// Decodes the encryption key from the given info and password. If info is
  /// passed as NULL, the password is ignored and no key is set
  EncryptionKey( string const & password, EncryptionKeyInfo const * );
  ~EncryptionKey();

  /// Returns true if key was set, false otherwise.
  bool hasKey() const
  { return isSet; }

  /// Returns the key. Check if there is one with hasKey() first. Note: the key
  /// should not be copied, as it may be allocated in a locked page in the
  /// future
  void const * getKey() const
  { return key; }

  /// Returns key size, in bytes
  unsigned getKeySize() const
  { return sizeof( key ); }

  /// Generates new key info using the given password
  static void generate( string const & password, EncryptionKeyInfo & );

  /// Returns a static instance without any key set
  static EncryptionKey const & noKey();
};

#endif
