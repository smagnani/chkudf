/*
 * directory.c
 *
 * PURPOSE
 *	Directory related functions
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

#include "udfdecl.h"
#include "udf_sb.h"

#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <linux/string.h>

#else

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#endif

#ifdef __KERNEL__

Uint8 * udf_filead_read(struct inode *dir, Uint8 *tmpad, Uint8 ad_size,
	lb_addr fe_loc, int *pos, int *offset, struct buffer_head **bh, int *error)
{
	int loffset = *offset;
	int block;
	Uint8 *ad;
	int remainder;

	*error = 0;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_filead_read(%p,%p,%d,%d,%p)\n",
		dir, tmpad, ad_size, *offset, *bh);
#endif

	ad = (Uint8 *)(*bh)->b_data + *offset;
	*offset += ad_size;

	if (!ad)
	{
		udf_release_data(*bh);
		*error = 1;
		return NULL;
	}

	if (*offset == dir->i_sb->s_blocksize)
	{
		udf_release_data(*bh);
		block = udf_get_lb_pblock(dir->i_sb, fe_loc, ++*pos);
		if (!block)
			return NULL;
		if (!(*bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
			return NULL;
	}
	else if (*offset > dir->i_sb->s_blocksize)
	{
		ad = tmpad;

		remainder = dir->i_sb->s_blocksize - loffset;
		memcpy((char *)ad, (*bh)->b_data + loffset, remainder);

		udf_release_data(*bh);
		block = udf_get_lb_pblock(dir->i_sb, fe_loc, ++*pos);
		if (!block)
			return NULL;
		if (!((*bh) = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
			return NULL;

		memcpy((char *)ad + remainder, (*bh)->b_data, ad_size - remainder);
		*offset = ad_size - remainder;
	}
	return ad;
}

struct FileIdentDesc *
udf_fileident_read(struct inode *dir, struct FileIdentDesc *tmpfi,
	int *nf_pos, int *offset, struct buffer_head **bh, int *error)
{
	int loffset = *offset;
	int block;
	struct FileIdentDesc *fi;
	int remainder;

	*error = 0;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_fileident_read(%p,%p,%d,%d,%p)\n",
		dir, tmpfi, *nf_pos, *offset, *bh);
#endif

	fi = udf_get_fileident((*bh)->b_data, dir->i_sb->s_blocksize,
		offset);

	if (!fi)
	{
		udf_release_data(*bh);
		*error = 1;
		return NULL;
	}

	*nf_pos += ((*offset - loffset) >> 2);

	if (*offset == dir->i_sb->s_blocksize)
	{
			
		udf_release_data(*bh);
		block = udf_bmap(dir, *nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
		if (!block)
			return NULL;
		if (!(*bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
			return NULL;
	}
	else if (*offset > dir->i_sb->s_blocksize)
	{
		int fi_len;

		fi = tmpfi;

		remainder = dir->i_sb->s_blocksize - loffset;
		memcpy((char *)fi, (*bh)->b_data + loffset, remainder);

		udf_release_data(*bh);
		block = udf_bmap(dir, *nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
		if (!block)
			return NULL;
		if (!((*bh) = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
			return NULL;

		if (sizeof(struct FileIdentDesc) > remainder)
		{
			memcpy((char *)fi + remainder, (*bh)->b_data, sizeof(struct FileIdentDesc) - remainder);

			if (fi->descTag.tagIdent != TID_FILE_IDENT_DESC)
			{
				printk(KERN_DEBUG "udf: (udf_enum_directory) - 0x%x != TID_FILE_IDENT_DESC\n",
					fi->descTag.tagIdent);
				udf_release_data(*bh);
				*error = 1;
				return NULL;
			}
			fi_len = sizeof(struct FileIdentDesc) + fi->lengthFileIdent + fi->lengthOfImpUse;
			fi_len += (4 - (fi_len % 4)) % 4;
			*nf_pos += ((fi_len - (*offset - loffset)) >> 2);
		}
		else
		{
			fi_len = sizeof(struct FileIdentDesc) + fi->lengthFileIdent + fi->lengthOfImpUse;
			fi_len += (4 - (fi_len % 4)) % 4;
		}

		memcpy((char *)fi + remainder, (*bh)->b_data, fi_len - remainder);
		*offset = fi_len - remainder;
	}
	return fi;
}
#endif

struct FileIdentDesc * 
udf_get_fileident(void * buffer, int bufsize, int * offset)
{
	struct FileIdentDesc *fi;
	int lengthThisIdent;
	Uint8 * ptr;
	int padlen;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: udf_get_fileident() invalidparms\n");
#endif
		return NULL;
	}

	ptr = buffer;

	if ( (*offset + sizeof(tag)) > bufsize )
	{
		/* This should not be legal, but it is done... */
