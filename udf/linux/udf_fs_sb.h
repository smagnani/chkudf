#if !defined(_LINUX_UDF_FS_SB_H)
#define _LINUX_UDF_FS_SB_H
/*
 * udf_fs_sb.h
 *
 * PURPOSE
 *	UDF specific super block stuff.
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

#define UDF_FLAG_STRICT	0x00000001U
#define UDF_FLAG_FIXED	0x00000004U

#define IS_STRICT(X)	((X)->u.udf_sb.s_flags & UDF_FLAG_STRICT)
#define IS_FIXED(X)	((X)->u.udf_sb.s_flags & UDF_FLAG_DEBUG)

#define UDF_SB(X)	((struct udf_sb_fs *)(X)->u.generic_sbp)

struct udf_sb_info {
	/* Inode numbering - partition in high bits, block in low bits */
	unsigned s_block_mask;
	unsigned s_block_bits;

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

	/* Miscellaneous flags */
	int s_flags;

	/* VAT data */
	struct udfvat *s_vat;
};

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
