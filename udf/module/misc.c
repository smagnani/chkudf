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

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/udf_fs.h>

/*
 * udf_stamp_to_time
 */
time_t * udf_stamp_to_time(time_t *dest, void * srcp)
{
	timestamp *src;

	if ((!dest) || (!srcp) )
	    return NULL;
	src=(timestamp *)srcp;

	/* this is very rough. need to find source to mktime() */
	*dest=(src->year) - 1970;
	*dest *= 12;
	*dest += src->month;
	*dest *= 30;
	*dest += src->day;
	*dest *= 24;
	*dest += src->hour;
	*dest *= 60;
	*dest += src->minute;
	*dest *= 60;
	*dest += src->second;
	return dest;
}

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
udf_read_tagged(struct super_block *sb, __u32 block, __u32 offset)
{
	tag *tag_p;
	struct buffer_head *bh;
	register __u8 checksum;
	register int i;

	/* Read the block */
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh) {
		printk(KERN_ERR "udf: udf_read_tagged(,%d) failed\n",
			block);
		return NULL;
	}

	tag_p = (tag *)(bh->b_data);

#ifdef USE_STRICT_LOCATION
	/* Verify the tag location */
	if ((block-offset) != tag_p->tagLocation) {
		printk(KERN_ERR "udf: location mismatch block %d, tag %d\n",
			block, tag_p->tagLocation);
		goto error_out;
	}
#endif
	
	/* Verify the tag checksum */
	checksum = 0U;
	for (i = 0; i < 4; i++)
		checksum += (__u8)(bh->b_data[i]);
	for (i = 5; i < 16; i++)
		checksum += (__u8)(bh->b_data[i]);
	if (checksum != tag_p->tagChecksum) {
		printk(KERN_ERR "udf: tag checksum failed\n");
		goto error_out;
	}

#ifdef USE_STRICT_VERSION
	/* Verify the tag version */
	if (tag_p->descVersion != 0x0002U) {
		printk(KERN_ERR "udf: tag version 0x%04x != 0x0002U\n",
			tag_p->descVersion);
		goto error_out;
	}
#endif

	/* Verify the descriptor CRC */
	if (tag_p->descCRC == udf_crc(bh->b_data + 16, tag_p->descCRCLength)) {
		return bh;
	}
	printk(KERN_ERR "udf: crc failure in udf_read_tagged\n");

error_out:
	brelse(bh);
	return NULL;
}
