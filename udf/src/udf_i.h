#ifndef __LINUX_UDF_I_H
#define __LINUX_UDF_I_H

#ifndef CONFIG_UDF_FS_EXT

#define UDF_I(X)	(&((X)->u.udf_i))

#else
/* we're not compiled in, so we can't expect our stuff in <linux/fs.h> */
#define UDF_I(X)	( (struct udf_inode_info *) &((X)->u.pipe_i))
	/* god, this is slimy. stealing another filesystem's union area. */
	/* for the record, pipe_i is 9 ints long, we're using 9  	 */
#endif

#define UDF_I_ALLOCTYPE(X)	( UDF_I(X)->i_alloc_type )
#define UDF_I_EXT0LOC(X)	( UDF_I(X)->i_ext0Location )
#define UDF_I_EXT0LEN(X)	( UDF_I(X)->i_ext0Length )
#define UDF_I_EXT0OFFS(X)	( UDF_I(X)->i_ext0Offset )
#define UDF_I_PARTREF(X)	( UDF_I(X)->i_partref )
#define UDF_I_FILELEN(X)	( UDF_I(X)->i_fileLength )
#define UDF_I_LOCATION(X)	( UDF_I(X)->i_location )

#endif /* !defined(_LINUX_UDF_I_H) */
