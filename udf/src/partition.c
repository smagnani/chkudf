/*
 * partition.c
 *
 * PURPOSE
 *	Partition handling routines for the OSTA-UDF(tm) filesystem.
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
 * 12/5/98 dgb	Added.
 *
 */

#include "udfdecl.h"
#include "udf_sb.h"
#include "udf_i.h"

#ifdef __linux__
#include <linux/fs.h>
#endif

static long udf_virtual_lookup(struct super_block *sb, int part, long block);
static long udf_sparable_lookup(struct super_block *sb, int part, long block);

/*
 * we need to lookup partitions here!
 *
 */
long
udf_block_from_inode(struct inode *inode)
{
	long block;
	int part;
	struct super_block *sb;

	sb=inode->i_sb;
	part = udf_part_from_inode(inode);
	block = (inode->i_ino & UDF_INODE_BLOCK_MASK);
	if ( part < UDF_SB_PARTITIONS ) {
		switch (UDF_SB(sb)->s_partmap[part].p_type) {
		case 1:
			block += UDF_SB(sb)->s_partmap[part].p_sector;
			break;
		case 2: /* virtual */
			block=udf_virtual_lookup(sb, part, block);
			break;
		case 3: /* sparable */
			block=udf_sparable_lookup(sb, part, block);
			break;
		}
	} else {
		printk(KERN_DEBUG "udf: block_from_inode part %u?\n", part);
		block += UDF_SB_PARTROOT(sb);
	}
	return block; 
}

long
udf_block_from_bmap(struct inode *inode, int block, int part)
{
	struct super_block *sb;

	sb=inode->i_sb;
	if ( part == -1 )
		part = udf_part_from_inode(inode);
	if ( part < UDF_SB_PARTITIONS ) {
		switch (UDF_SB(sb)->s_partmap[part].p_type) {
		case 1:
			block += UDF_SB(sb)->s_partmap[part].p_sector;
			break;
		case 2: /* virtual */
			block=udf_virtual_lookup(sb, part, block);
			break;
		case 3: /* sparable */
			block=udf_sparable_lookup(sb, part, block);
			break;
		}
	} else {
		printk(KERN_DEBUG "udf: block_from_bmap part %u?\n", part);
		block += UDF_SB_PARTROOT(sb);
	}
	return block; 
}

/* 
 * unused
 */
long
udf_inode_from_block(struct super_block *sb, long block, int partref)
{
	long ino;
	ino= block & UDF_INODE_BLOCK_MASK;
	ino += (partref << UDF_INODE_PART_BITS) & UDF_INODE_PART_MASK;
	return ino;
}

/*
 *
 */
int
udf_part_from_inode(struct inode *inode)
{
	return (inode->i_ino & UDF_INODE_PART_MASK) >> UDF_INODE_PART_BITS;
}

long
udf_virtual_lookup(struct super_block *sb, int part, long block)
{
	return  UDF_SB(sb)->s_partmap[part].p_sector + block;
}

long
udf_sparable_lookup(struct super_block *sb, int part, long block)
{
	return  UDF_SB(sb)->s_partmap[part].p_sector + block;
}


