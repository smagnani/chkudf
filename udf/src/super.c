/*
 * super.c
 *
 * CHANGES
 * 9/24/98 dgb:	changed to allow compiling outside of kernel, and
 *		added some debugging.
 * 10/1/98 dgb: updated to allow (some) possibility of compiling w/2.0.34
 * 10/16/98	attempting some multi-session support
 * 10/17/98	added freespace count for "df"
 * 11/11/98 gr: added novrs option
 * 11/26/98 dgb added fileset,anchor mount options
 *
 * PURPOSE
 *	Super block routines for UDF filesystem.
 *
 * DESCRIPTION
 *	OSTA-UDF(tm) = Optical Storage Technology Association
 *	Universal Disk Format.
 *
 *	This code is based on version 1.50 of the UDF specification,
 *	and revision 2 of the ECMA 167 standard [equivalent to ISO 13346].
 *	http://www.osta.org/	http://www.ecma.ch/	http://www.iso.org/
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

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/malloc.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/cdrom.h>
#include <asm/byteorder.h>

#if LINUX_VERSION_CODE > 0x020170
#include <linux/init.h>
#include <asm/uaccess.h>
#else
#define NEED_COPY_TO_USER
#define NEED_GET_HARDBLOCKSIZE
#define NEED_LE32_TO_CPU
#endif

#include <linux/udf_fs.h>
#include "udfdecl.h"

/* These are the "meat" - everything else is stuffing */
static struct super_block *udf_read_super(struct super_block *, void *, int);
static void udf_put_super(struct super_block *);
static int udf_check_valid(struct super_block *sb, int);
static int udf_cd001_vrs(struct super_block *sb, int silent);
static int udf_nsr02_vrs(struct super_block *sb, long block, int silent);
static int udf_load_partition(struct super_block *sb,struct AnchorVolDescPtr *);
static int udf_load_logicalvol(struct super_block *sb,struct buffer_head *);
static int udf_find_anchor(struct super_block *,long,struct AnchorVolDescPtr *);
static int udf_find_fileset(struct super_block *sb);
static void udf_load_pvoldesc(struct super_block *sb, struct buffer_head *);
static void udf_load_fileset(struct super_block *sb, struct buffer_head *);
static void udf_load_partdesc(struct super_block *sb, struct buffer_head *);
static unsigned int udf_count_free(struct super_block *sb);

static unsigned int isofs_get_last_session(kdev_t dev);
/* version specific functions */
#if LINUX_VERSION_CODE > 0x020100
static int udf_statfs(struct super_block *, struct statfs *, int);
#else
static void udf_statfs(struct super_block *, struct statfs *, int);
#endif

/* some debug stuff */
/*
static void udf_dump_fileset(struct super_block *, struct buffer_head *);
*/

/* UDF filesystem type */
#if LINUX_VERSION_CODE > 0x020100
static struct file_system_type udf_fstype = {
	"udf",			/* name */
	FS_REQUIRES_DEV,	/* fs_flags */
	udf_read_super,		/* read_super */
	NULL			/* next */
};
#else
static struct file_system_type udf_fstype = {
	udf_read_super,		/* read_super */
	"udf",			/* name */
	1,			/* fs_flags */
	NULL			/* next */
};
#endif

/* Superblock operations */
#if LINUX_VERSION_CODE > 0x020100
static struct super_operations udf_sb_ops = {
	udf_read_inode,		/* read_inode */
	udf_write_inode,	/* write_inode */
	udf_put_inode,		/* put_inode */
	udf_delete_inode,	/* delete_inode */
	NULL,			/* notify_change */
	udf_put_super,		/* put_super */
	NULL,			/* write_super */
	udf_statfs,		/* statfs */
	NULL			/* remount_fs */
};
#else
static struct super_operations udf_sb_ops = {
	udf_read_inode,		/* read_inode */
	NULL,			/* notify_change */
#ifdef CONFIG_UDF_WRITE
	udf_write_inode,	/* write_inode */
#else
	NULL,			/* write_inode */
#endif
	udf_put_inode,		/* put_inode */
	udf_put_super,		/* put_super */
	NULL,			/* write_super */
	udf_statfs,		/* statfs */
	NULL			/* remount_fs */
};
#endif

/* Debugging level. Affects all UDF filesystems! */
int udf_debuglvl=0;
int udf_strict=0;
int udf_undelete=0;
int udf_unhide=0;
int udf_novrs=0; /* don't check for volume recognition seq */

#ifdef NEED_COPY_TO_USER
static inline unsigned long copy_to_user(void *to, 
				const void *from, unsigned long n)
{
	int i;
	if ((i = verify_area(VERIFY_WRITE, to, n)) != 0)
		return i;
	memcpy_tofs(to, from, n);
	return 0;
}
#endif
#ifdef NEED_GET_HARDBLOCKSIZE
int get_hardblocksize(kdev_t dev)
{
	return 2048;
}
#endif
#ifdef NEED_LE32_TO_CPU
static inline unsigned long le32_to_cpu(unsigned long value)
{
	return value; /* for i386, others need ... */
}
#endif

