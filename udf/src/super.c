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
static int udf_vrs(struct super_block *sb, int silent);
static int udf_load_partition(struct super_block *sb,struct AnchorVolDescPtr *);
static int udf_load_logicalvol(struct super_block *sb,struct buffer_head *);
static int udf_find_anchor(struct super_block *,long,struct AnchorVolDescPtr *);
static int udf_find_fileset(struct super_block *sb);
static void udf_load_pvoldesc(struct super_block *sb, struct buffer_head *);
static void udf_load_fileset(struct super_block *sb, struct buffer_head *);
static void udf_load_partdesc(struct super_block *sb, struct buffer_head *);
static unsigned int udf_count_free(struct super_block *sb);

static unsigned int udf_get_last_session(kdev_t dev);
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
static struct file_system_type udf_fstype =
{
	udf_read_super,		/* read_super */
	"udf",				/* name */
	1,					/* fs_flags */
	NULL				/* next */
};
#endif

/* Superblock operations */
#if LINUX_VERSION_CODE > 0x020100
static struct super_operations udf_sb_ops =
{
	udf_read_inode,		/* read_inode */
	udf_write_inode,	/* write_inode */
	udf_put_inode,		/* put_inode */
	udf_delete_inode,	/* delete_inode */
	NULL,				/* notify_change */
	udf_put_super,		/* put_super */
	NULL,				/* write_super */
	udf_statfs,			/* statfs */
	NULL				/* remount_fs */
};
#else
static struct super_operations udf_sb_ops =
{
	udf_read_inode,		/* read_inode */
	NULL,				/* notify_change */
#ifdef CONFIG_UDF_WRITE
	udf_write_inode,	/* write_inode */
#else
	NULL,				/* write_inode */
#endif
	udf_put_inode,		/* put_inode */
	udf_put_super,		/* put_super */
	NULL,				/* write_super */
	udf_statfs,			/* statfs */
	NULL				/* remount_fs */
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
 *	session=	Set the CDROM session sector(default= last)
 *  lastblock=	Override the last sector
 *	anchor=		Override standard anchor location.
 *	volume=		Override the VolumeDesc location.
 *	partition=	Override the PartitionDesc location.
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

	UDF_SB_PARTITION(sb)=0;
	UDF_SB_SESSION(sb)=0;
	UDF_SB_LASTBLOCK(sb)=0;
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLUME(sb)=0;

	UDF_SB_ROOTDIR(sb).logicalBlockNum = 0xFFFFFFFF;
	UDF_SB_ROOTDIR(sb).partitionReferenceNum = 0xFFFF;
	UDF_SB_FILESET(sb).logicalBlockNum = 0xFFFFFFFF;
	UDF_SB_FILESET(sb).partitionReferenceNum = 0xFFFF;

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
			UDF_SB_SESSION(sb) = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "lastblock") && val)
			UDF_SB_LASTBLOCK(sb) = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "anchor") && val)
			UDF_SB_ANCHOR(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "volume") && val)
			UDF_SB_VOLUME(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "partition") && val)
			UDF_SB_PARTITION(sb)= simple_strtoul(val, NULL, 0);
#if 0
		else if (!strcmp(opt, "fileset") && val)
			UDF_SB_FILESET(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "rootdir") && val)
			UDF_SB_ROOTDIR(sb)= simple_strtoul(val, NULL, 0);
#endif
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
		case 512: sb->s_blocksize_bits = 9;	break;
		case 1024: sb->s_blocksize_bits = 10; break;
		case 0:
		case 2048: sb->s_blocksize_bits = 11; break;
		case 4096: sb->s_blocksize_bits = 12; break;
		case 8192: sb->s_blocksize_bits = 13; break;
		default:
		{
			printk(KERN_ERR "udf: bad block size (%d)\n", blocksize);
			return -1;
		}
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
udf_get_last_session(kdev_t dev)
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

