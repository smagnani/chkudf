/*
 * file.c
 *
 * PURPOSE
 *	This file attempts to document the kernel VFS.
 *
 * DESCRIPTION
 *	One day I'll have to write a book on this! :-)
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
 *
 * HISTORY
 *
 * 10/2/98 dgb   Attempt to integrate into udf.o
 * 10/7/98       Switched to using generic_readpage, etc., like isofs
 *               And it works!
 * 12/06/98 blf  Added udf_file_read. uses generic_file_read for all cases but
 *               ICB_FLAG_AD_IN_ICB.
 */


#include "udfdecl.h"
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "udf_i.h"
#include "udf_sb.h"

typedef void * poll_table; 

/* Prototypes for file operations */

#ifdef CONFIG_UDF_WRITE
static long udf_write(struct inode *, struct file *, const char *,
	unsigned long);
static int udf_fsync(struct file *, struct dentry *);
static int udf_fasync(struct file *, int);
static int udf_lock(struct file *, int, struct file_lock *);
static int udf_revalidate(kdev_t dev);
static int udf_flush(struct file *);
#endif

static ssize_t udf_file_read(struct file *, char *, size_t, loff_t *);
#ifdef CONFIG_UDF_FULL_FS
static loff_t udf_llseek(struct file *filp, loff_t offset, int origin);
static int udf_ioctl(struct inode *, struct file *, unsigned int,
	unsigned long);
static int udf_release(struct inode *, struct file *);
static int udf_open(struct inode *, struct file *);

static unsigned int udf_poll(struct file *, poll_table *);
static int udf_mmap(struct file *, struct vm_area_struct *);
static int udf_check_media_change(kdev_t dev);
#endif


struct file_operations udf_file_fops = {
	NULL,			/* llseek */
	udf_file_read,	/* read */
#ifdef CONFIG_UDF_WRITE
	udf_write,		/* write */
#else
	NULL,			/* write */
#endif
	NULL,			/* readdir */
#ifdef CONFIG_UDF_FULL_FS
	udf_poll,		/* poll */
	udf_ioctl,		/* ioctl */
	udf_mmap,		/* mmap */
	udf_open,		/* open */
	udf_flush,		/* flush */
	udf_release,		/* release */
#else
	NULL,			/* poll */
	NULL,			/* ioctl */
	generic_file_mmap,	/* mmap */
	NULL,			/* open */
	NULL,			/* flush */
	NULL,			/* release */
#endif
#ifdef CONFIG_UDF_WRITE
	udf_fsync,		/* fsync */
	udf_fasync,		/* fasync */
	udf_check_media_change,	/* check_media_change */
	udf_revalidate,		/* revalidate */
	udf_lock		/* lock */
#else
	NULL,			/* fsync */
	NULL,			/* fasync */
	NULL,			/* check_media_change */
	NULL,			/* revalidate */
	NULL			/* lock */
#endif
};

