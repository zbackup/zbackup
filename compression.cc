// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <string>

#include "compression.hh"
#include "check.hh"

EnDecoder::EnDecoder() { }
EnDecoder::EnDecoder(const EnDecoder&) { }
EnDecoder::~EnDecoder() { }

Compression::~Compression() { }


// LZMA

#include <lzma.h>

class LZMAEnDecoder : public EnDecoder
{
protected:
  static lzma_stream init_value;
  lzma_stream strm;
public:
  LZMAEnDecoder()
  {
    strm = init_value;
  }

  void setInput( const void* data, size_t size )
  {
    strm.next_in  = (const uint8_t *) data;
    strm.avail_in = size;
  }

  void setOutput( void* data, size_t size )
  {
    strm.next_out  = (uint8_t *) data;
    strm.avail_out = size;
  }

  size_t getAvailableInput()
  {
    return strm.avail_in;
  }

  size_t getAvailableOutput()
  {
    return strm.avail_out;
  }

  bool process( bool finish )
  {
    lzma_ret ret = lzma_code( &strm, ( finish ? LZMA_FINISH : LZMA_RUN ) );

    CHECK( ret == LZMA_OK || ret == LZMA_STREAM_END, "lzma_code error: %d", (int) ret );

    return ( ret == LZMA_STREAM_END );
  }
};
lzma_stream LZMAEnDecoder::init_value = LZMA_STREAM_INIT;

class LZMAEncoder : public LZMAEnDecoder
{
public:
  LZMAEncoder()
  {
    uint32_t preset = 6; // TODO: make this customizable, although 6 seems to be
                         // the best option
    lzma_ret ret = lzma_easy_encoder( &strm, preset, LZMA_CHECK_CRC64 );
    CHECK( ret == LZMA_OK, "lzma_easy_encoder error: %d", (int) ret );
  }
};

class LZMADecoder : public LZMAEnDecoder
{
public:
  LZMADecoder()
  {
    lzma_ret ret = lzma_stream_decoder( &strm, UINT64_MAX, 0 );
    CHECK( ret == LZMA_OK,"lzma_stream_decoder error: %d", (int) ret );
  }
};

class LZMACompression : public Compression
{
public:
  sptr<EnDecoder> createEncoder() const
  {
    return new LZMAEncoder();
  }

  sptr<EnDecoder> createDecoder() const
  {
    return new LZMADecoder();
  }

  std::string getName() const { return "lzma"; }
};


// LZO

// liblzo implements a lot of algorithms "for unlimited backward compatibility"

// The web site says:
// "My experiments have shown that LZO1B is good with a large blocksize
//  or with very redundant data, LZO1F is good with a small blocksize or
//  with binary data and that LZO1X is often the best choice of all.
//  LZO1Y and LZO1Z are almost identical to LZO1X - they can achieve a
//  better compression ratio on some files.
//  Beware, your mileage may vary."
// => I'm using LZO1X, as suggested

#include <string.h>

// Unfortunately, liblzo always works with the whole data, so it doesn't support
// the streaming approach that most other libraries use. This means that we have
// to use a big buffer for the data. The class NoStreamEnDecoder implements this
// so we can use it, if there is another library like liblzo.

// Collect all data and process it in one pass
class NoStreamEnDecoder : public EnDecoder
{
  std::string acc_data_in, acc_data_out;
  const char* data_in;
  char* data_out;
  size_t avail_in, avail_out;
  bool processed;
  size_t pos_in_acc_data_out;
protected:
  // you must implement these:

  // Should we try with the existing output buffer which has avail_out
  // bytes of free space? If you know that this will fail, return false.
  // You may peek into data_in which contains the complete compressed data.
  virtual bool shouldTryWith( const char* data_in, size_t avail_in, size_t avail_out ) =0;

  // We will allocate a buffer for the output data. How big should it be?
  // You may peek into data_in which contains the complete compressed data.
  virtual size_t suggestOutputSize( const char* data_in, size_t avail_in ) =0;

  // Is this input complete?
  // An encoder should return false.
  virtual bool isCompleteInput( const char* data_in, size_t avail_in ) =0;

  // Process the data in data_in and put the result into data_out. You musn't
  // write more than avail_out bytes! If the output buffer is big enough,
  // process the data and store the output size in output_size. If the output
  // buffer is too small, return false and we will give you a bigger one. If
  // any other error occurrs, abort the program. We don't have any better
  // error handling. Sorry. Do NOT return false for errors that won't be
  // remedied by a bigger buffer!
  virtual bool do_process( const char* data_in, size_t avail_in,
    char* data_out, size_t avail_out, size_t& output_size ) =0;