static unsigned int
udf_get_last_block(kdev_t dev)
{
	struct cdrom_tocentry toc;
	struct inode inode_fake;
	extern struct file_operations * get_blkfops(unsigned int);
	int i;

	if (get_blkfops(MAJOR(dev))->ioctl!=NULL)
	{
      /* Whoops.  We must save the old FS, since otherwise
       * we would destroy the kernels idea about FS on root
       * mount in read_super... [chexum]
       */
		mm_segment_t old_fs=get_fs();
		inode_fake.i_rdev=dev;
		toc.cdte_format=CDROM_LBA;
		toc.cdte_track = 0xAA;
		set_fs(KERNEL_DS);

		i = get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
					NULL,
					CDROMREADTOCENTRY,
					(unsigned long) &toc);
		set_fs(old_fs);

		if (i == 0)
			return toc.cdte_addr.lba - 1;
	}
	else
		printk(KERN_DEBUG "udf: device doesn't know how to ioctl?\n");
	return 0;
}

static int
udf_vrs(struct super_block *sb, int silent)
{
	struct VolStructDesc *vsd = NULL;
	int block_inc = 2048 >> sb->s_blocksize_bits;
	int sector = 32768 >> sb->s_blocksize_bits; 

	struct buffer_head *bh;
	int iso9660=0;
	int nsr02=0;
	int nsr03=0;

	/* Block size must be a multiple of 512 */
	if (sb->s_blocksize & 511)
		return sector;

	sector += UDF_SB_SESSION(sb);

	printk(KERN_DEBUG "Starting at sector %u (%ld byte sectors)\n", sector, sb->s_blocksize);
	/* Process the sequence (if applicable) */
	for (;!nsr02 && !nsr03;sector += block_inc)
	{
		/* Read a block */
		bh = bread(sb->s_dev, sector, sb->s_blocksize);
		if (!bh)
			break;

		/* Look for ISO  descriptors */
		vsd = (struct VolStructDesc *)bh->b_data;

		if (vsd->stdIdent[0] == 0)
			break;
		else if (!strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN))
		{
			iso9660 = sector;
			switch (vsd->structType)
			{
				case 0: 
					printk(KERN_DEBUG "udf: ISO9660 Boot Record found\n");
					break;
				case 1: 
					printk(KERN_DEBUG "udf: ISO9660 Primary Volume Descriptor found\n");
					break;
				case 2: 
					printk(KERN_DEBUG "udf: ISO9660 Supplementary Volume Descriptor found\n");
					break;
				case 3: 
					printk(KERN_DEBUG "udf: ISO9660 Volume Partition Descriptor found\n");
					break;
				case 255: 
					printk(KERN_DEBUG "udf: ISO9660 Volume Descriptor Set Terminator found\n");
					break;
				default: 
					printk(KERN_DEBUG "udf: ISO9660 VRS (%u) found\n", vsd->structType);
					break;
			}
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN))
		{
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN))
		{
			break;
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_NSR02, STD_ID_LEN))
		{
			nsr02 = sector;
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_NSR03, STD_ID_LEN))
		{
			nsr03 = sector;
		}
		brelse(bh);
	}

	if (nsr03)
		return nsr03;
	else if (nsr02)
		return nsr02;
	else
		return 0;
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
 
	ablock = 256 + UDF_SB_SESSION(sb);
	bh = udf_read_tagged(sb, ablock,0);
	if (!bh || ((tag *)bh->b_data)->tagIdent != TID_ANCHOR_VOL_DESC_PTR) {
		udf_release_data(bh);
		ablock=512 + UDF_SB_SESSION(sb);
		bh = udf_read_tagged(sb, ablock,0);
	}
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
	long lastblock;

	if (UDF_SB_FILESET(sb).logicalBlockNum != 0xFFFFFFFF ||
		UDF_SB_FILESET(sb).partitionReferenceNum != 0xFFFF)
	{
		bh = udf_read_ptagged(sb, UDF_SB_FILESET(sb));
		
		if (!bh)
			return 1;
	}
	else /* Search backwards through the partitions */
	{
		lb_addr fileset;
		
		for (fileset.partitionReferenceNum=UDF_SB_NUMPARTS(sb)-1;
			(fileset.partitionReferenceNum != 0xFFFF &&
				UDF_SB_FILESET(sb).logicalBlockNum == 0xFFFFFFFF &&
				UDF_SB_FILESET(sb).partitionReferenceNum == 0xFFFF);
			fileset.partitionReferenceNum--)
		{
			lastblock = UDF_SB_PARTLEN(sb, fileset.partitionReferenceNum);
			fileset.logicalBlockNum = 0;

			do
			{
				bh = udf_read_ptagged(sb, fileset);
				if (!bh)
					fileset.logicalBlockNum ++;

				tagp = (tag *)bh->b_data;
				switch (tagp->tagIdent)
				{
					case TID_SPACE_BITMAP_DESC:
					{
						struct SpaceBitmapDesc *sp;
						sp = (struct SpaceBitmapDesc *)bh->b_data;
						fileset.logicalBlockNum += 1 +
							((sp->numOfBytes + sizeof(struct SpaceBitmapDesc) - 1)
								>> sb->s_blocksize_bits);
						udf_release_data(bh);
						break;
					}
					case TID_FILE_SET_DESC:
					{
						memcpy(&UDF_SB_FILESET(sb), &fileset, sizeof(lb_addr));
						break;
					}
					default:
					{
						fileset.logicalBlockNum ++;
						udf_release_data(bh);
						break;
					}
				}
			}
			while (fileset.logicalBlockNum < lastblock &&
				UDF_SB_FILESET(sb).logicalBlockNum == 0xFFFFFFFF &&
				UDF_SB_FILESET(sb).partitionReferenceNum == 0xFFFF);
		}
	}
	if ((UDF_SB_FILESET(sb).logicalBlockNum != 0xFFFFFFFF ||
		UDF_SB_FILESET(sb).partitionReferenceNum != 0xFFFF) && bh)
	{
		printk(KERN_DEBUG "udf: (%d) FileSet is at block %d\n",
			UDF_SB_FILESET(sb).partitionReferenceNum,
			UDF_SB_FILESET(sb).logicalBlockNum);
		udf_load_fileset(sb, bh);
		udf_release_data(bh);
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

	pvoldesc = (struct PrimaryVolDesc *)bh->b_data;

	if ( udf_stamp_to_time(&recording, &pvoldesc->recordingDateAndTime) )
	{
	    timestamp *ts;
	    ts = &pvoldesc->recordingDateAndTime;
	    printk(KERN_INFO "udf: recording time %ld, %u/%u/%u %u:%u (%x)\n", 
		recording, ts->year, ts->month, ts->day, ts->hour, ts->minute,
		ts->typeAndTimezone);
	    UDF_SB_RECORDTIME(sb) = recording;
	    memcpy( &UDF_SB_TIMESTAMP(sb), ts, sizeof(timestamp));
	}

	if ( !udf_build_ustr(&instr, pvoldesc->volIdent, 32) )
	{
		if (!udf_CS0toUTF8(&outstr, &instr))
		{
	    	printk(KERN_INFO "udf: volIdent[] = '%s'\n", outstr.u_name);
			strncpy( UDF_SB_VOLIDENT(sb), outstr.u_name, 32);
		}
	}

	if ( !udf_build_ustr(&instr, pvoldesc->volSetIdent, 128) )
	{
		if (!udf_CS0toUTF8(&outstr, &instr))
	    		printk(KERN_INFO "udf: volSetIdent[] = '%s'\n", outstr.u_name);
	}
}

