#if !defined(_LINUX_UDF_FS_SB_H)
#define _LINUX_UDF_FS_SB_H

#include <linux/udf_fs.h>

#define UDF_SB(X)	(&((X)->u.udf_sb))

struct udf_sb_info {
	__u16 s_volumes;
	__u16 s_partitions;

	/* Inode management */
	__u8 *s_imap;
	unsigned s_ifree;
};

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
