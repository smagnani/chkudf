/*
 * udf_fs_i.h
 *
 * This file is intended for the Linux kernel/module. 
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 */

#ifndef _UDF_FS_I_H
#define _UDF_FS_I_H 1

#ifdef __KERNEL__

#ifndef _ECMA_167_H
typedef struct
{
	__u32			logicalBlockNum;
	__u16			partitionReferenceNum;
} __attribute__ ((packed)) lb_addr;

typedef struct
{
	__u32			extLength;
	__u32			extPosition;
} __attribute__ ((packed)) short_ad;

typedef struct
{
	__u32			extLength;
	lb_addr			extLocation;
	__u8			impUse[6];
} __attribute__ ((packed)) long_ad;
#endif

struct udf_inode_info
{
	long			i_umtime;
	long			i_uctime;
	long			i_crtime;
	long			i_ucrtime;
	/* Physical address of inode */
	lb_addr			i_location;
	__u64			i_unique;
	__u32			i_lenEAttr;
	__u32			i_lenAlloc;
	__u64			i_lenExtents;
	__u32			i_next_alloc_block;
	__u32			i_next_alloc_goal;
	unsigned		i_alloc_type : 3;
	unsigned		i_extended_fe : 1;
	unsigned		i_strat_4096 : 1;
	unsigned		i_new_inode : 1;
	unsigned		reserved : 26;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	struct inode vfs_inode;
	union
	{
		short_ad	i_sad[0];
		long_ad		i_lad[0];
		__u8		i_data[0];
	} i_ext;
#else
	union
	{
		short_ad	*i_sad;
		long_ad		*i_lad;
		__u8		*i_data;
	} i_ext;
#endif
};

#endif

/* exported IOCTLs, we have 'l', 0x40-0x7f */

#define UDF_GETEASIZE   _IOR('l', 0x40, int)
#define UDF_GETEABLOCK  _IOR('l', 0x41, void *)
#define UDF_GETVOLIDENT _IOR('l', 0x42, void *)
#define UDF_RELOCATE_BLOCKS _IOWR('l', 0x43, long)

#endif /* _UDF_FS_I_H */
