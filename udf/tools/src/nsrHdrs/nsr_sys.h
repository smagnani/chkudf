#include <stdbool.h>
#include <stdint.h>

// Uncomment one of the following:


// #define WIN32
// #define OS2
#define LINUX
// #define SOLARIS

/* 
 *  The following adjust for byte order on various machines and interfaces.
 *  All structures in UDF are little endian, though the compressed unicode
 *  algorithm makes 16 bit values appear to be big endian.  The Following
 *  defines determine whether to swap the bytes or not for values read from
 *  S_: the SCSI interface
 *  U_: UDF structures.
 */

#ifdef LINUX
#include <endian.h>
#endif

#ifdef SOLARIS
#define __BYTE_ORDER 4321
#endif

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN

/* For little endian machines */
#define S_endian32(x) endian32(x)
#define S_endian16(x) endian16(x)
#define U_endian32(x) (x)
#define U_endian16(x) (x)

#else /* __BYTE_ORDER == __BIG_ENDIAN */

/* For big endian machines */

#define S_endian32(x) (x)
#define S_endian16(x) (x)
#define U_endian32(x) endian32(x)
#define U_endian16(x) endian16(x)

#endif