  void setUnusedInput( size_t unused )
  {
    this->data_in += avail_in - unused;
    this->avail_in = unused;
  }
public:
  NoStreamEnDecoder()
  {
    data_in = data_out = NULL;
    avail_in = avail_out = pos_in_acc_data_out = 0;
    processed = false;
  }

  void setInput( const void* data, size_t size )
  {
    data_in  = (const char *) data;
    avail_in = size;
  }

  void setOutput( void* data, size_t size )
  {
    data_out  = (char *) data;
    avail_out = size;
  }

  size_t getAvailableInput()
  {
    return avail_in;
  }

  size_t getAvailableOutput()
  {
    return avail_out;
  }

  bool process( bool finish )
  {
    // try to process the input, if we haven't done it, yet
    if ( !processed )
    {
      // data has not been encoded
      if ( acc_data_in.empty() )
      {
        // this is the first piece of data
        if ( finish || isCompleteInput( data_in, avail_in ) )
        {
          // special case: all the data has been passed at once
          // -> process it without using acc_data_in
          process_finish( data_in, avail_in );
        }
      }

      // if we didn't process the data, put it into accumulator
      if ( !processed )
      {
        // accumulate data in acc_data_in
        acc_data_in.append( data_in, avail_in );

        // If this was the last bit of data, we process it, now.
        if ( finish || isCompleteInput( acc_data_in.data(), acc_data_in.size() ) )
        {
          process_finish( acc_data_in.data(), acc_data_in.size() );
        }
      }
    }

    // If the input has been processed, try to copy some of it to the output buffer.
    if ( processed )
    {
      // data has been encoded or decoded, remaining output is in acc_data_out
      // -> copy to output
      if (avail_out > 0 && acc_data_out.size() - pos_in_acc_data_out > 0)
      {
        size_t sz = avail_out;
        if ( sz > acc_data_out.size() - pos_in_acc_data_out )
          sz = acc_data_out.size() - pos_in_acc_data_out;

        memcpy( data_out, acc_data_out.data() + pos_in_acc_data_out, sz );
        data_out  += sz;
        avail_out -= sz;
        pos_in_acc_data_out += sz;
      }

      // no more data left? -> return true
      return ( acc_data_out.size() - pos_in_acc_data_out == 0 );
    } else {
      // not yet processed, so we cannot be done
      return false;
    }
  }

private:
  void process_finish( const char* data_in, size_t avail_in )
  {
    // should we try with the existing output buffer?
    if ( shouldTryWith( data_in, avail_in, avail_out ) )
    {
      size_t output_size;
      if ( do_process( data_in, avail_in, data_out, avail_out, output_size ) )
      {
        // it worked :-)
        processed = true;
        avail_out -= output_size;
        return ;
      }
    }

    // we use our own buffer
    size_t buffer_size = suggestOutputSize( data_in, avail_in );
    do {
      acc_data_out.resize(buffer_size);

      size_t output_size;
      //TODO doc says we mustn't modify the pointer returned by data()...
      if ( do_process( data_in, avail_in,
          (char*) acc_data_out.data(), buffer_size, output_size ) )
      {
        // buffer is big enough
        acc_data_out.resize( output_size );
        processed = true;
        return ;
      }

      // try a bigger one
      buffer_size *= 2;
    } while (true);
  }
};

#include <endian.h>

// like NoStreamEnDecoder, but also adds the uncompressed size before the stream
//NOTE You should make sure that the compression function doesn't overwrite any
//     memory, if this information is corrupted! This could be exploited by a
//     malicious person and there is nothing I can do about it. I could check for
//     an overflow, but when control gets back to this class, it is already too
//     late, as one 'ret' instruction is enough to do harm.
class NoStreamAndUnknownSizeDecoder : public NoStreamEnDecoder
{
protected:
  // You implement this one:
  // If you don't know the real decoded size, don't change output_size.
  virtual bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) =0;


  bool shouldTryWith( const char* data_in, size_t avail_in, size_t avail_out )
  {
    return suggestOutputSize( data_in, avail_in ) <= avail_out;
  }

  // Is this input complete?
  bool isCompleteInput( const char* data_in, size_t avail_in )
  {
    if ( avail_in < 2*sizeof(uint64_t) )
      return false;

    data_in += sizeof(uint64_t);

    size_t input_size = le32toh( *(uint32_t*) data_in );

    return ( avail_in >= input_size + 2*sizeof(uint64_t) );
  }

  size_t suggestOutputSize( const char* data_in, size_t avail_in )
  {
    CHECK( avail_in >= sizeof(uint64_t), "not enough input data" );
    // We're not using size_t because we need a type that has the same size on all
    // architectures. A 32-bit host won't be able to open files with more than
    // 4GB (actually much less), so 4 byte are enough. Even a 64-bit host would
    // have some trouble with allocating 8GB of RAM just for our buffers ;-)
    //NOTE If your compiler doesn't accept this cast, your size_t is smaller than
    //     uint32_t. In that case, you are in trouble...
    size_t output_size = le32toh( *(uint32_t*) data_in );
    return output_size;
  }

  bool do_process( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size )
  {
    if ( avail_in < 2*sizeof( uint64_t ) )
      return false;

    //NOTE We skip 8 bytes. If we later decide to drop compatibility with 32-bit
    //     hosts, we can save a 64-bit size. Well, that will be much later, when
    //     we can easily hold two copies of a 4GB file in main memory :-D

    size_t needed_output_size = le32toh( *(uint32_t*) data_in );
    data_in += sizeof(uint64_t);
    size_t input_size = le32toh( *(uint32_t*) data_in );
    data_in += sizeof(uint64_t);
    if ( output_size < needed_output_size )
      return false;

    avail_in -= 2*sizeof( uint64_t );

    // We might not need all of our input data.
    setUnusedInput( avail_in - input_size );
    avail_in = input_size;

    size_t reported_output_size = needed_output_size;
    if ( !do_process_no_size( data_in, avail_in, data_out, avail_out, reported_output_size ) )
      return false;

    CHECK( reported_output_size == needed_output_size,
      "Size of decoded data is different than expected" );

    output_size = needed_output_size;

    return true;
  }
};

