/*
 * super.c
 *
 * CHANGES
 * 9/24/98 dgb:	changed to allow compiling outside of kernel, and
 *		added some debugging.
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

#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/slab.h>	/* for kmalloc */
#include <asm/byteorder.h>
#include <asm/uaccess.h>


#include <config/udf.h>
#include <linux/udf_167.h>
#include <linux/udf_udf.h>
#include <linux/udf_fs_sb.h>
#include <linux/udf_fs_i.h>

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
static int udf_statfs(struct super_block *, struct statfs *, int);

/* some debug stuff */
static void udf_dump_fileset(struct super_block *, struct buffer_head *);

/* UDF filesystem type */
static struct file_system_type udf_fstype = {
	"udf",			/* name */
	FS_REQUIRES_DEV,	/* fs_flags */
	udf_read_super,		/* read_super */
	NULL			/* next */
};

/* Superblock operations */
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

/* Debugging level. Affects all UDF filesystems! */
int udf_debuglvl=0;
int udf_strict=0;
int udf_dumpfileset=1;

#if defined(MODULE)
MODULE_PARM(udf_debuglvl, "i");
MODULE_PARM(udf_strict, "i");
MODULE_PARM(udf_dumpfileset, "i");

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
		if (!strcmp(opt, "figed") && !val)
			UDF_SB(sb)->s_flags |= UDF_FLAG_FIXED;
		else if (!strcmp(opt, "gid") && val)
			UDF_SB(sb)->s_gid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "mode") && val)
			UDF_SB(sb)->s_mode = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "relaxed") && !val)
			UDF_SB(sb)->s_flags &= ~UDF_FLAG_STRICT;
		else if (!strcmp(opt, "uid") && val)
			UDF_SB(sb)->s_uid = simple_strtoul(val, NULL, 0);
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
	blocksize = sb->s_blocksize ? sb->s_blocksize:
		get_hardblocksize(sb->s_dev);

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

	DPRINTK(2,(KERN_ERR "udf: looking for ISO9660 volume recognition sequence\n"));

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

	DPRINTK(2,(KERN_ERR "udf: looking for ISO 13346 volume recognition sequence\n"));

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

		if ( udf_debuglvl > 1 )
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
 *	<return>		Pointer to buffer or NULL.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static __inline__ struct buffer_head *
udf_find_anchor(struct super_block *sb, long lastblock)
{
	struct buffer_head *bh;
	long ablock;

	DPRINTK(2,(KERN_ERR "udf: looking for anchor volume descriptor (0x02)\n"));

	/* Search for an anchor volume descriptor pointer */
	ablock=256;
	bh = udf_read_tagged(sb, ablock);
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=512;
		bh = udf_read_tagged(sb, ablock);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=lastblock-256;
		bh = udf_read_tagged(sb, ablock);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		ablock=lastblock;
		bh = udf_read_tagged(sb, ablock);
	}
	if (!bh || ((tag *)bh->b_data)->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		printk(KERN_ERR "udf: couldn't find an anchor\n");
		return NULL;
	}
	UDF_SB_ANCHOR(sb)=ablock;
	return bh;
}

/*
 * given fileset block bh, dump some info
 */
