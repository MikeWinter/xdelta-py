/* 
 * File:   config.h
 * Author: Michael Winter <mail@michael-winter.me.uk>
 *
 * Created on 23 December 2014, 19:00
 */

#ifndef CONFIG_H
#define	CONFIG_H

// Provide 64-bit support for file operations.
#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64

// Enable large file support.
#define XD3_USE_LARGEFILE64 1
// Enable DJW compressor
#define SECONDARY_DJW 1
// Disable the configurable compression algorithm; presets will suffice.
#define XD3_BUILD_SOFT 0
// The library attempts an optimisation during matching by using unaligned, unsigned
// integers for bulk comparison and copying. While this works on some processors,
// including those based on the Intel 32- and 64-bit architectures, it will trap on
// others.
#if defined(__i386) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#   define UNALIGNED_OK 1
#else
#   define UNALIGNED_OK 0
#endif

// The size of `size_t', as computed by sizeof.
#define SIZEOF_SIZE_T 8

#endif	/* CONFIG_H */
