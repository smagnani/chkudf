/*
 * super.c
 *
 * CHANGES
 * 9/24/98 dgb:	changed to allow compiling outside of kernel, and
 *		added some debugging.
 *
 * 10/1/98 dgb: updated to allow (some) possibility of compiling w/2.0.34
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
#include <asm/byteorder.h>

#if LINUX_VERSION_CODE > 0x020170
#include <linux/init.h>
#include <asm/uaccess.h>
#else
#define NEED_COPY_TO_USER
#define NEED_GET_HARDBLOCKSIZE
#define NEED_LE32_TO_CPU
#endif



#include <config/udf.h>
#include <linux/udf_fs.h>

#include "debug.h"

/* Thses are defined in inode.c */
extern void udf_read_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
#ifdef CONFIG_UDF_WRITE
extern void udf_write_inode(struct inode *);
#endif
extern struct inode *udf_iget(struct super_block *, unsigned long);

/* These are the "meat" - everything else is stuffing */
static struct super_block *udf_read_super(struct super_block *, void *, int);
static void udf_put_super(struct super_block *);
static int udf_check_valid(struct super_block *sb);
static int udf_load_partition(struct super_block *sb,struct AnchorVolDescPtr *);
static int udf_find_anchor(struct super_block *,long,struct AnchorVolDescPtr *);
static int udf_find_fileset(struct super_block *sb);
static void udf_load_pvoldesc(struct super_block *sb, struct buffer_head *);
static void udf_load_fileset(struct super_block *sb, struct buffer_head *);
static void udf_load_partdesc(struct super_block *sb, struct buffer_head *);

/* version specific functions */
#if LINUX_VERSION_CODE > 0x020100
static int udf_statfs(struct super_block *, struct statfs *, int);
#else
static void udf_statfs(struct super_block *, struct statfs *, int);
#endif

/* some debug stuff */
/*
static void udf_dump_voldesc(struct super_block *sb, int voldesc);
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
#ifdef CONFIG_UDF_WRITE
	udf_write_inode,	/* write_inode */
#else
	NULL,			/* write_inode */