#if defined(MODULE)
#if LINUX_VERSION_CODE > 0x020170
MODULE_PARM(udf_debuglvl, "i");
MODULE_PARM(udf_strict, "i");
MODULE_PARM(udf_undelete, "i");
MODULE_PARM(udf_unhide, "i");
#endif

/*
 * cleanup_module
 *
 * PURPOSE
 *	Unregister the UDF filesystem type.
 *
 * DESCRIPTION
 *	Clean-up before the module is unloaded.
 *	This routine only applies when compiled as a module.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int
cleanup_module(void)
{
	printk(KERN_NOTICE "udf: unregistering filesystem\n");
	return unregister_filesystem(&udf_fstype);
}

/*
 * init_module / init_udf_fs
 *
 * PURPOSE
 *	Register the UDF filesystem type.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int init_module(void)
#else /* if !defined(MODULE) */
__initfunc(int init_udf_fs(void))
#endif
{
	printk(KERN_NOTICE "udf: registering filesystem\n");
	udf_debuglvl = UDF_DEBUG_NONE;
	return register_filesystem(&udf_fstype);
}

/*
 * udf_parse_options
 *
 * PURPOSE
 *	Parse mount options.
 *
 * DESCRIPTION
 *	The following mount options are supported:
 *
 *	bs=		Set the block size.
 *	debug=		Set the debugging level for _all_ UDF filesystems.
 *	fixed		Disable removable media checks.
 *	gid=		Set the default group.
 *	mode=		Set the default mode.
 *	strict		Set strick conformance.
 *	uid=		Set the default user.
 *	session=	Set the CDROM session (default= last)
 *	anchor=		Override standard anchor location.
 *	volume=		Override the VolumeDesc location.
 *	partition=	Override the PartitionDesc location.
 *	partroot=	Override the partitionStartingLocation.
 *	fileset=	Override the fileset block location.
 *	rootdir=	Override the root directory location.
 *      unhide
 *      undelete
 *      novrs           Skip volume sequence recognition (my RW cd recorded by 
 *                       ADAPTEC DirectCD 2.5 has no such sequence but otherwise is fine
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	opts			Pointer to mount options string.
 *
 * POST-CONDITIONS
 *	<return>	0	Mount options parsed okay.
 *	<return>	-1	Error parsing mount options.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_parse_options(struct super_block *sb, char *opts)
{
	char *opt, *val;

#ifdef DEBUG
	udf_debuglvl=2;
#endif
	/* Set defaults */
	sb->s_blocksize = 0;
	UDF_SB(sb)->s_flags = (udf_strict) ? UDF_FLAG_STRICT : 0;
	UDF_SB(sb)->s_mode = S_IRUGO | S_IXUGO;
	UDF_SB(sb)->s_gid = 0;
	UDF_SB(sb)->s_uid = 0;

	UDF_SB_SESSION(sb)=0;
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLUME(sb)=0;
	UDF_SB_PARTITION(sb)=0;
	UDF_SB_PARTROOT(sb)=0;
	UDF_SB_FILESET(sb)=0;
	UDF_SB_ROOTDIR(sb)=0;

	/* Break up the mount options */
	for (opt = strtok(opts, ","); opt; opt = strtok(NULL, ",")) {

		/* Make "opt=val" into two strings */
		val = strchr(opt, '=');
		if (val)
			*(val++) = 0;
		if (!strcmp(opt, "novrs") && !val)
			udf_novrs= 1;  
		else if (!strcmp(opt, "bs") && val)
			sb->s_blocksize = simple_strtoul(val, NULL, 0);
	        else if (!strcmp(opt, "debug") &&val)
			udf_debuglvl = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "fixed") && !val)
			UDF_SB(sb)->s_flags |= UDF_FLAG_FIXED;
		else if (!strcmp(opt, "unhide") && !val)
			udf_unhide=1;
		else if (!strcmp(opt, "undelete") && !val)
			udf_undelete=1;
		else if (!strcmp(opt, "gid") && val)
			UDF_SB(sb)->s_gid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "mode") && val)
			UDF_SB(sb)->s_mode = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "strick") && !val)
			UDF_SB(sb)->s_flags &= UDF_FLAG_STRICT;
		else if (!strcmp(opt, "uid") && val)
			UDF_SB(sb)->s_uid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "session") && val)
			UDF_SB_SESSION(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "anchor") && val)
			UDF_SB_ANCHOR(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "volume") && val)
			UDF_SB_VOLUME(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "partition") && val)
			UDF_SB_PARTITION(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "partroot") && val)
			UDF_SB_PARTROOT(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "fileset") && val)
			UDF_SB_FILESET(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "rootdir") && val)
			UDF_SB_ROOTDIR(sb)= simple_strtoul(val, NULL, 0);
		else if (val) {
			printk(KERN_ERR "udf: bad mount option \"%s=%s\"\n",
				opt, val);
			return -1;
		} else {
			printk(KERN_ERR "udf: bad mount option \"%s\"\n",
				opt);
			return -1;
		}
	}
	return 0;
}

