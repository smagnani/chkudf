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


#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <linux/udf_fs.h>

#else

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/udf_fs.h>

int udf_blocksize=0;
int udf_errno=0;

void 
udf_setblocksize(int size)
{
	udf_blocksize=size;
}
#endif

Uint32
udf64_low32(Uint64 indat)
{
	Uint32 * uptr;
	uptr=(Uint32 *)&indat;
	return uptr[0];
}

Uint32
udf64_high32(Uint64 indat)
{
	Uint32 * uptr;
	uptr=(Uint32 *)&indat;
	return uptr[1];
}

/*
 * udf_stamp_to_time
 */
time_t * 
udf_stamp_to_time(time_t *dest, void * srcp)
{
	timestamp *src;
	struct ktm tm;

	if ((!dest) || (!srcp) )
	    return NULL;
	src=(timestamp *)srcp;

	/* this is very rough. need to find source to mktime() */
	tm.tm_year=(src->year) - 1900;	
	tm.tm_mon=(src->month);
	tm.tm_mday=(src->day);
	tm.tm_hour=src->hour;
	tm.tm_min=src->minute;
	tm.tm_sec=src->second;
	*dest = udf_converttime(&tm);
	return dest;
}

uid_t udf_convert_uid(int uidin)
{
    if ( uidin == -1 )
	return 0;
    if ( uidin > (64*1024U - 1) )
	return 0;
    return uidin;
}

gid_t udf_convert_gid(int gidin)
{
    if ( gidin == -1 )
	return 0;
    if ( gidin > (64*1024U - 1) )
	return 0;
    return gidin;
}

#if defined(__linux__) && defined(__KERNEL__)
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

	/* Verify the tag location */
	if ((block-offset) != tag_p->tagLocation) {
		/* this is actually kind of normal, */
		/* only tagged blocks will match, data blocks won't */ 
		printk(KERN_DEBUG "udf: location mismatch block %d, tag %d\n",
			block, tag_p->tagLocation);
		goto error_out;
	}
	
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

void
udf_release_data(struct buffer_head *bh)
{
	if (bh)
		brelse(bh);
}

#endif

#ifndef __KERNEL__
/*
 * udf_read_tagged_data
 *
 * PURPOSE
 *	Read the first block of a tagged descriptor.
 *	Usable from user-land.
 *
 * HISTORY
 *      10/4/98 dgb: written
 */
int
udf_read_tagged_data(char *buffer, int size, int fd, int block, int offset)
{
	tag *tag_p;
	register __u8 checksum;
	register int i;

	if (!buffer) {
		udf_errno=1;
		return -1;
	}

	if ( !udf_blocksize ) {
		udf_errno=2;
		return -1;
	}

	if ( size < udf_blocksize ) {
		udf_errno=3;
		return -1;
	}
	udf_errno=0;
	
	if ( lseek(fd, block*udf_blocksize, SEEK_SET) ) {
		udf_errno=4;
		return -1;
	}

	i=read(fd, buffer, udf_blocksize);
	if ( i < udf_blocksize ) {
		udf_errno=5;
		return -1;
	}

	tag_p = (tag *)(buffer);

	/* Verify the tag location */
	if ((block-offset) != tag_p->tagLocation) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: location mismatch block %d, tag %d\n",
			block, tag_p->tagLocation);
#else
		udf_errno=6;
#endif
		goto error_out;
	}
	
	/* Verify the tag checksum */
	checksum = 0U;
	for (i = 0; i < 4; i++)
		checksum += (__u8)(buffer[i]);
	for (i = 5; i < 16; i++)
		checksum += (__u8)(buffer[i]);
	if (checksum != tag_p->tagChecksum) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: tag checksum failed\n");
#else
		udf_errno=7;
#endif
		goto error_out;
	}

	/* Verify the tag version */
	if (tag_p->descVersion != 0x0002U) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: tag version 0x%04x != 0x0002U\n",
			tag_p->descVersion);
#else
		udf_errno=8;
#endif
		goto error_out;
	}

	/* Verify the descriptor CRC */
	if (tag_p->descCRC == udf_crc(buffer + 16, tag_p->descCRCLength)) {
		udf_errno=0;
		return 0;
	}
#ifdef __KERNEL__
	printk(KERN_ERR "udf: crc failure in udf_read_tagged\n");
#else
	udf_errno=9;
#endif

error_out:
	return -1;
}
#endif

#ifdef __KERNEL__
long
udf_block_from_inode(struct super_block *sb, long ino)
{
	return ino + UDF_BLOCK_OFFSET(sb);
}

long
udf_inode_from_block(struct super_block *sb, long block)
{
	return block - UDF_BLOCK_OFFSET(sb);
}
#endif