#endif
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
int udf_dumpfileset=1;

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
MODULE_PARM(udf_dumpfileset, "i");
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
extern int
cleanup_module(void)
{
	PRINTK((KERN_NOTICE "udf: unregistering filesystem\n"));
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
extern int init_module(void)
#else /* if !defined(MODULE) */
__initfunc(int init_udf_fs(void))
#endif
{
	PRINTK((KERN_NOTICE "udf: registering filesystem\n"));
	udf_debuglvl = DEBUG_NONE;
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
 *	relaxed		Set relaxed conformance.
 *	uid=		Set the default user.
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
static __inline__ int
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

	/* Break up the mount options */
	for (opt = strtok(opts, ","); opt; opt = strtok(NULL, ",")) {

		/* Make "opt=val" into two strings */
		val = strchr(opt, '=');
		if (val)
			*(val++) = 0;

		if (!strcmp(opt, "bs") && !val)
			sb->s_blocksize = simple_strtoul(val, NULL, 0);
		if (!strcmp(opt, "debug") &&!val)
			udf_debuglvl = simple_strtoul(val, NULL, 0);
		if (!strcmp(opt, "fixed") && !val)
			UDF_SB(sb)->s_flags |= UDF_FLAG_FIXED;
		else if (!strcmp(opt, "gid") && val)
			UDF_SB(sb)->s_gid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "mode") && val)
			UDF_SB(sb)->s_mode = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "relaxed") && !val)
			UDF_SB(sb)->s_flags &= ~UDF_FLAG_STRICT;
		else if (!strcmp(opt, "uid") && val)
			UDF_SB(sb)->s_uid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "volume") && val)
			UDF_SB_VOLUME(sb)= simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "partition") && val)
			UDF_SB_PARTITION(sb)= simple_strtoul(val, NULL, 0);
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
static __inline__ int
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
static __inline__ int
udf_cd001_vrs(struct super_block *sb)
{
	struct VolStructDesc *vsd = NULL;
	int block = 32768 >> sb->s_blocksize_bits;
	struct buffer_head *bh;
	int save_bea=0;

	DPRINTK(3,(KERN_ERR "udf: looking for ISO9660 volume recognition sequence\n"));

	/* Block size must be a multiple of 2048 */
	if (sb->s_blocksize & 2047)
		return block;

	/* Process the sequence (if applicable) */
	for (;;) {
		/* Read a block */
		bh = bread(sb->s_dev, block, sb->s_blocksize);
		if (!bh)
			break;

		/* Look for ISO 9660 descriptors */
		vsd = (struct VolStructDesc *)bh->b_data;

		if (IS_STRICT(sb)
			&& !strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN)
		) {
			PRINTK((KERN_ERR "udf: ISO9660 vrs failed strict test.\n"));
			brelse(bh);
			break;
		}

		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN)) {
			save_bea=block;
		}
		if ( !IS_STRICT(sb) ) {
			if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN)) {
				brelse(bh);
				break;	
			}
		}

		/* Sequence complete on terminator */
		if (vsd->structType == 0xff) {
			PRINTK((KERN_ERR "udf: ISO9660 vrs failed, structType == 0xff\n"));
			brelse(bh);
			break;
		}

		block++;
		brelse(bh);
	}

	if (IS_STRICT(sb) && vsd && vsd->structType != 0xff)
		printk(KERN_ERR "udf: Incorrect volume recognition sequence. "
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
static __inline__ int
udf_nsr02_vrs(struct super_block *sb, long block)
{
	struct VolStructDesc *vsd = NULL;
	int block_inc = 2048 >> sb->s_blocksize_bits;
	int is_nsr = 0;
	struct buffer_head *bh;

	DPRINTK(3,(KERN_ERR "udf: looking for ISO13346 volume recognition sequence\n"));

	/* Avoid infinite loops */
	if (!block_inc) {
		printk(KERN_ERR "udf: can't handle blocks > 2048\n");
		return 0;
	}

	/* Look for beginning of extended area */
	if (IS_STRICT(sb)) {
		bh = bread(sb->s_dev, block++, sb->s_blocksize);
		if (!bh)
			return 0;
		vsd = (struct VolStructDesc *)bh->b_data;
		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN)) {
			printk(KERN_ERR "udf: not an NSR compliant volume\n");
			brelse(bh);
			return 0;
		}
	}

	/* Process the extended area */
	for (;;) {
		/* Read a block */
		bh = bread(sb->s_dev, block, sb->s_blocksize);
		if (!bh)
			break;

		/* Process the descriptor */
		vsd = (struct VolStructDesc *)bh->b_data;

		if ( udf_debuglvl > 2 )
		  printk(KERN_ERR "udf: block %ld found '%5.5s' header\n",
				block, vsd->stdIdent);

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
		block += block_inc;
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
static __inline__ int
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
 
	ablock=256;
	bh = udf_read_tagged(sb, ablock,0);
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=512;
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=lastblock-256;
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=lastblock;
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=512;
		bh = udf_read_tagged(sb, ablock,0);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		printk(KERN_ERR "udf: couldn't find an anchor\n");
		return 1;
	}
	UDF_SB_ANCHOR(sb)=ablock;
	memcpy(ap, bh->b_data, sizeof(struct AnchorVolDescPtr));
	brelse(bh);
	return 0;
}