/*
 * udf_set_blocksize
 *
 * PURPOSE
 *	Set the block size to be used in all transfers.
 *
 * DESCRIPTION
 *	To allow room for a DMA transfer, it is best to guess big when unsure.
 *	This routine picks 2048 bytes as the blocksize when guessing. This
 *	should be adequate until devices with larger block sizes become common.
 *
 *	Note that the Linux kernel can currently only deal with blocksizes of
 *	512, 1024, 2048, 4096, and 8192 bytes.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *
 * POST-CONDITIONS
 *	sb->s_blocksize		Blocksize.
 *	sb->s_blocksize_bits	log2 of blocksize.
 *	<return>	0	Blocksize is valid.
 *	<return>	1	Blocksize is invalid.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_set_blocksize(struct super_block *sb)
{
	int blocksize;

	/* Use specified block size if specified */
	blocksize = get_hardblocksize(sb->s_dev);
	blocksize = blocksize ? blocksize : 2048;
	blocksize = sb->s_blocksize ? sb->s_blocksize: blocksize;

	/* Block size must be an even multiple of 512 */
	switch (blocksize) {
		case 512:
		sb->s_blocksize_bits = 9;
		break;

		case 1024:
		sb->s_blocksize_bits = 10;
		break;

		case 0:
		case 2048:
		sb->s_blocksize_bits = 11;
		break;

		default:
		printk(KERN_ERR "udf: bad block size (%d)\n", blocksize);
		return -1;
	}

	/* Set the block size */
	set_blocksize(sb->s_dev, blocksize);
	sb->s_blocksize = blocksize;

	return 0;
}

/*
 * from fs/isofs
 */
static unsigned int 
isofs_get_last_session(kdev_t dev)
{
  struct cdrom_multisession ms_info;
  unsigned int vol_desc_start;
  struct inode inode_fake;
  extern struct file_operations * get_blkfops(unsigned int);
  int i;

  vol_desc_start=0;
  if (get_blkfops(MAJOR(dev))->ioctl!=NULL) {
      /* Whoops.  We must save the old FS, since otherwise
       * we would destroy the kernels idea about FS on root
       * mount in read_super... [chexum]
       */
      mm_segment_t old_fs=get_fs();
      inode_fake.i_rdev=dev;
      ms_info.addr_format=CDROM_LBA;
      set_fs(KERNEL_DS);
      i=get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
				       NULL,
				       CDROMMULTISESSION,
				       (unsigned long) &ms_info);
      set_fs(old_fs);
#ifdef DEBUG
      if (i==0) {
	  printk(KERN_INFO "udf: XA disk: %s\n", ms_info.xa_flag ? "yes":"no");
	  printk(KERN_INFO "udf: vol_desc_start = %d\n", ms_info.addr.lba);
      } else
      	  printk(KERN_DEBUG "udf: CDROMMULTISESSION not supported: rc=%d\n",i);
#endif

#define WE_OBEY_THE_WRITTEN_STANDARDS 1

      if (i==0) {
#if WE_OBEY_THE_WRITTEN_STANDARDS
        if (ms_info.xa_flag) /* necessary for a valid ms_info.addr */
#endif
          vol_desc_start=ms_info.addr.lba;
      }
  } else {
	printk(KERN_DEBUG "udf: device doesn't know how to ioctl?\n");
  }
  return vol_desc_start;
}

#ifdef DEBUG
#define ISODCL(from, to) (to - from + 1)

#ifdef __linux__
#pragma pack(1)
#endif
struct iso_primary_descriptor {
	char type			[ISODCL (  1,   1)]; /* 711 */
	char id				[ISODCL (  2,   6)];
	char version			[ISODCL (  7,   7)]; /* 711 */
	char unused1			[ISODCL (  8,   8)];
	char system_id			[ISODCL (  9,  40)]; /* achars */
	char volume_id			[ISODCL ( 41,  72)]; /* dchars */
	char unused2			[ISODCL ( 73,  80)];
	char volume_space_size		[ISODCL ( 81,  88)]; /* 733 */
	char unused3			[ISODCL ( 89, 120)];
	char volume_set_size		[ISODCL (121, 124)]; /* 723 */
	char volume_sequence_number	[ISODCL (125, 128)]; /* 723 */
	char logical_block_size		[ISODCL (129, 132)]; /* 723 */
	char path_table_size		[ISODCL (133, 140)]; /* 733 */
	char type_l_path_table		[ISODCL (141, 144)]; /* 731 */
	char opt_type_l_path_table	[ISODCL (145, 148)]; /* 731 */
	char type_m_path_table		[ISODCL (149, 152)]; /* 732 */
	char opt_type_m_path_table	[ISODCL (153, 156)]; /* 732 */
	char root_directory_record	[ISODCL (157, 190)]; /* 9.1 */
	char volume_set_id		[ISODCL (191, 318)]; /* dchars */
	char publisher_id		[ISODCL (319, 446)]; /* achars */
	char preparer_id		[ISODCL (447, 574)]; /* achars */
	char application_id		[ISODCL (575, 702)]; /* achars */
	char copyright_file_id		[ISODCL (703, 739)]; /* 7.5 dchars */
	char abstract_file_id		[ISODCL (740, 776)]; /* 7.5 dchars */
	char bibliographic_file_id	[ISODCL (777, 813)]; /* 7.5 dchars */
	char creation_date		[ISODCL (814, 830)]; /* 8.4.26.1 */
	char modification_date		[ISODCL (831, 847)]; /* 8.4.26.1 */
	char expiration_date		[ISODCL (848, 864)]; /* 8.4.26.1 */
	char effective_date		[ISODCL (865, 881)]; /* 8.4.26.1 */
	char file_structure_version	[ISODCL (882, 882)]; /* 711 */
	char unused4			[ISODCL (883, 883)];
	char application_data		[ISODCL (884, 1395)];
	char unused5			[ISODCL (1396, 2048)];
};
#ifdef __linux__
#pragma pack()
#endif
#endif

