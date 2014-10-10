// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef MESSAGE_HH_INCLUDED__
#define MESSAGE_HH_INCLUDED__

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message_lite.h>
#include <exception>

#include "ex.hh"

/// Some utilities for protobuffer messages
namespace Message {

DEF_EX( Ex, "Message exception", std::exception )
DEF_EX_STR( exCantParse, "Can't parse message", Ex )
DEF_EX_STR( exCantSerialize, "Can't serialize message", Ex )

using google::protobuf::io::ZeroCopyOutputStream;
using google::protobuf::io::ZeroCopyInputStream;
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::CodedOutputStream;
using google::protobuf::MessageLite;

/// Serializes the given message to the given zero-copy stream
void serialize( MessageLite const &, ZeroCopyOutputStream & );

/// Serializes the given message to the given coded stream
void serialize( MessageLite const &, CodedOutputStream & );

/// Reads and parses the given message from the given zero-copy stream
void parse( MessageLite &, ZeroCopyInputStream & );

/// Reads and parses the given message from the given coded stream
void parse( MessageLite &, CodedInputStream & );
}

#endif