#ifdef USE_UDF_LLSEEK
/*
 * udf_llseek
 *
 * PURPOSE
 *	Change the current file position.
 *
 * DESCRIPTION
 *	Optional - the kernel will call default_llseek() if this routine is not
 *	available.
 *
 *	Refer to sys_llseek() in fs/read_write.c
 *	sys_llseek() -> llseek() -> .
 *
 * PRE-CONDITIONS
 *	filp			Pointer to the file (never NULL).
 *	offset			Offset to seek (signed!).
 *	origin			Origin to seek from:
 *				0 = start, 1 = end, 2 = current
 *
 * POST-CONDITIONS
 *	<retval>		New file position (>=0), or an error code (<0) 
 *				that sys_llseek() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
loff_t udf_llseek(struct file *filp, loff_t offset, int origin)
{
	printk(KERN_ERR "udf: udf_llseek(,%ld, %d)\n",
		(long)offset, origin);
	return -1;
}
#endif

/*
 * udf_file_read
 *
 * PURPOSE
 *	Read from an open file.
 *
 * DESCRIPTION
 *	Optional - sys_read() will return -EINVAL if this routine is not
 *	available.
 *
 *	Refer to sys_read() in fs/read_write.c
 *	sys_read() -> .
 *
 *	Note that you can use generic_file_read() instead, which requires that
 *	udf_readpage() be available, but you can use generic_readpage(), which
 *	requires that udf_bmap() be available. Reading will then be done by
 *	memory-mapping the file a page at a time. This is not suitable for
 *	devices that don't handle read-ahead [example: CD-R/RW that may have
 *	blank sectors that shouldn't be read].
 *
 *	Refer to generic_file_read() in mm/filemap.c and to generic_readpage()
 *	in fs/buffer.c
 *
 *	Block devices can use block_read() instead. Refer to fs/block_dev.c
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode to read from (never NULL).
 *	filp			Pointer to file to read from (never NULL).
 *	buf			Point to read buffer (validated).
 *	bufsize			Size of read buffer.
 *
 * POST-CONDITIONS
 *	<return>		Bytes read (>=0) or an error code (<0) that
 *				sys_read() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static ssize_t udf_file_read(struct file * filp, char * buf, size_t bufsize, 
	loff_t * loff)
{
        struct inode *inode = filp->f_dentry->d_inode;

	if (!UDF_I_EXT0OFFS(inode))
		return generic_file_read(filp, buf, bufsize, loff);
	else
	{
		Uint32 size, left, pos, block;
		struct buffer_head *bh;

		size = inode->i_size;
		if (*loff > size)
			left = 0;
		else
			left = size - *loff;
		if (left > bufsize)
			left = bufsize;

		if (left <= 0)
			return 0;

		pos = *loff + UDF_I_EXT0OFFS(inode);
		block = udf_bmap(inode, 0);
		if (!(bh = bread(inode->i_dev, block, inode->i_sb->s_blocksize)))
			return 0;
		if (!copy_to_user(buf, bh->b_data + pos, left))
			*loff += left;
		else
			return -EFAULT;

		return left;
	}
}

#ifdef CONFIG_UDF_WRITE
/*
 * udf_write
 *
 * PURPOSE
 *	Write to an open file.
 *
 * DESCRIPTION
 *	Optional - sys_write() will return -EINVAL if this routine is not
 *	available.
 *
 *	Refer to sys_write() in fs/read_write.c
 *	sys_write() -> .
 *
 *	Block devices can use block_write() instead. Refer to fs/block_dev.c
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode to write to (never NULL).
 *	filep			Pointer to file to write to (Never NULL).
 *	buf			Pointer to write buffer (validated).
 *	bufsize			Size of write buffer.
 *
 * POST-CONDITIONS
 *	<return>		Bytes written (>=0) or an error code (<0) that
 *				sys_write() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
long udf_write(struct inode *inode, struct file *filp, const char *buf,
	unsigned long bufsize)
{
	return -1;
}
#endif


#ifdef CONFIG_UDF_FULL_FS
/*?
 * udf_poll
 *
 * PURPOSE
 *	Poll file(s).
 *
 * DESCRIPTION
 *	Optional - sys_poll() uses DEFAULT_POLLMASK if this routine is not
 *	available.
 *
 *	Refer to sys_poll() in fs/select.c
 *	sys_poll() -> do_poll() -> .
 *
 * PRE-CONDITIONS
 *	filp			Pointer to file to poll.
 *	table			Pointer to poll table.
 *
 * POST-CONDITIONS
 *	<return>		Poll mask.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
unsigned int udf_poll(struct file *filp, poll_table *table)
{
	return -1;
}
#endif

/*
 * udf_ioctl
 *
 * PURPOSE
 *	Issue an ioctl.
 *
 * DESCRIPTION
 *	Optional - sys_ioctl() will return -ENOTTY if this routine is not
 *	available, and the ioctl cannot be handled without filesystem help.
 *
 *	sys_ioctl() handles these ioctls that apply only to regular files:
 *		FIBMAP [requires udf_bmap()], FIGETBSZ, FIONREAD
 *	These ioctls are also handled by sys_ioctl():
 *		FIOCLEX, FIONCLEX, FIONBIO, FIOASYNC
 *	All other ioctls are passed to the filesystem.
 *
 *	Refer to sys_ioctl() in fs/ioctl.c
 *	sys_ioctl() -> .
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode that ioctl was issued on.
 *	filp			Pointer to file that ioctl was issued on.
 *	cmd			The ioctl command.
 *	arg			The ioctl argument [can be interpreted as a
 *				user-space pointer if desired].
 *
 * POST-CONDITIONS
 *	<return>		Success (>=0) or an error code (<=0) that
 *				sys_ioctl() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	printk(KERN_ERR "udf: udf_ioctl(ino %lu,, %d,)\n",
		inode->i_ino, cmd);
	return -1;
}

#ifdef CONFIG_UDF_FULL_FS
/*
 * udf_mmap
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_mmap(struct file *filp, struct vm_area_struct *)
{
	return -1;
}

/*
 * udf_open
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_open(struct inode *inode, struct file *filp)
{
	printk(KERN_ERR "udf: udf_open(ino %lu,)\n",
		inode->i_ino);
	return -1;
}

/*
 * udf_release
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ERR "udf: udf_release(ino %lu,)\n",
		inode->i_ino);
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_fsync
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_fsync(struct file *filp, struct dentry *)
{
	return -1;
}

/*
 * udf_fasync
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_fasync(struct file *filp, int)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_FULL_FS
/*
 * udf_check_media_change
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_check_media_change(kdev_t dev)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_revalidate
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_revalidate(kdev_t dev)
{
	return -1;
}

/*
 * udf_lock
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_lock(struct file *filp, int, struct file_lock *)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
static int udf_create(struct inode *, struct dentry *, int);
static int udf_link(struct inode *, struct inode *, struct dentry *);
static int udf_unlink(struct inode *, struct dentry *);
static int udf_symlink(struct inode *, struct dentry *, const char *);
static int udf_mkdir(struct inode *, struct dentry *, int);
static int udf_rmdir(struct inode *, struct dentry *);
static int udf_mknod(struct inode *, struct dentry *, ints, int);
static int udf_rename(struct inode *, struct dentry *, struct inode *,
	struct dentry *);
static int udf_writepage(struct inode *, struct page *);
static void udf_truncate(struct inode *);
static int udf_updatepage(struct inode *, struct page *, const char *,
	unsigned long, unsigned int, int);
static int udf_revalidate(struct inode *);
#endif

#ifdef CONFIG_UDF_FULL_FS
static int udf_readlink(struct inode *, char *, int);
static struct dentry * udf_follow_link(struct inode *, struct dentry *);
static int udf_readpage(struct inode *, struct page *);
static int udf_permission(struct inode *, int);
static int udf_smap(struct inode *, int);
#endif


struct inode_operations udf_file_inode_operations= {
	&udf_file_fops,
#ifdef CONFIG_UDF_WRITE
	udf_create,		/* create */
	udf_lookup,		/* lookup */
	udf_link,		/* link */
	udf_unlink,		/* unlink */
	udf_symlink,		/* symlink */
	udf_mkdir,		/* mkdir */
	udf_rmdir,		/* rmdir */
	udf_mknod,		/* mknod */
	udf_rename,		/* rename */
	udf_readlink,		/* readlink */
	udf_follow_link,	/* follow_link */
	udf_readpage,		/* readpage */
	udf_writepage,		/* writepage */
	udf_bmap,		/* bmap */
	udf_truncate,		/* truncate */
	udf_permission,		/* permission */
	udf_smap,		/* smap */
	udf_updatepage,		/* updatepage */
	udf_revalidate		/* revalidate */
