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
 */

#include <linux/fs.h>

typedef void * poll_table; 

/* Prototypes for file operations */
static long long udf_llseek(struct file *, long long, int);
static ssize_t udf_read(struct file *, char *, size_t, loff_t *);
static long udf_write(struct inode *, struct file *, const char *,
	unsigned long);
static int udf_readdir(struct file *, void *, filldir_t);
static unsigned int udf_poll(struct file *, poll_table *);
static int udf_ioctl(struct inode *, struct file *, unsigned int,
	unsigned long);
static int udf_mmap(struct file *, struct vm_area_struct *);
static int udf_open(struct inode *, struct file *);
static int udf_release(struct inode *, struct file *);
static int udf_fsync(struct file *, struct dentry *);
static int udf_fasync(struct file *, int);
static int udf_check_media_change(kdev_t dev);
static int udf_revalidate(kdev_t dev);
static int udf_lock(struct file *, int, struct file_lock *);

struct file_operations udf_file_fops = {
	udf_llseek,		/* llseek */
	udf_read,		/* read */
	udf_write,		/* write */
	udf_readdir,		/* readdir */
	udf_poll,		/* poll */
	udf_ioctl,		/* ioctl */
	udf_mmap,		/* mmap */
	udf_open,		/* open */
	udf_release,		/* release */
	udf_fsync,		/* fsync */
	udf_fasync,		/* fasync */
	udf_check_media_change,	/* check_media_change */
	udf_revalidate,		/* revalidate */
	udf_lock		/* lock */
};

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
long long udf_llseek(struct file *filp, long long offset, int origin)
{
	return -1;
}

/*
 * udf_read
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
static ssize_t udf_read(struct file * filp, char * buf, size_t bufsize, 
	loff_t *);
{
	return -1;
}

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

/*
 * udf_readdir
 *
 * PURPOSE
 *	Read a directory entry.
 *
 * DESCRIPTION
 *	Optional - sys_getdents() will return -ENOTDIR if this routine is not
 *	available.
 *
 *	Refer to sys_getdents() in fs/readdir.c
 *	sys_getdents() -> .
 *
 * PRE-CONDITIONS
 *	filp			Pointer to directory file.
 *	buf			Pointer to directory entry buffer.
 *	filldir			Pointer to filldir function.
 *
 * POST-CONDITIONS
 *	<return>		>=0 on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_readdir(struct file *filp, void *buf, filldir_t filldir)
{
	filldir(buf, filename, filename_len, offset, inode->i_no)
	return -1;
}

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
	return -1;
}

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
int udf_open(struct inode *, struct file *filp)
{
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
int udf_release(struct inode *, struct file *filp)
{
	return -1;
}

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

/* Inode Operations */
static int udf_create(struct inode *, struct dentry *, int);
static int udf_lookup(struct inode *, struct dentry *);
static int udf_link(struct inode *, struct inode *, struct dentry *);
static int udf_unlink(struct inode *, struct dentry *);
static int udf_symlink(struct inode *, struct dentry *, const char *);
static int udf_mkdir(struct inode *, struct dentry *, int);
static int udf_rmdir(struct inode *, struct dentry *);
static int udf_mknod(struct inode *, struct dentry *, ints, int);
static int udf_rename(struct inode *, struct dentry *, struct inode *,
	struct dentry *);
static int udf_readlink(struct inode *, char *, int);
static struct dentry * udf_follow_link(struct inode *, struct dentry *);
static int udf_readpage(struct inode *, struct page *);
static int udf_writepage(struct inode *, struct page *);
static int udf_bmap(struct inode *, int);
static void udf_truncate(struct inode *);
static int udf_permission(struct inode *, int);
static int udf_smap(struct inode *, int);
static int udf_updatepage(struct inode *, struct page *, const char *,
	unsigned long, unsigned int, int);
static int udf_revalidate(struct inode *);

extern struct inode_operations {
	&default_file_ops,
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
};

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

/*
 * udf_lookup
 *
 * PURPOSE
 *	Look-up the inode for a given name.
 *
 * DESCRIPTION
 *	Required - lookup_dentry() will return -ENOTDIR if this routine is not
 *	available for a directory. The filesystem is useless if this routine is
 *	not available for at least the filesystem's root directory.
 *
 *	This routine is passed an incomplete dentry - it must be completed by
 *	calling d_add(dentry, inode). If the name does not exist, then the
 *	specified inode must be set to null. An error should only be returned
 *	when the lookup fails for a reason other than the name not existing.
 *	Note that the directory inode semaphore is held during the call.
 *
 *	Refer to lookup_dentry() in fs/namei.c
 *	lookup_dentry() -> lookup() -> real_lookup() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to complete.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	/* Temporary - name doesn't exist, but it is okay to create it */
	d_add(dentry, NULL);
	return 0;
}

/*
 * udf_link
 *
 * PURPOSE
 *	Create a hard link.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of file to link to.
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new link.
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_link(struct inode *inode, struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_unlink
 *
 * PURPOSE
 *	Unlink (remove) an inode.
 *
 * DESCRIPTION
 *	Call d_delete(dentry) when ready to delete the dentry and inode.
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to unlink.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_unlink(struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_symlink
 *
 * PURPOSE
 *	Create a symbolic link.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new symlink.
 *	symname			Pointer to symbolic name of new symlink.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	return -1;
}

/*
 * udf_mkdir
 *
 * PURPOSE
 *	Create a directory.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Refer to sys_mkdir() in fs/namei.c
 *	sys_mkdir() -> do_mkdir() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer toinode of parent directory.
 *	dentry			Pointer to dentry of new directory.
 *	mode			Mode of new directory.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	return -1;
}

/*
 * udf_rmdir
 *
 * PURPOSE
 *	Remove a directory.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of directory to remove.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_rmdir(struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_mknod
 *
 * PURPOSE
 *	Make a special node.
 *
 * DESCRIPTION
 *	Optional - sys_mknod() will retrun -EPERM if this routine is not
 *	available.
 *
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Note that only root is allowed to create sockets and device files.
 *
 *	Refer to sys_mknod() in fs/namei.c
 *	sys_mknod() -> do_mknod() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new node.
 *	mode			Mode of new node.
 *	rdev			Real device of new node.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_mknod(struct inode *dir, struct dentry *dentry, int mode, int rdev)
{
	return -1;
}

/*
 * udf_rename
 *
 * PURPOSE
 *	Rename an existing inode.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	old_dir			Pointer to inode of old parent directory.
 *	old_dentry		Pointer to old dentry.
 *	new_dir			Pointer to inode of new parent directory.
 *	new_dentry		Pointer to new dentry.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_rename(struct inode *inode, struct dentry *dentry, struct inode *, struct dentry *)
{
	return -1;
}

/*
 * udf_readlink
 *
 * PURPOSE
 *	Read the contents of a symbolic link.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of link.
 *	buf			Pointer to buffer for name.
 *	buf_size		Size of buffer.
 *	
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_readlink(struct inode *inode, char *buf, int buf_size)
{
	return -1;
}

/*
 * udf_follow_link
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
static struct dentry *
udf_follow_link(struct inode *inode, struct dentry *dentry)
{
	return ERR_PTR(-1);
}

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
 * udf_bmap
 *
 * PURPOSE
 *	Get the Nth block of an inode.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *	<return>		Nth block of inode.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_bmap(struct inode *inode, int block)
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
