#if !defined(_LINUX_UDF_FS_SB_H)
#define _LINUX_UDF_FS_SB_H
/*
 * udf_fs_sb.h
 * 
 * This include file is for the Linux kernel/module.
 *
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

#define UDF_SB_APPROX_SIZE	40

#if !defined(CONFIG_UDF_FS)

/* make things easier for the kernel when we're not compiled in ... */
struct udf_sb_info {
	__u32 reserved[UDF_SB_APPROX_SIZE]; 
			/* ... but leave enough room for the module! */
};

#else

#pragma pack(1)

#define UDF_TYPE1_MAP15			0x1511U
#define UDF_VIRTUAL_MAP15		0x1512U
#define UDF_VIRTUAL_MAP20		0x2012U
#define UDF_SPARABLE_MAP15		0x1522U

struct udf_sparing_data
{
	__u32	  s_spar_loc;
	__u16	  s_spar_plen;
};

struct udf_virtual_data
{
	__u16	  s_start_offset;
	__u32	  s_num_entries;
};

struct udf_part_map
{
	__u16	s_partition_type;
	__u32	s_partition_root;
	__u32	s_partition_len;
	__u32	s_uspace_bitmap;
	__u16	s_volumeseqnum;
	__u16	s_partition_num;
	union
	{
		struct udf_sparing_data s_sparing;
		struct udf_virtual_data s_virtual;
	} s_type_specific;
};

struct udf_sb_info
{
	struct udf_part_map *s_partmaps;
	__u8  s_volident[32];
	__u64 s_nextid;

	/* Default permissions */
	mode_t s_umask;
	gid_t s_gid;
	uid_t s_uid;

	/* Overall info */
	__u16 s_blocksize; 
	__u16 s_maxVolSeqNum;
	__u16 s_partitions;
	__u32 s_thispartition;

	/* Volume ID */
	__u32 s_id_block;
	__u16 s_id_crc;

	/* Sector headers */
	__u32 s_session;
	__u32 s_anchor;
	__u32 s_lastblock;
	__u32 s_voldesc;
	lb_addr s_fileset;
	lb_addr s_rootdir;

	__u32 s_filecount;

	lb_addr s_location;


	/* directory info */
	__u32 s_lastdirino;
	__u32 s_lastdirnum;

	/* Root Info */
	time_t s_recordtime;
	timestamp s_timestamp;

	/* Miscellaneous flags */
	int s_flags;

	/* Character Mapping Info */
	struct nls_table *s_nls_iocharset;
	unsigned char s_utf8;

	/* VAT inode */
	struct inode    *s_vat;

	/* Debugging level */
	int s_debug;
};
#pragma pack()

#endif  /* !defined(CONFIG_UDF_FS) */

#endif /* !defined(_LINUX_UDF_FS_SB_H) */