/*
 * udf_cd001_vrs
 *
 * PURPOSE
 *	Process an ISO 9660 volume recognition sequence.
 *
 * DESCRIPTION
 *	The block size must be a multiple of 2048.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *
 * POST-CONDITIONS
 *	<return>		Last block + 1 processed.
 *
 * HISTORY
 *	Oct 7, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_cd001_vrs(struct super_block *sb, int silent)
{
	struct VolStructDesc *vsd = NULL;

	int sector = 32768 >> sb->s_blocksize_bits; 
	/* normally sector=16, but that assumes blocksize 2048.         */
	/* that's always true for ISO9660 volumes, but not other media */

	struct buffer_head *bh;
	int save_bea=0;
	int iso9660=0;

	DPRINTK(3,(KERN_DEBUG "udf: looking for ISO9660 volume recognition sequence\n"));

	/* Block size must be a multiple of 2048 */
	if (sb->s_blocksize & 2047)
		return sector;

	sector += UDF_SB_SESSION(sb);

	printk(KERN_DEBUG "Starting at sector %u\n", sector);
	/* Process the sequence (if applicable) */
	for (;;) {
		/* Read a block */
		bh = bread(sb->s_dev, sector, sb->s_blocksize);
		if (!bh)
			break;

		/* Look for ISO 9660 descriptors */
		vsd = (struct VolStructDesc *)bh->b_data;

		if (!strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN))
		{
			switch (vsd->structType) {
				case 0: 
		printk(KERN_DEBUG "udf: ISO9660 Boot Record found\n"); break;
				case 1: 
		printk(KERN_DEBUG "udf: ISO9660 Primary Volume Descriptor found\n"); break;
				case 2: 
		printk(KERN_DEBUG "udf: ISO9660 Supplementary Volume Descriptor found\n"); break;
				case 3: 
		printk(KERN_DEBUG "udf: ISO9660 Volume Partition Descriptor found\n"); break;
				case 255: 
		printk(KERN_DEBUG "udf: ISO9660 Volume Descriptor Set Terminator found\n"); break;
				default: 
		printk(KERN_DEBUG "udf: ISO9660 VRS (%u) found\n", vsd->structType); break;
			}
			iso9660 = 1;
#ifdef VDEBUG
		  {
		    struct iso_primary_descriptor * isopd;

		    isopd=(struct iso_primary_descriptor *)bh->b_data;
		    printk(KERN_DEBUG "udf: iso(%s) vss %u vsets %u\n",
			isopd->volume_id, 
			*(unsigned int *)&isopd->volume_space_size,
			*(unsigned int *)&isopd->volume_set_size);
		  }
#endif
		}

		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN))
		{
			save_bea=sector;
		}
		if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN))
		{
			if ( !IS_STRICT(sb) )
			{
				brelse(bh);
				break;	
			}
		}
		if (!strncmp(vsd->stdIdent, STD_ID_NSR02, STD_ID_LEN) ||
		    !strncmp(vsd->stdIdent, STD_ID_NSR03, STD_ID_LEN))
		{
			save_bea = sector;
			if (iso9660)
				printk(KERN_DEBUG "udf: ISO9660 Bridge Disk Detected\n");
			if ( !IS_STRICT(sb) )
			{
				brelse(bh);
				break;
			}
		}

		sector++;
		brelse(bh);
	}

	if (IS_STRICT(sb) && vsd && (vsd->structType != 0xff) && !silent)
		printk(KERN_INFO "udf: Incorrect volume recognition sequence. "
			"Notify vendor!\n");

	return save_bea;
}

