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
#ifdef __KERNEL__

#include <linux/time.h>

#define UDF_TYPE1_MAP		0x0101U
#define UDF_VIRTUAL_MAP		0x0102U
#define UDF_SPARABLE_MAP	0x0202U

struct udf_part_map
{
	Uint16		s_partition_type;
	Uint32		s_partition_root;
	Uint32		s_partition_len;
	Uint16		s_volumeseqnum;
	Uint16		s_partition_num;
};

struct udf_sb_info
{
	/* Default permissions */
	mode_t		s_mode;
	gid_t		s_gid;
	uid_t		s_uid;

	/* Overall info */
	Uint16		s_maxVolSeqNum;
	Uint16		s_partitions;
	Uint16		s_thisvolume;

	/* Volume ID */
	Uint32		s_id_block;
	Uint16		s_id_crc;

	/* Block headers */
	Uint32		s_session;
	Uint32		s_anchor;
	Uint32		s_lastblock;
	Uint32		s_voldesc;
	lb_addr		s_fileset;
	lb_addr		s_rootdir;
	Uint32		s_filecount;
	Uint8		s_volident[32];

	/* directory info */
	Uint32		s_lastdirino;
	Uint32		s_lastdirnum;

	/* Root Info */
	time_t		s_recordtime;
	timestamp	s_timestamp;

	/* Miscellaneous flags */
	int			s_flags;

	/* Debugging level */
	int			s_debug;

	struct udf_part_map *s_partmaps;
};

#define UDF_FLAG_STRICT	0x00000001U
#define UDF_FLAG_FIXED	0x00000004U

#ifdef CONFIG_UDF_FS
#define UDF_SB_ALLOC(X) 
#define UDF_SB_FREE(X)\
{\
	if (UDF_SB(X))\
	{\
		if (UDF_SB_PARTMAPS(X))\
			kfree(UDF_SB_PARTMAPS(X));\
		UDF_SB_PARTMAPS(X) = NULL;\
	}\
}\ 

#define UDF_SB(X)	(&((X)->u.udf_sb))
#else
	/* else we kmalloc a page to hold our stuff */
#define UDF_SB_ALLOC(X) (UDF_SB(X) = kmalloc(sizeof(struct udf_sb_info), GFP_KERNEL))
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

#define IS_STRICT(X)	( UDF_SB(X)->s_flags & UDF_FLAG_STRICT)
#define IS_FIXED(X)	( UDF_SB(X)->s_flags & UDF_FLAG_DEBUG)

#define UDF_SB_SESSION(X)		( UDF_SB(X)->s_session )
#define UDF_SB_ANCHOR(X)		( UDF_SB(X)->s_anchor )
#define UDF_SB_NUMPARTS(X)		( UDF_SB(X)->s_partitions )
#define UDF_SB_VOLUME(X)		( UDF_SB(X)->s_thisvolume )
#define UDF_SB_LASTBLOCK(X)		( UDF_SB(X)->s_lastblock )
#define UDF_SB_VOLDESC(X)		( UDF_SB(X)->s_voldesc )
#define UDF_SB_FILESET(X)		( UDF_SB(X)->s_fileset )
#define UDF_SB_ROOTDIR(X)		( UDF_SB(X)->s_rootdir )
#define UDF_SB_PARTITION(X)		( UDF_SB_FILESET(sb).partitionReferenceNum )
#define UDF_SB_RECORDTIME(X)	( UDF_SB(X)->s_recordtime )
#define UDF_SB_TIMESTAMP(X)		( UDF_SB(X)->s_timestamp )
#define UDF_SB_FILECOUNT(X)		( UDF_SB(X)->s_filecount )
#define UDF_SB_VOLIDENT(X)		( UDF_SB(X)->s_volident )
#define UDF_SB_LASTDIRINO(X)	( UDF_SB(X)->s_lastdirino )
#define UDF_SB_LASTDIRNUM(X)	( UDF_SB(X)->s_lastdirnum )
#define UDF_SB_PARTMAPS(X)		( UDF_SB(X)->s_partmaps )

#if 0
#define UDF_SB_PARTTYPE(X,Y)	( (UDF_SB_NUMPARTS(X) > (Uint32)Y) ?\
	UDF_SB_PARTMAPS(X)[Y].s_partition_type : 0xFFFF )
#define UDF_SB_PARTROOT(X,Y)	( (UDF_SB_NUMPARTS(X) > (Uint32)Y) ?\
	UDF_SB_PARTMAPS(X)[Y].s_partition_root : 0xFFFFFFFF )
#define UDF_SB_PARTLEN(X,Y)		( (UDF_SB_NUMPARTS(X) > (Uint32)Y) ?\
	UDF_SB_PARTMAPS(X)[Y].s_partition_len : 0xFFFFFFFF )
#define UDF_SB_PARTVSN(X,Y)		( (UDF_SB_NUMPARTS(X) > (Uint32)Y) ?\
	UDF_SB_PARTMAPS(X)[Y].s_volumeseqnum : 0xFFFF )
#define UDF_SB_PARTNUM(X,Y)		( (UDF_SB_NUMPARTS(X) > (Uint32)Y) ?\
	UDF_SB_PARTMAPS(X)[Y].s_partition_num : 0xFFFF )
#else
#define UDF_SB_PARTTYPE(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_type )
#define UDF_SB_PARTROOT(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_root )
#define UDF_SB_PARTLEN(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_len )
#define UDF_SB_PARTVSN(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_volumeseqnum )
#define UDF_SB_PARTNUM(X,Y)	( UDF_SB_PARTMAPS(X)[Y].s_partition_num )
#endif

#endif	/* defined(__KERNEL__) */
#endif /* !defined(_LINUX_UDF_FS_SB_H) */
