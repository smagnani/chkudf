/*
 * namei.c
 *
 * PURPOSE
 *      Inode name handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *      E-mail regarding any portion of the Linux UDF file system should be
 *      directed to the development team mailing list (run by majordomo):
 *              linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *      This file is distributed under the terms of the GNU General Public
 *      License (GPL). Copies of the GPL can be obtained from:
 *              ftp://prep.ai.mit.edu/pub/gnu/GPL
 *      Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  12/12/98 blf  Created. Split out the lookup code from dir.c
 *  04/19/99 blf  link, mknod, symlink support
 */

#include "udfdecl.h"

#include "udf_i.h"
#include "udf_sb.h"
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/quotaops.h>
#include <linux/version.h>

static inline int udf_match(int len, const char * const name, struct qstr *qs)
{
	if (len != qs->len)
		return 0;
	return !memcmp(name, qs->name, len);
}

int udf_write_fi(struct fileIdentDesc *cfi,
	struct fileIdentDesc *sfi, struct udf_fileident_bh *fibh,
	uint8_t *impuse, uint8_t *fileident)
{
	uint16_t crclen = fibh->eoffset - fibh->soffset - sizeof(tag);
	uint16_t crc;
	uint8_t checksum = 0;
	int i;
	int offset;
	uint16_t liu = le16_to_cpu(cfi->lengthOfImpUse);
	uint8_t lfi = cfi->lengthFileIdent;
	int padlen = fibh->eoffset - fibh->soffset - liu - lfi -
		sizeof(struct fileIdentDesc);
	int adinicb = 0;

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
		adinicb = 1;

	offset = fibh->soffset + sizeof(struct fileIdentDesc);

	if (impuse)
	{
		if (adinicb || (offset + liu < 0))
			memcpy((uint8_t *)sfi->impUse, impuse, liu);
		else if (offset >= 0)
			memcpy(fibh->ebh->b_data + offset, impuse, liu);
		else
		{
			memcpy((uint8_t *)sfi->impUse, impuse, -offset);
			memcpy(fibh->ebh->b_data, impuse - offset, liu + offset);
		}
	}

	offset += liu;

	if (fileident)
	{
		if (adinicb || (offset + lfi < 0))
			memcpy((uint8_t *)sfi->fileIdent + liu, fileident, lfi);
		else if (offset >= 0)
			memcpy(fibh->ebh->b_data + offset, fileident, lfi);
		else
		{
			memcpy((uint8_t *)sfi->fileIdent + liu, fileident, -offset);
			memcpy(fibh->ebh->b_data, fileident - offset, lfi + offset);
		}
	}

	offset += lfi;

	if (adinicb || (offset + padlen < 0))
		memset((uint8_t *)sfi->padding + liu + lfi, 0x00, padlen);
	else if (offset >= 0)
		memset(fibh->ebh->b_data + offset, 0x00, padlen);
	else
	{
		memset((uint8_t *)sfi->padding + liu + lfi, 0x00, -offset);
		memset(fibh->ebh->b_data, 0x00, padlen + offset);
	}

	crc = udf_crc((uint8_t *)cfi + sizeof(tag), sizeof(struct fileIdentDesc) -
		sizeof(tag), 0);

	if (fibh->sbh == fibh->ebh)
		crc = udf_crc((uint8_t *)sfi->impUse,
			crclen + sizeof(tag) - sizeof(struct fileIdentDesc), crc);
	else if (sizeof(struct fileIdentDesc) >= -fibh->soffset)
		crc = udf_crc(fibh->ebh->b_data + sizeof(struct fileIdentDesc) + fibh->soffset,
			crclen + sizeof(tag) - sizeof(struct fileIdentDesc), crc);
	else
	{
		crc = udf_crc((uint8_t *)sfi->impUse,
			-fibh->soffset - sizeof(struct fileIdentDesc), crc);
		crc = udf_crc(fibh->ebh->b_data, fibh->eoffset, crc);
	}

	cfi->descTag.descCRC = cpu_to_le32(crc);
	cfi->descTag.descCRCLength = cpu_to_le16(crclen);

	for (i=0; i<16; i++)
		if (i != 4)
			checksum += ((uint8_t *)&cfi->descTag)[i];

	cfi->descTag.tagChecksum = checksum;
	if (adinicb || (sizeof(struct fileIdentDesc) <= -fibh->soffset))
		memcpy((uint8_t *)sfi, (uint8_t *)cfi, sizeof(struct fileIdentDesc));
	else
	{
		memcpy((uint8_t *)sfi, (uint8_t *)cfi, -fibh->soffset);
		memcpy(fibh->ebh->b_data, (uint8_t *)cfi - fibh->soffset,
			sizeof(struct fileIdentDesc) + fibh->soffset);
	}

	if (adinicb)
		mark_inode_dirty(inode);
	else
	{
		if (fibh->sbh != fibh->ebh)
			mark_buffer_dirty(fibh->ebh, 1);
		mark_buffer_dirty(fibh->sbh, 1);
	}
	return 0;
}

static struct fileIdentDesc *
udf_find_entry(struct inode *dir, struct dentry *dentry,
	struct udf_fileident_bh *fibh,
	struct fileIdentDesc *cfi)
{
	struct fileIdentDesc *fi=NULL;
	loff_t f_pos;
	int block, flen;
	char fname[255];
	char *nameptr;
	uint8_t lfi;
	uint16_t liu;
	int size = (udf_ext0_offset(dir) + dir->i_size) >> 2;
	lb_addr bloc, eloc;
	uint32_t extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	if (!dir)
		return NULL;

	f_pos = (udf_ext0_offset(dir) >> 2);