/*
 * udf_nsr02_vrs
 *
 * PURPOSE
 *	Process an ISO 13346 volume recognition sequence.
 *
 * DESCRIPTION
 *	This routine cannot handle block sizes > 2048.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	block			Block to start processing at.
 *
 * POST-CONDITIONS
 *	<return>	0	Not NSR compliant.
 *	<return>	1	NSR compliant.
 *
 * HISTORY
 *	Oct 7, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_nsr02_vrs(struct super_block *sb, long sector, int silent)
{
	struct VolStructDesc *vsd = NULL;
	int block_inc = 2048 >> sb->s_blocksize_bits;
	int is_nsr = 0;
	struct buffer_head *bh;

	DPRINTK(3,(KERN_INFO "udf: looking for ISO13346 volume recognition sequence\n"));

	/* Avoid infinite loops */
	if (!block_inc) {
		printk(KERN_ERR "udf: can't handle blocks > 2048\n");
		return 0;
	}

	/* Look for beginning of extended area */
	if (IS_STRICT(sb)) {
		bh = bread(sb->s_dev, sector++, sb->s_blocksize);
		if (!bh)
			return 0;
		vsd = (struct VolStructDesc *)bh->b_data;
		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN)) {
			if (!silent)
			  printk(KERN_ERR "udf: not an NSR compliant volume\n");
			brelse(bh);
			return 0;
		}
	}

	/* Process the extended area */
	for (;;) {
		/* Read a block */
		bh = bread(sb->s_dev, sector, sb->s_blocksize);
		if (!bh)
			break;

		/* Process the descriptor */
		vsd = (struct VolStructDesc *)bh->b_data;

		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN)) {
			;
		} else if (!strncmp(vsd->stdIdent, STD_ID_BOOT2, STD_ID_LEN)) {
			;
		} else if (!strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN)) {
			;
		} else if (!strncmp(vsd->stdIdent, STD_ID_CDW02, STD_ID_LEN)) {
			;
		} else if (!strncmp(vsd->stdIdent, STD_ID_NSR02, STD_ID_LEN)) {
			is_nsr = 1;
		} else if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN)) {
			brelse(bh);
			break;
		} else {
			brelse(bh);
			break;
		}
		brelse(bh);
		sector += block_inc;
	}

	return is_nsr;
}

/*
 * udf_find_anchor
 *
 * PURPOSE
 *	Find an anchor volume descriptor.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	lastblock		Last block on media.
 *
 * POST-CONDITIONS
 *	<return>		1 if not found, 0 if ok
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_find_anchor(struct super_block *sb, long lastblock, 
	struct AnchorVolDescPtr *ap)
{
	struct buffer_head *bh;
	long ablock;

	/* Search for an anchor volume descriptor pointer */

	/*  according to spec, anchor is in either:
	 *     block 256
	 *     lastblock-256
	 *     lastblock
	 *  however, if the disc isn't closed, it could be 512 */
 
	ablock=256 + UDF_SB_SESSION(sb);
	bh = udf_read_tagged(sb, ablock,0);
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		ablock=512 + UDF_SB_SESSION(sb);
		bh = udf_read_tagged(sb, ablock,0);
	}
#ifdef UDF_NEED_BETTER_LASTBLOCK
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		ablock=lastblock-256;
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		ablock=lastblock;
		bh = udf_read_tagged(sb, ablock,0);
	}
#endif
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		ablock=512 + UDF_SB_SESSION(sb);
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		printk(KERN_ERR "udf: couldn't find an anchor\n");
		return 1;
	}
	UDF_SB_ANCHOR(sb)=ablock;
	memcpy(ap, bh->b_data, sizeof(struct AnchorVolDescPtr));
	udf_release_data(bh);
	return 0;
}

static int 
udf_find_fileset(struct super_block *sb)
{
	struct buffer_head *bh=NULL;
	tag * tagp;
	long block, lastblock, offset;
	int done=0;

	offset=block=UDF_SB_PARTROOT(sb);

	/* lastblock=UDF_SB_LASTBLOCK(sb); */ /* don't know how yet */
	lastblock=block+100; /* look in the first 100 sectors */

	if (UDF_SB_FILESET(sb)) {
		/* set by LogicalVolDesc or mount option */
	    	bh = udf_read_tagged(sb, UDF_SB_FILESET(sb)+offset, offset);
	    	if (!bh) {
			return 1;
	    	}
	} else {
	    while ( (!done) && (block < lastblock)) {
		printk(KERN_DEBUG "udf: find_fileset block %lu?\n", 
			block-offset);
	    	bh = udf_read_tagged(sb, block, offset);
	    	if (!bh) {
			block++;
			continue;
	    	}
	    
	    	tagp=(tag *)bh->b_data;

		switch ( tagp->tagIdent ) {
		case TID_SPACE_BITMAP_DESC:
			{
				/* these are untagged! */
				/* carefully skip to avoid being fooled */
				struct SpaceBitmapDesc *sp;
				sp=(struct SpaceBitmapDesc *)bh->b_data;
				block +=sp->numOfBytes/sb->s_blocksize;
				/* skip bitmap sectors */
			}
			break;
		case TID_FILE_SET_DESC:
			UDF_SB_FILESET(sb)=block-offset;
			done=1;
			break;
		case TID_FILE_ENTRY:
			break;
		default:
			break;
		}
		udf_release_data(bh);

	    	block++;
	    } /* end while */
	}
	if ( (UDF_SB_FILESET(sb)) && (bh) ) {
		printk(KERN_DEBUG "udf: (%d) FileSet is at block %d\n",
			UDF_SB_PARTITION(sb), UDF_SB_FILESET(sb));
		udf_load_fileset(sb, bh);
		return 0;
	}
	return 1;
}

