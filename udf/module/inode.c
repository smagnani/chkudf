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
#include <linux/udf_fs.h>

#include "udfdecl.h"

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
void
udf_read_inode(struct inode *inode)
{
	struct buffer_head *bh;
	struct FileEntry *fe;
	time_t modtime;
	int block;

	COOKIE(("udf_read_inode: inode=0x%lx\n", (unsigned long)inode));

	block=udf_block_from_inode(inode->i_sb, inode->i_ino);
	bh=udf_read_tagged(inode->i_sb, block, UDF_BLOCK_OFFSET(inode->i_sb));
	if ( !bh ) {
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) block %d failed !bh\n",
			inode->i_ino, block);
		return;
	}

	fe=(struct FileEntry *)bh->b_data;
	if ( fe->descTag.tagIdent == TID_FILE_ENTRY) {
		printk(KERN_INFO
	"udf: inode %ld FILE_ENTRY: perm 0x%x link %d type %x\n",
			inode->i_ino, 
			fe->permissions, 
			fe->fileLinkCount, fe->icbTag.fileType);

		inode->i_uid = udf_convert_uid(fe->uid);
		if ( !inode->i_uid ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

		inode->i_gid = udf_convert_gid(fe->gid);
		if ( !inode->i_gid ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

		inode->i_nlink = fe->fileLinkCount;
		inode->i_size = udf64_low32(fe->informationLength);

		if ( udf_stamp_to_time(&modtime, &fe->modificationTime) ) {
			inode->i_atime = modtime;
			inode->i_mtime = modtime;
			inode->i_ctime = modtime;
		} else {
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
		}

		switch (fe->icbTag.fileType) {
		case FILE_TYPE_DIRECTORY:
			inode->i_op = &udf_dir_inode_operations;;
			inode->i_mode = S_IFDIR|S_IRUGO|S_IXUGO;
			break;
		case FILE_TYPE_REGULAR:
			inode->i_op = &udf_file_inode_operations;
			inode->i_mode = S_IFREG|S_IRUGO;
			break;
		case FILE_TYPE_SYMLINK:
			inode->i_op = &udf_file_inode_operations;
			inode->i_mode = S_IFLNK|S_IRUGO|S_IXUGO;
			break;
		}
	} else {
		printk(KERN_ERR "udf: inode %ld is tag 0x%x, not FILE_ENTRY\n",
			inode->i_ino, ((tag *)bh->b_data)->tagIdent );
	}
	udf_release_data(bh);
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
void
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
void
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
void
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
struct inode *
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

	if ( ino >= UDF_SB_PARTLEN(sb) ) {
		printk(KERN_ERR "udf: iget(,%ld) out of range\n",
			ino);
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
	/*
	if (inode->i_nlink)
		printk(KERN_WARNING "udf: i_nlink != 0, %d\n",
			inode->i_nlink);
	*/

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
	 *	(!)i_op = NULL
	 *	i_nlink = 0
	 */
	inode->i_blksize = sb->s_blocksize;
	inode->i_mode = UDF_SB(sb)->s_mode;
	inode->i_gid = UDF_SB(sb)->s_gid;
	inode->i_uid = UDF_SB(sb)->s_uid;

	/* Mark the inode as empty */
	inode->i_op = NULL; 
	inode->i_nlink = 0; 

	inode->i_version = 1;

	UDF_I_VOL(inode) = 0; /* for volume sets, leave 0 for now */
	UDF_I_BLOCK(inode) = inode->i_ino;

	udf_read_inode(inode);

	return inode;
}

