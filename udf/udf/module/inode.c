/*
 * inode.c
 *
 * PURPOSE
 *	Inode handling routines for the OSTA-UDF(tm) filesystem.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/udf_fmt.h>

/* External Prototypes */
void udf_read_inode(struct inode *);

#ifdef CONFIG_UDF_WRITE
void udf_write_inode(struct inode *);
void udf_put_inode(struct inode *);
void udf_delete_inode(struct inode *);
void udf_notify_change_inode(struct inode *);
#endif

/*
 * udf_read_inode
 *
 * PURPOSE
 *	Read the specified inode in order to complete the inode structure.
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel puts an inode into use.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
udf_read_inode(struct inode *inode)
{
	printk(KERN_INFO "udf_read_inode() i_ino=%lu\n", inode->i_ino);
	goto fail;

fail:
	inode->i_mtime = inode->i_atime = inode->i_ctime = 0;
	inode->i_size = 0;
	inode->i_blocks = inode->i_blksize = 0;
	inode->i_nlink = 1;
	inode->i_uid = inode->i_gid = 0;
	if ( inode->i_ino != UDF_ROOT_INODE )
		inode->i_mode = S_IFREG;
	else
		inode->i_mode = S_IFDIR;
	inode->i_op = NULL;
	return;
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
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
udf_write_inode(struct inode *inode)
{
	printk(KERN_INFO "udf_read_inode() i_ino=%u\n", inode->i_ino);
}

/*
 * udf_put_inode
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel no longer needs the inode.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
udf_put_inode(struct inode *inode)
{
	printk(KERN_INFO "udf_put_inode() i_ino=%u\n", inode->i_ino);
}

/*
 * udf_delete_inode
 *
 * PURPOSE
 *	Clean-up before the specified inode is destroyed.
 *
 * DESCRIPTION
 *	This routine is called when the kernel destroys an inode structure.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
udf_delete_inode(struct inode *inode)
{
	printk(KERN_INFO "udf_delete_inode() i_ino=%u\n", inode->i_ino);
}

void 
udf_notify_change_inode(struct inode *inode)
{
	printk(KERN_INFO "udf_notify_change_inode() i_ino=%u\n", inode->i_ino);
}
#endif /* CONFIG_UDF_WRITE */