static void 
udf_load_pvoldesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PrimaryVolDesc *pvoldesc;
	time_t recording;
	struct ustr instr;
	struct ustr outstr;

	pvoldesc=(struct PrimaryVolDesc *)bh->b_data;

	if ( udf_stamp_to_time(&recording, &pvoldesc->recordingDateAndTime) ) {
	    timestamp *ts;
	    ts=&pvoldesc->recordingDateAndTime;
	    printk(KERN_INFO "udf: recording time %ld, %u/%u/%u %u:%u (%x)\n", 
		recording, ts->year, ts->month, ts->day, ts->hour, ts->minute,
		ts->typeAndTimezone);
	    UDF_SB_RECORDTIME(sb)=recording;
	    memcpy( &UDF_SB_TIMESTAMP(sb), ts, sizeof(timestamp));
	}
	if ( !udf_build_ustr(&instr, pvoldesc->volIdent, 32) ) {
		if (!udf_CS0toUTF8(&outstr, &instr)) {
	    		printk(KERN_INFO "udf: volIdent[] = '%s'\n", outstr.u_name);
			strncpy( UDF_SB_VOLIDENT(sb), outstr.u_name, 32);
		}
	}
	if ( !udf_build_ustr(&instr, pvoldesc->volSetIdent, 128) ) {
		if (!udf_CS0toUTF8(&outstr, &instr)) {
	    		printk(KERN_INFO "udf: volSetIdent[] = '%s'\n", outstr.u_name);
		}
	}
}

static void 
udf_load_fileset(struct super_block *sb, struct buffer_head *bh)
{
	struct FileSetDesc *fset;
	long_ad *r;

	fset=(struct FileSetDesc *)bh->b_data;
	r=&fset->rootDirectoryICB;
	if (UDF_SB_ROOTDIR(sb)) {
		UDF_SB_ROOTDIR(sb) -= UDF_SB_PARTROOT(sb);
		printk(KERN_INFO "udf: (%d) RootDir at %u, override to %u\n",
			r->extLocation.partitionReferenceNum,
			r->extLocation.logicalBlockNum, UDF_SB_ROOTDIR(sb));
	} else {
		UDF_SB_ROOTDIR(sb)= r->extLocation.logicalBlockNum;
	}
	printk(KERN_INFO "udf: (%d) RootDir is at block %u (+ %u= sector %lu), %u bytes\n",
		r->extLocation.partitionReferenceNum,
		r->extLocation.logicalBlockNum,
		UDF_SB_PARTROOT(sb), 
		udf_block_from_inode(sb,UDF_SB_ROOTDIR(sb)),
		r->extLength);
	
}

static void 
udf_load_partdesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PartitionDesc *p;
	p=(struct PartitionDesc *)bh->b_data;

	UDF_SB_PARTITION(sb)=p->partitionNumber;
	UDF_SB_PARTLEN(sb)=p->partitionLength; /* blocks */
	if (UDF_SB_PARTROOT(sb)) {
		printk(KERN_INFO "udf: partition(%d) override (%d) at sector %d, block length %d\n",
			p->partitionNumber, 
			(p->partitionStartingLocation + UDF_SB_SESSION(sb)),
			UDF_SB_PARTROOT(sb), UDF_SB_PARTLEN(sb));
	} else {
		UDF_SB_PARTROOT(sb)=p->partitionStartingLocation 
			+ UDF_SB_SESSION(sb);
		printk(KERN_INFO "udf: partition(%d) at sector %d, block length %d\n",
			p->partitionNumber,
			UDF_SB_PARTROOT(sb), UDF_SB_PARTLEN(sb));
	}
}

static int 
udf_load_logicalvol(struct super_block *sb,struct buffer_head * bh)
{
	struct LogicalVolDesc *p;
	long_ad * la;
	p=(struct LogicalVolDesc *)bh->b_data;

	la=(long_ad *)p->logicalVolContentsUse;
	printk(KERN_DEBUG "udf: '%s' lvcu loc lbn %lu ref %u len %lu\n",
		p->logicalVolIdent,
		(long unsigned)la->extLocation.logicalBlockNum, 
		la->extLocation.partitionReferenceNum,
		(long unsigned)la->extLength);
	if (!UDF_SB_FILESET(sb)) {
		UDF_SB_FILESET(sb)= la->extLocation.logicalBlockNum;
		printk(KERN_DEBUG "udf: FileSet(%d) found in LogicalVolDesc at %d\n",
			la->extLocation.partitionReferenceNum,
			UDF_SB_FILESET(sb));
	}
	return 0;
}

/*
 * udf_process_sequence
 *
 * PURPOSE
 *	Process a main/reserve volume descriptor sequence.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	block			First block of first extent of the sequence.
 *	lastblock		Lastblock of first extent of the sequence.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_process_sequence(struct super_block *sb, long block, long lastblock)
{
	struct buffer_head *bh;
	__u32 descIdent = TID_UNUSED_DESC;

	/* Read the main descriptor sequence */
	for (; (descIdent != TID_TERMINATING_DESC) &&
	       (block <= lastblock); block++) {

		bh = udf_read_tagged(sb, block,0);
		if (!bh) 
			break;

		/* Process each descriptor (ISO 13346 3/8.3-8.4) */
		descIdent = ((tag *)bh->b_data)->tagIdent;
		switch (descIdent) {
			case TID_PRIMARY_VOL_DESC: /* ISO 13346 3/10.1 */
			udf_load_pvoldesc(sb, bh);
			break;

			case TID_VOL_DESC_PTR: /* ISO 13346 3/10.3 */
			/*lastblock = block = le32_to_cpu(*/
			lastblock = le32_to_cpu(
				((struct AnchorVolDescPtr *)bh->b_data)
				->mainVolDescSeqExt.extLocation);
			lastblock += le32_to_cpu(((struct AnchorVolDescPtr *)
				bh->b_data)->mainVolDescSeqExt.extLength)
				 >> sb->s_blocksize_bits;
			if ( udf_debuglvl )
			  printk(KERN_ERR "udf: newlastblock = %lu\n",
					lastblock);
			break;

			case TID_IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
			break;

			case TID_PARTITION_DESC: /* ISO 13346 3/10.5 */
			udf_load_partdesc(sb, bh);
			break;

			case TID_LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
			udf_load_logicalvol(sb, bh);
			break;

			case TID_UNALLOC_SPACE_DESC: /* ISO 13346 3/10.8 */
			break;

			case TID_TERMINATING_DESC: /* ISO 13346 3/10.9 */
			break;

			case TID_FILE_SET_DESC:
			break;

			case TID_FILE_IDENT_DESC:
			break;
	
			default:
			break;
		}
		udf_release_data(bh);
	}
	return 0;
}

