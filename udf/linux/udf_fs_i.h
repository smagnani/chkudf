#if !defined(_LINUX_UDF_FS_I_H)
#define _LINUX_UDF_FS_I_H
/*
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
 */
#include <linux/udf_fs.h>

struct udf_inode_info {
	/* Physical address of inode */
	__u16 i_volume;
	__u16 i_partition;
	__u32 i_block;
	__u32 i_sector;
};

#ifdef CONFIG_UDF
#define UDF_I(X)	(&((X)->u.udf_i))
#else
/* we're not compiled in, so we can't expect our stuff in <linux/fs.h> */
#define UDF_I(X)	( (struct udf_inode_info *) &((X)->u.pipe_i))
#endif

#define UDF_I_VOL(X)	(UDF_I(X)->i_volume)
#define UDF_I_PART(X)	(UDF_I(X)->i_partition)
#define UDF_I_BLOCK(X)	(UDF_I(X)->i_block)
#define UDF_I_SECTOR(X)	(UDF_I(X)->i_sector)

#endif /* !defined(_LINUX_UDF_FS_I_H) */