	fibh->soffset = fibh->eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
		fibh->sbh = fibh->ebh = NULL;
	else if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == (EXT_RECORDED_ALLOCATED >> 30))
	{
		offset >>= dir->i_sb->s_blocksize_bits;
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if ((++offset << dir->i_sb->s_blocksize_bits) < elen)
		{
			if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;

		if (!(fibh->sbh = fibh->ebh = udf_tread(dir->i_sb, block)))
		{
			udf_release_data(bh);
			return NULL;
		}
	}
	else
	{
		udf_release_data(bh);
		return NULL;
	}

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi, &bloc, &extoffset, &eloc, &elen, &offset, &bh);

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
			udf_release_data(bh);
			return NULL;
		}

		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (fibh->sbh == fibh->ebh)
		{
			nameptr = fi->fileIdent + liu;
		}
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = fibh->soffset + sizeof(struct fileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (uint8_t *)(fibh->ebh->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, fibh->ebh->b_data, poffset);
			}
		}

		if ( (cfi->fileCharacteristics & FID_FILE_CHAR_DELETED) != 0 )
		{
			if ( !UDF_QUERY_FLAG(dir->i_sb, UDF_FLAG_UNDELETE) )
				continue;
		}
	    
		if ( (cfi->fileCharacteristics & FID_FILE_CHAR_HIDDEN) != 0 )
		{
			if ( !UDF_QUERY_FLAG(dir->i_sb, UDF_FLAG_UNHIDE) )
				continue;
		}

		if (!lfi)
			continue;

		if ((flen = udf_get_filename(dir->i_sb, nameptr, fname, lfi)))
		{
			if (udf_match(flen, fname, &(dentry->d_name)))
			{
				udf_release_data(bh);
				return fi;
			}
		}
	}
	if (fibh->sbh != fibh->ebh)
		udf_release_data(fibh->ebh);
	udf_release_data(fibh->sbh);
	udf_release_data(bh);
	return NULL;
}

/*
 * udf_lookup
 *
 * PURPOSE
 *	Look-up the inode for a given name.
 *
 * DESCRIPTION
 *	Required - lookup_dentry() will return -ENOTDIR if this routine is not
 *	available for a directory. The filesystem is useless if this routine is
 *	not available for at least the filesystem's root directory.
 *
 *	This routine is passed an incomplete dentry - it must be completed by
 *	calling d_add(dentry, inode). If the name does not exist, then the
 *	specified inode must be set to null. An error should only be returned
 *	when the lookup fails for a reason other than the name not existing.
 *	Note that the directory inode semaphore is held during the call.
 *
 *	Refer to lookup_dentry() in fs/namei.c
 *	lookup_dentry() -> lookup() -> real_lookup() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to complete.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
int
#else
struct dentry *
#endif
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct fileIdentDesc cfi, *fi;
	struct udf_fileident_bh fibh;

	if (dentry->d_name.len > UDF_NAME_LEN)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
		return -ENAMETOOLONG;
#else
		return ERR_PTR(-ENAMETOOLONG);
#endif

#ifdef UDF_RECOVERY
	/* temporary shorthand for specifying files by inode number */
	if (!strncmp(dentry->d_name.name, ".B=", 3) )
	{
		lb_addr lb = { 0, simple_strtoul(dentry->d_name.name+3, NULL, 0) };
		inode = udf_iget(dir->i_sb, lb);
		if (!inode)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
			return -EACCES;
#else
			return ERR_PTR(-EACCES);
#endif
	}
	else
#endif /* UDF_RECOVERY */

	if ((fi = udf_find_entry(dir, dentry, &fibh, &cfi)))
	{
		if (fibh.sbh != fibh.ebh)
			udf_release_data(fibh.ebh);
		udf_release_data(fibh.sbh);

		inode = udf_iget(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation));
		if ( !inode )
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
			return -EACCES;
#else
			return ERR_PTR(-EACCES);
#endif
	}
	d_add(dentry, inode);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
	return 0;
#else
	return NULL;
#endif
}

static struct fileIdentDesc *
udf_add_entry(struct inode *dir, struct dentry *dentry,
	struct udf_fileident_bh *fibh,
	struct fileIdentDesc *cfi, int *err)
{
	struct super_block *sb;
	struct fileIdentDesc *fi=NULL;
	char name[UDF_NAME_LEN], fname[UDF_NAME_LEN];
	int namelen;
	loff_t f_pos;
	int flen;
	char *nameptr;
	int size = (udf_ext0_offset(dir) + dir->i_size) >> 2;
	int nfidlen;
	uint8_t lfi;
	uint16_t liu;
	int block;
	lb_addr bloc, eloc;
	uint32_t extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	sb = dir->i_sb;

	if (dentry)
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
		if (dentry->d_name.len >= UDF_NAME_LEN)
		{
			*err = -ENAMETOOLONG;
			return NULL;
		}
#endif
		if (!dentry->d_name.len)
		{
			*err = -EINVAL;
			return NULL;
		}

