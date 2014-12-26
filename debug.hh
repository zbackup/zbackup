// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef DEBUG_HH_INCLUDED__
#define DEBUG_HH_INCLUDED__

#include <stdio.h>

// Macros we use to output debugging information

#ifndef NDEBUG

#define __FILE_BASE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define dPrintf( ... ) ({ fprintf( stderr, "[DEBUG] at %s( %s:%d ): ", __func__,\
      __FILE_BASE, __LINE__ );\
    fprintf( stderr, __VA_ARGS__ ); })

#else

#define dPrintf( ... )

#endif

extern bool verboseMode;

#define verbosePrintf( ... ) ({ if ( verboseMode ) \
                                  fprintf( stderr, __VA_ARGS__ ); })

#endif
