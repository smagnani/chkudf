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
#include <linux/udf_fs.h>

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
	udf_debug("%p,%p,%d,%d,%p\n",
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
		if (!(*bh = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
			return NULL;
	}
	else if (*offset > dir->i_sb->s_blocksize)
	{
		ad = tmpad;

		remainder = dir->i_sb->s_blocksize - loffset;
		memcpy((Uint8 *)ad, (*bh)->b_data + loffset, remainder);

		udf_release_data(*bh);
		block = udf_get_lb_pblock(dir->i_sb, fe_loc, ++*pos);
		if (!block)
			return NULL;
		if (!((*bh) = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
			return NULL;

		memcpy((Uint8 *)ad + remainder, (*bh)->b_data, ad_size - remainder);
		*offset = ad_size - remainder;
	}
	return ad;
}

struct FileIdentDesc *
udf_fileident_read(struct inode *dir, int *nf_pos,
	int *soffset, struct buffer_head **sbh,
	int *eoffset, struct buffer_head **ebh,
	struct FileIdentDesc *cfi)
{
	struct FileIdentDesc *fi;
	int block;

	*soffset = *eoffset;

	if (*eoffset == dir->i_sb->s_blocksize)
	{
		block = udf_bmap(dir, *nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
		if (!block)
			return NULL;
		udf_release_data(*sbh);
		if (!(*sbh = *ebh = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
			return NULL;
		*soffset = *eoffset = 0;
	}
	else if (*sbh != *ebh)
	{
		udf_release_data(*sbh);
		*sbh = *ebh;
	}

	fi = udf_get_fileident((*sbh)->b_data, dir->i_sb->s_blocksize,
		eoffset);

	if (!fi)
		return NULL;

	*nf_pos += ((*eoffset - *soffset) >> 2);

	if (*eoffset <= dir->i_sb->s_blocksize)
	{
		memcpy((Uint8 *)cfi, (Uint8 *)fi, sizeof(struct FileIdentDesc));
	}
	else if (*eoffset > dir->i_sb->s_blocksize)
	{
		*soffset -= dir->i_sb->s_blocksize;
		*eoffset -= dir->i_sb->s_blocksize;

		block = udf_bmap(dir, *nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
		if (!block)
			return NULL;
		if (!(*ebh = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
			return NULL;

		if (sizeof(struct FileIdentDesc) > - *soffset)
		{
			int fi_len;

			memcpy((Uint8 *)cfi, (Uint8 *)fi, - *soffset);
			memcpy((Uint8 *)cfi - *soffset, (*ebh)->b_data,
				sizeof(struct FileIdentDesc) + *soffset);

			fi_len = (sizeof(struct FileIdentDesc) + cfi->lengthFileIdent +
				le16_to_cpu(cfi->lengthOfImpUse) + 3) & ~3;

			*nf_pos += ((fi_len - (*eoffset - *soffset)) >> 2);
			*eoffset = *soffset + fi_len;
		}
		else
		{
			memcpy((Uint8 *)cfi, (Uint8 *)fi, sizeof(struct FileIdentDesc));
		}
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
		udf_debug("invalidparms\n, buffer=%p, offset=%p\n", buffer, offset);
#endif
		return NULL;
	}

	ptr = buffer;

	if ( (*offset > 0) && (*offset < bufsize) ) {
		ptr += *offset;
	}
	fi=(struct FileIdentDesc *)ptr;
	if (le16_to_cpu(fi->descTag.tagIdent) != TID_FILE_IDENT_DESC)
	{
#ifdef __KERNEL__
		udf_debug("0x%x != TID_FILE_IDENT_DESC\n",
			le16_to_cpu(fi->descTag.tagIdent));
		udf_debug("offset: %u sizeof: %u bufsize: %u\n",
			*offset, sizeof(struct FileIdentDesc), bufsize);
#endif
		return NULL;
	}
	if ( (*offset + sizeof(struct FileIdentDesc)) > bufsize )
	{
		lengthThisIdent = sizeof(struct FileIdentDesc);
	}
	else
		lengthThisIdent = sizeof(struct FileIdentDesc) +
			fi->lengthFileIdent + le16_to_cpu(fi->lengthOfImpUse);

	/* we need to figure padding, too! */
	padlen = lengthThisIdent % UDF_NAME_PAD;
	if (padlen)
		lengthThisIdent += (UDF_NAME_PAD - padlen);
	*offset = *offset + lengthThisIdent;

	return fi;
}

extent_ad *
udf_get_fileextent(void * buffer, int bufsize, int * offset)
{
	extent_ad * ext;
	struct FileEntry *fe;
	Uint8 * ptr;

	if ( (!buffer) || (!offset) )
	{
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_fileextent() invalidparms\n");
#endif
		return NULL;
	}

	fe = (struct FileEntry *)buffer;

	if ( le16_to_cpu(fe->descTag.tagIdent) != TID_FILE_ENTRY )
	{
#ifdef __KERNEL__
		udf_debug("0x%x != TID_FILE_ENTRY\n",
			le16_to_cpu(fe->descTag.tagIdent));
#endif
		return NULL;
	}

	ptr=(Uint8 *)(fe->extendedAttr) + le32_to_cpu(fe->lengthExtendedAttr);

	if ( (*offset > 0) && (*offset < le32_to_cpu(fe->lengthAllocDescs)) )
	{
		ptr += *offset;
	}

	ext = (extent_ad *)ptr;

	*offset = *offset + sizeof(extent_ad);
	return ext;
}

short_ad *
udf_get_fileshortad(void * buffer, int maxoffset, int *offset)
{
	short_ad * sa;
	Uint8 * ptr;

	if ( (!buffer) || (!offset) )
	{
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_fileshortad() invalidparms\n");
#endif
		return NULL;
	}

	ptr = (Uint8 *)buffer;

	if ( (*offset > 0) && (*offset < maxoffset) )
		ptr += *offset;
	else
		return NULL;

	sa = (short_ad *)ptr;
	*offset = *offset + sizeof(short_ad);
	return sa;
}

long_ad *
udf_get_filelongad(void * buffer, int maxoffset, int * offset)
{
	long_ad * la;
	Uint8 * ptr;

	if ( (!buffer) || !(offset) ) 
	{
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_filelongad() invalidparms\n");
#endif
		return NULL;
	}

	ptr = (Uint8 *)buffer;

	if ( (*offset > 0) && (*offset < maxoffset) )
		ptr += *offset;
	else
		return NULL;

	la = (long_ad *)ptr;
	*offset = *offset + sizeof(long_ad);
	return la;
}
