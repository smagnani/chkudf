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
#include <linux/udf_fs.h>


struct udf_sb_info {
	/* Default permissions */
	mode_t s_mode;
	gid_t s_gid;
	uid_t s_uid;

	/* Overall info */
	__u16 s_maxVolSeqNum;
	__u16 s_partitions;

	/* Volume ID */
	__u32 s_id_block;
	__u16 s_id_crc;

	/* Block headers */
	__u32 s_anchor;
	__u32 s_lastblock;
	__u32 s_voldesc;
	__u32 s_fileset;

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
#define UDF_SB_LASTBLOCK(X)	( UDF_SB(X)->s_lastblock )
#define UDF_SB_VOLDESC(X)	( UDF_SB(X)->s_voldesc )
#define UDF_SB_FILESET(X)	( UDF_SB(X)->s_fileset )

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
