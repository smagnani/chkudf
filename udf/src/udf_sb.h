#ifndef __LINUX_UDF_SB_H
#define __LINUX_UDF_SB_H

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC	0x15013346

/* Default block size - bigger is better */
#define UDF_BLOCK_SIZE	2048

#define UDF_NAME_PAD 4

#define UDF_FLAG_STRICT		0x00000001U
#define UDF_FLAG_UNDELETE	0x00000002U
#define UDF_FLAG_UNHIDE		0x00000004U

/* following is set only when compiling outside kernel tree */
#ifndef CONFIG_UDF_FS_EXT

#define UDF_SB_ALLOC(X)
#define UDF_SB_FREE(X)\
{\
	if (UDF_SB(X))\
	{\
		if (UDF_SB_PARTMAPS(X))\
			kfree(UDF_SB_PARTMAPS(X));\
		UDF_SB_PARTMAPS(X) = NULL;\
	}\
}
#define UDF_SB(X)	(&((X)->u.udf_sb))

#else
	/* else we kmalloc a page to hold our stuff */
#define UDF_SB_ALLOC(X)\
	(UDF_SB(X) = kmalloc(sizeof(struct udf_sb_info), GFP_KERNEL))
#define UDF_SB_FREE(X)\
{\
	if (UDF_SB(X))\
	{\
		if (UDF_SB_PARTMAPS(X))\
			kfree(UDF_SB_PARTMAPS(X));\
		kfree(UDF_SB(X));\
		UDF_SB(X) = NULL;\
	}\
}

#define UDF_SB(X)	((struct udf_sb_info *) ((X)->u.generic_sbp))
#endif

#define UDF_SB_ALLOC_PARTMAPS(X,Y)\
{\
	UDF_SB_NUMPARTS(X) = Y;\
	UDF_SB_PARTMAPS(X) = kmalloc(sizeof(struct udf_part_map) * Y, GFP_KERNEL);\
}

#define IS_STRICT(X)			( UDF_SB(X)->s_flags & UDF_FLAG_STRICT )
#define IS_UNDELETE(X)			( UDF_SB(X)->s_flags & UDF_FLAG_UNDELETE )
#define IS_UNHIDE(X)			( UDF_SB(X)->s_flags & UDF_FLAG_UNHIDE )

#define UDF_SB_SESSION(X)		( UDF_SB(X)->s_session )
#define UDF_SB_ANCHOR(X)		( UDF_SB(X)->s_anchor )
#define UDF_SB_NUMPARTS(X)		( UDF_SB(X)->s_partitions )
#define UDF_SB_VOLUME(X)		( UDF_SB(X)->s_thisvolume )
#define UDF_SB_LASTBLOCK(X)		( UDF_SB(X)->s_lastblock )
#define UDF_SB_BSIZE(X)			( UDF_SB(X)->s_blocksize )
#define UDF_SB_VOLDESC(X)		( UDF_SB(X)->s_voldesc )
#define UDF_SB_FILESET(X)		( UDF_SB(X)->s_fileset )
#define UDF_SB_ROOTDIR(X)		( UDF_SB(X)->s_rootdir )
#define UDF_SB_LOGVOLINT(X)		( UDF_SB(X)->s_logvolint )
#define UDF_SB_PARTITION(X)		( UDF_SB_FILESET(sb).partitionReferenceNum )
#define UDF_SB_RECORDTIME(X)	( UDF_SB(X)->s_recordtime )
#define UDF_SB_TIMESTAMP(X)		( UDF_SB(X)->s_timestamp )
#define UDF_SB_FILECOUNT(X)		( UDF_SB(X)->s_filecount )
#define UDF_SB_VOLIDENT(X)		( UDF_SB(X)->s_volident )
#define UDF_SB_LASTDIRINO(X)	( UDF_SB(X)->s_lastdirino )
#define UDF_SB_LASTDIRNUM(X)	( UDF_SB(X)->s_lastdirnum )
#define UDF_SB_PARTMAPS(X)		( UDF_SB(X)->s_partmaps )
#define UDF_SB_LOCATION(X)		( UDF_SB(X)->s_location )
#define UDF_SB_CHARSET(X)		( UDF_SB(X)->s_nls_iocharset )
#define UDF_SB_VAT(X)			( UDF_SB(X)->s_vat )

#define UDF_SB_PARTTYPE(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_type )
#define UDF_SB_PARTROOT(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_root )
#define UDF_SB_PARTLEN(X,Y)		( UDF_SB_PARTMAPS(X)[Y].s_partition_len )
#define UDF_SB_PARTVSN(X,Y)		( UDF_SB_PARTMAPS(X)[Y].s_volumeseqnum )
#define UDF_SB_PARTNUM(X,Y)		( UDF_SB_PARTMAPS(X)[Y].s_partition_num )
#define UDF_SB_TYPESPAR(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_type_specific.s_sparing )
#define UDF_SB_TYPEVIRT(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_type_specific.s_virtual )

#endif /* __LINUX_UDF_SB_H */
