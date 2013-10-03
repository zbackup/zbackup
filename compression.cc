// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include <lzma.h>

#include "compression.hh"
#include "check.hh"

EnDecoder::EnDecoder() { }
EnDecoder::EnDecoder(const EnDecoder&) { }
EnDecoder::~EnDecoder() {}

Compression::~Compression() {}


// LZMA

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


// LZOP

//TODO


// register them

static const Compression* compressions[] = {
  new LZMACompression(),
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
