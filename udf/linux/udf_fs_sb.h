#if !defined(_LINUX_UDF_FS_SB_H)
#define _LINUX_UDF_FS_SB_H
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
#include <linux/time.h>


struct udf_sb_info {
	/* Default permissions */
	mode_t s_mode;
	gid_t s_gid;
	uid_t s_uid;

	/* Overall info */
	__u16 s_maxVolSeqNum;
	__u16 s_partitions;
	__u16 s_thispartition;
	__u16 s_thisvolume;

	/* Volume ID */
	__u32 s_id_block;
	__u16 s_id_crc;

	/* Block headers */
	__u32 s_anchor;
	__u32 s_lastblock;
	__u32 s_voldesc;
	__u32 s_fileset;
	__u32 s_rootdir;
	__u32 s_partition_root;
	__u32 s_partition_len;
	__u32 s_filecount;

	/* Root Info */
	time_t s_recordtime;

	/* Miscellaneous flags */
	int s_flags;

	/* Debugging level */
	int s_debug;
};

#define UDF_FLAG_STRICT	0x00000001U
#define UDF_FLAG_FIXED	0x00000004U

#ifdef CONFIG_UDF
#define UDF_SB_ALLOC(X) 
#define UDF_SB_FREE(X)  
#define UDF_SB(X)	(&((X)->u.udf_sb))
#else
#define UDF_SB_ALLOC(X) \
	((X)->u.generic_sbp=kmalloc(sizeof(struct udf_sb_info), GFP_KERNEL))
#define UDF_SB_FREE(X)  { if ((X)->u.generic_sbp) { \
				kfree( (X)->u.generic_sbp ); \
				(X)->u.generic_sbp=NULL; \
			  } }
#define UDF_SB(X)	((struct udf_sb_info *) ((X)->u.generic_sbp))
#endif

#define IS_STRICT(X)	( UDF_SB(X)->s_flags & UDF_FLAG_STRICT)
#define IS_FIXED(X)	( UDF_SB(X)->s_flags & UDF_FLAG_DEBUG)

#define UDF_SB_ANCHOR(X)	( UDF_SB(X)->s_anchor )
#define UDF_SB_VOLUME(X)	( UDF_SB(X)->s_thisvolume )
#define UDF_SB_PARTITION(X)	( UDF_SB(X)->s_thispartition )
#define UDF_SB_LASTBLOCK(X)	( UDF_SB(X)->s_lastblock )
#define UDF_SB_VOLDESC(X)	( UDF_SB(X)->s_voldesc )
#define UDF_SB_FILESET(X)	( UDF_SB(X)->s_fileset )
#define UDF_SB_ROOTDIR(X)	( UDF_SB(X)->s_rootdir )
#define UDF_SB_RECORDTIME(X)	( UDF_SB(X)->s_recordtime )
#define UDF_SB_PARTROOT(X)	( UDF_SB(X)->s_partition_root )
#define UDF_SB_PARTLEN(X)	( UDF_SB(X)->s_partition_len )
#define UDF_SB_FILECOUNT(X)	( UDF_SB(X)->s_filecount )

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
