// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef CHECK_HH_INCLUDED__
#define CHECK_HH_INCLUDED__

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

// Run-time assertion macro

// Usage: CHECK( value == 16, "Value is not 16: %d", value );
// This will abort() if the value is not 16 with the message stating so.

// TODO: show the backtrace here, without using __FILE__ __LINE__

#define CHECK( condition, message, ... ) ({if (!(condition)) \
{ \
  fprintf( stderr, "Check failed: " ); \
  fprintf( stderr, message, ##__VA_ARGS__ ); \
  fprintf( stderr, "\nAt %s:%d\n", __FILE__, __LINE__ ); \
  abort(); \
}})

#define FAIL( ... ) CHECK( false, __VA_ARGS__ )


// Debug-only versions. Only instantiated in debug builds
#ifndef NDEBUG
#define DCHECK CHECK
#define DFAIL FAIL
#else
#define DCHECK( ... )
#define DFAIL( ... )
#endif

#endif
