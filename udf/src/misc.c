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
extern struct buffer_head *
udf_read_untagged(struct super_block *sb, Uint32 block, Uint32 offset)
{
	struct buffer_head *bh;

	/* Read the block */
	bh = bread(sb->s_dev, block+offset, sb->s_blocksize);
	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_untagged(%p,%d,%d) failed\n",
			sb, block, offset);
		return NULL;
	}
	return bh;
}

extern Uint32 get_pblock(struct super_block *sb, Uint32 block, Uint16 partition)
{
	switch (UDF_SB_PARTTYPE(sb, partition))
	{
		case UDF_TYPE1_MAP15:
		{
			return UDF_SB_PARTROOT(sb, partition) + block;
		}
		case UDF_VIRTUAL_MAP15:
		case UDF_VIRTUAL_MAP20:
		{
			Uint32 offset;
			Uint32 loc;

			offset = (sb->s_blocksize - UDF_SB_TYPEVIRT(sb,partition).s_start_offset) / sizeof(Uint32));

			if (block >= offset)
			{
				Uint32 newblock;

				block -= offset;
				newblock = (block / (sb->s_blocksize / sizeof(Uint32)));
				offset = block % (sb->s_blocksize / sizeof(Uint32));

				loc = udf_bmap(UDF_SB_TYPEVIRT(sb,partition).s_vat, newblock);
			}
			else
			{
				newblock = 0;
				offset = UDF_SB_TYPEVIRT(sb,partition).s_start_offset / sizeof(Uint32);
			}

			loc = udf_bmap(UDF_SB_TYPEVIRT(sb,partition).s_vat, newblock);

			if (!(bh = bread(sb->s_dev, loc, sb->s_blocksize)))
			{
				printk(KERN_DEBUG "udf: get_pblock(UDF_VIRTUAL_MAP:%p,%d,%d) VAT: %d[%d]\n",
					sb, block, partition, loc, offset);
				return 0xFFFFFFFF;
			}

			loc = ((Uint32 *)bh)[offset];
			udf_release_data(bh);

			return get_pblock(sb, loc, UDF_I_LOCATION(inode).partitionReferenceNum);
		}
		case UDF_SPARABLE_MAP15:
		{
			Uint32 newblock = UDF_SB_PARTROOT(sb, partition) + block;
			Uint32 spartable = UDF_SB_TYPESPAR(sb,partition).s_spar_loc;
			Uint32 plength = UDF_SB_TYPESPAR(sb,partition).s_spar_plen;
			Uint32 packet = newblock / plength;
			struct buffer_head *bh;
			struct SparingTable *st;
			SparingEntry *se;

			bh = udf_read_tagged(sb, spartable, spartable);

			if (!bh)
			{
				printk(KERN_ERR "udf: udf_read_tagged(%p,%d,%d)\n",
					sb, spartable, spartable);
				return 0xFFFFFFFF;
			}

			st = (struct SparingTable *)bh->b_data;
			if (st->descTag.tagIdent == 0)
			{
				if (!strncmp(st->sparingIdent.ident, UDF_ID_SPARING, strlen(UDF_ID_SPARING)))
				{
					Uint16 rtl = st->reallocationTableLen;
					Uint16 offset;
					Uint32 cblock = 0;

					/* If the sparing table span multiple blocks, find out which block we are on */

					se = &(st->mapEntry[0]);

					if (rtl * sizeof(SparingEntry) + sizeof(struct SparingTable) > sb->s_blocksize)
					{
						offset = (sb->s_blocksize - sizeof(struct Sparing Table)) / sizeof(SparingEntry);
						if (se[offset-1].origLocation == packet)
						{
							udf_release_data(bh);
							return se[offset-1].mappedLocation + newblock % plength;
						}
						else if (se[offset-1].origLocation < packet)
						{
							do
							{
								udf_release_data(bh);
								bh = bread(sb, spartable, sb->s_blocksize);
								if (!bh)
									return 0xFFFFFFFF;
								se = (SparingEntry *)bh->b_data;
								spartable ++;
								rtl -= offset;
								offset = sb->s_blocksize / sizeof(SparingEntry);

								if (se[offset].origLocation == packet)
								{
									udf_release_data(bh);
									return se[offset-1].mappedLocation + newblock % plength;
								}
							} while (rtl * sizeof(SparingEntry) > sb->s_blocksize && 
								se[offset-1].origLocation < packet);
						}
					}
			
					for (offset=0; offset<rtl; offset++)
					{
						if (se[offset].origLocation == packet)
						{
							udf_release_data(bh);
							return se[offset].mappedLocation + newblock % plength;
						}
						else if (se[offset].origLocation > packet)
						{
							udf_release_data(bh);
							return newblock;
						}
					}

					udf_release_data(bh);
					return newblock;
				}
			}
			udf_release_data(bh);
		}
	}
	return 0xFFFFFFFF;
}

extern Uint32 get_lb_pblock(struct super_block *sb, lb_addr loc)
{
	return get_pblock(sb, loc.logicalBlockNum, loc.partitionReferenceNum);
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
udf_read_tagged(struct super_block *sb, Uint32 block, Uint32 offset)
{
	tag *tag_p;
	struct buffer_head *bh;
	register Uint8 checksum;
	register int i;

	/* Read the block */
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_read_tagged(%p,%d,%d)\n",
		sb, block, offset);
#endif
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_tagged(%p,%d,%d) failed\n",
			sb, block, offset);
		return NULL;
	}

	tag_p = (tag *)(bh->b_data);

	/* Verify the tag location */
	if ( ((block-offset) != tag_p->tagLocation) &&
	     (block != tag_p->tagLocation) ) {
		static int seen_msg=0;

		if (!seen_msg) {	
			printk(KERN_DEBUG "udf: location mismatch block %d, offset %d, tag %d\n",
				block, offset, tag_p->tagLocation);
			seen_msg=1;
		}
		/*
		goto error_out;
		*/
	}
	
	/* Verify the tag checksum */
	checksum = 0U;
	for (i = 0; i < 4; i++)
		checksum += (Uint8)(bh->b_data[i]);
	for (i = 5; i < 16; i++)
		checksum += (Uint8)(bh->b_data[i]);
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

extern struct buffer_head *
udf_read_ptagged(struct super_block *sb, lb_addr loc)
{
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_read_ptagged(%p,%d,%d)\n",
		sb, loc.logicalBlockNum, loc.partitionReferenceNum);
#endif
	return udf_read_tagged(sb, get_lb_pblock(sb, loc),
		 get_pblock(sb, 0, loc.partitionReferenceNum));
}

void udf_release_data(struct buffer_head *bh)
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
	register Uint8 checksum;
	register int i;
	unsigned long offs;

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
	
	offs=(long)block * udf_blocksize;
	if ( lseek(fd, offs, SEEK_SET) != offs ) {
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
		checksum += (Uint8)(buffer[i]);
	for (i = 5; i < 16; i++)
		checksum += (Uint8)(buffer[i]);
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
