/*
 * file.c
 *
 * PURPOSE
 *	File handling routines for the OSTA-UDF(tm) filesystem.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/udf_fmt.h>

static struct file_operations udf_file_fops = {
	udf_llseek,		/* llseek */
	udf_read,		/* read */
	udf_write,		/* write */
	NULL,			/* readdir */
	NULL,			/* poll */
	NULL,			/* ioctl */
	generic_file__mmap,	/* mmap */
	NULL,			/* open */
	udf_release,		/* release */
	udf_fsync,		/* fsync */
	NULL,			/* fasync */
	NULL,			/* check_media_change */
	NULL,			/* revalidate */
	NULL,			/* lock */
};

extern struct inode_operations udf_file_iops = {
	&udf_file_fops,		/* default_file_ops */
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
