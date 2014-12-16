// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <string>

#include "compression.hh"
#include "check.hh"

namespace Compression {

EnDecoder::EnDecoder() { }
EnDecoder::~EnDecoder() { }

CompressionMethod::~CompressionMethod() { }


// LZMA

#include <lzma.h>

class LZMAEnDecoder : public EnDecoder
{
protected:
  static lzma_stream initValue;
  lzma_stream strm;
public:
  LZMAEnDecoder()
  {
    strm = initValue;
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

  ~LZMAEnDecoder()
  {
    lzma_end( &strm );
  }
};
lzma_stream LZMAEnDecoder::initValue = LZMA_STREAM_INIT;

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

class LZMACompression : public CompressionMethod
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
  std::string accDataIn, accDataOut;
  const char* dataIn;
  char* dataOut;
  size_t availIn, availOut;
  bool processed;
  size_t posInAccDataOut;
protected:
  // you must implement these:

  // Should we try with the existing output buffer which has availOut
  // bytes of free space? If you know that this will fail, return false.
  // You may peek into dataIn which contains the complete compressed data.
  virtual bool shouldTryWith( const char* dataIn, size_t availIn, size_t availOut ) =0;

  // We will allocate a buffer for the output data. How big should it be?
  // You may peek into dataIn which contains the complete compressed data.
  virtual size_t suggestOutputSize( const char* dataIn, size_t availIn ) =0;

  // Is this input complete?
  // An encoder should return false.
  virtual bool isCompleteInput( const char* dataIn, size_t availIn ) =0;

  // Process the data in dataIn and put the result into dataOut. You musn't
  // write more than availOut bytes! If the output buffer is big enough,
  // process the data and store the output size in outputSize. If the output
  // buffer is too small, return false and we will give you a bigger one. If
  // any other error occurrs, abort the program. We don't have any better
  // error handling. Sorry. Do NOT return false for errors that won't be
  // remedied by a bigger buffer!
  virtual bool doProcess( const char* dataIn, size_t availIn,
    char* dataOut, size_t availOut, size_t& outputSize ) =0;

  void setUnusedInput( size_t unused )
  {
    this->dataIn += availIn - unused;
    this->availIn = unused;
  }
public:
  NoStreamEnDecoder()
  {
    dataIn = dataOut = NULL;
    availIn = availOut = posInAccDataOut = 0;
    processed = false;
  }

  void setInput( const void* data, size_t size )
  {
    dataIn  = (const char *) data;
    availIn = size;
  }

  void setOutput( void* data, size_t size )
  {
    dataOut  = (char *) data;
    availOut = size;
  }

  size_t getAvailableInput()
  {
    return availIn;
  }

  size_t getAvailableOutput()
  {
    return availOut;
  }

