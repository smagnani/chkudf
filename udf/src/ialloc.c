/*
 * ialloc.c
 *
 * PURPOSE
 *	Inode allocation handling routines for the OSTA-UDF(tm) filesystem.
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
 *  2/24/98 blf  Created.
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/locks.h>

#include "udf_i.h"
#include "udf_sb.h"

void udf_free_inode(struct inode * inode)
{
	struct super_block * sb = inode->i_sb;
	int is_directory;
	unsigned long ino;

	if (!inode->i_dev)
	{
		printk(KERN_DEBUG "udf: udf_free_inode: inode has no device\n");
		return;
	}
	if (inode->i_count > 1)
	{
		printk(KERN_DEBUG "udf: udf_free_inode: inode has count=%d\n",
			inode->i_count);
		return;
	}
	if (inode->i_nlink)
	{
		printk(KERN_DEBUG "udf: udf_free_inode: inode has nlink=%d\n",
			inode->i_nlink);
		return;
	}
	if (!sb)
	{
		printk(KERN_DEBUG "udf: udf_free_inode: inode on nonexistent device\n");
		return;
	}

	ino = inode->i_ino;

	lock_super(sb);

	is_directory = S_ISDIR(inode->i_mode);

	clear_inode(inode);

	if (UDF_SB_LVIDBH(sb))
	{
		if (is_directory)
			UDF_SB_LVIDIU(sb)->numDirs =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numDirs) - 1);
		else
			UDF_SB_LVIDIU(sb)->numFiles =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numFiles) - 1);
		
		mark_buffer_dirty(UDF_SB_LVIDBH(sb), 1);
	}

	unlock_super(sb);

	udf_free_blocks(inode, UDF_I_LOCATION(inode), 0, 1);
}