#if 0
#ifdef __KERNEL__
		printk(KERN_DEBUG 
			"udf: udf_get_fileident() buffer too short (%u + %u > %u)\n", *offset, sizeof(tag), bufsize);
#endif
		return NULL;
#endif
	}

	if ( (*offset > 0) && (*offset < bufsize) ) {
		ptr += *offset;
	}
	fi=(struct FileIdentDesc *)ptr;
	if (fi->descTag.tagIdent != TID_FILE_IDENT_DESC)
	{
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileident - 0x%x != TID_FILE_IDENT_DESC\n",
			fi->descTag.tagIdent);
		printk(KERN_DEBUG "udf: offset: %u sizeof: %u bufsize: %u\n",
			*offset, sizeof(struct FileIdentDesc), bufsize);
		return NULL;
#endif
	}
	if ( (*offset + sizeof(struct FileIdentDesc)) > bufsize )
	{
		lengthThisIdent = sizeof(struct FileIdentDesc);
	}
	else
		lengthThisIdent=sizeof(struct FileIdentDesc) +
			fi->lengthFileIdent + fi->lengthOfImpUse;

	/* we need to figure padding, too! */
	padlen=lengthThisIdent % UDF_NAME_PAD;
	if (padlen)
		lengthThisIdent+= (UDF_NAME_PAD - padlen);
	*offset = *offset + lengthThisIdent;

	return fi;
}

extent_ad *
udf_get_fileextent(void * buffer, int bufsize, int * offset)
{
	extent_ad * ext;
	struct FileEntry *fe;
	int lengthExtents;
	Uint8 * ptr;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_fileextent() invalidparms\n");
#endif
		return NULL;
	}

	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileextent - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthExtents = fe->lengthAllocDescs;

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthExtents) ) {
		ptr += *offset;
	}
	ext=(extent_ad *)ptr;

	*offset = *offset + sizeof(extent_ad);
	return ext;
}

short_ad *
udf_get_fileshortad(void * buffer, int bufsize, int * offset)
{
	short_ad * sa;
	struct FileEntry *fe;
	int lengthShortAds;
	Uint8 * ptr;
	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
                printk(KERN_ERR "udf: udf_get_fileshortad() invalidparms\n");
#endif
		return NULL;
	}
	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileshortad - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthShortAds = fe->lengthAllocDescs;

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthShortAds) ) {
		ptr += *offset;
	}
	sa = (short_ad *)ptr;
	*offset = *offset + sizeof(short_ad);
	return sa;
}

long_ad *
udf_get_filelongad(void * buffer, int bufsize, int * offset)
{
	long_ad * la;
	struct FileEntry *fe;
	int lengthLongAds;
	Uint8 * ptr;

	if ( (!buffer) || !(offset) ) 
	{
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_filelongad() invalidparms\n");
#endif
		return NULL;
	}

	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _filelongad - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthLongAds = fe->lengthAllocDescs;

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthLongAds) ) {
		ptr += *offset;
	}
	la=(long_ad *)ptr;

	*offset = *offset + sizeof(long_ad);
	return la;
}