static void 
udf_load_fileset(struct super_block *sb, struct buffer_head *bh)
{
	struct FileSetDesc *fset;

	fset = (struct FileSetDesc *)bh->b_data;

	UDF_SB_ROOTDIR(sb) = fset->rootDirectoryICB.extLocation;
}

static void 
udf_load_partdesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PartitionDesc *p;
	int i;

	p=(struct PartitionDesc *)bh->b_data;

	for (i=0; i<UDF_SB_NUMPARTS(sb); i++)
	{
		printk(KERN_DEBUG "udf: searching map: (%d == %d)\n", UDF_SB_PARTMAPS(sb)[i].s_partition_num, p->partitionNumber);
		if (UDF_SB_PARTMAPS(sb)[i].s_partition_num == p->partitionNumber)
		{
			UDF_SB_PARTMAPS(sb)[i].s_partition_len = p->partitionLength; /* blocks */
			UDF_SB_PARTMAPS(sb)[i].s_partition_root = p->partitionStartingLocation + UDF_SB_SESSION(sb);
			if (!UDF_SB_PARTITION(sb))
				UDF_SB_PARTITION(sb) = i;
			break;
		}
	}
	if (i == UDF_SB_NUMPARTS(sb))
		printk(KERN_DEBUG "udf: partition(%d) not found in partition map!\n", p->partitionNumber);
	else
		printk(KERN_INFO "udf: partition(%d:%d type %x) starts at physical %d, block length %d\n",
			p->partitionNumber, i, UDF_SB_PARTTYPE(sb,i),
			UDF_SB_PARTROOT(sb,i), UDF_SB_PARTLEN(sb,i));
}