// encoder for NoStreamAndUnknownSizeDecoder
class NoStreamAndUnknownSizeEncoder : public NoStreamEnDecoder
{
protected:
  // You implement this one:
  virtual bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) =0;


  bool shouldTryWith( const char*, size_t, size_t avail_out )
  {
    // If the compression doesn't use any spaces...
    return avail_out > sizeof( uint64_t );
  }

  bool isCompleteInput( const char* data_in, size_t avail_in )
  {
    // We cannot know whether the user wants to send more data.
    // -> return false; user must use finish=true to signal end of data
    return false;
  }

  size_t getOverhead()
  {
    return 2*sizeof( uint64_t );
  }

  size_t suggestOutputSize( const char*, size_t avail_in )
  {
    // We assume that the compression won't make the data any bigger.
    return avail_in + getOverhead();
  }

  bool do_process( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size )
  {
    CHECK( avail_in <= UINT32_MAX,
      "You want to compress more than 4GB of data?! Sorry, we don't support that, yet." );

    memcpy(data_out, "ABCDEFGHIJKLMNOP", 16);

    // store size
    *(uint32_t*)data_out = htole32( avail_in );
    uint32_t* compressed_size = (uint32_t*) ( data_out + sizeof( uint64_t ) );

    // compressed data goes after the size
    // We skip more than we actually use; see NoStreamAndUnknownSizeDecoder::do_process(...).
    data_out  += getOverhead();
    avail_out -= getOverhead();

    if ( !do_process_no_size( data_in, avail_in, data_out, avail_out, output_size ) )
      return false;

    CHECK( output_size <= UINT32_MAX,
      "The compressed data is more than 4GB?! Sorry, we don't support that, yet." );
    *compressed_size = htole32( (uint32_t) output_size );

    output_size += getOverhead();

    return true;
  }
};


#ifdef HAVE_LIBLZO

#include <lzo/lzo1x.h>

// finally, we can implement lzo
class LZO1X_1_Decoder : public NoStreamAndUnknownSizeDecoder
{
protected:
  bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size )
  {
    // same argument is used for available output size and size of decompressed data
    output_size = avail_out;
    int ret = lzo1x_decompress_safe( (const lzo_bytep) data_in, avail_in,
      (lzo_bytep) data_out, &output_size, NULL );

  if ( ret == LZO_E_OUTPUT_OVERRUN )
    return false;

  CHECK( ret >= LZO_E_OK, "lzo1x_decompress_safe failed (code %d)", ret );

  return true;
  }
};
class LZO1X_1_Compression;
class LZO1X_1_Encoder : public NoStreamAndUnknownSizeEncoder
{
  const LZO1X_1_Compression* compression;
  static size_t calcMaxCompressedSize(size_t avail_in);
public:
  LZO1X_1_Encoder(const LZO1X_1_Compression* compression)
  {
    this->compression = compression;
  }

protected:
  bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size );
  bool shouldTryWith( const char*, size_t, size_t avail_out );
  size_t suggestOutputSize( const char*, size_t avail_in );
};
class LZO1X_1_Compression : public Compression
{
  static bool initialized;
  static void init()
  {
    //TODO This is not thread-safe. Does it have to be?
    if (!initialized)
    {
      int ret = lzo_init();
      CHECK( ret == LZO_E_OK, "lzo_init failed (%d)", ret );
      initialized = true;
    }
  }
public:
  sptr<EnDecoder> createEncoder() const
  {
    init();
    return new LZO1X_1_Encoder(this);
  }

