#if !defined(_LINUX_UDF_FS_H)
#define _LINUX_UDF_FS_H
/*
 * udf_fs.h
 *
 * PURPOSE
 *	Global header file for OSTA-UDF(tm) filesystem.
 *
 * DESCRIPTION
 *	This file is included by other header files, so keep it short.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/types.h>
#include <linux/fs.h>

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC	0x15013346

/* Default block size - bigger is better */
#define UDF_BLOCK_SIZE	2048

/* This is completely arbitrary... */
#define UDF_ROOT_INODE	1

/* Prototype for fs/filesystem.c */
extern int init_udf_fs(void);

/* Architecture dependent UDF types */
typedef __u8 dstring;

/* Miscellaneous UDF Prototypes */
extern __u16 udf_crc(__u8 *, __u32);
extern int udf_CS0toUnicode(__u16 *, dstring *, __u32);
extern int udf_UnicodetoCS0(dstring *, __u16 *, __u32);
#ifdef __KERNEL__
extern struct buffer_head *udf_read_tagged(struct super_block *, __u32);
#endif
extern int count_bits(unsigned);

#endif /* !defined(_LINUX_UDF_FS_H) */
