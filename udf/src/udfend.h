#ifndef __UDF_ENDIAN_H
#define __UDF_ENDIAN_H

#if __BYTE_ORDER == 0

#error "__BYTE_ORDER must be defined"

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16_to_cpu(x) \
	((Uint16)((((Uint16)(x) & 0x00FFU) << 8) | \
		  (((Uint16)(x) & 0xFF00U) >> 8)))
 
#define le32_to_cpu(x) \
	((Uint32)((((Uint32)(x) & 0x000000FFU) << 24) | \
		  (((Uint32)(x) & 0x0000FF00U) <<  8) | \
		  (((Uint32)(x) & 0x00FF0000U) >>  8) | \
		  (((Uint32)(x) & 0xFF000000U) >> 24)))

#define le64_to_cpu(x) \
	((Uint64)((((Uint64)(x) & 0x00000000000000FFULL) << 56) | \
		  (((Uint64)(x) & 0x000000000000FF00ULL) << 40) | \
		  (((Uint64)(x) & 0x0000000000FF0000ULL) << 24) | \
		  (((Uint64)(x) & 0x00000000FF000000ULL) <<  8) | \
		  (((Uint64)(x) & 0x000000FF00000000ULL) >>  8) | \
		  (((Uint64)(x) & 0x0000FF0000000000ULL) >> 24) | \
		  (((Uint64)(x) & 0x00FF000000000000ULL) >> 40) | \
		  (((Uint64)(x) & 0xFF00000000000000ULL) >> 56)))		

#define fstohs(x) (htofss(x))
#define fstohl(x) (htofsl(x))
#define fstohll(x) (htofsll(x))

#else /* __BYTE_ORDER == __LITTLE_ENDIAN */

#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#define fstohs(x) (x)
#define fstohl(x) (x)
#define fstohll(x) (x)

#endif

#endif /* __UDF_ENDIAN_H */
