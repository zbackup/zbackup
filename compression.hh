// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#ifndef COMPRESSION_HH_INCLUDED__
#define COMPRESSION_HH_INCLUDED__

#include "sptr.hh"


// used for encoding or decoding
class EnDecoder
{
protected:
  EnDecoder();
  // cannot be copied
  EnDecoder( const EnDecoder& );
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
class Compression
{
public:
  virtual ~Compression();

  // returns name of compression method
  // This name is saved in the file header of the compressed file.
  virtual std::string getName() const =0;

  virtual sptr<EnDecoder> createEncoder() const =0;
  virtual sptr<EnDecoder> createDecoder() const =0;

  // find a compression by name
  // If optional is false, it will either return a valid Compression
  // object or abort the program. If optional is true, it will return
  // NULL, if it cannot find the a compression with that name.
  static const_sptr<Compression> findCompression(
    const std::string& name, bool optional = false );

  static const_sptr<Compression> default_compression;

  class iterator
  {
    friend class Compression;

    const const_sptr<Compression>* ptr;
    iterator( const const_sptr<Compression>* ptr );
  public:
    iterator( const iterator& it );
    iterator& operator =( const iterator& it );

    bool operator ==( const iterator& other ) const;
    bool operator !=( const iterator& other ) const;

    bool at_end() const;

    iterator& operator ++();

    const_sptr<Compression> operator *();
  };
  static iterator begin();
  static iterator end();
};

#endif
