/*
 * fsync.c
 *
 * PURPOSE
 *  Fsync handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *  E-mail regarding any portion of the Linux UDF file system should be
 *  directed to the development team mailing list (run by majordomo):
 *      linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *      ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1999-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  05/22/99 blf  Created.
 */

#include "udfdecl.h"

#include <linux/fs.h>
#include <linux/udf_fs.h>
#include "udf_i.h"

static int sync_block (struct inode * inode, uint32_t * block, int wait)
{
	struct buffer_head * bh;
	
	if (!*block)
		return 0;
	bh = get_hash_table (inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return 0;
	if (wait && buffer_req(bh) && !buffer_uptodate(bh)) {
		brelse (bh);
		return -1;
	}
	if (wait || !buffer_uptodate(bh) || !buffer_dirty(bh)) {
		brelse (bh);
		return 0;
	}
	ll_rw_block (WRITE, 1, &bh);
	bh->b_count--;
	return 0;
}

static int sync_extent (struct inode * inode, lb_addr *loc, uint32_t *len, int wait)
{
	uint32_t i, block;
	int rc, err = 0;

	for (i = 0; i < *len; i++)
	{
		block = udf_get_lb_pblock(inode->i_sb, *loc, i);
		rc = sync_block (inode, &block, wait);
		if (rc)
			err = rc;
	}
	return err;
}

static int sync_all_extents(struct inode * inode, int wait)
{
	lb_addr bloc, eloc;
	uint32_t extoffset, elen, offset;
	int err = 0, etype;
	struct buffer_head *bh = NULL;
	
	if ((etype = inode_bmap(inode, 0, &bloc, &extoffset, &eloc, &elen, &offset, &bh)) != -1)
	{
		err |= sync_extent(inode, &eloc, &elen, wait);

		while ((etype = udf_next_aext(inode, &bloc, &extoffset, &eloc, &elen, &bh, 1)) != -1)
		{
			if (etype == (EXT_RECORDED_ALLOCATED >> 30))
				err |= sync_extent(inode, &eloc, &elen, wait);
		}
	}
	udf_release_data(bh);
	return err;
}

/*
 *	File may be NULL when we are called. Perhaps we shouldn't
 *	even pass file to fsync ?
 */

int udf_sync_file(struct file * file, struct dentry *dentry)
{
	int wait, err = 0;
	struct inode *inode = dentry->d_inode;

	if (S_ISLNK(inode->i_mode) && !(inode->i_blocks))
	{
		/*
		 * Don't sync fast links! or ICB_FLAG_AD_IN_ICB
		 */
		goto skip;
	}

	for (wait=0; wait<=1; wait++)
	{
		err |= sync_all_extents (inode, wait);
	}
skip:
	err |= udf_sync_inode (inode);
	return err ? -EIO : 0;
}
