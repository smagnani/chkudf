/*
 * namei.c
 *
 * PURPOSE
 *      Inode name handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *      E-mail regarding any portion of the Linux UDF file system should be
 *      directed to the development team mailing list (run by majordomo):
 *              linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *      This file is distributed under the terms of the GNU General Public
 *      License (GPL). Copies of the GPL can be obtained from:
 *              ftp://prep.ai.mit.edu/pub/gnu/GPL
 *      Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *
 * 12/12/98 blf  Created. Split out the lookup code from dir.c
 *
 */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/version.h>
#include "udf_i.h"
#include "udf_sb.h"
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#endif

#include "udfdecl.h"

static inline int udf_match(int len, const char * const name, struct qstr *qs)
{
	if (len != qs->len)
		return 0;
	return !memcmp(name, qs->name, len);
}

static struct buffer_head *
udf_find_entry(struct inode *dir, struct dentry *dentry, lb_addr *ino)
{
	struct buffer_head *bh;
	struct FileIdentDesc *fi=NULL;
	struct FileIdentDesc *tmpfi;
	int block;
	int offset;
	int f_pos;
	int flen;
	char fname[255];
	int error = 0;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;

	if (!dir)
		return NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	offset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	block = udf_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2));

	if (!block || !(bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
		return NULL;

	tmpfi = (struct FileIdentDesc *) __get_free_page(GFP_KERNEL);
	memset(ino, 0, sizeof(lb_addr));

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, tmpfi, &f_pos, &offset, &bh, &error);

		if (!fi)
		{
			free_page((unsigned long) tmpfi);
			return NULL;
		}

		if ( (fi->fileCharacteristics & FILE_DELETED) != 0 )
		{
			if ( !IS_UNDELETE(dir->i_sb) )
				continue;
		}
	    
		if ( (fi->fileCharacteristics & FILE_HIDDEN) != 0 )
		{
			if ( !IS_UNHIDE(dir->i_sb) )
				continue;
		}

		if (!fi->lengthFileIdent)
			continue;

		if ((flen = udf_get_filename(fi, fname, dir)))
		{
			if (udf_match(flen, fname, &(dentry->d_name)))
			{
				*ino = lelb_to_cpu(fi->icb.extLocation);
				free_page((unsigned long) tmpfi);
				return bh;
			}
		}
	}
	udf_release_data(bh);
	free_page((unsigned long) tmpfi);
	return NULL;
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

int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct buffer_head *bh;
	struct inode *inode = NULL;
	lb_addr ino;

	if ((bh = udf_find_entry(dir, dentry, &ino)))
	{
		udf_release_data(bh);

		inode = udf_iget(dir->i_sb, ino);
		if ( !inode )
			return -EACCES;
	}
	d_add(dentry, inode);
	return 0;
}
