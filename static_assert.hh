// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef STATIC_ASSERT_HH_INCLUDED__
#define STATIC_ASSERT_HH_INCLUDED__

// Based on the one from the Boost library. It wouldn't make sense to depend on
// boost just for that

namespace StaticAssert {

template < bool >
struct AssertionFailure;

template <>
struct AssertionFailure< true >
{};

template< int > struct Test
{};
}

#define STATIC_ASSERT( B ) \
  typedef ::StaticAssert::Test< \
    sizeof( ::StaticAssert::AssertionFailure< bool( B ) > ) >\
      static_assert_typedef_ ## __LINE__

#endif
