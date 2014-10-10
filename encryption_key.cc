// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string.h>

#include "check.hh"
#include "encryption_key.hh"
#include "random.hh"

namespace {
/// Derives an encryption key from a password and key info
void deriveKey( string const & password, EncryptionKeyInfo const & info,
                        void * key, unsigned keySize )
{
  CHECK( PKCS5_PBKDF2_HMAC_SHA1( password.data(), password.size(),
                                 (unsigned char const *) info.salt().data(),
                                 info.salt().size(), info.rounds(), keySize,
                                 (unsigned char *) key ) == 1,
         "encryption key derivation failed" );
}

string calculateKeyHmac( void const * key, unsigned keySize,
                         string const & input )
{
  char result[ EVP_MAX_MD_SIZE ];
  unsigned resultSize;
  CHECK( HMAC( EVP_sha1(), (unsigned char const *) key, keySize,
               (unsigned char const *) input.data(), input.size(),
               (unsigned char *) result, &resultSize ),
         "encryption key HMAC calcuation failed" );

  return string( result, result + resultSize );
}
}

EncryptionKey::EncryptionKey( string const & password,
                              EncryptionKeyInfo const * info )
{
  if ( !info )
    isSet = false;
  else
  {
    isSet = true;

    char derivedKey[ KeySize ];
    deriveKey( password, *info, derivedKey, sizeof( derivedKey ) );

    AES_KEY aesKey;
    AES_set_decrypt_key( ( unsigned char const * ) derivedKey, 128, &aesKey );
    AES_decrypt( ( unsigned char const * ) info->encrypted_key().data(),
                 ( unsigned char * ) key, &aesKey );

    if ( calculateKeyHmac( key, sizeof( key ), info->key_check_input() ) !=
         info->key_check_hmac() )
      throw exInvalidPassword();
  }
}

EncryptionKey::~EncryptionKey()
{
  // Clear the key from memory
  memset( key, 0, sizeof( key ) );
}

void EncryptionKey::generate( string const & password,
                              EncryptionKeyInfo & info )
{
  // Use this buf for salts
  char buf[ 16 ];

  Random::genaratePseudo( buf, sizeof( buf ) );
  info.set_salt( buf, sizeof( buf ) );
  info.set_rounds( 10000 ); // TODO: make this configurable

  char derivedKey[ KeySize ];
  deriveKey( password, info, derivedKey, sizeof( derivedKey ) );

  char key[ KeySize ];

  Random::genarateTrue( key, sizeof( key ) );

  // Fill in the HMAC verification part
  Random::genaratePseudo( buf, sizeof( buf ) );
  info.set_key_check_input( buf, sizeof( buf ) );
  info.set_key_check_hmac( calculateKeyHmac( key, sizeof( key ),
                                             info.key_check_input() ) );

  // Encrypt the key
  AES_KEY aesKey;
  AES_set_encrypt_key( ( unsigned char const * ) derivedKey, 128, &aesKey );
  char encryptedKey[ sizeof( key ) ];
  AES_encrypt( ( unsigned char const * ) key,
               ( unsigned char * ) encryptedKey, &aesKey );
  info.set_encrypted_key( encryptedKey, sizeof( encryptedKey ) );

  // Clear the key from memory
  memset( key, 0, sizeof( key ) );
}

EncryptionKey const & EncryptionKey::noKey()
{
  static EncryptionKey key( string(), NULL );
  return key;
}
