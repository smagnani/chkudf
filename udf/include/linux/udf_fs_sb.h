#if !defined(_LINUX_UDF_FS_SB_H)
#define _LINUX_UDF_FS_SB_H
/*
 * udf_fs_sb.h
 * 
 * This include file is for the Linux kernel/module.
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

#define UDF_SB_APPROX_SIZE	40

#if !defined(CONFIG_UDF_FS)

/* make things easier for the kernel when we're not compiled in ... */
struct udf_sb_info {
	__u32 reserved[UDF_SB_APPROX_SIZE]; 
			/* ... but leave enough room for the module! */
};

#else

#pragma pack(1)

#define UDF_SB_PARTITIONS	4

struct udf_partition {
	__u16 p_reference;
	__u16 p_type; /* 0 'unused', 1 'type 1', 2 'virtual', 3 'sparable' */
	__u32 p_sector; /* sector offset for 1, VAT for 2, SpareTab for 3 */
};

struct udf_sb_info {
	struct udf_partition s_partmap[UDF_SB_PARTITIONS];
	__u8  s_volident[32];
	__u64 s_nextid;

	/* Default permissions */
	mode_t s_mode;
	gid_t s_gid;
	uid_t s_uid;

	/* Overall info */
	__u16 s_blocksize; 
	__u16 s_maxVolSeqNum;
	__u16 s_partitions;
	__u32 s_thispartition;

	/* Volume ID */
	__u32 s_id_block;
	__u16 s_id_crc;

	/* Sector headers */
	__u32 s_session;
	__u32 s_anchor;
	__u32 s_lastblock;
	__u32 s_voldesc;
	__u32 s_fileset;
	__u32 s_rootdir;
	__u32 s_partition_root;
	__u32 s_partition_len;

	__u32 s_filecount;


	/* directory info */
	__u32 s_lastdirino;
	__u32 s_lastdirnum;

	/* Root Info */
	time_t s_recordtime;
	__u8  s_timestamp[12];

	/* Miscellaneous flags */
	int s_flags;

	/* Debugging level */
	int s_debug;
};
#pragma pack()

#endif  /* !defined(CONFIG_UDF_FS) */

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
