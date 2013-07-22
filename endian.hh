// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#ifndef ENDIAN_HH_INCLUDED__
#define ENDIAN_HH_INCLUDED__

#include <stdint.h>
#include <arpa/inet.h>
#ifdef __APPLE__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error Please add support for architectures different from little-endian.
#endif

/// Converts the given host-order value to big-endian value
inline uint32_t toBigEndian( uint32_t v ) { return htonl( v ); }
/// Converts the given host-order value to little-endian value
inline uint32_t toLittleEndian( uint32_t v ) { return v; }
inline uint64_t toLittleEndian( uint64_t v ) { return v; }
/// Converts the given little-endian value to host-order value
inline uint32_t fromLittleEndian( uint32_t v ) { return v; }
inline uint64_t fromLittleEndian( uint64_t v ) { return v; }

#endif
