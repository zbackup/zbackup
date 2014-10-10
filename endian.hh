// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef ENDIAN_HH_INCLUDED__
#define ENDIAN_HH_INCLUDED__

#include <stdint.h>
#include <arpa/inet.h>
#ifdef __APPLE__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN

/// Converts the given host-order value to big-endian value
inline uint32_t toBigEndian( uint32_t v ) { return htonl( v ); }
/// Converts the given host-order value to little-endian value
inline uint32_t toLittleEndian( uint32_t v ) { return v; }
inline uint64_t toLittleEndian( uint64_t v ) { return v; }
/// Converts the given little-endian value to host-order value
inline uint32_t fromLittleEndian( uint32_t v ) { return v; }
inline uint64_t fromLittleEndian( uint64_t v ) { return v; }

#elif __BYTE_ORDER == __BIG_ENDIAN

// Note: the functions used are non-standard. Add more ifdefs if needed

/// Converts the given host-order value to big-endian value
inline uint32_t toBigEndian( uint32_t v ) { return v; }
/// Converts the given host-order value to little-endian value
inline uint32_t toLittleEndian( uint32_t v ) { return htole32( v ); }
inline uint64_t toLittleEndian( uint64_t v ) { return htole64( v ); }
/// Converts the given little-endian value to host-order value
inline uint32_t fromLittleEndian( uint32_t v ) { return le32toh( v ); }
inline uint64_t fromLittleEndian( uint64_t v ) { return le64toh( v ); }

#else

#error Please add support for architectures different from little-endian and\
 big-endian.

#endif

#endif