		if ( !(namelen = udf_put_filename(sb, dentry->d_name.name, name, dentry->d_name.len)))
		{
			*err = -ENAMETOOLONG;
			return NULL;
		}
	}
	else
		namelen = 0;

	nfidlen = (sizeof(struct fileIdentDesc) + namelen + 3) & ~3;

	f_pos = (udf_ext0_offset(dir) >> 2);

	fibh->soffset = fibh->eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
		fibh->sbh = fibh->ebh = NULL;
	else if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == (EXT_RECORDED_ALLOCATED >> 30))
	{
		offset >>= dir->i_sb->s_blocksize_bits;
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if ((++offset << dir->i_sb->s_blocksize_bits) < elen)
		{
			if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;

		if (!(fibh->sbh = fibh->ebh = udf_tread(dir->i_sb, block)))
		{
			udf_release_data(bh);
			*err = -EIO;
			return NULL;
		}

		block = UDF_I_LOCATION(dir).logicalBlockNum;
	}
	else
	{
		block = udf_get_lb_pblock(dir->i_sb, UDF_I_LOCATION(dir), 0);
		fibh->sbh = fibh->ebh = NULL;
		fibh->soffset = fibh->eoffset = sb->s_blocksize;
		goto add;
	}


	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi, &bloc, &extoffset, &eloc, &elen, &offset, &bh);

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
			udf_release_data(bh);
			*err = -EIO;
			return NULL;
		}

		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (fibh->sbh == fibh->ebh)
			nameptr = fi->fileIdent + liu;
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = fibh->soffset + sizeof(struct fileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (char *)(fibh->ebh->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, fibh->ebh->b_data, poffset);
			}
		}

		if ( (cfi->fileCharacteristics & FID_FILE_CHAR_DELETED) != 0 )
		{
			if (((sizeof(struct fileIdentDesc) + liu + lfi + 3) & ~3) == nfidlen)
			{
				udf_release_data(bh);
				cfi->descTag.tagSerialNum = cpu_to_le16(1);
				cfi->fileVersionNum = cpu_to_le16(1);
				cfi->fileCharacteristics = 0;
				cfi->lengthFileIdent = namelen;
				cfi->lengthOfImpUse = cpu_to_le16(0);
				if (!udf_write_fi(cfi, fi, fibh, NULL, name))
					return fi;
				else
				{
					*err = -EIO;
					return NULL;
				}
			}
		}

		if (!lfi || !dentry)
			continue;

		if ((flen = udf_get_filename(dir->i_sb, nameptr, fname, lfi)) &&
			udf_match(flen, fname, &(dentry->d_name)))
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
			udf_release_data(bh);
			*err = -EEXIST;
			return NULL;
		}
	}

add:
	f_pos += nfidlen;

	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB &&
		sb->s_blocksize - fibh->eoffset < nfidlen)
	{
		udf_release_data(bh);
		bh = NULL;
		fibh->soffset -= udf_ext0_offset(dir);
		fibh->eoffset -= udf_ext0_offset(dir);
		f_pos -= (udf_ext0_offset(dir) >> 2);
		if (fibh->sbh != fibh->ebh)
			udf_release_data(fibh->ebh);
		udf_release_data(fibh->sbh);
		if (!(fibh->sbh = fibh->ebh = udf_expand_dir_adinicb(dir, &block, err)))
			return NULL;
		bloc = UDF_I_LOCATION(dir);
		eloc.logicalBlockNum = block;
		eloc.partitionReferenceNum = UDF_I_LOCATION(dir).partitionReferenceNum;
		elen = dir->i_sb->s_blocksize;
		extoffset = udf_file_entry_alloc_offset(dir);
		if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_SHORT)
			extoffset += sizeof(short_ad);
		else if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_LONG)
			extoffset += sizeof(long_ad);
	}

	if (sb->s_blocksize - fibh->eoffset >= nfidlen)
	{
		fibh->soffset = fibh->eoffset;
		fibh->eoffset += nfidlen;
		if (fibh->sbh != fibh->ebh)
		{
			udf_release_data(fibh->sbh);
			fibh->sbh = fibh->ebh;
		}

		if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
		{
			block = UDF_I_LOCATION(dir).logicalBlockNum;
			fi = (struct fileIdentDesc *)(UDF_I_DATA(dir) + fibh->soffset - udf_ext0_offset(dir));
		}
		else
		{
			block = eloc.logicalBlockNum + ((elen - 1) >>
				dir->i_sb->s_blocksize_bits);
			fi = (struct fileIdentDesc *)(fibh->sbh->b_data + fibh->soffset);
		}
	}
	else
	{
		fibh->soffset = fibh->eoffset - sb->s_blocksize;
		fibh->eoffset += nfidlen - sb->s_blocksize;
		if (fibh->sbh != fibh->ebh)
		{
			udf_release_data(fibh->sbh);
			fibh->sbh = fibh->ebh;
		}

		block = eloc.logicalBlockNum + ((elen - 1) >>
			dir->i_sb->s_blocksize_bits);

		if (!(fibh->ebh = udf_bread(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2), 1, err)))
		{
			udf_release_data(bh);
			udf_release_data(fibh->sbh);
			return NULL;
		}

		if (!(fibh->soffset))
		{
			if (udf_next_aext(dir, &bloc, &extoffset, &eloc, &elen, &bh, 1) ==
				(EXT_RECORDED_ALLOCATED >> 30))
			{
				block = eloc.logicalBlockNum + ((elen - 1) >>
					dir->i_sb->s_blocksize_bits);
			}
			else
				block ++;

			udf_release_data(fibh->sbh);
			fibh->sbh = fibh->ebh;
			fi = (struct fileIdentDesc *)(fibh->sbh->b_data);
		}
		else
		{
			fi = (struct fileIdentDesc *)
				(fibh->sbh->b_data + sb->s_blocksize + fibh->soffset);
		}
	}

	memset(cfi, 0, sizeof(struct fileIdentDesc));
	if (UDF_SB_UDFREV(sb) >= 0x0200)
		udf_new_tag((char *)cfi, TAG_IDENT_FID, 3, 1, block, sizeof(tag));
	else
		udf_new_tag((char *)cfi, TAG_IDENT_FID, 2, 1, block, sizeof(tag));
	cfi->fileVersionNum = cpu_to_le16(1);
	cfi->lengthFileIdent = namelen;
	cfi->lengthOfImpUse = cpu_to_le16(0);
	if (!udf_write_fi(cfi, fi, fibh, NULL, name))
	{
		udf_release_data(bh);
		dir->i_size += nfidlen;
		if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
			UDF_I_LENALLOC(dir) += nfidlen;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		dir->i_version = ++event;
#else
		dir->i_version = ++global_event;
#endif
		mark_inode_dirty(dir);
		return fi;
	}
	else
	{
		udf_release_data(bh);
		if (fibh->sbh != fibh->ebh)
			udf_release_data(fibh->ebh);
		udf_release_data(fibh->sbh);
		*err = -EIO;
		return NULL;
	}
}

