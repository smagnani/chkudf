/*
 * super.c
 *
 * PURPOSE
 *	Super block routines for UDF filesystem.
 *
 * DESCRIPTION
 *	OSTA-UDF(tm) = Optical Storage Technology Association
 *	Universal Disk Format.
 *
 *	This code is based on version 1.50 of the UDF specification:
 *		http://www.osta.org/
 *	and revision 2 of the ECMA 167 standard
 *		http://www.ecma.ch/
 *	which is equivalent to ISO 13346.
 *		http://www.iso.org/
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/config.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/udf_fmt.h>

#include <asm/byteorder.h>
#include <asm/uaccess.h>

#include "debug.h"


/* External Prototypes */
extern void udf_read_inode(struct inode *);
#ifdef CONFIG_UDF_WRITE
extern void udf_write_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern void udf_notify_change(struct inode *);
#endif

/* Internal  Prototypes */
struct udf_options {
	mode_t mode;
	gid_t gid;
	uid_t uid;
	char  test_only;
	char  rw;	
};

static int udf_parse_options(char *, struct udf_options *);
static struct super_block *udf_read_super(struct super_block *, void *, int);
static void udf_put_super(struct super_block *);
static int udf_statfs(struct super_block *, struct statfs *, int);

static struct file_system_type udf_fstype = {
	"udf",			/* name */
	FS_REQUIRES_DEV,	/* fs_flags */
	udf_read_super,		/* read_super */
	NULL			/* next */
};

static struct super_operations udf_sb_ops = {
	udf_read_inode,		/* read_inode */
#ifdef CONFIG_UDF_WRITE
	udf_write_inode,	/* write_inode */
	udf_put_inode,		/* put_inode */
	udf_delete_inode,	/* delete_inode */
	udf_notify_change_inode,/* notify_change */
#else
	NULL,			/* write_inode */
	NULL,			/* put_inode */
	NULL,			/* delete_inode */
	NULL,			/* notify_change */
#endif
	udf_put_super,		/* put_super */
	NULL,			/* write_super */
	udf_statfs,		/* statfs */
	NULL			/* remount_fs */
};


#if defined(MODULE)
int test_only=0;	/* if mounting is unsafe! */
MODULE_PARM(test_only,"i"); /* fail all mounts after dumping debugging info */

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
	return register_filesystem(&udf_fstype);
}

