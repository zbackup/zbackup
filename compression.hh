// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef COMPRESSION_HH_INCLUDED__
#define COMPRESSION_HH_INCLUDED__

#include "sptr.hh"
#include "ex.hh"
#include "nocopy.hh"


namespace Compression {

DEF_EX( Ex, "Compression exception", std::exception )
DEF_EX_STR( exUnsupportedCompressionMethod, "Unsupported compression method: ", Ex )


// used for encoding or decoding
class EnDecoder: NoCopy
{
protected:
  EnDecoder();
public:
  virtual ~EnDecoder();

  // encoder can read up to size bytes from data
  virtual void setInput ( const void* data, size_t size ) =0;
  // how many bytes of the last input haven't been used, yet?
  virtual size_t getAvailableInput() =0;

  // encoder can write up to size bytes to output
  virtual void setOutput( void* data, size_t size ) =0;
  // how many bytes of free space are remaining in the output buffer
  virtual size_t getAvailableOutput() =0;

  // process some bytes
  // finish: will you pass more data to the encoder via setOutput?
  // NOTE You must eventually set finish to true.
  // returns, whether all output bytes have been written
  virtual bool process( bool finish ) =0;
};

// compression method
class CompressionMethod
{
public:
  virtual ~CompressionMethod();

  // returns name of compression method
  // This name is saved in the file header of the compressed file.
  virtual std::string getName() const =0;

  virtual sptr<EnDecoder> createEncoder() const =0;
  virtual sptr<EnDecoder> createDecoder() const =0;

  // find a compression by name
  // If optional is false, it will either return a valid CompressionMethod
  // object or abort the program. If optional is true, it will return
  // NULL, if it cannot find the a compression with that name.
  static const_sptr<CompressionMethod> findCompression(
    const std::string& name, bool optional = false );

  static const_sptr<CompressionMethod> defaultCompression;

  class iterator
  {
    friend class CompressionMethod;

    const const_sptr<CompressionMethod>* ptr;
    iterator( const const_sptr<CompressionMethod>* ptr );
  public:
    iterator( const iterator& it );
    iterator& operator =( const iterator& it );

    bool operator ==( const iterator& other ) const;
    bool operator !=( const iterator& other ) const;

    bool atEnd() const;

    iterator& operator ++();

    const_sptr<CompressionMethod> operator *();
  };
  static iterator begin();
  static iterator end();
};

}

#endif
