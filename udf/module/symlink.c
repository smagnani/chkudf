/*
 * symlink.c
 *
 * PURPOSE
 *	Symlink handling routines for the OSTA-UDF(tm) filesystem.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/udf_fmt.h>

/* Internal Prototypes */
static void udf_readlink(struct inode *, char *, int);
static void udf_follow_link(struct inode *, struct dentry *);

extern struct inode_operations udf_sym_iops = {
	NULL,		/* default_file_ops */
	NULL,		/* create */
	NULL,		/* lookup */
	NULL,		/* link */
	NULL,		/* unlink */
	NULL,		/* symlink */
	NULL,		/* mkdir */
	NULL,		/* rmdir */
	NULL,		/* mknod */
	NULL,		/* rename */
	udf_readlink,	/* readlink */
	udf_follow_link	/* follow_link */
	NULL,		/* readpage */
	NULL,		/* writepage */
	NULL,		/* bmap */
	NULL,		/* truncate */
	NULL,		/* permission */
	NULL,		/* smap */
	NULL,		/* updatepage */
	NULL		/* revalidate */
};