#else
	NULL,			/* create */
	udf_lookup,		/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
#ifdef CONFIG_UDF_FULL_FS
	udf_readpage,		/* readpage */
#else
	generic_readpage,	/* readpage */
#endif
	NULL,			/* writepage */
	udf_bmap,		/* bmap */
	NULL,			/* truncate */
#ifdef CONFIG_UDF_FULL_FS
	udf_permission,		/* permission */
#else
	NULL,
#endif
	NULL,			/* smap */
	NULL,			/* updatepage */
	NULL			/* revalidate */
#endif
};

#ifdef CONFIG_UDF_WRITE
/*
 * udf_create
 *
 * PURPOSE
 *	Create an inode.
 *
 * DESCRIPTION
 *	Optional - sys_open() will return -EACCESS if not available.
 *
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Refer to sys_open() in fs/open.c and open_namei() in fs/namei.c
 *	sys_open() -> do_open() -> open_namei() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to complete.
 *	mode			Mode of new inode.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_create(struct inode *dir, struct dentry *dentry, int mode)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_readpage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	Used for memory mapping. See generic_readpage() in fs/buffer.c for
 *	filesystems that support the bmap() operation.
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_readpage(struct inode *inode, struct page *)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_writepage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	Used for memory mapping. Required if readpage() is defined.
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_writepage(struct inode *inode, struct page *)
{
	return -1;
}

/*
 * udf_truncate
 *
 * PURPOSE
 *	Truncate a file.
 *
 * DESCRIPTION
 *	This routine is called by sys_truncate() to reduce the size of a file.
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of file to truncate.
 *
 * POST-CONDITIONS
 *	None.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static void
udf_truncate(struct inode *inode)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_FULL_FS
/*
 * udf_permissions
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_permission(struct inode *inode, int)
{
	return -1;
}

/*
 * udf_smap
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_smap(struct inode *inode, int)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_updatepage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_updatepage(struct inode *inode, struct page *, const char *, unsigned long, unsigned int, int)
{
	return -1;
}

/*
 *
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_revalidate(struct inode *inode)
{
	return -1;
}
#endif