static int udf_delete_entry(struct super_block *sb, struct fileIdentDesc *fi,
	struct udf_fileident_bh *fibh, struct fileIdentDesc *cfi)
{
	cfi->fileCharacteristics |= FID_FILE_CHAR_DELETED;
	if (UDF_QUERY_FLAG(sb, UDF_FLAG_STRICT))
		memset(&(cfi->icb), 0x00, sizeof(long_ad));
	return udf_write_fi(cfi, fi, fibh, NULL, NULL);
}

int udf_create(struct inode *dir, struct dentry *dentry, int mode)
{
	struct udf_fileident_bh fibh;
	struct inode *inode;
	struct fileIdentDesc cfi, *fi;
	int err;

	inode = udf_new_inode(dir, mode, &err);
	if (!inode)
		return err;

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
		inode->i_op = &udf_file_inode_operations_adinicb;
	else
		inode->i_op = &udf_file_inode_operations;
	inode->i_mode = mode;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
	{
		inode->i_nlink --;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		dir->i_version = ++event;
#else
		dir->i_version = ++global_event;
#endif
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	d_instantiate(dentry, inode);
	return 0;
}

int udf_mknod(struct inode * dir, struct dentry * dentry, int mode, int rdev)
{
	struct inode * inode;
	struct udf_fileident_bh fibh;
	int err;
	struct fileIdentDesc cfi, *fi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	err = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	err = -EIO;
	inode = udf_new_inode(dir, mode, &err);
	if (!inode)
		goto out;

	inode->i_uid = current->fsuid;
	inode->i_mode = mode;
	inode->i_op = NULL;
	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
	{
		inode->i_nlink --;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		dir->i_version = ++event;
#else
		dir->i_version = ++global_event;
#endif
	}
	if (S_ISREG(inode->i_mode))
	{
		if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
			inode->i_op = &udf_file_inode_operations_adinicb;
		else
			inode->i_op = &udf_file_inode_operations;
	}
	else if (S_ISCHR(inode->i_mode))
	{
		inode->i_op = &chrdev_inode_operations;
	} 
	else if (S_ISBLK(inode->i_mode))
	{
		inode->i_op = &blkdev_inode_operations;
	}
	else if (S_ISFIFO(inode->i_mode))
	{
		init_fifo(inode);
	}
	if (S_ISBLK(mode) || S_ISCHR(mode))
		inode->i_rdev = to_kdev_t(rdev);
	mark_inode_dirty(inode);

	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	d_instantiate(dentry, inode);
	err = 0;
out:
	return err;
}

int udf_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	struct inode * inode;
	struct udf_fileident_bh fibh;
	int err;
	struct fileIdentDesc cfi, *fi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	err = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	err = -EMLINK;
	if (dir->i_nlink >= (256<<sizeof(dir->i_nlink))-1)
		goto out;

	err = -EIO;
	inode = udf_new_inode(dir, S_IFDIR, &err);
	if (!inode)
		goto out;

	inode->i_op = &udf_dir_inode_operations;
	if (!(fi = udf_add_entry(inode, NULL, &fibh, &cfi, &err)))
	{
		inode->i_nlink--;
		mark_inode_dirty(inode);
		iput(inode);
		goto out;
	}
	inode->i_nlink = 2;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(dir));
	*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(dir) & 0x00000000FFFFFFFFUL);
	cfi.fileCharacteristics = FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT;
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	udf_release_data(fibh.sbh);
	inode->i_mode = S_IFDIR | (mode & (S_IRWXUGO|S_ISVTX) & ~current->fs->umask);
	if (dir->i_mode & S_ISGID)
		inode->i_mode |= S_ISGID;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
	{
		inode->i_nlink = 0;
		mark_inode_dirty(inode);
		iput(inode);
		goto out;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	cfi.fileCharacteristics |= FID_FILE_CHAR_DIRECTORY;
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
	dir->i_version = ++event;
#else
	dir->i_version = ++global_event;
#endif
	dir->i_nlink++;
	mark_inode_dirty(dir);
	d_instantiate(dentry, inode);
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	err = 0;
out:
	return err;
}

static int empty_dir(struct inode *dir)
{
	struct fileIdentDesc *fi, cfi;
	struct udf_fileident_bh fibh;
	loff_t f_pos;
	int size = (udf_ext0_offset(dir) + dir->i_size) >> 2;
	int block;
	lb_addr bloc, eloc;
	uint32_t extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	f_pos = (udf_ext0_offset(dir) >> 2);

	fibh.soffset = fibh.eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;

	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
		fibh.sbh = fibh.ebh = NULL;
	else if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == (EXT_RECORDED_ALLOCATED >> 30))
	{
		offset >>= dir->i_sb->s_blocksize_bits;
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if ((++offset << dir->i_sb->s_blocksize_bits) < elen)
		{
			if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;

		if (!(fibh.sbh = fibh.ebh = udf_tread(dir->i_sb, block)))
		{
			udf_release_data(bh);
			return 0;
		}
	}
	else
	{
		udf_release_data(bh);
		return 0;
	}

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, &fibh, &cfi, &bloc, &extoffset, &eloc, &elen, &offset, &bh);

		if (!fi)
		{
			if (fibh.sbh != fibh.ebh)
				udf_release_data(fibh.ebh);
			udf_release_data(fibh.sbh);
			udf_release_data(bh);
			return 0;
		}

		if (cfi.lengthFileIdent && (cfi.fileCharacteristics & FID_FILE_CHAR_DELETED) == 0)
		{
			if (fibh.sbh != fibh.ebh)
				udf_release_data(fibh.ebh);
			udf_release_data(fibh.sbh)
			udf_release_data(bh);
			return 0;
		}
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	udf_release_data(bh);
	return 1;
}