  bool process( bool finish )
  {
    // try to process the input, if we haven't done it, yet
    if ( !processed )
    {
      // data has not been encoded
      if ( accDataIn.empty() )
      {
        // this is the first piece of data
        if ( finish || isCompleteInput( dataIn, availIn ) )
        {
          // special case: all the data has been passed at once
          // -> process it without using accDataIn
          processFinish( dataIn, availIn );
        }
      }

      // if we didn't process the data, put it into accumulator
      if ( !processed )
      {
        // accumulate data in accDataIn
        accDataIn.append( dataIn, availIn );

        // If this was the last bit of data, we process it, now.
        if ( finish || isCompleteInput( accDataIn.data(), accDataIn.size() ) )
        {
          processFinish( accDataIn.data(), accDataIn.size() );
        }
      }
    }

    // If the input has been processed, try to copy some of it to the output buffer.
    if ( processed )
    {
      // data has been encoded or decoded, remaining output is in accDataOut
      // -> copy to output
      if (availOut > 0 && accDataOut.size() - posInAccDataOut > 0)
      {
        size_t sz = availOut;
        if ( sz > accDataOut.size() - posInAccDataOut )
          sz = accDataOut.size() - posInAccDataOut;

        memcpy( dataOut, accDataOut.data() + posInAccDataOut, sz );
        dataOut  += sz;
        availOut -= sz;
        posInAccDataOut += sz;
      }

      // no more data left? -> return true
      return ( accDataOut.size() - posInAccDataOut == 0 );
    }
    else
    {
      // not yet processed, so we cannot be done
      return false;
    }
  }

private:
  void processFinish( const char* dataIn, size_t availIn )
  {
    // should we try with the existing output buffer?
    if ( shouldTryWith( dataIn, availIn, availOut ) )
    {
      size_t outputSize;
      if ( doProcess( dataIn, availIn, dataOut, availOut, outputSize ) )
      {
        // it worked :-)
        processed = true;
        availOut -= outputSize;
        return ;
      }
    }

    // we use our own buffer
    size_t bufferSize = suggestOutputSize( dataIn, availIn );
    do {
      accDataOut.resize(bufferSize);

      size_t outputSize;
      //TODO doc says we mustn't modify the pointer returned by data()...
      if ( doProcess( dataIn, availIn,
          (char*) accDataOut.data(), bufferSize, outputSize ) )
      {
        // buffer is big enough
        accDataOut.resize( outputSize );
        processed = true;
        return ;
      }

      // try a bigger one
      bufferSize *= 2;
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
  // If you don't know the real decoded size, don't change outputSize.
  virtual bool doProcessNoSize( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize ) =0;


  bool shouldTryWith( const char* dataIn, size_t availIn, size_t availOut )
  {
    return suggestOutputSize( dataIn, availIn ) <= availOut;
  }

  // Is this input complete?
  bool isCompleteInput( const char* dataIn, size_t availIn )
  {
    if ( availIn < 2*sizeof(uint64_t) )
      return false;

    dataIn += sizeof(uint64_t);

    size_t inputSize = le32toh( *(uint32_t*) dataIn );

    return ( availIn >= inputSize + 2*sizeof(uint64_t) );
  }

  size_t suggestOutputSize( const char* dataIn, size_t availIn )
  {
    CHECK( availIn >= sizeof(uint64_t), "not enough input data" );
    // We're not using size_t because we need a type that has the same size on all
    // architectures. A 32-bit host won't be able to open files with more than
    // 4GB (actually much less), so 4 byte are enough. Even a 64-bit host would
    // have some trouble with allocating 8GB of RAM just for our buffers ;-)
    //NOTE If your compiler doesn't accept this cast, your size_t is smaller than
    //     uint32_t. In that case, you are in trouble...
    size_t outputSize = le32toh( *(uint32_t*) dataIn );
    return outputSize;
  }

  bool doProcess( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize )
  {
    if ( availIn < 2*sizeof( uint64_t ) )
      return false;

    //NOTE We skip 8 bytes. If we later decide to drop compatibility with 32-bit
    //     hosts, we can save a 64-bit size. Well, that will be much later, when
    //     we can easily hold two copies of a 4GB file in main memory :-D

    size_t neededOutputSize = le32toh( *(uint32_t*) dataIn );
    dataIn += sizeof(uint64_t);
    size_t inputSize = le32toh( *(uint32_t*) dataIn );
    dataIn += sizeof(uint64_t);
    /*if ( outputSize < neededOutputSize )
      return false;*/

    outputSize = neededOutputSize;

    availIn -= 2*sizeof( uint64_t );

    // We might not need all of our input data.
    setUnusedInput( availIn - inputSize );
    availIn = inputSize;

    size_t reportedOutputSize = neededOutputSize;
    if ( !doProcessNoSize( dataIn, availIn, dataOut, availOut, reportedOutputSize ) )
      return false;

    CHECK( reportedOutputSize == neededOutputSize,
      "Size of decoded data is different than expected" );

    return true;
  }
};

// encoder for NoStreamAndUnknownSizeDecoder
class NoStreamAndUnknownSizeEncoder : public NoStreamEnDecoder
{
protected:
  // You implement this one:
  virtual bool doProcessNoSize( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize ) =0;


  bool shouldTryWith( const char*, size_t, size_t availOut )
  {
    // If the compression doesn't use any spaces...
    return availOut > sizeof( uint64_t );
  }

  bool isCompleteInput( const char* dataIn, size_t availIn )
  {
    // We cannot know whether the user wants to send more data.
    // -> return false; user must use finish=true to signal end of data
    return false;
  }

  size_t getOverhead()
  {
    return 2*sizeof( uint64_t );
  }

  size_t suggestOutputSize( const char*, size_t availIn )
  {
    // We assume that the compression won't make the data any bigger.
    return availIn + getOverhead();
  }

  bool doProcess( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize )
  {
    CHECK( availIn <= UINT32_MAX,
      "You want to compress more than 4GB of data?! Sorry, we don't support that, yet." );

    memcpy(dataOut, "ABCDEFGHIJKLMNOP", 16);

    // store size
    *(uint32_t*)dataOut = htole32( availIn );
    uint32_t* compressedSize = (uint32_t*) ( dataOut + sizeof( uint64_t ) );

    // compressed data goes after the size
    // We skip more than we actually use; see NoStreamAndUnknownSizeDecoder::doProcess(...).
    dataOut  += getOverhead();
    availOut -= getOverhead();

    if ( !doProcessNoSize( dataIn, availIn, dataOut, availOut, outputSize ) )
      return false;

    CHECK( outputSize <= UINT32_MAX,
      "The compressed data is more than 4GB?! Sorry, we don't support that, yet." );
    *compressedSize = htole32( (uint32_t) outputSize );

    outputSize += getOverhead();

    return true;
  }
};


#ifdef HAVE_LIBLZO

#include <lzo/lzo1x.h>

// finally, we can implement lzo
class LZO1X_1_Decoder : public NoStreamAndUnknownSizeDecoder
{
protected:
  bool doProcessNoSize( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize )
  {
    // same argument is used for available output size and size of decompressed data
    outputSize = availOut;
    int ret = lzo1x_decompress_safe( (const lzo_bytep) dataIn, availIn,
      (lzo_bytep) dataOut, (lzo_uintp) &outputSize, NULL );

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
  static size_t calcMaxCompressedSize(size_t availIn);
public:
  LZO1X_1_Encoder(const LZO1X_1_Compression* compression)
  {
    this->compression = compression;
  }

protected:
  bool doProcessNoSize( const char* dataIn, size_t availIn,
      char* dataOut, size_t availOut, size_t& outputSize );
  bool shouldTryWith( const char*, size_t, size_t availOut );
  size_t suggestOutputSize( const char*, size_t availIn );
};
class LZO1X_1_Compression : public CompressionMethod
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


size_t LZO1X_1_Encoder::calcMaxCompressedSize( size_t availIn )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // The documentation refers to example/simple.c which says:
  // "Because the input block may be incompressible, we must provide a little more
  //  output space in case that compression is not possible."
  // -> We use the same formula.
  return (availIn + availIn / 16 + 64 + 3);
}

bool LZO1X_1_Encoder::shouldTryWith( const char* dataIn, size_t availIn, size_t availOut )
{
  return availOut >= suggestOutputSize( dataIn, availIn );
}

size_t LZO1X_1_Encoder::suggestOutputSize( const char*, size_t availIn )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // The documentation refers to example/simple.c which says:
  // "Because the input block may be incompressible, we must provide a little more
  //  output space in case that compression is not possible."
  // -> We use the same formula.
  return calcMaxCompressedSize( availIn ) + getOverhead();
}

bool LZO1X_1_Encoder::doProcessNoSize( const char* dataIn, size_t availIn,
    char* dataOut, size_t availOut, size_t& outputSize )
{
  // It seems that lzo1x_1_compress does NOT check whether the buffer is big enough.
  // Therefore, we won't try it unless we are sure that the buffer is big enough.
  if ( availOut < calcMaxCompressedSize( availIn ) )
    return false;

  // same argument is used for available output size (haha, see above)
  // and size of decompressed data
  outputSize = availOut;

  lzo_voidp wrkmem = compression->getWorkmem(LZO1X_1_MEM_COMPRESS);
  int ret = lzo1x_1_compress( (const lzo_bytep) dataIn, availIn,
    (lzo_bytep) dataOut, (lzo_uintp) &outputSize, wrkmem );
  compression->giveBackWorkmem(wrkmem);

  if ( ret == LZO_E_OUTPUT_OVERRUN )
    return false;

  CHECK( ret >= LZO_E_OK, "lzo1x_1_compress failed (code %d)", ret );

  return true;
}

#endif  // HAVE_LIBLZO


// register them

static const_sptr<CompressionMethod> const compressions[] = {
  new LZMACompression(),
# ifdef HAVE_LIBLZO
  new LZO1X_1_Compression(),
# endif
  // NULL entry marks end of list. Don't remove it!
  NULL
};

const_sptr<CompressionMethod> CompressionMethod::defaultCompression = compressions[0];

const_sptr<CompressionMethod> CompressionMethod::findCompression( const std::string& name, bool optional )
{
  for ( const const_sptr<CompressionMethod>* c = compressions+0; *c; ++c )
  {
    if ( (*c)->getName() == name )
    {
      return (*c);
    }
  }
  if ( !optional )
  {
    throw exUnsupportedCompressionMethod( name );
  }
  return NULL;
}

// iterator over compressions

CompressionMethod::iterator::iterator( const const_sptr<CompressionMethod>* ptr ) : ptr(   ptr) { }
CompressionMethod::iterator::iterator( const iterator&                 it ) : ptr(it.ptr) { }

CompressionMethod::iterator& CompressionMethod::iterator::operator =( const iterator& it )
{
  this->ptr = it.ptr;
  return *this;
}

bool CompressionMethod::iterator::operator ==( const iterator& other ) const
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
bool CompressionMethod::iterator::operator !=( const iterator& other ) const
{
  return !( *this == other );
}

bool CompressionMethod::iterator::atEnd() const
{
  return !ptr || !*ptr;
}

CompressionMethod::iterator& CompressionMethod::iterator::operator ++()
{
  CHECK( ptr && *ptr, "Cannot increment the end iterator" );

  ++ptr;

  return *this;
}

const_sptr<CompressionMethod> CompressionMethod::iterator::operator *()
{
  CHECK( ptr && *ptr, "Cannot dereference the end iterator" );

  return *ptr;
}

CompressionMethod::iterator CompressionMethod::begin()
{
  return iterator(compressions);
}
CompressionMethod::iterator CompressionMethod::end()
{
  return iterator(NULL);
}

}
