// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include "compression.hh"
#include "check.hh"

EnDecoder::EnDecoder() { }
EnDecoder::EnDecoder(const EnDecoder&) { }
EnDecoder::~EnDecoder() {}

Compression::~Compression() {}


// LZMA

#include <lzma.h>

class LZMAEnDecoder : public EnDecoder {
protected:
  static lzma_stream init_value;
  lzma_stream strm;
public:
  LZMAEnDecoder() {
    strm = init_value;
  }

  void setInput(const void* data, size_t size) {
    strm.next_in  = (const uint8_t *) data;
    strm.avail_in = size;
  }

  void setOutput(void* data, size_t size) {
    strm.next_out  = (uint8_t *) data;
    strm.avail_out = size;
  }

  size_t getAvailableInput() {
    return strm.avail_in;
  }

  size_t getAvailableOutput() {
    return strm.avail_out;
  }

  bool process(bool finish) {
    lzma_ret ret = lzma_code( &strm, ( finish ? LZMA_FINISH : LZMA_RUN ) );

    CHECK( ret == LZMA_OK || ret == LZMA_STREAM_END, "lzma_code error: %d", (int) ret );

    return (ret == LZMA_STREAM_END);
  }
};
lzma_stream LZMAEnDecoder::init_value = LZMA_STREAM_INIT;

class LZMAEncoder : public LZMAEnDecoder {
public:
  LZMAEncoder() {
    uint32_t preset = 6; // TODO: make this customizable, although 6 seems to be
                         // the best option
    lzma_ret ret = lzma_easy_encoder( &strm, preset, LZMA_CHECK_CRC64 );
    CHECK( ret == LZMA_OK, "lzma_easy_encoder error: %d", (int) ret );
  }
};

class LZMADecoder : public LZMAEnDecoder {
public:
  LZMADecoder() {
    lzma_ret ret = lzma_stream_decoder( &strm, UINT64_MAX, 0 );
    CHECK( ret == LZMA_OK,"lzma_stream_decoder error: %d", (int) ret );
  }
};

class LZMACompression : public Compression {
public:
  EnDecoder* getEncoder() const {
    return new LZMAEncoder();
  }