int udf_rmdir(struct inode * dir, struct dentry * dentry)
{
	int retval;
	struct inode * inode = dentry->d_inode;
	struct udf_fileident_bh fibh;
	struct fileIdentDesc *fi, cfi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	DQUOT_INIT(inode);

	retval = -EIO;
	if (udf_get_lb_pblock(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation), 0) != inode->i_ino)
		goto end_rmdir;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	if (!empty_dir(inode))
		retval = -ENOTEMPTY;
	else if (udf_get_lb_pblock(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation), 0) !=
		inode->i_ino)
	{
		retval = -ENOENT;
	}
	else
	{
		retval = udf_delete_entry(dir->i_sb, fi, &fibh, &cfi);
		dir->i_version = ++event;
	}
#else
	retval = -ENOTEMPTY;
	if (!empty_dir(inode))
		goto end_rmdir;
	retval = udf_delete_entry(dir->i_sb, fi, &fibh, &cfi);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
	dir->i_version = ++event;
#else
	dir->i_version = ++global_event;
#endif
#endif
	if (retval)
		goto end_rmdir;
	if (inode->i_nlink != 2)
		udf_warning(inode->i_sb, "udf_rmdir",
			"empty directory has nlink != 2 (%d)",
			inode->i_nlink);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
	inode->i_version = ++event;
#else
	inode->i_version = ++global_event;
#endif
	inode->i_nlink = 0;
	inode->i_size = 0;
	mark_inode_dirty(inode);
	dir->i_nlink --;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	UDF_I_UCTIME(inode) = UDF_I_UCTIME(dir) = UDF_I_UMTIME(dir) = CURRENT_UTIME;
	mark_inode_dirty(dir);
	d_delete(dentry);

end_rmdir:
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
out:
	return retval;
}

int udf_unlink(struct inode * dir, struct dentry * dentry)
{
	int retval;
	struct inode * inode = dentry->d_inode;
	struct udf_fileident_bh fibh;
	struct fileIdentDesc *fi;
	struct fileIdentDesc cfi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	DQUOT_INIT(inode);

	retval = -EIO;

	if (udf_get_lb_pblock(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation), 0) !=
		inode->i_ino)
	{
		goto end_unlink;
	}

	if (!inode->i_nlink)
	{
		udf_debug("Deleting nonexistent file (%lu), %d\n",
			inode->i_ino, inode->i_nlink);
		inode->i_nlink = 1;
	}
	retval = udf_delete_entry(dir->i_sb, fi, &fibh, &cfi);
	if (retval)
		goto end_unlink;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	UDF_I_UCTIME(dir) = UDF_I_UMTIME(dir) = CURRENT_UTIME;
	mark_inode_dirty(dir);
	inode->i_nlink--;
	mark_inode_dirty(inode);
	inode->i_ctime = dir->i_ctime;
	retval = 0;
	d_delete(dentry);	/* This also frees the inode */

end_unlink:
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
out:
	return retval;
}

