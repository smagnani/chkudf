#if !defined(_LINUX_UDF_FS_I_H)
#define _LINUX_UDF_FS_I_H
/*
 * udf_fs_i.h
 *
 * PURPOSE
 *	UDF specific inode stuff.
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL) version 2.0. Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 */
#include <linux/udf_fs.h>

#define UDF_I(X)	((struct udf_inode_info *)(X)->u.generic_ip)

struct udf_inode_info {
	__u16 i_volume;
};

#endif /* !defined(_LINUX_UDF_FS_I_H) */
