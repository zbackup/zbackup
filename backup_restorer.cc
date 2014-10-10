// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <vector>

#include "backup_restorer.hh"
#include "chunk_id.hh"
#include "message.hh"
#include "zbackup.pb.h"

namespace BackupRestorer {

using std::vector;
using google::protobuf::io::CodedInputStream;

void restore( ChunkStorage::Reader & chunkStorageReader,
              std::string const & backupData,
              DataSink & output )
{
  google::protobuf::io::ArrayInputStream is( backupData.data(),
                                             backupData.size() );
  CodedInputStream cis( &is );
  CodedInputStream::Limit limit = cis.PushLimit( backupData.size() );
  // The following line prevents it from barfing on large backupData.
  // TODO: this disables size checks for each separate message. Figure a better
  // way to do this while keeping them enabled. It seems we need to create an
  // instance of CodedInputStream for each message, but it might be expensive
  cis.SetTotalBytesLimit( backupData.size(), -1 );

  // Used when emitting chunks
  string chunk;

  BackupInstruction instr;
  while ( cis.BytesUntilLimit() > 0 )
  {
    Message::parse( instr, cis );

    if ( instr.has_chunk_to_emit() )
    {
      // Need to emit a chunk, reading it from the store
      size_t chunkSize;
      chunkStorageReader.get( ChunkId( instr.chunk_to_emit() ), chunk,
                              chunkSize );
      output.saveData( chunk.data(), chunkSize );
    }

    if ( instr.has_bytes_to_emit() )
    {
      // Need to emit the bytes directly
      string const & bytes = instr.bytes_to_emit();
      output.saveData( bytes.data(), bytes.size() );
    }
  }

  cis.PopLimit( limit );
}
}
