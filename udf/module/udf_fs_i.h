#if !defined(_LINUX_UDF_FS_I_H)
#define _LINUX_UDF_FS_I_H

#include <linux/udf_fs.h>

#define UDF_I(X)	(&((X)->u.udf_i))

struct udf_inode_info {
	/* Physical address of inode */
	__u16 i_volume;
	__u16 i_partition;
	__u32 i_block;
	__u32 i_offset;
};

#endif /* !defined(_LINUX_UDF_FS_I_H) */
