/*
 * truncate.c
 *
 * PURPOSE
 *	Truncate handling routines for the OSTA-UDF(tm) filesystem.
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
 *  (C) 1999 Ben Fennema
 *
 * HISTORY
 *
 *  02/24/99 blf  Created.
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/udf_fs.h>

#include "udf_i.h"
#include "udf_sb.h"

static int extent_trunc(struct inode * inode, lb_addr eloc, Uint32 elen,
	Uint32 offset)
{
	struct buffer_head *bh;
	int i, retry = 0;
	unsigned long block_to_free = 0, free_count = 0;
	int blocks = inode->i_sb->s_blocksize / 512;
	int last_block = (elen + inode->i_sb->s_blocksize - 1) / inode->i_sb->s_blocksize;

#ifdef VDEBUG
	udf_debug("eloc: block=%d, partition=%d, elen=%d, offset=%d\n",
		eloc.logicalBlockNum, eloc.partitionReferenceNum, elen, offset);
#endif

	for (i=offset; i<last_block; i++)
	{
		int tmp = udf_get_lb_pblock(inode->i_sb, eloc, i);

		if (!tmp)
		{
			udf_debug("i=%d\n", i);
			continue;
		}

		bh = find_buffer(inode->i_dev, tmp, inode->i_sb->s_blocksize);
		if (bh)
		{
			bh->b_count++;
			if (bh->b_count != 1 || buffer_locked(bh))
			{
				brelse(bh);
				retry = 1;
				continue;
			}
		}

		inode->i_blocks -= blocks;
		mark_inode_dirty(inode);
		bforget(bh);

		if (free_count == 0)
			goto free_this;
		else if (block_to_free == i - free_count)
			free_count ++;
		else
		{
			udf_free_blocks(inode, eloc, block_to_free, free_count);
		free_this:
			block_to_free = i;
			free_count = 1;
		}
	}
	if (free_count > 0)
		udf_free_blocks(inode, eloc, block_to_free, free_count);
	return retry;
}

static int trunc(struct inode * inode)
{
	lb_addr bloc, eloc;
	Uint32 ext, elen, offset;
	int retry = 0;
	int first_block = (inode->i_size + inode->i_sb->s_blocksize - 1) / inode->i_sb->s_blocksize;

	if (block_bmap(inode, first_block, &bloc, &ext, &eloc, &elen, &offset))
	{
#ifdef VDEBUG
		udf_debug("first_block = %d\n", first_block);
#endif
		retry |= extent_trunc(inode, eloc, elen, offset);

		while (udf_next_aext(inode, &bloc, &ext, &eloc, &elen))
		{
#ifdef VDEBUG
			udf_debug("bloc: block=%d, partition=%d, ext=%d, eloc: block=%d, partition=%d elen=%d\n",
				bloc.logicalBlockNum, bloc.partitionReferenceNum, ext,
				eloc.logicalBlockNum, eloc.partitionReferenceNum, elen);
#endif
			retry |= extent_trunc(inode, eloc, elen, 0);
		}
	}
	return retry;
}

void udf_truncate(struct inode * inode)
{
	int offset, retry;

#ifdef VDEBUG
	udf_debug("ino=%ld\n", inode->i_ino);
#endif

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
			S_ISLNK(inode->i_mode)))
		return;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return;

	if (!UDF_I_EXT0OFFS(inode))
	{
		udf_discard_prealloc(inode);

		while (1)
		{
			retry = trunc(inode);
			if (!retry)
				break;
			current->counter = 0;
			schedule();
		}
	}

	offset = (inode->i_size & (inode->i_sb->s_blocksize - 1)) +
		UDF_I_EXT0OFFS(inode);
	if (offset)
	{
		struct buffer_head *bh;
		bh = bread(inode->i_dev,
			udf_bmap(inode, inode->i_size >> inode->i_sb->s_blocksize_bits),
			inode->i_sb->s_blocksize);
		if (bh)
		{
			memset(bh->b_data + offset, 0, inode->i_sb->s_blocksize - offset);
			mark_buffer_dirty(bh, 0);
			udf_release_data(bh);
		}
	}

	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
}
