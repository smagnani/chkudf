/*
 * lookup.c
 *
 * PURPOSE
 *	The routines to lookup filenames on an UDF filesystem.
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
#include <linux/udf_fmt.h>

/* External Prototypes */
extern struct dentry * udf_lookup(struct dentry *, char *, int);
extern struct dentry * udf_physical_lookup(struct inode *, struct dentry *);

/*
 * udf_lookup
 *
 * PURPOSE
 *	Lookup a dentry for a given file name.
 *
 * DESCRIPTION
 *	Directories _must_ have this operation defined, or the kernel will
 *	segfault in lookup() [refer to fs/lookup.c].
 *
 *	This routine is called as a last resort when there isn't a dentry.
 *
 * HISTORY
 *	October 3, 1997 - Andrew E. Mileski
 *	Commented, tested, and released.
 */
static struct inode *
udf_lookup(struct inode *parent, struct dentry *dentry)
{
	struct inode *inode;
	struct udf_dir_entry *de;
	struct buffer_head *bh;

	if (dentry->d_name.len > UDF_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	/* Convert the UTF name to OSTA compressed Unicode */
	udf_UTFtoCS0(name, dentry->d_name);

	rturn dentry;
}