  EnDecoder* getDecoder() const {
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
class NoStreamEnDecoder : public EnDecoder {
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

  // Process the data in data_in and put the result into data_out. You musn't
  // write more than avail_out bytes! If the output buffer is big enough,
  // process the data and store the output size in output_size. If the output
  // buffer is too small, return false and we will give you a bigger one. If
  // any other error occurrs, abort the program. We don't have any better
  // error handling. Sorry. Do NOT return false for errors that won't be
  // remedied by a bigger buffer!
  virtual bool do_process( const char* data_in, size_t avail_in,
    char* data_out, size_t avail_out, size_t& output_size ) =0;
public:
  NoStreamEnDecoder() {
    data_in = data_out = NULL;
    avail_in = avail_out = pos_in_acc_data_out = 0;
    processed = false;
  }

  void setInput(const void* data, size_t size) {
    data_in  = (const char *) data;
    avail_in = size;
  }

  void setOutput(void* data, size_t size) {
    data_out  = (char *) data;
    avail_out = size;
  }

  size_t getAvailableInput() {
    return avail_in;
  }

  size_t getAvailableOutput() {
    return avail_out;
  }

  bool process(bool finish) {
    if ( processed ) {
      // data has been encoded or decoded, remaining output is in acc_data_out
      // -> copy to output
      if (avail_out > 0 && acc_data_out.size() - pos_in_acc_data_out > 0) {
        size_t sz = avail_out;
        if ( sz > acc_data_out.size() - pos_in_acc_data_out )
          sz = acc_data_out.size() - pos_in_acc_data_out;

        memcpy( data_out, acc_data_out.c_str(), sz );
        data_out  += sz;
        avail_out -= sz;
        pos_in_acc_data_out += sz;

        // no more data left? -> return true
        return ( acc_data_out.size() - pos_in_acc_data_out == 0 );
      }
    } else {
      // data has not been encoded
      if ( finish && acc_data_in.empty() ) {
        // special case: all the data has been passed at once
        // -> process it without using acc_data_in
        process_finish( data_in, avail_in );
        return true;
      } else {
        // accumulate data in acc_data_in
        acc_data_in.append( data_in, avail_in );

        // If this was the last bit of data, we process it, now.
        if ( finish ) {
          process_finish( acc_data_in.c_str(), acc_data_in.size() );
          return true;
        } else
          return false;
      }
    }
  }

private:
  void process_finish(const char* data_in, size_t avail_in) {
    // should we try with the existing output buffer?
    if ( shouldTryWith( data_in, avail_in, avail_out ) ) {
      if ( do_process( data_in, avail_in, data_out, avail_out, avail_out ) ) {
        // it worked :-)
        processed = true;
        return ;
      }
    }

    // we use our own buffer
    size_t buffer_size = suggestOutputSize( data_in, avail_in );
    do {
      acc_data_out.resize(buffer_size);

      size_t output_size;
      //TODO doc says we mustn't modify the pointer returned by data()...
      if ( do_process( data_in, avail_in, (char*) acc_data_out.data(), avail_out, output_size ) ) {
        // buffer is big enough
        acc_data_out.resize( output_size );
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
class NoStreamAndUnknownSizeDecoder : public NoStreamEnDecoder {
protected:
  // You implement this one:
  // If you don't know the real decoded size, don't change output_size.
  virtual bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) =0;


  bool shouldTryWith( const char* data_in, size_t avail_in, size_t avail_out ) {
    return suggestOutputSize( data_in, avail_in ) <= avail_out;
  }

  size_t suggestOutputSize( const char* data_in, size_t avail_in ) {
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
      char* data_out, size_t avail_out, size_t& output_size ) {
    size_t needed_output_size = le32toh( *(uint32_t*) data_in );
    if ( output_size < needed_output_size )
      return false;

    //NOTE We skip 8 bytes. If we later decide to drop compatibility with 32-bit
    //     hosts, we can save a 64-bit size. Well, that will be much later, when
    //     we can easily hold two copies of a 4GB file in main memory :-D
    data_in += sizeof( uint64_t );

    size_t reported_output_size = needed_output_size;
    if ( !do_process_no_size( data_in, avail_in, data_out, avail_out, reported_output_size ) )
      return false;

    CHECK( reported_output_size == needed_output_size, "Size of decoded data is different than expected" );

    output_size = needed_output_size;

    return true;
  }
};

// encoder for NoStreamAndUnknownSizeDecoder
class NoStreamAndUnknownSizeEncoder : public NoStreamEnDecoder {
protected:
  // You implement this one:
  virtual bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) =0;


  bool shouldTryWith( const char* data_in, size_t avail_in, size_t avail_out ) {
    // If the compression doesn't use any spaces...
    return avail_out > sizeof( uint64_t );
  }

  size_t suggestOutputSize( const char* data_in, size_t avail_in ) {
    // We assume that the compression won't make the data any bigger.
    return avail_in + sizeof( uint64_t );
  }

  bool do_process( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) {
    CHECK( avail_in <= UINT32_MAX, "You want to compress more than 4GB of data?! Sorry, we don't support that, yet." );

    // store size
    *(uint32_t*)data_in = htole32( avail_in );

    // compressed data goes after the size
    // We skip more than we actually use; see NoStreamAndUnknownSizeDecoder::do_process(...).
    data_in += sizeof( uint64_t );

    if ( !do_process_no_size( data_in, avail_in, data_out, avail_out, output_size ) )
      return false;

    return true;
  }
};


#ifdef HAVE_LIBLZO

#include <lzo/lzo1x.h>

// finally, we can implement lzo
class LZO1X_1_Decoder : public NoStreamAndUnknownSizeDecoder {
protected:
  // You implement this one:
  bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size ) {
    // same argument is used for available output size and size of decompressed data
    output_size = avail_out;
    int ret = lzo1x_decompress_safe( (const lzo_bytep) data_in, avail_in,
      (lzo_bytep) data_out, &output_size, NULL );
    //TODO look up exit codes
    CHECK( ret == 0, "lzo1x_decompress_safe probably failed" );
  }
};
class LZO1X_1_Compression;
class LZO1X_1_Encoder : public NoStreamAndUnknownSizeEncoder {
  const LZO1X_1_Compression* compression;
public:
  LZO1X_1_Encoder(const LZO1X_1_Compression* compression) {
    this->compression = compression;
  }
protected:
  // You implement this one:
  bool do_process_no_size( const char* data_in, size_t avail_in,
      char* data_out, size_t avail_out, size_t& output_size );
};
class LZO1X_1_Compression : public Compression {
public:
  EnDecoder* getEncoder() const {
    return new LZO1X_1_Encoder(this);
  }

  EnDecoder* getDecoder() const {
    return new LZO1X_1_Decoder();
  }

  std::string getName() const { return "lzo1x_1"; }


  lzo_voidp getWorkmem(size_t size) const {
    return new char[size];
  }

  void giveBackWorkmem(lzo_voidp wrkmem) const {
    //TODO I think we should keep the memory around and reuse it. After all
    //     it is only a few kilobytes and we will need it a lot. However, I
    //     won't risk anything here because I don't know whether this will be
    //     called by more than one thread.
    delete[] (char*)wrkmem;
  }
};
bool LZO1X_1_Encoder::do_process_no_size( const char* data_in, size_t avail_in,
    char* data_out, size_t avail_out, size_t& output_size ) {
  // same argument is used for available output size and size of decompressed data
  output_size = avail_out;

  lzo_voidp wrkmem = compression->getWorkmem(LZO1X_1_MEM_COMPRESS);
  int ret = lzo1x_1_compress( (const lzo_bytep) data_in, avail_in,
    (lzo_bytep) data_out, &output_size, wrkmem );
  compression->giveBackWorkmem(wrkmem);
  //TODO look up exit codes
  CHECK( ret == 0, "lzo1x_decompress_safe probably failed" );
}

#endif  // HAVE_LIBLZO


// register them

static const Compression* compressions[] = {
  new LZMACompression(),
# ifdef HAVE_LIBLZO
  new LZO1X_1_Compression(),
# endif
  // NULL entry marks end of list. Don't remove it!
  NULL
};

const Compression* Compression::default_compression = compressions[0];

const Compression* Compression::findCompression( const std::string& name, bool optional ) {
  for (const Compression** c = compressions+0; *c; ++c) {
    if ( (*c)->getName() == name ) {
      return (*c);
    }
  }
  CHECK( !optional, "Couldn't find compression method '%s'", name.c_str() );
  return NULL;
}
