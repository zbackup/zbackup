// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "message.hh"

#include <stdint.h>

namespace Message {

void serialize( MessageLite const & message, ZeroCopyOutputStream & stream )
{
  CodedOutputStream cos( &stream );
  serialize( message, cos );
}

void serialize( MessageLite const & message, CodedOutputStream & cos )
{
  cos.WriteVarint32( message.ByteSize() );
  message.SerializeWithCachedSizes( &cos );
  if ( cos.HadError() )
    throw exCantSerialize( message.GetTypeName() );
}

void parse( MessageLite & message, ZeroCopyInputStream & stream )
{
  CodedInputStream cis( &stream );
  parse( message, cis );
}

void parse( MessageLite & message, CodedInputStream & cis )
{
  uint32_t v;
  if ( !cis.ReadVarint32( &v ) )
    throw exCantParse( message.GetTypeName() );

  CodedInputStream::Limit limit = cis.PushLimit( v );

  if( !message.ParseFromCodedStream( &cis ) )
    throw exCantParse( message.GetTypeName() );

  cis.PopLimit( limit );
}

}
