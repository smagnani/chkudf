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

#define UDF_I_EXT0LOC(X)	( UDF_I(X)->i_ext0Location )
#define UDF_I_EXT0LEN(X)	( UDF_I(X)->i_ext0Length )
#define UDF_I_EXT0OFFS(X)	( UDF_I(X)->i_ext0Offset )
#define UDF_I_LOCATION(X)	( UDF_I(X)->i_location )
#define UDF_I_LENEATTR(X)	( UDF_I(X)->i_lenEAttr )
#define UDF_I_LENALLOC(X)	( UDF_I(X)->i_lenAlloc )
#define UDF_I_UNIQUE(X)		( UDF_I(X)->i_unique )
#define UDF_I_ALLOCTYPE(X)	( UDF_I(X)->i_alloc_type )
#define UDF_I_EXTENDED_FE(X)( UDF_I(X)->i_extended_fe )
#define UDF_I_STRAT4096(X)	( UDF_I(X)->i_strat_4096 )

#endif /* !defined(_LINUX_UDF_I_H) */
