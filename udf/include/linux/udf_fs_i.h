#if !defined(_LINUX_UDF_FS_I_H)
#define _LINUX_UDF_FS_I_H
/*
 * udf_fs_i.h
 *
 * This file is intended for the Linux kernel/module. 
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
 */

struct udf_inode_info {
	/* Physical address of inode */
	__u32 i_alloc_type;
		/* next 3 are shortcuts to first extent */
	lb_addr i_ext0Location;	/* partition relative */
	__u32 i_ext0Length;  	/* in blocks */
	__u32 i_ext0Offset;	/* for short directories */
	lb_addr i_location;
	__u32 i_partref;
	__u64 i_fileLength;
	__u64 i_unique;
};

#endif /* !defined(_LINUX_UDF_FS_I_H) */