/*
 * udf_check_valid()
 */
static int
udf_check_valid(struct super_block *sb, int silent)
{
	long block;

	if ( !UDF_SB_SESSION(sb) )
		UDF_SB_SESSION(sb)=isofs_get_last_session(sb->s_dev);
	if ( UDF_SB_SESSION(sb) && !silent )
		printk(KERN_INFO "udf: multi-session=%d\n", UDF_SB_SESSION(sb));

	/* Check that it is NSR02 compliant */
	/* Process any "CD-ROM Volume Descriptor Set" (ECMA 167 2/8.3.1) */
	block = udf_cd001_vrs(sb, silent);
	if (!block) 	/* block = begining of extended area block */
		return 1;

	/* Check that it is NSR02 compliant */
	if (!udf_nsr02_vrs(sb, block, silent)) { /* expects block = BEA block */
		printk(KERN_ERR "udf: volume is not NSR02 compliant\n");
		return 1;
	}

	return 0;
}

static int
udf_load_partition(struct super_block *sb,struct AnchorVolDescPtr *anchor)
{
	long main_s, main_e, reserve_s, reserve_e;

	if (!sb)
	    return 1;
	/* Locate the main sequence */
	main_s = le32_to_cpu( anchor->mainVolDescSeqExt.extLocation );
	main_e = le32_to_cpu( anchor->mainVolDescSeqExt.extLength );
	main_e = main_e >> sb->s_blocksize_bits;
	main_e += main_s;
	UDF_SB_VOLDESC(sb)=main_s;


	/* Locate the reserve sequence */
	reserve_s = le32_to_cpu(anchor->mainVolDescSeqExt.extLocation);
	reserve_e = le32_to_cpu(anchor->mainVolDescSeqExt.extLength);
	reserve_e = reserve_e >> sb->s_blocksize_bits;
	reserve_e += reserve_s;

	UDF_SB_LASTBLOCK(sb)= (main_e > reserve_e) ? main_e : reserve_e;

	/* Process the main & reserve sequences */
	/* responsible for finding the PartitionDesc(s) */
	if ( udf_process_sequence(sb, main_s, main_e) &&
	     udf_process_sequence(sb, reserve_s, reserve_e) )
		return 1;
	return 0;
}

/*
 * udf_read_super
 *
 * PURPOSE
 *	Complete the specified super block.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to superblock to complete - never NULL.
 *	sb->s_dev		Device to read suberblock from.
 *	options			Pointer to mount options.
 *	silent			Silent flag.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static struct super_block *
udf_read_super(struct super_block *sb, void *options, int silent)
{
	struct inode *inode=NULL;
	struct AnchorVolDescPtr anchor;
	long lastblock=512;

	/* Lock the module in memory (if applicable) */
	MOD_INC_USE_COUNT;

	lock_super(sb);
	UDF_SB_ALLOC(sb); /* kmalloc, if needed */
	UDF_SB_SESSION(sb)=0; /* for multisession discs */
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLDESC(sb)=0;
	UDF_SB_LASTBLOCK(sb)=0;
	UDF_SB_FILESET(sb)=0;
	UDF_SB_RECORDTIME(sb)=0;
	UDF_SB_PARTROOT(sb)=0;
	UDF_SB_PARTLEN(sb)=0;
	UDF_SB_FILECOUNT(sb)=0;
	UDF_SB_VOLIDENT(sb)[0]=0;

	/* Parse any mount options */
	if (udf_parse_options(sb, (char *)options))
		goto error_out;

	/* Set the block size for all transfers */
	if (udf_set_blocksize(sb))
		goto error_out;
	if  (!udf_novrs){
		if (udf_check_valid(sb, silent)) /* read volume recognition sequences */
 		 goto error_out;
	}
	else
 		printk(KERN_ERR "udf: validity check skipped because of novrs option\n");

	if (!UDF_SB_ANCHOR(sb)) {
	    /* Find an anchor volume descriptor pointer */
	    /* dgb: how do we determine the last block before this point? */
	    if (udf_find_anchor(sb, lastblock, &anchor)) {
		printk(KERN_ERR "udf: no anchor block found\n");
		goto error_out;
	    }
	}

	if (udf_load_partition(sb, &anchor)) {
		printk(KERN_ERR "udf: no partition found\n");
		goto error_out;
	}

	if ( !UDF_SB_PARTROOT(sb) ) {
		printk(KERN_ERR "udf: no partition found (2)\n");
		goto error_out;
	}
	if ( udf_find_fileset(sb) ) {
		printk(KERN_ERR "udf: no fileset found\n");
		goto error_out;
	}

	if (!silent) {
		timestamp *ts;
		ts= &UDF_SB_TIMESTAMP(sb);
		printk(KERN_NOTICE "udf: mounting volume '%s', timestamp %u/%02u/%u %02u:%02u\n",
			UDF_SB_VOLIDENT(sb), ts->year, ts->month, ts->day, ts->hour, ts->minute);
	}
		
	/* Fill in the rest of the superblock */
	sb->s_op = &udf_sb_ops;
	sb->s_time = 0;
	sb->dq_op = NULL;
	sb->s_dirt = 0;
	sb->s_magic = UDF_SUPER_MAGIC;
	sb->s_flags |= MS_NODEV | MS_NOSUID; /* should be overridden by mount */