int udf_symlink(struct inode * dir, struct dentry * dentry, const char * symname)
{
	struct inode * inode;
	struct pathComponent *pc;
	char *compstart;
	struct udf_fileident_bh fibh;
	struct buffer_head *bh = NULL;
	int eoffset, elen = 0;
	struct fileIdentDesc *fi;
	struct fileIdentDesc cfi;
	char *ea;
	int err;
	int block;
	char name[UDF_NAME_LEN];
	int namelen;

	if (!(inode = udf_new_inode(dir, S_IFLNK, &err)))
		goto out;

	inode->i_mode = S_IFLNK | S_IRWXUGO;
	inode->i_op = &udf_symlink_inode_operations;

	if (UDF_I_ALLOCTYPE(inode) != ICBTAG_FLAG_AD_IN_ICB)
	{
		struct buffer_head *bh = NULL;
		lb_addr bloc, eloc;
		uint32_t elen, extoffset;

		block = udf_new_block(inode->i_sb, inode,
			UDF_I_LOCATION(inode).partitionReferenceNum,
			UDF_I_LOCATION(inode).logicalBlockNum, &err);
		if (!block)
			goto out_no_entry;
		bloc = UDF_I_LOCATION(inode);
		eloc.logicalBlockNum = block;
		eloc.partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
		elen = inode->i_sb->s_blocksize;
		UDF_I_LENEXTENTS(inode) = elen;
		extoffset = udf_file_entry_alloc_offset(inode);
		udf_add_aext(inode, &bloc, &extoffset, eloc, elen, &bh, 0);
		udf_release_data(bh);

		inode->i_blocks = inode->i_sb->s_blocksize / 512;
		block = udf_get_pblock(inode->i_sb, block,
			UDF_I_LOCATION(inode).partitionReferenceNum, 0);
		bh = udf_tread(inode->i_sb, block);
		ea = bh->b_data + udf_ext0_offset(inode);
	}
	else
		ea = UDF_I_DATA(inode) + UDF_I_LENEATTR(inode);

	eoffset = inode->i_sb->s_blocksize - udf_ext0_offset(inode);
	pc = (struct pathComponent *)ea;

	if (*symname == '/')
	{
		do
		{
			symname++;
		} while (*symname == '/');

		pc->componentType = 1;
		pc->lengthComponentIdent = 0;
		pc->componentFileVersionNum = 0;
		pc += sizeof(struct pathComponent);
		elen += sizeof(struct pathComponent);
	}

	err = -ENAMETOOLONG;

	while (*symname)
	{
		if (elen + sizeof(struct pathComponent) > eoffset)
			goto out_no_entry;

		pc = (struct pathComponent *)(ea + elen);

		compstart = (char *)symname;

		do
		{
			symname++;
		} while (*symname && *symname != '/');

		pc->componentType = 5;
		pc->lengthComponentIdent = 0;
		pc->componentFileVersionNum = 0;
		if (pc->componentIdent[0] == '.')
		{
			if (pc->lengthComponentIdent == 1)
				pc->componentType = 4;
			else if (pc->lengthComponentIdent == 2 && pc->componentIdent[1] == '.')
				pc->componentType = 3;
		}

		if (pc->componentType == 5)
		{
			if ( !(namelen = udf_put_filename(inode->i_sb, compstart, name, symname - compstart)))
				goto out_no_entry;

			if (elen + sizeof(struct pathComponent) + namelen > eoffset)
				goto out_no_entry;
			else
				pc->lengthComponentIdent = namelen;

			memcpy(pc->componentIdent, name, namelen);
		}

		elen += sizeof(struct pathComponent) + pc->lengthComponentIdent;

		if (*symname)
		{
			do
			{
				symname++;
			} while (*symname == '/');
		}
	}

	udf_release_data(bh);
	inode->i_size = elen;
	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
		UDF_I_LENALLOC(inode) = inode->i_size;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
		goto out_no_entry;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	if (UDF_SB_LVIDBH(inode->i_sb))
	{
		struct logicalVolHeaderDesc *lvhd;
		uint64_t uniqueID;
		lvhd = (struct logicalVolHeaderDesc *)(UDF_SB_LVID(inode->i_sb)->logicalVolContentsUse);
		uniqueID = le64_to_cpu(lvhd->uniqueID);
		*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
			cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		if (!(++uniqueID & 0x00000000FFFFFFFFUL))
			uniqueID += 16;
		lvhd->uniqueID = cpu_to_le64(uniqueID);
		mark_buffer_dirty(UDF_SB_LVIDBH(inode->i_sb), 1);
	}
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		dir->i_version = ++event;
#else
		dir->i_version = ++global_event;
#endif
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	d_instantiate(dentry, inode);
	err = 0;

out:
	return err;

out_no_entry:
	inode->i_nlink--;
	mark_inode_dirty(inode);
	iput(inode);
	goto out;
}

int udf_link(struct dentry * old_dentry, struct inode * dir,
	 struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;
	struct udf_fileident_bh fibh;
	int err;
	struct fileIdentDesc cfi, *fi;

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return -EPERM;
#endif

	if (inode->i_nlink >= (256<<sizeof(inode->i_nlink))-1)
		return -EMLINK;

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
		return err;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	if (UDF_SB_LVIDBH(inode->i_sb))
	{
		struct logicalVolHeaderDesc *lvhd;
		uint64_t uniqueID;
		lvhd = (struct logicalVolHeaderDesc *)(UDF_SB_LVID(inode->i_sb)->logicalVolContentsUse);
		uniqueID = le64_to_cpu(lvhd->uniqueID);
		*(uint32_t *)((struct allocDescImpUse *)cfi.icb.impUse)->impUse =
			cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		if (!(++uniqueID & 0x00000000FFFFFFFFUL))
			uniqueID += 16;
		lvhd->uniqueID = cpu_to_le64(uniqueID);
		mark_buffer_dirty(UDF_SB_LVIDBH(inode->i_sb), 1);
	}
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICBTAG_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		dir->i_version = ++event;
#else
		dir->i_version = ++global_event;
#endif
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	inode->i_nlink ++;
	inode->i_ctime = CURRENT_TIME;
	UDF_I_UCTIME(inode) = CURRENT_UTIME;
	mark_inode_dirty(inode);
	inode->i_count ++;
	d_instantiate(dentry, inode);
	return 0;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)

/*
 * rename uses retrying to avoid race-conditions: at least they should be
 * minimal.
 * it tries to allocate all the blocks, then sanity-checks, and if the sanity-
 * checks fail, it tries to restart itself again. Very practical - no changes
 * are done until we know everything works ok.. and then all the changes can be
 * done in one fell swoop when we have claimed all the buffers needed.
 *
 * Anybody can rename anything with this: the permission checks are left to the
 * higher-level routines.
 */