static int 
udf_find_fileset(struct super_block *sb)
{
	struct buffer_head *bh;
	tag * tagp;
	long block, lastblock, offset;
	int done=0;

	offset=block=UDF_SB_PARTROOT(sb);

	/* lastblock=UDF_SB_LASTBLOCK(sb); */ /* don't know how yet */
	lastblock=block+100; /* look in the first 100 sectors */

	while ( (!done) && (block < lastblock)) {
	    	bh = udf_read_tagged(sb, block, offset);
	    	if (!bh) {
			done=1;
			printk(KERN_ERR "udf: lineno= %d read_tagged(%ld) failed\n",
				__LINE__, block);
			break;
	    	}
	    
	    	tagp=(tag *)bh->b_data;

		switch ( tagp->tagIdent ) {
		case SPACE_BITMAP_DESC:
			{
				struct SpaceBitmap *sp;
				sp=(struct SpaceBitmap *)bh->b_data;
				block +=sp->numOfBytes/sb->s_blocksize;
				/* skip bitmap sectors */
			}
			break;
		case FILE_SET_DESC:
			if ( udf_debuglvl )
			    printk(KERN_ERR "udf: FileSet(%d) at block %ld\n",
				UDF_SB_PARTITION(sb), block);
			UDF_SB_FILESET(sb)=block;
			udf_load_fileset(sb, bh);
			done=1;
			break;
		case FILE_ENTRY:
			break;
		default:
			/*
			if ( tagp->tagIdent )
			    printk(KERN_ERR "udf: tag %x found at block %lu\n",
				tagp->tagIdent, block);
			*/
			break;
		}
		brelse(bh);

	    	block++;
	}
	if ( UDF_SB_FILESET(sb) )
		return 0;
	return 1;
}

static void 
udf_load_pvoldesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PrimaryVolDesc *pvoldesc;
	pvoldesc=(struct PrimaryVolDesc *)bh->b_data;
}

static void 
udf_load_fileset(struct super_block *sb, struct buffer_head *bh)
{
	struct FileSetDesc *fset;
	long_ad *r;

	fset=(struct FileSetDesc *)bh->b_data;
	r=&fset->rootDirectoryICB;
	UDF_SB_ROOTDIR(sb)= UDF_SB_PARTROOT(sb)+r->extLocation.logicalBlockNum;

	printk(KERN_ERR "udf: FileSet(%d) RootDir length %u at offset %u (+%u= block %u)\n",

		r->extLocation.partitionReferenceNum,
		r->extLength, 
		r->extLocation.logicalBlockNum,
		UDF_SB_PARTROOT(sb), UDF_SB_ROOTDIR(sb));
	
}

static void 
udf_load_partdesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PartitionDesc *p;
	p=(struct PartitionDesc *)bh->b_data;

	UDF_SB_PARTROOT(sb)=p->partitionStartingLocation;
	UDF_SB_PARTLEN(sb)=p->partitionLength; /* bytes */
	printk(KERN_ERR "udf: partition(%d) start %d length %d\n",
		p->partitionNumber,
		UDF_SB_PARTROOT(sb), UDF_SB_PARTLEN(sb));
}

#ifdef USE_DUMP_VOLDESC
/*
 * given voldesc block bh, dump some info
 */
static void
udf_dump_voldesc(struct super_block *sb, int voldesc)
{
	struct PrimaryVolDesc * p;
	struct buffer_head *bh;
	struct ustr in1, in2;
	struct ustr out1, out2;
	time_t recording;

	bh = bread(sb->s_dev, voldesc, sb->s_blocksize);
	if (!bh)
		return;

	p=(struct PrimaryVolDesc *)bh->b_data;

	memset(&in1, 0, sizeof(struct ustr));
	memset(&out1, 0, sizeof(struct ustr));
	memset(&in2, 0, sizeof(struct ustr));
	memset(&out2, 0, sizeof(struct ustr));

	memcpy(in1.u_name, p->volIdent, 32);
	if ( udf_CS0toUTF8(&out1, &in1) == 0 ) {
	    printk(KERN_ERR "volIdent '%s'\n", out1.u_name+1);
	}

	memcpy(in2.u_name, p->volSetIdent, 128);
	if ( udf_CS0toUTF8(&out2, &in2) == 0 ) {
	    printk(KERN_ERR "volSetIdent '%s'\n", out2.u_name+1);
	}

	if ( udf_stamp_to_time(&recording, &p->recordingDateAndTime) ) {
	    timestamp *ts;
	    ts=&p->recordingDateAndTime;
	    printk(KERN_ERR "recording time %lu, %u/%u/%u %u:%u\n", 
		recording, ts->year, ts->month, ts->day, ts->hour, ts->minute);
	    UDF_SB_RECORDTIME(sb)=recording;
	}
	brelse(bh);
	return;
}
#endif

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
static __inline__ int
udf_process_sequence(struct super_block *sb, long block, long lastblock)
{
	struct buffer_head *bh;
	__u32 descIdent = UNUSED_DESC;

	/* Read the main descriptor sequence */
	for (; (descIdent != TERMINATING_DESC) &&
	       (block <= lastblock); block++) {

		bh = udf_read_tagged(sb, block,0);
		if (!bh) 
			break;

		/* Process each descriptor (ISO 13346 3/8.3-8.4) */
		descIdent = ((tag *)bh->b_data)->tagIdent;
		switch (descIdent) {
			case PRIMARY_VOL_DESC: /* ISO 13346 3/10.1 */
			udf_load_pvoldesc(sb, bh);
			break;

			case VOL_DESC_PTR: /* ISO 13346 3/10.3 */
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

			case IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
			break;

			case PARTITION_DESC: /* ISO 13346 3/10.5 */
			udf_load_partdesc(sb, bh);
			break;

			case LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
			break;

			case UNALLOC_SPACE_DESC: /* ISO 13346 3/10.8 */
			break;

			case TERMINATING_DESC: /* ISO 13346 3/10.9 */
			break;

			case FILE_SET_DESC:
			break;

			case FILE_IDENT_DESC:
			break;
	
			default:
			if ( udf_debuglvl ) 
			  printk(KERN_ERR "udf: tag 0x%x at block %ld\n",
				descIdent, block);	
			break;
			/*descIdent = TERMINATING_DESC;*/
		}
		brelse(bh);
	}
	return 0;
}