static void
udf_dump_fileset(struct super_block *sb, struct buffer_head *bh)
{
	DUMP(bh);	
	return;
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
static __inline__ int
udf_process_sequence(struct super_block *sb, long block, long lastblock)
{
	struct buffer_head *bh;
	__u32 descIdent = UNUSED_DESC;

	DPRINTK(2,(KERN_ERR "udf: udf_process_sequence begin\n"));

	/* Read the main descriptor sequence */
	for (; descIdent != TERMINATING_DESC && block <= lastblock; block++) {
		bh = udf_read_tagged(sb, block);
		if (!bh) 
			break;

		/* Process each descriptor (ISO 13346 3/8.3-8.4) */
		descIdent = ((tag *)bh->b_data)->tagIdent;
		if ( udf_debuglvl )
			printk(KERN_ERR "udf: sequence: tag 0x%x block %ld\n",
				descIdent, block);
		switch (descIdent) {
			case PRIMARY_VOL_DESC: /* ISO 13346 3/10.1 */
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
			  printk(KERN_ERR "udf: lastblock = %lu\n",
					lastblock);
			break;

			case IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
			break;

			case PARTITION_DESC: /* ISO 13346 3/10.5 */
			break;

			case LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
			if (udf_debuglvl)
			  DUMP(bh);
			break;

			case UNALLOC_SPACE_DESC: /* ISO 13346 3/10.8 */
			break;

			case TERMINATING_DESC: /* ISO 13346 3/10.9 */
			break;

			case FILE_SET_DESC:
			/*if (udf_dumpfileset) */
				udf_dump_fileset(sb, bh);
			break;

			case FILE_IDENT_DESC:
			if ( udf_debuglvl ) {
			  static int seen=0;
			  if (!seen) {
			    seen=1;
			    printk(KERN_ERR "udf: FILE_IDENT_DESC at blk %lu\n", block);
			  }
			}
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
	long block, lastblock;
	long main_s, main_e, reserve_s, reserve_e;
	struct buffer_head *bh;

	/* Lock the module in memory (if applicable) */
	MOD_INC_USE_COUNT;

	lock_super(sb);
	UDF_SB_ALLOC(sb); /* kmalloc, if needed */
	UDF_SB_ANCHOR(sb)=0;
	UDF_SB_VOLDESC(sb)=0;
	UDF_SB_LASTBLOCK(sb)=0;
	UDF_SB_FILESET(sb)=0;

	/* Parse any mount options */
	if (udf_parse_options(sb, (char *)options))
		goto error_out;

	/* Set the block size for all transfers */
	if (udf_set_blocksize(sb))
		goto error_out;

	/* Check that it is NSR02 compliant */
	/* Process any "CD-ROM Volume Descriptor Set" (ECMA 167 2/8.3.1) */
	block = udf_cd001_vrs(sb);
	if (!block) /* block = begining of extended area block */
		goto error_out;

	/* Check that it is NSR02 compliant */
	if (!udf_nsr02_vrs(sb, block)) { /* expects block = BEA block */
		printk(KERN_ERR "udf: volume is not NSR02 compliant\n");
		goto error_out;
	}

	/* Get the last block of media */
	lastblock = 257; 
	if (lastblock < 257) {
		printk(KERN_ERR "udf: media is too small\n");
		goto error_out;
	}

	/* Find an anchor volume descriptor pointer */
	bh = udf_find_anchor(sb, lastblock);
	if (!bh)
		goto error_out;

	/* Locate the main sequence */
	main_s = le32_to_cpu(((struct AnchorVolDescPtr *)
		bh->b_data)->mainVolDescSeqExt.extLocation);
	main_e = le32_to_cpu(((struct AnchorVolDescPtr *)bh->b_data)
		->mainVolDescSeqExt.extLength) >> sb->s_blocksize_bits;
	main_e += main_s;
	if (udf_debuglvl)
		printk(KERN_ERR "udf: main_s = %ld main_e = %ld\n",
			main_s, main_e);
	UDF_SB_VOLDESC(sb)=main_s;
	/*lastblock += block;*/
	lastblock = main_e;
	UDF_SB_LASTBLOCK(sb)=lastblock;

	if ( udf_debuglvl )
		printk(KERN_ERR "udf: anchor %d last %d voldesc %d\n",
			UDF_SB_ANCHOR(sb), UDF_SB_LASTBLOCK(sb),
			UDF_SB_VOLDESC(sb));

	/* Locate the reserve sequence */
	reserve_s = le32_to_cpu(((struct AnchorVolDescPtr *)
		bh->b_data)->mainVolDescSeqExt.extLocation);
	reserve_e = le32_to_cpu(((struct AnchorVolDescPtr *)bh->b_data)
		->mainVolDescSeqExt.extLength) >> sb->s_blocksize_bits;
	reserve_e += reserve_s;
	brelse(bh);

	/* Process the main sequence */
	if (udf_process_sequence(sb, block, lastblock) &&
		udf_process_sequence(sb, reserve_s, reserve_e))
		goto error_out;

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

	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if (!sb->s_root) {
		iput(inode);
		printk(KERN_ERR "udf: couldn't allocate root dentry\n");
		goto error_out;
	}

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
	/*
	lock_super(sb);
	sb->s_dev = NODEV;
	unlock_super(sb);
	*/
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
static int
udf_statfs(struct super_block *sb, struct statfs *buf, int bufsize)
{
	int size;
	struct statfs tmp;

	size = (bufsize < sizeof(tmp)) ? bufsize: sizeof(tmp);

	memset(&tmp, 0, sizeof(tmp));
	tmp.f_type = UDF_SUPER_MAGIC;
	tmp.f_bsize = sb->s_blocksize;
	tmp.f_blocks = 1L;
	tmp.f_bfree = 0L;
	tmp.f_bavail = 0L;
	tmp.f_files = 1L;
	tmp.f_ffree = 0L;
	/* __kernel_fsid_t f_fsid */
	tmp.f_namelen = UDF_NAME_LEN;

	return copy_to_user(buf, &tmp, size) ? -EFAULT: 0;
}