static int do_udf_rename(struct inode *old_dir, struct dentry *old_dentry,
	struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode * old_inode, * new_inode;
	struct udf_fileident_bh ofibh, nfibh;
	struct fileIdentDesc *ofi = NULL, *nfi = NULL, *dir_fi = NULL, ocfi, ncfi;
	struct buffer_head *dir_bh = NULL;
	int retval = -ENOENT;

	if (old_dentry->d_name.len > UDF_NAME_LEN)
		goto end_rename;

	old_inode = old_dentry->d_inode;
	if ((ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi)))
	{
		if (ofibh.sbh != ofibh.ebh)
			udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}
	if (!ofi || udf_get_lb_pblock(old_dir->i_sb, lelb_to_cpu(ocfi.icb.extLocation), 0) !=
		old_inode->i_ino)
	{
		goto end_rename;
	}

	new_inode = new_dentry->d_inode;
	nfi = udf_find_entry(new_dir, new_dentry, &nfibh, &ncfi);
	if (nfi)
	{
		if (!new_inode)
		{
			if (nfibh.sbh != nfibh.ebh)
				udf_release_data(nfibh.ebh);
			udf_release_data(nfibh.sbh);
			nfi = NULL;
		}
		else
		{
			DQUOT_INIT(new_inode);
		}
	}
	retval = 0;
	if (new_inode == old_inode)
		goto end_rename;
	if (S_ISDIR(old_inode->i_mode))
	{
		uint32_t offset = udf_ext0_offset(old_inode);
		retval = -EINVAL;
		if (is_subdir(new_dentry, old_dentry))
			goto end_rename;

		if (new_inode)
		{
			/* Prune any children before testing for busy */
			if (new_dentry->d_count > 1)
				shrink_dcache_parent(new_dentry);
				retval = -EBUSY;
				if (new_dentry->d_count > 1)
					goto end_rename;
			retval = -ENOTEMPTY;
			if (!empty_dir(new_inode))
				goto end_rename;
		}
		if (UDF_I_ALLOCTYPE(old_inode) == ICBTAG_FLAG_AD_IN_ICB)
		{
			dir_fi = udf_get_fileident(UDF_I_DATA(old_inode) -
				(UDF_I_EFE(old_inode) ?
					sizeof(struct extendedFileEntry) :
					sizeof(struct fileEntry)),
				old_inode->i_sb->s_blocksize, &offset);
		}
		else
		{
			dir_bh = udf_bread(old_inode, 0, 0, &retval);
			if (!dir_bh)
				goto end_rename;
			dir_fi = udf_get_fileident(dir_bh->b_data, old_inode->i_sb->s_blocksize, &offset);
		}
		if (!dir_fi)
			goto end_rename;
		if (udf_get_lb_pblock(old_inode->i_sb, cpu_to_lelb(dir_fi->icb.extLocation), 0) !=
			old_dir->i_ino)
		{
			goto end_rename;
		}
		retval = -EMLINK;
		if (!new_inode && new_dir->i_nlink >= (256<<sizeof(new_dir->i_nlink))-1)
			goto end_rename;
	}
	if (!nfi)
	{
		nfi = udf_add_entry(new_dir, new_dentry, &nfibh, &ncfi, &retval);
		if (!nfi)
			goto end_rename;
	}
	new_dir->i_version = ++event;

	/*
	 * Like most other Unix systems, set the ctime for inodes on a
	 * rename.
	 */
	old_inode->i_ctime = CURRENT_TIME;
	UDF_I_UCTIME(old_inode) = CURRENT_UTIME;
	mark_inode_dirty(old_inode);

	/*
	 * ok, that's it
	 */
	ncfi.fileVersionNum = ocfi.fileVersionNum;
	ncfi.fileCharacteristics = ocfi.fileCharacteristics;
	memcpy(&(ncfi.icb), &(ocfi.icb), sizeof(long_ad));
	udf_write_fi(&ncfi, nfi, &nfibh, NULL, NULL);

	/* The old fid may have moved - find it again */
	ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi);
	udf_delete_entry(old_inode->i_sb, ofi, &ofibh, &ocfi);

	old_dir->i_version = ++event;
	if (new_inode)
	{
		new_inode->i_nlink--;
		new_inode->i_ctime = CURRENT_TIME;
		UDF_I_UCTIME(new_inode) = CURRENT_UTIME;
		mark_inode_dirty(new_inode);
	}
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	UDF_I_UCTIME(old_dir) = UDF_I_UMTIME(old_dir) = CURRENT_UTIME;
	mark_inode_dirty(old_dir);

	if (dir_fi)
	{
		dir_fi->icb.extLocation = lelb_to_cpu(UDF_I_LOCATION(new_dir));
		udf_update_tag((char *)dir_fi, (sizeof(struct fileIdentDesc) +
			cpu_to_le16(dir_fi->lengthOfImpUse) + 3) & ~3);
		if (UDF_I_ALLOCTYPE(old_inode) == ICBTAG_FLAG_AD_IN_ICB)
		{
			mark_inode_dirty(old_inode);
			old_inode->i_version = ++event;
		}
		else
			mark_buffer_dirty(dir_bh, 1);
		old_dir->i_nlink --;
		mark_inode_dirty(old_dir);
		if (new_inode)
		{
			new_inode->i_nlink --;
			mark_inode_dirty(new_inode);
		}
		else
		{
			new_dir->i_nlink ++;
			mark_inode_dirty(new_dir);
		}
	}

	/* Update the dcache */
	d_move(old_dentry, new_dentry);

	if (ofi)
	{
		if (ofibh.sbh != ofibh.ebh)
			udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}

	retval = 0;
end_rename:
	udf_release_data(dir_bh);
	if (nfi)
	{
		if (nfibh.sbh != nfibh.ebh)
			udf_release_data(nfibh.ebh);
		udf_release_data(nfibh.sbh);
	}
	return retval;
}

/* Ok, rename also locks out other renames, as they can change the parent of
 * a directory, and we don't want any races. Other races are checked for by
 * "do_rename()", which restarts if there are inconsistencies.
 *
 * Note that there is no race between different filesystems: it's only within
 * the same device that races occur: many renames can happen at once, as long
 * as they are on different partitions.
 *
 * In the udf file system, we use a lock flag stored in the memory
 * super-block.  This way, we really lock other renames only if they occur
 * on the same file system
 */