/*
 * udf_parse_options
 *
 * PURPOSE
 *	Parse any mount options
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_parse_options(char *options, struct udf_options * uopt)
{
	char *this_char;
	/*char, *value;*/

	uopt->test_only=test_only; /* module parm */
	uopt->rw=0;
	uopt->mode= S_IRUGO| S_IXUGO;
	uopt->gid=0;
	uopt->uid=0;

	if (!options) return 1;
	for (this_char = strtok(options,","); this_char; this_char = strtok(NULL,",")) {
		if ( strncmp(this_char,"test_only",9) == 0) {
		  uopt->test_only=1;
		  continue;
		}
		if ( strncmp(this_char,"notest_only",11) == 0) {
		  uopt->test_only=0;
		  continue;
		}
		if ( strncmp(this_char,"rw",2) == 0) {
		  uopt->rw=1;
		  continue;
		}
		if ( strncmp(this_char,"ro",2) == 0) {
		  uopt->rw=0;
		  continue;
		}
		/* need uid, gid, mode! */
	}
	return 1;
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
	kdev_t dev;
	__u32 i;
	int block, blocksize, blocksize_bits, block_inc, is_udf, lastblock;
	struct buffer_head *bh;
	struct udf_options opt;
	struct inode	   *inode;

	/* Lock the module in memory (if applicable) */
	MOD_INC_USE_COUNT;
	/* lock before any blocking operations */
	lock_super(sb);

	/* Parse any mount options */
	if (!udf_parse_options((char *)options, &opt))
		goto out_unlock;


	/* Set block size - bigger is better (ISO 13346 3/8.1.2, UDF 1.50/2) */
	dev = sb->s_dev;
	blocksize = get_hardblocksize(dev);
	PRINTK((KERN_NOTICE "udf: hardblocksize=%d\n", blocksize));
	if (blocksize < UDF_BLOCK_SIZE)
		blocksize = UDF_BLOCK_SIZE;
	if (blocksize % 512) {
		printk(KERN_ERR "udf: bad block size (%d)\n", blocksize);
		goto out_unlock;
	}
	set_blocksize(dev, blocksize);
	sb->s_blocksize = blocksize;
	PRINTK((KERN_NOTICE "udf: blocksize=%d\n", blocksize));

	/* Get the block size in bits */
	blocksize_bits = sb->s_blocksize_bits = count_bits(blocksize);
	PRINTK((KERN_NOTICE "udf: blocksize_bits=%d\n", blocksize_bits));

	/* Last block of media */
	lastblock = 256;
	PRINTK((KERN_NOTICE "udf: lastblock= %d\n", lastblock));
	
	/* Volume recognition area (ISO 13346 2/8.3) */
	is_udf = 0;
	block = 32768 >> blocksize_bits;
	block_inc = UDF_BLOCK_SIZE >> blocksize_bits;
	for (; block <= lastblock; block += block_inc) {
		struct VolStructDesc *vsd;

		/* Read a block from the recognition sequence */
		bh = bread(dev, block, blocksize);
		if (!bh) {
			printk(KERN_WARNING "udf_read_super: "
			  "bread failed, dev=%s, block=%u, blocksize=%u\n",
			  kdevname(dev), block, blocksize);
			goto out_unlock;
		}

		/* See if descriptor is useful */
		vsd = (struct VolStructDesc *)bh->b_data;
		if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN)) {
			/* Begin Extended Area */
			PRINTK((KERN_NOTICE "udf: STD_ID_BEA01\n"));
		} else if (!strncmp(vsd->stdIdent, STD_ID_BOOT2, STD_ID_LEN)) {
			/* Boot code */
			PRINTK((KERN_NOTICE "udf: STD_ID_BOOT2\n"));
		} else if (!strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN)) {
			/* ISO 9660 / ECMA 119 */
			PRINTK((KERN_NOTICE "udf: STD_ID_CD001\n"));
		} else if (!strncmp(vsd->stdIdent, STD_ID_CDW02, STD_ID_LEN)) {
			/* ECMA 168 */
			PRINTK((KERN_NOTICE "udf: STD_ID_CDW02\n"));
		} else if (!strncmp(vsd->stdIdent, STD_ID_NSR02, STD_ID_LEN)) {
			/* ISO 13346 / ECMA 167 */
			PRINTK((KERN_NOTICE "udf: STD_ID_NSR02\n"));
			is_udf = 1;
		} else if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN)) {
			/* Terminate Extended Area */
			PRINTK((KERN_NOTICE "udf: STD_ID_TEA01\n"));
			block = lastblock;
		} else {
			PRINTK((KERN_NOTICE "udf: unknown standard\n"));
			block = lastblock;
		}
		DEBUG_DUMP(bh);
		brelse(bh);
	}

	/* Check that it is NSR compliant */
	if (!is_udf) {
		if (!silent)
			printk(KERN_ERR "udf: not a valid UDF filesystem\n");
		goto out_unlock;
	}

	/* Search for an anchor volume descriptor pointer */
	bh = udf_read_tagged(sb, 256);
	if (!bh || ((tag *)(bh->b_data))->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		bh = udf_read_tagged(sb, 512);
	}
	if (!bh || ((tag *)(bh->b_data))->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		bh = udf_read_tagged(sb, lastblock - 256);
	}
	if (!bh || ((tag *)(bh->b_data))->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		bh = udf_read_tagged(sb, lastblock);
	}
	if (!bh || ((tag *)(bh->b_data))->tagIdent != ANCHOR_VOL_DESC_PTR) {
		brelse(bh);
		if (!silent)
			printk(KERN_ERR "udf: couldn't find an anchor");
		goto out_unlock;
	}

	/*DEBUG_CRUMB;*/
	DEBUG_DUMP(bh);

	/* Locate the main descriptor sequence */
	lastblock = block = le32_to_cpu(((struct AnchorVolDescPtr *)
		(bh->b_data))->mainVolDescSeqExt.extLocation);
	lastblock += le32_to_cpu(((struct AnchorVolDescPtr *)(bh->b_data))
		->mainVolDescSeqExt.extLength) >> blocksize_bits;

	brelse(bh);

	/* Read the main descriptor sequence */
	for (i = 0; i != TERMINATING_DESC && block <= lastblock; block++) {
		bh = udf_read_tagged(sb, block);
		if (!bh)
			break;

		/* Process each descriptor (ISO 13346 3/8.3-8.4) */
		i = ((tag *)(bh->b_data))->tagIdent;
		switch (i) {
			case PRIMARY_VOL_DESC: /* ISO 13346 3/10.1 */
			PRINTK((KERN_NOTICE "udf: PRIMARY_VOL_DESC\n"));
			break;

			case VOL_DESC_PTR: /* ISO 13346 3/10.3 */
			PRINTK((KERN_NOTICE "udf: VOL_DESC_PTR\n"));
			lastblock = block = le32_to_cpu(
				((struct AnchorVolDescPtr *)(bh->b_data))
				->mainVolDescSeqExt.extLocation);
			lastblock += le32_to_cpu(((struct AnchorVolDescPtr *)
				(bh->b_data))->mainVolDescSeqExt.extLength)
				 >> blocksize_bits;
			break;

			case IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
			PRINTK((KERN_NOTICE "udf: IMP_USE_VOL_DESC\n"));
			break;

			case PARTITION_DESC: /* ISO 13346 3/10.5 */
			PRINTK((KERN_NOTICE "udf: PARTITION_DESC\n"));
			break;

			case LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
			PRINTK((KERN_NOTICE "udf: LOGICAL_VOL_DESC\n"));
			break;

			case UNALLOC_SPACE_DESC: /* ISO 13346 3/10.8 */
			PRINTK((KERN_NOTICE "udf: UNALLOC_SPACE_DESC\n"));
			break;

			case TERMINATING_DESC: /* ISO 13346 3/10.9 */
			PRINTK((KERN_NOTICE "udf: TERMINATING_DESC\n"));
			break;

			default:
			i = TERMINATING_DESC;
			PRINTK((KERN_NOTICE "udf: unexpected descriptor\n"));

		}
		DEBUG_DUMP(bh);
		brelse(bh);
	}
	
	/* Set s_time to last closed integrity descriptor */
	sb->s_time = 0;

	/* Fill in the rest of the superblock */
	sb->s_magic = UDF_SUPER_MAGIC;
	
	if ( !opt.rw )
		sb->s_flags |= MS_RDONLY;
	sb->s_flags |= MS_NODEV | MS_NOSUID; /* should be overridden by mount options */
	sb->s_op = &udf_sb_ops;
	sb->dq_op = NULL;
	sb->s_dirt = 0;

	if ( opt.test_only ) 
		goto out_unlock;

	inode = iget(sb, UDF_ROOT_INODE);
	PRINTK((KERN_NOTICE "udf: inode = %ld\n", inode->i_ino));

	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if ((!sb->s_root)) {
		printk(KERN_ERR "udf: couldn't allocate root dentry\n");
		iput(inode);
		goto out_unlock;
	}

	if(!check_disk_change(dev)) {
		unlock_super(sb);
		return sb; /* NORMAL return */
	}

	/* disk changed */
	dput(sb->s_root);
	goto out_unlock;

out_unlock:
	sb->s_dev = NODEV;
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
