/*
 * inode.c
 *
 * PURPOSE
 *	Inode handling routines for the OSTA-UDF(tm) filesystem.
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

#include <config/udf.h>
#include <linux/fs.h>
#include <linux/udf_167.h>
#include <linux/udf_udf.h>
#include <linux/udf_fs_sb.h>
#include <linux/udf_fs_i.h>
#include "debug.h"

/* Defined in dir.c */
/* extern struct inode_operations udf_dir_iops; */

/* External Prototypes */
extern void udf_read_inode(struct inode *);
extern void udf_write_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern struct inode *udf_iget(struct super_block *, unsigned long);

/*
 * udf_read_inode
 *
 * PURPOSE
 *	Read an inode.
 *
 * DESCRIPTION
 *	This routine is called by iget() [replaced by udf_iget()]
 *	when an inode is first read into memory.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern void
udf_read_inode(struct inode *inode)
{
	COOKIE(("udf_read_inode: inode=0x%lx\n", (unsigned long)inode));

	/* Mark the inode as empty */
	inode->i_op = NULL;
	inode->i_nlink = 0;
}

#ifdef CONFIG_UDF_WRITE
/*
 * udf_write_inode
 *
 * PURPOSE
 *	Write out the specified inode.
 *
 * DESCRIPTION
 *	This routine is called whenever an inode is synced.
 *	Currently this routine is just a placeholder.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern void
udf_write_inode(struct inode *inode)
{
	COOKIE(("udf_write_inode: inode=0x%lx\n", (unsigned long)inode));
}
#endif

/*
 * udf_put_inode
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel no longer needs the inode.
 *	Currently this routine is just a placeholder.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern void
udf_put_inode(struct inode *inode)
{
	COOKIE(("udf_put_inode: inode=0x%lx\n", (unsigned long)inode));

	/* Delete unused inodes */
	if (inode && inode->i_count == 1)
		inode->i_nlink = 0;
}

/*
 * udf_delete_inode
 *
 * PURPOSE
 *	Clean-up before the specified inode is destroyed.
 *
 * DESCRIPTION
 *	This routine is called when the kernel destroys an inode structure
 *	ie. when iput() finds i_count == 0.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern void
udf_delete_inode(struct inode *inode)
{
	COOKIE(("udf_delete_inode: inode=0x%lx\n", (unsigned long)inode));

	clear_inode(inode);
}

/*
 * udf_iget
 *
 * PURPOSE
 *	Get an inode.
 *
 * DESCRIPTION
 *	This routine replaces iget() and read_inode().
 *
 * HISTORY
 *	October 3, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern struct inode *
udf_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;

	COOKIE(("udf_iget: ino=0x%lx\n", ino));

	/* Get the inode */
	inode = iget(sb, ino);
	if (!inode) {
		printk(KERN_ERR "udf: iget() failed\n");
		return NULL;
	}

	/* Cached inode - nothing to do */
	if (inode->i_op && inode->i_nlink)
		return inode;

	/* Sanity check */
	if (inode->i_op) {
		printk(KERN_WARNING "udf: i_op != NULL\n");
		inode->i_op = NULL;
	}
	if (inode->i_nlink)
		printk(KERN_WARNING "udf: i_nlink != 0\n");

	/*
	 * Set defaults, but the inode is still incomplete!
	 * Note: iget() sets the following on a new inode:
	 *	i_sb = sb
	 *	i_dev = sb->s_dev;
	 *	i_no = ino
	 *	i_flags = sb->s_flags
	 *	i_count = 1
	 *	i_state = 0
	 * and udf_read_inode() sets these:
	 *	i_op = NULL
	 *	i_nlink = 0
	 */
	inode->i_blksize = sb->s_blocksize;
	inode->i_mode = UDF_SB(sb)->s_mode;
	inode->i_gid = UDF_SB(sb)->s_gid;
	inode->i_uid = UDF_SB(sb)->s_uid;
	if ( inode->i_ino == UDF_ROOT_INODE ) {
		inode->i_mode = S_IFDIR;
		inode->i_mtime = UDF_SB_RECORDTIME(sb);
		inode->i_ctime = UDF_SB_RECORDTIME(sb);
	}

	inode->i_version = 1;

	UDF_I_VOL(inode) = 0;
	UDF_I_PART(inode) = 0;
	UDF_I_BLOCK(inode) = 0;
	UDF_I_SECTOR(inode) = 0;

	udf_read_inode(inode);

	return inode;
}
