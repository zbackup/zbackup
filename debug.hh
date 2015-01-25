// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef DEBUG_HH_INCLUDED
#define DEBUG_HH_INCLUDED

#include <stdio.h>
#include <typeinfo>

// Macros we use to output debugging information

#define __CLASS typeid( *this ).name()

#ifndef NDEBUG

#define __FILE_BASE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define dPrintf( ... ) ({ fprintf( stderr, "[DEBUG] at %s( %s:%d ): ", __func__,\
      __FILE_BASE, __LINE__ );\
    fprintf( stderr, __VA_ARGS__ ); })

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>

// TODO: pretty backtraces
#define dPrintBacktrace( ... ) ()
#else
#define dPrintBacktrace( ... ) ()
#endif

#else

#define dPrintf( ... )
#define dPrintBacktrace( ... ) ()

#endif

extern bool verboseMode;

#define verbosePrintf( ... ) ({ if ( verboseMode ) \
                                  fprintf( stderr, __VA_ARGS__ ); })

#endif
