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
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/types.h>
#include <linux/fs.h>

#include <linux/udf_udf.h>
#include <linux/udf_167.h>
#include <linux/udf_fs_sb.h>
#include <linux/udf_fs_i.h>

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC	0x15013346

/* Default block size - bigger is better */
#define UDF_BLOCK_SIZE	2048

/* This is completely arbitrary... */
#define UDF_ROOT_INODE	1

#define UDF_PRINTK(X,Y)	do { if(UDF_DEBUG(sb)) printk(Y); } while (0)
#define UDF_CRUMB(X)	do { if(UDF_DEBUG(sb)) printk(KERN_DEBUG "udf: %s (%d)\n", __FILE__, __LINE__); } while (0)
#define UDF_DUMP(X,Y)	do { if (UDF_DEBUG(sb)) udf_debug_dump(Y); } while (0)

/* Prototype for fs/filesystem.c */
extern int init_udf_fs(void);

struct ustr {
	__u8 u_cmpID;
	__u8 u_name[UDF_NAME_LEN];
	__u8 u_len;
	__u8 padding;
	unsigned long u_hash;
};

/* Miscellaneous UDF Prototypes */
extern void udf_debug_dump(struct buffer_head *);
extern __u16 udf_crc(__u8 *, __u32);
extern int udf_CS0toUTF8(struct ustr *, struct ustr *);
extern int udf_UTF8toCS0(struct ustr *, struct ustr *);
extern struct buffer_head *udf_read_tagged(struct super_block *, __u32, __u32);
extern time_t * udf_stamp_to_time(time_t *, void *);

#endif /* !defined(_LINUX_UDF_FS_H) */
