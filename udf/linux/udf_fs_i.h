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

#ifdef __KERNEL__

#include <linux/udf_fs.h>

struct udf_inode_info {
	/* Physical address of inode */
	__u32 i_alloc_type;
	__u32 i_ext0Location;	/* partition relative */
	__u32 i_ext0Length;  	/* in blocks */
	__u32 i_ext0Offset;	/* for short directories */
	__u32 i_fileLengthHigh;
	__u32 i_fileLengthLow;
	__u32 i_dir_position;
};

#ifdef CONFIG_UDF
#define UDF_I(X)	(&((X)->u.udf_i))
#else
/* we're not compiled in, so we can't expect our stuff in <linux/fs.h> */
#define UDF_I(X)	( (struct udf_inode_info *) &((X)->u.pipe_i))
	/* god, this is slimy. stealing another filesystem's union area. */
	/* for the record, pipe_i is 9 ints long, we're using 4  	 */
#endif

#define UDF_I_ALLOCTYPE(X)	(UDF_I(X)->i_alloc_type)
#define UDF_I_EXT0LOC(X)	(UDF_I(X)->i_ext0Location)
#define UDF_I_EXT0LEN(X)	(UDF_I(X)->i_ext0Length)
#define UDF_I_EXT0OFFS(X)	(UDF_I(X)->i_ext0Offset)
#define UDF_I_FILELENHIGH(X)	(UDF_I(X)->i_fileLengthHigh)
#define UDF_I_FILELENLOW(X)	(UDF_I(X)->i_fileLengthLow)
#define UDF_I_DIRPOS(X)		(UDF_I(X)->i_dir_position)

#endif /* defined(__KERNEL__) */

#endif /* !defined(_LINUX_UDF_FS_I_H) */
