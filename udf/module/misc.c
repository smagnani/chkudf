/*
 * misc.c
 *
 * PURPOSE
 *	Miscellaneous routines for the OSTA-UDF(tm) filesystem.
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
#include <linux/udf_167.h>
#include <linux/udf_udf.h>

/*
 * udf_read_tagged
 *
 * PURPOSE
 *	Read the first block of a tagged descriptor.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern struct buffer_head *
udf_read_tagged(struct super_block *sb, __u32 block)
{
	tag *tag_p;
	struct buffer_head *bh;
	register __u8 checksum;
	register int i;

	/* Read the block */
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh)
		return NULL;

	/* Verify the tag location */
	tag_p = (tag *)(bh->b_data);
	if (block != tag_p->tagLocation)
		goto error_out;
	
	/* Verify the tag checksum */
	checksum = 0U;
	for (i = 0; i < 4; i++)
		checksum += (__u8)(bh->b_data[i]);
	for (i = 5; i < 16; i++)
		checksum += (__u8)(bh->b_data[i]);
	if (checksum != tag_p->tagChecksum)
		goto error_out;

	/* Verify the tag version */
	if (tag_p->descVersion != 0x0002U)
		goto error_out;

	/* Verify the descriptor CRC */
	if (tag_p->descCRC == udf_crc(bh->b_data + 16, tag_p->descCRCLength))
		return bh;

error_out:
	brelse(bh);
	return NULL;
}
