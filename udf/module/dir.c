/*
 * dir.c
 *
 * PURPOSE
 *	Directory handling routines for the OSTA-UDF(tm) filesystem.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/udf_fmt.h>

/* Internal Prototypes */
static int udf_readdir(struct inode *, struct file *, void *, filldir_t);
#ifdef CONFIG_UDF_WRITE
static long udf_dir_read(struct inode *, struct file *, char *, unsigned long);
static long udf_dir_write(struct inode *, struct file *, char *, unsigned long);
#endif

static struct file_operations udf_dir_fops = {
	NULL,			/* llseek */
#ifdef CONFIG_UDF_WRITE
	udf_dir_read,		/* read */
	udf_dir_write,		/* write */
#else
	NULL,
	NULL,			/* write */
#endif
	udf_readdir,		/* readdir */
	NULL,			/* poll */
	NULL,			/* ioctl */
	NULL,			/* mmap */
	NULL,			/* open */
	NULL,			/* release */
#ifdef CONFIG_UDF_WRITE
	udf_fsync,		/* fsync */
	NULL,			/* fasync */
#else
	NULL,
	NULL,
#endif
	NULL,			/* check_media_change */
	NULL,			/* revalidate */
	NULL			/* lock */
};

extern struct inode_operations udf_dir_iops = {
	&udf_dir_fops,		/* default_file_ops */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	generic_readpage,	/* readpage */
	NULL,			/* writepage */
	udf_file_bmap,		/* bmap */
	udf_file_truncate,	/* truncate */
	udf_file_permission,	/* permission */
	NULL,			/* smap */
	NULL,			/* updatepage */
	NULL			/* revalidate */
};

/*
 * udf_dir_read
 *
 * PURPOSE
 *	Routine to read a directory.
 *
 * DESCRIPTION
 *	This operation is not supported. Returns -EISDIR.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static long
udf_dir_read(struct inode *inode, struct file *filp, char *buf,
	unsigned long bufsize)
{
	return -EISDIR;
}

/*
 * udf_readdir
 *
 * PURPOSE
 *	Routine to read a directory.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
readdir(struct inode *inode, struct file *filp, void *dirent, filldir_t filldir)
{
	/* Temporary */
	return -1;
}
