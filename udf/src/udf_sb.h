#ifndef __LINUX_UDF_SB_H
#define __LINUX_UDF_SB_H

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC	0x15013346

/* Default block size - bigger is better */
#define UDF_BLOCK_SIZE	2048

#define UDF_NAME_PAD 4

#define UDF_FLAG_STRICT	0x00000001U
#define UDF_FLAG_FIXED	0x00000004U

/* following is set only when compiling outside kernel tree */
#ifndef CONFIG_UDF_FS_EXT

#define UDF_SB_ALLOC(X) 
#define UDF_SB_FREE(X)  
#define UDF_SB(X)	(&((X)->u.udf_sb))

#else
	/* else we kmalloc a page to hold our stuff */
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

#define UDF_SB_SESSION(X)	( UDF_SB(X)->s_session )
#define UDF_SB_ANCHOR(X)	( UDF_SB(X)->s_anchor )
#define UDF_SB_VOLUME(X)	( UDF_SB(X)->s_thisvolume )
#define UDF_SB_PARTITION(X)	( UDF_SB(X)->s_thispartition )
#define UDF_SB_LASTBLOCK(X)	( UDF_SB(X)->s_lastblock )
#define UDF_SB_BSIZE(X)		( UDF_SB(X)->s_blocksize )
#define UDF_SB_VOLDESC(X)	( UDF_SB(X)->s_voldesc )
#define UDF_SB_FILESET(X)	( UDF_SB(X)->s_fileset )
#define UDF_SB_ROOTDIR(X)	( UDF_SB(X)->s_rootdir )
#define UDF_SB_RECORDTIME(X)	( UDF_SB(X)->s_recordtime )
#define UDF_SB_TIMESTAMP(X)	( (timestamp *)UDF_SB(X)->s_timestamp )
#define UDF_SB_PARTROOT(X)	( UDF_SB(X)->s_partition_root )
#define UDF_SB_PARTLEN(X)	( UDF_SB(X)->s_partition_len )
#define UDF_SB_FILECOUNT(X)	( UDF_SB(X)->s_filecount )
#define UDF_SB_VOLIDENT(X)	( UDF_SB(X)->s_volident )
#define UDF_SB_LASTDIRINO(X)	( UDF_SB(X)->s_lastdirino )
#define UDF_SB_LASTDIRNUM(X)	( UDF_SB(X)->s_lastdirnum )

#endif