#ifndef CONFIG_UDF_WRITE
	sb->s_flags |= MS_RDONLY;
#endif

	/* Assign the root inode */
	/* assign inodes by physical block number */
	/* perhaps it's not extensible enough, but for now ... */
	inode = udf_iget(sb, UDF_SB_ROOTDIR(sb) ); 
	if (!inode) {
		printk(KERN_DEBUG "udf: error in udf_iget(, %d)\n",
			UDF_SB_ROOTDIR(sb) );
		goto error_out;
	}

#if LINUX_VERSION_CODE > 0x020170
	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if (!sb->s_root) {
		iput(inode);
		printk(KERN_DEBUG "udf: couldn't allocate root dentry\n");
		goto error_out;
	}
#endif

	unlock_super(sb);
	return sb;

error_out:
	sb->s_dev = NODEV;
	UDF_SB_FREE(sb);
	unlock_super(sb);
	MOD_DEC_USE_COUNT;
	return NULL;
}

/*
 * udf_put_super
 *
 * PURPOSE
 *	Prepare for destruction of the superblock.
 *
 * DESCRIPTION
 *	Called before the filesystem is unmounted.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static void
udf_put_super(struct super_block *sb)
{
	UDF_SB_FREE(sb);
	MOD_DEC_USE_COUNT;
}

/*
 * udf_stat_fs
 *
 * PURPOSE
 *	Return info about the filesystem.
 *
 * DESCRIPTION
 *	Called by sys_statfs()
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
#if LINUX_VERSION_CODE > 0x020100
static int
#else
static void
#endif
udf_statfs(struct super_block *sb, struct statfs *buf, int bufsize)
{
	int size;
	struct statfs tmp;
	int rc;

	size = (bufsize < sizeof(tmp)) ? bufsize: sizeof(tmp);

	memset(&tmp, 0, sizeof(tmp));
	tmp.f_type = UDF_SUPER_MAGIC;
	tmp.f_bsize = sb->s_blocksize;
	tmp.f_blocks = UDF_SB_PARTLEN(sb);
	tmp.f_bavail = udf_count_free(sb);
	tmp.f_bfree = tmp.f_bavail;
	tmp.f_files = UDF_SB_FILECOUNT(sb);
	tmp.f_ffree = 0L;
	/* __kernel_fsid_t f_fsid */
	tmp.f_namelen = UDF_NAME_LEN;

	rc= copy_to_user(buf, &tmp, size) ? -EFAULT: 0;
#if LINUX_VERSION_CODE > 0x020100
	return rc;
#endif
}

static unsigned char udf_bitmap_lookup[16] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};
	
static unsigned int
udf_count_free(struct super_block *sb)
{
	struct buffer_head *bh;
	struct SpaceBitmapDesc *bm;
	unsigned int accum=0;
	int index;
	int block;
	Uint32  bytes;
	Uint8   value;
	Uint8 * ptr;

	block=UDF_SB_PARTROOT(sb);
	bh = udf_read_tagged(sb, block++, UDF_SB_PARTROOT(sb));
	if (!bh) {
		printk(KERN_ERR "udf: udf_count_free failed\n");
		return 0;
	}
	bm=(struct SpaceBitmapDesc *)bh->b_data;
	bytes=bm->numOfBytes;
	bytes += 24;
	index=24; /* offset in first block only */
	ptr=(Uint8 *)bh->b_data;

	while ( bytes > 0 ) {
		while ((bytes > 0) && (index < sb->s_blocksize)) {
			value=ptr[index];
			accum += udf_bitmap_lookup[ value & 0x0f ];
			accum += udf_bitmap_lookup[ value >> 4 ];
			index++;
			bytes--;
		}
		if ( bytes ) {
			udf_release_data(bh);
			bh = bread(sb->s_dev, block++, sb->s_blocksize);
			if (!bh) {
			  printk(KERN_DEBUG "udf: udf_count_free failed\n");
			  return accum;
			}
			index=0;
			ptr=(Uint8 *)bh->b_data;
		}
	}
	udf_release_data(bh);
	return accum;
}