/*
 * udf_check_valid()
 */
static int
udf_check_valid(struct super_block *sb)
{
	long block;

	/* Check that it is NSR02 compliant */
	/* Process any "CD-ROM Volume Descriptor Set" (ECMA 167 2/8.3.1) */
	block = udf_cd001_vrs(sb);
	if (!block) 	/* block = begining of extended area block */
		return 1;

	/* Check that it is NSR02 compliant */
	if (!udf_nsr02_vrs(sb, block)) { /* expects block = BEA block */
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

	/*
	udf_dump_voldesc(sb, UDF_SB_VOLDESC(sb));
	*/

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
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLDESC(sb)=0;
	UDF_SB_LASTBLOCK(sb)=0;
	UDF_SB_FILESET(sb)=0;
	UDF_SB_RECORDTIME(sb)=0;
	UDF_SB_PARTROOT(sb)=0;
	UDF_SB_PARTLEN(sb)=0;
	UDF_SB_FILECOUNT(sb)=0;

	/* Parse any mount options */
	if (udf_parse_options(sb, (char *)options))
		goto error_out;

	/* Set the block size for all transfers */
	if (udf_set_blocksize(sb))
		goto error_out;

	if (udf_check_valid(sb)) /* read volume recognition sequences */
		goto error_out;

	/* Find an anchor volume descriptor pointer */
	/* dgb: how do we determine the last block before this point? */
	if (udf_find_anchor(sb, lastblock, &anchor)) {
		printk(KERN_ERR "udf: no anchor block found\n");
		goto error_out;
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

	/* Read the root inode */
/*
	inode = udf_namei(sb, & (const struct qstr) { "/", 0, 0 });
*/
	inode = udf_iget(sb, UDF_ROOT_INODE);
	if (!inode)
		goto error_out;

#if LINUX_VERSION_CODE > 0x020170
	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if (!sb->s_root) {
		iput(inode);
		printk(KERN_ERR "udf: couldn't allocate root dentry\n");
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
	tmp.f_blocks = UDF_SB_PARTLEN(sb)/sb->s_blocksize;
	if ( (UDF_SB_PARTLEN(sb) % sb->s_blocksize) != 0 )
		tmp.f_blocks++;
	tmp.f_bfree = 0L;
	tmp.f_bavail = 0L;
	tmp.f_files = UDF_SB_FILECOUNT(sb);
	tmp.f_ffree = 0L;
	/* __kernel_fsid_t f_fsid */
	tmp.f_namelen = UDF_NAME_LEN;

	rc= copy_to_user(buf, &tmp, size) ? -EFAULT: 0;
#if LINUX_VERSION_CODE > 0x020100
	return rc;
#endif
}
