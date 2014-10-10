// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ENCRYPTED_FILE_HH_INCLUDED__
#define ENCRYPTED_FILE_HH_INCLUDED__

#include <google/protobuf/io/zero_copy_stream.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <exception>
#include <vector>

#include "adler32.hh"
#include "encryption.hh"
#include "encryption_key.hh"
#include "ex.hh"
#include "unbuffered_file.hh"

/// Google's ZeroCopyStream implementations which read and write files encrypted
/// with our encryption mechanism. They also calculate adler32 of all file
/// content and write/check it at the end.
/// Encryption-wise we implement AES-128 in CBC mode with PKCS#7 padding. We
/// don't use EVP for this currently - everyone is welcome to change this, and
/// to add support for arbitrary ciphers, key lengths and modes of operations as
/// well. When no encryption key is set, no encryption or padding is done, but
/// everything else works the same way otherwise
namespace EncryptedFile {

DEF_EX( Ex, "Encrypted file exception", std::exception )
DEF_EX( exFileCorrupted, "encrypted file data is currupted", Ex )
DEF_EX( exIncorrectFileSize, "size of the encrypted file is incorrect", exFileCorrupted )
DEF_EX( exReadFailed, "read failed", Ex ) // Only thrown by InputStream::read()
DEF_EX( exAdlerMismatch, "adler32 mismatch", Ex )

class InputStream: public google::protobuf::io::ZeroCopyInputStream
{
public:
  /// Opens the input file. If EncryptionKey contains no key, the input won't be
  /// decrypted and iv would be ignored
  InputStream( char const * fileName, EncryptionKey const &, void const * iv );
  virtual bool Next( void const ** data, int * size );
  virtual void BackUp( int count );
  virtual bool Skip( int count );
  virtual int64_t ByteCount() const;


  /// Returns adler32 of all data read so far. Calling this makes backing up
  /// for the previous Next() call impossible - the data has to be consumed
  Adler32::Value getAdler32();

  /// Performs a traditional read, for convenience purposes
  void read( void * buf, size_t size );

  /// Reads an adler32 value from the stream and compares with checkAdler32().
  /// Throws an exception on mismatch
  void checkAdler32();

  /// Reads and discards the number of bytes equivalent to an IV size. This is
  /// used when no IV is initially provided.
  /// If there's no encryption key set, does nothing
  void consumeRandomIv();

  /// Closes the file
  ~InputStream() {}

private:
  UnbufferedFile file;
  UnbufferedFile::Offset filePos;
  EncryptionKey const & key;
  char iv[ Encryption::IvSize ];
  std::vector< char > buffer;
  char * start; /// Points to the start of the data currently held in buffer
  size_t fill; /// Number of bytes held in buffer
  size_t remainder; /// Number of bytes held in buffer just after the main
                    /// 'fill'-bytes portion. We have to keep those to implement
                    /// PKCS#7 padding
  bool backedUp; /// True if the BackUp operation was performed, and the buffer
                 /// contents are therefore unconsumed
  Adler32 adler32;

  /// Decrypts 'fill' bytes at 'start', adjusting 'fill' and setting 'remainder'
  void decrypt();
  /// Only used by decrypt()
  void doDecrypt();
};

class OutputStream: public google::protobuf::io::ZeroCopyOutputStream
{
public:
  /// Creates the output file. If EncryptionKey contains no key, the output
  /// won't be encrypted and iv would be ignored
  OutputStream( char const * fileName, EncryptionKey const &, void const * iv );
  virtual bool Next( void ** data, int * size );
  virtual void BackUp( int count );
  virtual int64_t ByteCount() const;

  /// Returns adler32 of all data written so far. Calling this makes backing up
  /// for the previous Next() call impossible - the data has to be consumed
  Adler32::Value getAdler32();

  /// Performs a traditional write, for convenience purposes
  void write( void const * buf, size_t size );

  /// Writes the current adler32 value returned by getAdler32() to the stream
  void writeAdler32();

  /// Writes the number of random bytes equivalent to an IV size. This is used
  /// when no IV is initially provided, and provides an equivalent of having
  /// a random IV when used just after the stream has been opened.
  /// If there's no encryption key set, does nothing
  void writeRandomIv();

  /// Finishes writing and closes the file
  ~OutputStream();

private:
  UnbufferedFile file;
  UnbufferedFile::Offset filePos;
  EncryptionKey const & key;
  char iv[ Encryption::IvSize ];
  std::vector< char > buffer;
  char * start; /// Points to the start of the area currently available for
                /// writing to in buffer
  size_t avail; /// Number of bytes available for writing to in buffer
  bool backedUp; /// True if the BackUp operation was performed, and the buffer
                 /// contents are therefore unconsumed
  Adler32 adler32;

  /// Encrypts and writes 'bytes' bytes from the beginning of the buffer.
  /// 'bytes' must be non-zero and in multiples of BlockSize
  void encryptAndWrite( size_t bytes );
};

}

#endif
