// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef EX_HH_INCLUDED__
#define EX_HH_INCLUDED__

#include <exception>
#include <stdio.h>
#include <string>

/// A way to declare an exception class fast
/// Do like this:
/// DEF_EX( exErrorInFoo, "An error in foo encountered", std::exception )
/// DEF_EX( exFooNotFound, "Foo was not found", exErrorInFoo )

#define DEF_EX( exName, exDescription, exParent ) \
class exName: public exParent { \
public: \
virtual const char * what() const throw() { return (exDescription); } \
virtual ~exName() throw() {} };

/// Same as DEF_EX, but takes a runtime string argument, which gets concatenated
/// with the description.
/// 
///   DEF_EX_STR( exCantOpen, "can't open file", std::exception )
///   ...
///   throw exCantOpen( "example.txt" );
///
///   what() would return "can't open file example.txt"

#define DEF_EX_STR( exName, exDescription, exParent ) \
class exName: public exParent { \
  std::string value; \
public: \
  exName( std::string const & value_ ): value( std::string( exDescription ) + " " + value_ ) {} \
  exName( char const * value_, unsigned size ): value( std::string( exDescription ) + " " + std::string( value_, size ) ) {} \
virtual const char * what() const throw() { return value.c_str(); } \
virtual ~exName() throw() {} };

/// An exception class to wrap leave code into an std::exception
class exLeaveWrapped: public std::exception
{
  char buf[ 32 ];

public:

  exLeaveWrapped( int error )
  { sprintf( buf, "%d", error ); }

  char const * what() const throw()
  { return buf; }
};

#endif