int udf_rename(struct inode *old_dir, struct dentry *old_dentry,
	struct inode *new_dir, struct dentry *new_dentry)
{
	int result;

	while (UDF_SB_RENAME_LOCK(old_dir->i_sb))
		sleep_on(&UDF_SB_RENAME_WAIT(old_dir->i_sb));
	UDF_SB_RENAME_LOCK(old_dir->i_sb) = 1;
	result = do_udf_rename(old_dir, old_dentry, new_dir, new_dentry);
	UDF_SB_RENAME_LOCK(old_dir->i_sb) = 0;
	wake_up(&UDF_SB_RENAME_WAIT(old_dir->i_sb));
	return result;
}

#else

/* Anybody can rename anything with this: the permission checks are left to the
 * higher-level routines.
 */
int udf_rename (struct inode * old_dir, struct dentry * old_dentry,
	struct inode * new_dir, struct dentry * new_dentry)
{
	struct inode * old_inode, * new_inode;
	struct udf_fileident_bh ofibh, nfibh;
	struct fileIdentDesc *ofi = NULL, *nfi = NULL, *dir_fi = NULL, ocfi, ncfi;
	struct buffer_head *dir_bh = NULL;
	int retval = -ENOENT;

	old_inode = old_dentry->d_inode;
	if ((ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi)))
	{
		if (ofibh.sbh != ofibh.ebh)
				udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}
	if (!ofi || udf_get_lb_pblock(old_dir->i_sb, lelb_to_cpu(ocfi.icb.extLocation), 0) !=
		old_inode->i_ino)
	{
		goto end_rename;
	}

	new_inode = new_dentry->d_inode;
	nfi = udf_find_entry(new_dir, new_dentry, &nfibh, &ncfi);
	if (nfi)
	{
		if (!new_inode)
		{
			if (nfibh.sbh != nfibh.ebh)
				udf_release_data(nfibh.ebh);
			udf_release_data(nfibh.sbh);
			nfi = NULL;
		}
		else
		{
			DQUOT_INIT(new_inode);
		}
	}
	if (S_ISDIR(old_inode->i_mode))
	{
		uint32_t offset = udf_ext0_offset(old_inode);

		if (new_inode)
		{
			retval = -ENOTEMPTY;
			if (!empty_dir(new_inode))
				goto end_rename;
		}
		retval = -EIO;
		dir_bh = udf_bread(old_inode, 0, 0, &retval);
		if (!dir_bh)
			goto end_rename;
		dir_fi = udf_get_fileident(dir_bh->b_data, old_inode->i_sb->s_blocksize, &offset);
		if (!dir_fi)
			goto end_rename;
		if (udf_get_lb_pblock(old_inode->i_sb, cpu_to_lelb(dir_fi->icb.extLocation), 0) !=
			old_dir->i_ino)
		{
			goto end_rename;
		}
		retval = -EMLINK;
		if (!new_inode && new_dir->i_nlink >= (256<<sizeof(new_dir->i_nlink))-1)
			goto end_rename;
	}
	if (!nfi)
	{
		nfi = udf_add_entry(new_dir, new_dentry, &nfibh, &ncfi, &retval);
		if (!nfi)
			goto end_rename;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
	new_dir->i_version = ++event;
#else
	new_dir->i_version = ++global_event;
#endif

	/*
	 * ok, that's it
	 */
	ncfi.fileVersionNum = ocfi.fileVersionNum;
	ncfi.fileCharacteristics = ocfi.fileCharacteristics;
	memcpy(&(ncfi.icb), &(ocfi.icb), sizeof(long_ad));
	udf_write_fi(&ncfi, nfi, &nfibh, NULL, NULL);

	/* The old fid may have moved - find it again */
	ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi);
	udf_delete_entry(old_inode->i_sb, ofi, &ofibh, &ocfi);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
	old_dir->i_version = ++event;
#else
	old_dir->i_version = ++global_event;
#endif
	if (new_inode)
	{
		new_inode->i_nlink--;
		new_inode->i_ctime = CURRENT_TIME;
		UDF_I_UCTIME(new_inode) = CURRENT_UTIME;
		mark_inode_dirty(new_inode);
	}
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	UDF_I_UCTIME(old_dir) = UDF_I_UMTIME(old_dir) = CURRENT_UTIME;
	mark_inode_dirty(old_dir);

	if (dir_bh)
	{
		dir_fi->icb.extLocation = lelb_to_cpu(UDF_I_LOCATION(new_dir));
		udf_update_tag((char *)dir_fi, (sizeof(struct fileIdentDesc) +
			cpu_to_le16(dir_fi->lengthOfImpUse) + 3) & ~3);
		if (UDF_I_ALLOCTYPE(old_inode) == ICBTAG_FLAG_AD_IN_ICB)
		{
			mark_inode_dirty(old_inode);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
			old_inode->i_version = ++event;
#else
			old_inode->i_version = ++global_event;
#endif
		}
		else
			mark_buffer_dirty(dir_bh, 1);
		old_dir->i_nlink --;
		mark_inode_dirty(old_dir);
		if (new_inode)
		{
			new_inode->i_nlink --;
			mark_inode_dirty(new_inode);
		}
		else
		{
			new_dir->i_nlink ++;
			mark_inode_dirty(new_dir);
		}
	}

	if (ofi)
	{
		if (ofibh.sbh != ofibh.ebh)
			udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}
	retval = 0;

end_rename:
	udf_release_data(dir_bh);
	if (nfi)
	{
		if (nfibh.sbh != nfibh.ebh)
			udf_release_data(nfibh.ebh);
		udf_release_data(nfibh.sbh);
	}
	return retval;
}
#endif
