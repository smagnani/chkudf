#ifndef __LINUX_UDF_SB_H
#define __LINUX_UDF_SB_H

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC				0x15013346

#define UDF_MAX_READ_VERSION		0x0200
#define UDF_MAX_WRITE_VERSION		0x0200

#define UDF_FLAG_USE_EXTENDED_FE	0
#define UDF_VERS_USE_EXTENDED_FE	0x0200
#define UDF_FLAG_USE_STREAMS		1
#define UDF_VERS_USE_STREAMS		0x0200
#define UDF_FLAG_USE_SHORT_AD		2
#define UDF_FLAG_USE_AD_IN_ICB		3
#define UDF_FLAG_USE_FILE_CTIME_EA	4
#define UDF_FLAG_STRICT				5
#define UDF_FLAG_UNDELETE			6
#define UDF_FLAG_UNHIDE				7
#define UDF_FLAG_VARCONV			8

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
	memset(UDF_SB_PARTMAPS(X), 0x00, sizeof(struct udf_part_map) * Y);\
}

#define UDF_QUERY_FLAG(X,Y)				( UDF_SB(X)->s_flags & ( 1 << (Y) ) )
#define UDF_SET_FLAG(X,Y)				( UDF_SB(X)->s_flags |= ( 1 << (Y) ) )
#define UDF_CLEAR_FLAG(X,Y)				( UDF_SB(X)->s_flags &= ~( 1 << (Y) ) )

#define UDF_UPDATE_UDFREV(X,Y)			( ((Y) > UDF_SB_UDFREV(X)) ? UDF_SB_UDFREV(X) = (Y) : UDF_SB_UDFREV(X) )

#define UDF_SB_PARTMAPS(X)				( UDF_SB(X)->s_partmaps )
#define UDF_SB_PARTTYPE(X,Y)			( UDF_SB_PARTMAPS(X)[(Y)].s_partition_type )
#define UDF_SB_PARTROOT(X,Y)			( UDF_SB_PARTMAPS(X)[(Y)].s_partition_root )
#define UDF_SB_PARTLEN(X,Y)				( UDF_SB_PARTMAPS(X)[(Y)].s_partition_len )
#define UDF_SB_PARTVSN(X,Y)				( UDF_SB_PARTMAPS(X)[(Y)].s_volumeseqnum )
#define UDF_SB_PARTNUM(X,Y)				( UDF_SB_PARTMAPS(X)[(Y)].s_partition_num )
#define UDF_SB_TYPESPAR(X,Y)			( UDF_SB_PARTMAPS(X)[(Y)].s_type_specific.s_sparing )
#define UDF_SB_TYPEVIRT(X,Y)			( UDF_SB_PARTMAPS(X)[(Y)].s_type_specific.s_virtual )
#define UDF_SB_PARTFUNC(X,Y)			( UDF_SB_PARTMAPS(X)[(Y)].s_partition_func )

#define UDF_SB_VOLIDENT(X)				( UDF_SB(X)->s_volident )
#define UDF_SB_NUMPARTS(X)				( UDF_SB(X)->s_partitions )
#define UDF_SB_PARTITION(X)				( UDF_SB(X)->s_partition )
#define UDF_SB_SESSION(X)				( UDF_SB(X)->s_session )
#define UDF_SB_ANCHOR(X)				( UDF_SB(X)->s_anchor )
#define UDF_SB_LASTBLOCK(X)				( UDF_SB(X)->s_lastblock )
#define UDF_SB_LVIDBH(X)				( UDF_SB(X)->s_lvidbh )
#define UDF_SB_LVID(X)					( (struct LogicalVolIntegrityDesc *)UDF_SB_LVIDBH(X)->b_data )
#define UDF_SB_LVIDIU(X)				( (struct LogicalVolIntegrityDescImpUse *)&(UDF_SB_LVID(X)->impUse[UDF_SB_LVID(X)->numOfPartitions * 2 * sizeof(Uint32)/sizeof(Uint8)]) )

#define UDF_SB_LOADED_BLOCK_BITMAPS(X)	( UDF_SB(X)->s_loaded_block_bitmaps )
#define UDF_SB_BLOCK_BITMAP_NUMBER(X,Y) ( UDF_SB(X)->s_block_bitmap_number[(Y)] )
#define UDF_SB_BLOCK_BITMAP(X,Y)		( UDF_SB(X)->s_block_bitmap[(Y)] )
#define UDF_SB_UMASK(X)					( UDF_SB(X)->s_umask )
#define UDF_SB_GID(X)					( UDF_SB(X)->s_gid )
#define UDF_SB_UID(X)					( UDF_SB(X)->s_uid )
#define UDF_SB_RECORDTIME(X)			( UDF_SB(X)->s_recordtime )
#define UDF_SB_SERIALNUM(X)				( UDF_SB(X)->s_serialnum )
#define UDF_SB_UDFREV(X)				( UDF_SB(X)->s_udfrev )
#define UDF_SB_FLAGS(X)					( UDF_SB(X)->s_flags )
#define UDF_SB_VAT(X)					( UDF_SB(X)->s_vat )

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
#define UDF_SB_RENAME_LOCK(X)			( UDF_SB(X)->s_rename_lock )
#define UDF_SB_RENAME_WAIT(X)			( UDF_SB(X)->s_rename_wait )
#endif

#endif /* __LINUX_UDF_SB_H */