  sptr<EnDecoder> createDecoder() const
  {
    init();
    return new LZO1X_1_Decoder();
  }

  std::string getName() const { return "lzo1x_1"; }


  lzo_voidp getWorkmem( size_t size ) const
  {
    return new char[size];
  }

  void giveBackWorkmem( lzo_voidp wrkmem ) const
  {
    //TODO I think we should keep the memory around and reuse it. After all
    //     it is only a few kilobytes and we will need it a lot. However, I
    //     won't risk anything here because I don't know whether this will be
    //     called by more than one thread.
    delete[] (char*)wrkmem;
  }
};

bool LZO1X_1_Compression::initialized = false;


size_t LZO1X_1_Encoder::calcMaxCompressedSize( size_t avail_in )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // The documentation refers to example/simple.c which says:
  // "Because the input block may be incompressible, we must provide a little more
  //  output space in case that compression is not possible."
  // -> We use the same formula.
  return (avail_in + avail_in / 16 + 64 + 3);
}

bool LZO1X_1_Encoder::shouldTryWith( const char* data_in, size_t avail_in, size_t avail_out )
{
  return avail_out >= suggestOutputSize( data_in, avail_in );
}

size_t LZO1X_1_Encoder::suggestOutputSize( const char*, size_t avail_in )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // The documentation refers to example/simple.c which says:
  // "Because the input block may be incompressible, we must provide a little more
  //  output space in case that compression is not possible."
  // -> We use the same formula.
  return calcMaxCompressedSize( avail_in ) + getOverhead();
}

bool LZO1X_1_Encoder::do_process_no_size( const char* data_in, size_t avail_in,
    char* data_out, size_t avail_out, size_t& output_size )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // Therefore, we won't try it unless we are sure that the buffer is big enough.
  if ( avail_out < calcMaxCompressedSize( avail_in ) )
    return false;

  // same argument is used for available output size (haha, see above)
  // and size of decompressed data
  output_size = avail_out;

  lzo_voidp wrkmem = compression->getWorkmem(LZO1X_1_MEM_COMPRESS);
  int ret = lzo1x_1_compress( (const lzo_bytep) data_in, avail_in,
    (lzo_bytep) data_out, &output_size, wrkmem );
  compression->giveBackWorkmem(wrkmem);

  if ( ret == LZO_E_OUTPUT_OVERRUN )
    return false;

  CHECK( ret >= LZO_E_OK, "lzo1x_1_compress failed (code %d)", ret );

  return true;
}

#endif  // HAVE_LIBLZO


// register them

static const_sptr<Compression> const compressions[] = {
  new LZMACompression(),
# ifdef HAVE_LIBLZO
  new LZO1X_1_Compression(),
# endif
  // NULL entry marks end of list. Don't remove it!
  NULL
};

const_sptr<Compression> Compression::default_compression = compressions[0];

const_sptr<Compression> Compression::findCompression( const std::string& name, bool optional )
{
  for ( const const_sptr<Compression>* c = compressions+0; *c; ++c )
  {
    if ( (*c)->getName() == name )
    {
      return (*c);
    }
  }
  CHECK( !optional, "Couldn't find compression method '%s'", name.c_str() );
  return NULL;
}

// iterator over compressions

Compression::iterator::iterator( const const_sptr<Compression>* ptr ) : ptr(   ptr) { }
Compression::iterator::iterator( const iterator&                 it ) : ptr(it.ptr) { }

Compression::iterator& Compression::iterator::operator =( const iterator& it )
{
  this->ptr = it.ptr;
  return *this;
}

bool Compression::iterator::operator ==( const iterator& other ) const
{
  // special case: one has ptr==NULL (end iterator returned by end()) and the
  // other has *ptr==NULL (end iterator obtained by calling ++)
  if ( !ptr && ( !other.ptr || !*other.ptr ) )
    return true;
  else if ( !other.ptr && ( !ptr || !*ptr ) )
    return true;
  else
    return (ptr == other.ptr);
}
bool Compression::iterator::operator !=( const iterator& other ) const
{
  return !( *this == other );
}

bool Compression::iterator::at_end() const
{
  return !ptr || !*ptr;
}

Compression::iterator& Compression::iterator::operator ++()
{
  CHECK( ptr && *ptr, "Cannot increment the end iterator" );

  ++ptr;

  return *this;
}

const_sptr<Compression> Compression::iterator::operator *()
{
  CHECK( ptr && *ptr, "Cannot dereference the end iterator" );

  return *ptr;
}

Compression::iterator Compression::begin()
{
  return iterator(compressions);
}
Compression::iterator Compression::end()
{
  return iterator(NULL);
}
