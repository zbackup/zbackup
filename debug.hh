// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef DEBUG_HH_INCLUDED__
#define DEBUG_HH_INCLUDED__

#include <stdio.h>

// Macros we use to output debugging information

#ifndef NDEBUG

#define dPrintf( ... ) (fprintf( stderr, __VA_ARGS__ ))

#else

#define dPrintf( ... )

#endif

extern bool verboseMode;

#define verbosePrintf( ... ) ({ if ( verboseMode ) \
                                  fprintf( stderr, __VA_ARGS__ ); })

#endif