static int 
udf_load_logicalvol(struct super_block *sb,struct buffer_head * bh)
{
	struct LogicalVolDesc *lvd;
	int i, offset;
	Uint8 type;

	lvd = (struct LogicalVolDesc *)bh->b_data;

	UDF_SB_NUMPARTS(sb) = lvd->numPartitionMaps;
	UDF_SB_ALLOC_PARTMAPS(sb, lvd->numPartitionMaps);

	for (i=0,offset=0;
		 i<UDF_SB_NUMPARTS(sb) && offset<lvd->mapTableLength;
		 i++,offset+=((struct GenericPartitionMap *)&(lvd->partitionMaps[offset]))->partitionMapLength)
	{
		type = ((struct GenericPartitionMap *)&(lvd->partitionMaps[offset]))->partitionMapType;
		printk(KERN_DEBUG "udf: Partition (%d) type %d\n", i, type);
		if (type == 1)
		{
			struct GenericPartitionMap1 *gpm1 = (struct GenericPartitionMap1 *)&(lvd->partitionMaps[offset]);
			UDF_SB_PARTTYPE(sb,i) = UDF_TYPE1_MAP;
			UDF_SB_PARTVSN(sb,i) = gpm1->volSeqNum;
			UDF_SB_PARTNUM(sb,i) = gpm1->partitionNum;
		}
		else if (type == 2)
		{
			struct UdfPartitionMap2 *upm2 = (struct UdfPartitionMap2 *)&(lvd->partitionMaps[offset]);
			if (!strncmp(upm2->partIdent.ident, UDF_ID_PARTITION, strlen(UDF_ID_PARTITION)))
			{
				UDF_SB_PARTTYPE(sb,i) = UDF_VIRTUAL_MAP;
				/*
					Setup VAT
				 */
			}
			else if (!strncmp(upm2->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)))
			{
				UDF_SB_PARTTYPE(sb,i) = UDF_SPARABLE_MAP;
				/*
					Setup sparing tables
				*/
			}
			else
			{
				printk(KERN_DEBUG "udf: Unknown ident: %s\n", upm2->partIdent.ident);
				continue;
			}
			UDF_SB_PARTVSN(sb,i) = upm2->volSeqNum;
			UDF_SB_PARTNUM(sb,i) = upm2->partitionNum;
		}
	}

	if (UDF_SB_FILESET(sb).logicalBlockNum == 0xFFFFFFFF &&
		UDF_SB_FILESET(sb).partitionReferenceNum == 0xFFFF)
	{
		long_ad *la = (long_ad *)&(lvd->logicalVolContentsUse[0]);

		UDF_SB_FILESET(sb) = la->extLocation;
		printk(KERN_DEBUG "udf: FileSet(%d) found in LogicalVolDesc at %d\n",
			UDF_SB_FILESET(sb).partitionReferenceNum,
			UDF_SB_FILESET(sb).logicalBlockNum);
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
	printk(KERN_DEBUG "udf: udf_process_sequence (%lx,%lx)\n", block, lastblock);

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
			break;

			case TID_IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
			break;

			case TID_PARTITION_DESC: /* ISO 13346 3/10.5 */
			udf_load_partdesc(sb, bh);
			break;

			case TID_LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
			{
				printk(KERN_DEBUG "udf: Logical Vol Desc: block %lx\n", block);
				udf_load_logicalvol(sb, bh);
			}
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
		UDF_SB_SESSION(sb)=udf_get_last_session(sb->s_dev);
	if ( !UDF_SB_LASTBLOCK(sb) )
		UDF_SB_LASTBLOCK(sb)=udf_get_last_block(sb->s_dev);
	if ( UDF_SB_SESSION(sb) && !silent )
		printk(KERN_INFO "udf: multi-session=%d\n", UDF_SB_SESSION(sb));
	if ( UDF_SB_LASTBLOCK(sb) && !silent )
		printk(KERN_INFO "udf: lastblock=%d\n", UDF_SB_LASTBLOCK(sb));

	/* Check that it is NSR02 compliant */
	/* Process any "CD-ROM Volume Descriptor Set" (ECMA 167 2/8.3.1) */
	block = udf_vrs(sb, silent);
	if (!block) 	/* block = begining of extended area block */
		return 1;
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

#if 0
	UDF_SB_LASTBLOCK(sb)= (main_e > reserve_e) ? main_e : reserve_e;
#endif

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

	/* Lock the module in memory (if applicable) */
	MOD_INC_USE_COUNT;

	lock_super(sb);
	UDF_SB_ALLOC(sb); /* kmalloc, if needed */
	UDF_SB_SESSION(sb)=0; /* for multisession discs */
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLDESC(sb)=0;
	UDF_SB_LASTBLOCK(sb)=0;
	UDF_SB_FILESET(sb).logicalBlockNum = 0xFFFFFFFF;
	UDF_SB_FILESET(sb).partitionReferenceNum = 0xFFFF;
	UDF_SB_RECORDTIME(sb)=0;
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
	    if (udf_find_anchor(sb, UDF_SB_LASTBLOCK(sb), &anchor))
		{
		printk(KERN_ERR "udf: no anchor block found\n");
		goto error_out;
	    }
	}

	if (udf_load_partition(sb, &anchor)) {
		printk(KERN_ERR "udf: no partition found\n");
		goto error_out;
	}

	if ( !UDF_SB_NUMPARTS(sb) ) {
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
	if (!inode)
	{
		printk(KERN_DEBUG "udf: error in udf_iget(%d,%d)\n",
			UDF_SB_ROOTDIR(sb).partitionReferenceNum, UDF_SB_ROOTDIR(sb).logicalBlockNum );
		goto error_out;
	}

#if LINUX_VERSION_CODE > 0x020170
	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if (!sb->s_root)
	{
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
	tmp.f_blocks = UDF_SB_PARTLEN(sb, UDF_SB_PARTITION(sb));
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
	int block=0;
	lb_addr loc;
	Uint32  bytes;
	Uint8   value;
	Uint8 * ptr;

	loc = UDF_SB_FILESET(sb);
	loc.logicalBlockNum ++;

	bh = udf_read_ptagged(sb, loc);

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
