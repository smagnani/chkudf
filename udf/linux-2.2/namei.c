/*
 * namei.c
 *
 * PURPOSE
 *      Inode name handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *      E-mail regarding any portion of the Linux UDF file system should be
 *      directed to the development team mailing list (run by majordomo):
 *              linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *      This file is distributed under the terms of the GNU General Public
 *      License (GPL). Copies of the GPL can be obtained from:
 *              ftp://prep.ai.mit.edu/pub/gnu/GPL
 *      Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-1999 Ben Fennema
 *  (C) 1999 Stelias Computing Inc
 *
 * HISTORY
 *
 * 12/12/98 blf  Created. Split out the lookup code from dir.c
 * 04/19/99 blf  link, mknod, symlink support
 *
 */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/version.h>
#include "udf_i.h"
#include "udf_sb.h"
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/udf_fs.h>
#endif

#include "udfdecl.h"

static inline int udf_match(int len, const char * const name, struct qstr *qs)
{
	if (len != qs->len)
		return 0;
	return !memcmp(name, qs->name, len);
}

int udf_write_fi(struct FileIdentDesc *cfi, struct FileIdentDesc *sfi,
	struct udf_fileident_bh *fibh,
	Uint8 *impuse, Uint8 *fileident)
{
	struct FileIdentDesc *efi;
	Uint16 crclen = fibh->eoffset - fibh->soffset - sizeof(tag);
	Uint16 crc;
	Uint8 checksum = 0;
	int i;
	int offset, len;
	int padlen = fibh->eoffset - fibh->soffset - cfi->lengthOfImpUse - cfi->lengthFileIdent -
		sizeof(struct FileIdentDesc);

	crc = udf_crc((Uint8 *)cfi + sizeof(tag), sizeof(struct FileIdentDesc) -
		sizeof(tag), 0);
	efi = (struct FileIdentDesc *)(fibh->ebh->b_data + fibh->soffset);
	if (fibh->sbh == fibh->ebh ||
		(!fileident &&
			(sizeof(struct FileIdentDesc) + (impuse ? cfi->lengthOfImpUse : 0))
			<= -fibh->soffset))
	{
		memcpy((Uint8 *)sfi, (Uint8 *)cfi, sizeof(struct FileIdentDesc));

		if (impuse)
			memcpy(sfi->impUse, impuse, cfi->lengthOfImpUse);

		if (fileident)
			memcpy(sfi->fileIdent + cfi->lengthOfImpUse, fileident,
				cfi->lengthFileIdent);

        /* Zero padding */
		memset(sfi->fileIdent + cfi->lengthOfImpUse + cfi->lengthFileIdent, 0,
			padlen);

		if (fibh->sbh == fibh->ebh)
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen, 0);
		else
		{
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen - fibh->eoffset, 0);
			crc = udf_crc(fibh->ebh->b_data, fibh->eoffset, crc);
		}

		sfi->descTag.descCRC = cpu_to_le32(crc);
		sfi->descTag.descCRCLength = cpu_to_le16(crclen);

		for (i=0; i<16; i++)
			if (i != 4)
				checksum += ((Uint8 *)&sfi->descTag)[i];

		sfi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(fibh->sbh, 1);
	}
	else
	{
		offset = -fibh->soffset;
		len = sizeof(struct FileIdentDesc);

		if (len <= offset)
			memcpy((Uint8 *)sfi, (Uint8 *)cfi, len);
		else
		{
			memcpy((Uint8 *)sfi, (Uint8 *)cfi, offset);
			memcpy(fibh->ebh->b_data, (Uint8 *)cfi + offset, len - offset);
		}

		offset -= len;
		len = cfi->lengthOfImpUse;

		if (impuse)
		{
			if (offset <= 0)
				memcpy(efi->impUse, impuse, len);
			else if (sizeof(struct FileIdentDesc) + len <= -fibh->soffset)
				memcpy(sfi->impUse, impuse, len);
			else
			{
				memcpy(sfi->impUse, impuse, offset);
				memcpy(efi->impUse + offset, impuse + offset, len - offset);
			}
		}

		offset -= len;
		len = cfi->lengthFileIdent;

		if (fileident)
		{
			if (offset <= 0)
				memcpy(efi->fileIdent + cfi->lengthOfImpUse, fileident, len);
			else
			{
				memcpy(sfi->fileIdent + cfi->lengthOfImpUse, fileident, offset);
				memcpy(efi->fileIdent + cfi->lengthOfImpUse + offset,
					fileident + offset, len - offset);
			}
		}

        /* Zero padding */
        memset(efi->fileIdent + cfi->lengthOfImpUse + cfi->lengthFileIdent, 0x00,
			padlen);

		if (sizeof(tag) < -fibh->soffset)
		{
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen - fibh->eoffset, 0);
			crc = udf_crc(fibh->ebh->b_data, fibh->eoffset, crc);
		}
		else
			crc = udf_crc((Uint8 *)fibh->ebh->b_data + fibh->eoffset - crclen, crclen, 0);

		if (&(efi->descTag.descCRC) < (Uint16 *)fibh->ebh->b_data)
		{
			sfi->descTag.descCRC = cpu_to_le16(crc);
			sfi->descTag.descCRCLength = cpu_to_le16(crclen);
		}
		else
		{
			efi->descTag.descCRC = cpu_to_le16(crc);
			efi->descTag.descCRCLength = cpu_to_le16(crclen);
		}

		for (i=0; i<16; i++)
		{
			if (i != 4)
			{
				if (&(((Uint8 *)&efi->descTag)[i]) < (Uint8 *)fibh->ebh->b_data)
					checksum += ((Uint8 *)&sfi->descTag)[i];
				else
					checksum += ((Uint8 *)&efi->descTag)[i];
			}
		}

		if (&(cfi->descTag.tagChecksum) < (Uint8 *)fibh->ebh->b_data)
			sfi->descTag.tagChecksum = checksum;
		else
			efi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(fibh->sbh, 1);
		mark_buffer_dirty(fibh->ebh, 1);
	}
	return 0;
}

static struct FileIdentDesc *
udf_find_entry(struct inode *dir, struct dentry *dentry,
	struct udf_fileident_bh *fibh,
	struct FileIdentDesc *cfi)
{
	struct FileIdentDesc *fi=NULL;
	int f_pos, block;
	int flen;
	char fname[255];
	char *nameptr;
	Uint8 lfi;
	Uint16 liu;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	lb_addr bloc, eloc;
	Uint32 extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	if (!dir)
		return NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	fibh->soffset = fibh->eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == EXTENT_RECORDED_ALLOCATED)
	{
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if (++offset < (elen >> dir->i_sb->s_blocksize_bits))
		{
			if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;
	}
	else
	{
		udf_release_data(bh);
		return NULL;
	}

	if (!(fibh->sbh = fibh->ebh = udf_tread(dir->i_sb, block, dir->i_sb->s_blocksize)))
	{
		udf_debug("udf_tread failed: block=%d\n", block);
		udf_release_data(bh);
		return NULL;
	}

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi, &bloc, &extoffset, &offset, &bh);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
			udf_release_data(bh);
			return NULL;
		}

		if (fibh->sbh == fibh->ebh)
		{
			nameptr = fi->fileIdent + liu;
		}
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = fibh->soffset + sizeof(struct FileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (Uint8 *)(fibh->ebh->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, fibh->ebh->b_data, poffset);
			}
		}

		if ( (cfi->fileCharacteristics & FILE_DELETED) != 0 )
		{
			if ( !IS_UNDELETE(dir->i_sb) )
				continue;
		}
	    
		if ( (cfi->fileCharacteristics & FILE_HIDDEN) != 0 )
		{
			if ( !IS_UNHIDE(dir->i_sb) )
				continue;
		}

		if (!lfi)
			continue;

		if ((flen = udf_get_filename(nameptr, fname, lfi)))
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
	struct FileIdentDesc cfi, *fi;
	struct udf_fileident_bh fibh;

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

static struct FileIdentDesc *
udf_add_entry(struct inode *dir, struct dentry *dentry,
	struct udf_fileident_bh *fibh,
	struct FileIdentDesc *cfi, int *err)
{
	struct super_block *sb;
	struct FileIdentDesc *fi=NULL;
	struct ustr unifilename;
	char name[UDF_NAME_LEN], fname[UDF_NAME_LEN];
	int namelen;
	int f_pos;
	int flen;
	char *nameptr;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	int nfidlen;
	Uint8 lfi;
	Uint16 liu;
	int block;
	lb_addr bloc, eloc;
	Uint32 extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	*err = -EINVAL;
	if (!dir || !dir->i_nlink)
		return NULL;
	sb = dir->i_sb;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	if (dentry->d_name.len >= UDF_NAME_LEN)
	{
		*err = -ENAMETOOLONG;
		return NULL;
	}
#endif

	if (!dentry->d_name.len)
		return NULL;

	if (dir->i_size == 0)
	{
		*err = -ENOENT;
		return NULL;
	}

	if ( !(udf_char_to_ustr(&unifilename, dentry->d_name.name, dentry->d_name.len)) )
	{
		*err = -ENAMETOOLONG;
		return NULL;
	}

	if ( !(namelen = udf_UTF8toCS0(name, &unifilename, UDF_NAME_LEN)) )
		return 0;

	nfidlen = (sizeof(struct FileIdentDesc) + 0 + namelen + 3) & ~3;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	fibh->soffset = fibh->eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == EXTENT_RECORDED_ALLOCATED)
	{
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if (++offset < (elen >> dir->i_sb->s_blocksize_bits))
		{
			if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;
	}
	else
	{
		udf_release_data(bh);
		return NULL;
	}

	if (!(fibh->sbh = fibh->ebh = udf_tread(dir->i_sb, block, dir->i_sb->s_blocksize)))
		return NULL;

	block = UDF_I_LOCATION(dir).logicalBlockNum;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi, &bloc, &extoffset, &offset, &bh);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
			udf_release_data(bh);
			return NULL;
		}

		if (fibh->sbh == fibh->ebh)
			nameptr = fi->fileIdent + liu;
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = fibh->soffset + sizeof(struct FileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (char *)(fibh->ebh->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, fibh->ebh->b_data, poffset);
			}
		}

		if ( (cfi->fileCharacteristics & FILE_DELETED) != 0 )
		{
			if (((sizeof(struct FileIdentDesc) + liu + lfi + 3) & ~3) == nfidlen)
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
					return NULL;
			}
		}

		if (!lfi)
			continue;

		if ((flen = udf_get_filename(nameptr, fname, lfi)))
		{
			if (udf_match(flen, fname, &(dentry->d_name)))
			{
				if (fibh->sbh != fibh->ebh)
					udf_release_data(fibh->ebh);
				udf_release_data(fibh->sbh);
				udf_release_data(bh);
				*err = -EEXIST;
				return NULL;
			}
		}
	}

	f_pos += nfidlen;

	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB &&
		sb->s_blocksize - fibh->eoffset < nfidlen)
	{
		udf_release_data(bh);
		bh = NULL;
		fibh->soffset -= UDF_I_EXT0OFFS(dir);
		fibh->eoffset -= UDF_I_EXT0OFFS(dir);
		f_pos -= (UDF_I_EXT0OFFS(dir) >> 2);
		udf_release_data(fibh->sbh);
		if (!(fibh->sbh = fibh->ebh = udf_expand_adinicb(dir, &block, 1, err)))
			return NULL;
		bloc = UDF_I_LOCATION(dir);
		extoffset = udf_file_entry_alloc_offset(dir);
	}
	else
	{
		if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_SHORT)
			extoffset -= sizeof(short_ad);
		else if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_LONG)
			extoffset -= sizeof(long_ad);
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

		if (UDF_I_ALLOCTYPE(dir) != ICB_FLAG_AD_IN_ICB)
		{
			Uint32 lextoffset = extoffset;
			if (udf_next_aext(dir, &bloc, &extoffset, &eloc, &elen, &bh, 1) !=
				EXTENT_RECORDED_ALLOCATED)
			{
				udf_release_data(bh);
				udf_release_data(fibh->sbh);
				return NULL;
			}
			else
			{
				elen += nfidlen;
				elen = (EXTENT_RECORDED_ALLOCATED << 30) | elen;
				udf_write_aext(dir, bloc, &lextoffset, eloc, elen, &bh, 1);
				block = eloc.logicalBlockNum + (elen >> dir->i_sb->s_blocksize_bits);
			}
		}
		else
			block = UDF_I_LOCATION(dir).logicalBlockNum;
				
		fi = (struct FileIdentDesc *)(fibh->sbh->b_data + fibh->soffset);
	}
	else
	{
		Uint32 lextoffset = extoffset;

		fibh->soffset = fibh->eoffset - sb->s_blocksize;
		fibh->eoffset += nfidlen - sb->s_blocksize;
		if (fibh->sbh != fibh->ebh)
		{
			udf_release_data(fibh->sbh);
			fibh->sbh = fibh->ebh;
		}

		if (udf_next_aext(dir, &bloc, &extoffset, &eloc, &elen, &bh, 1) !=
			EXTENT_RECORDED_ALLOCATED)
		{
			udf_release_data(bh);
			udf_release_data(fibh->sbh);
			return NULL;
		}
		else
			block = eloc.logicalBlockNum + (elen >> dir->i_sb->s_blocksize_bits);

		*err = -ENOSPC;
		if (!(fibh->ebh = udf_getblk(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2), 1, err)))
		{
			udf_release_data(bh);
			udf_release_data(fibh->sbh);
			return NULL;
		}
		if (!(fibh->soffset))
		{
			if (udf_next_aext(dir, &bloc, &lextoffset, &eloc, &elen, &bh, 1) ==
				EXTENT_RECORDED_ALLOCATED)
			{
				block = eloc.logicalBlockNum + (elen >> dir->i_sb->s_blocksize_bits);
			}
			else
				block ++;
		}

		fi = (struct FileIdentDesc *)(fibh->sbh->b_data + sb->s_blocksize + fibh->soffset);
	}

	memset(cfi, 0, sizeof(struct FileIdentDesc));
	udf_new_tag((char *)cfi, TID_FILE_IDENT_DESC, 2, 1, block, sizeof(tag));
	cfi->fileVersionNum = cpu_to_le16(1);
	cfi->lengthFileIdent = namelen;
	cfi->lengthOfImpUse = cpu_to_le16(0);
	if (!udf_write_fi(cfi, fi, fibh, NULL, name))
	{
		udf_release_data(bh);
		dir->i_size += nfidlen;
		if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
			UDF_I_LENALLOC(dir) += nfidlen;
		dir->i_version = ++event;
		mark_inode_dirty(dir);
		return fi;
	}
	else
	{
		udf_release_data(bh);
		if (fibh->sbh != fibh->ebh)
			udf_release_data(fibh->ebh);
		udf_release_data(fibh->sbh);
		return NULL;
	}
}

static int udf_delete_entry(struct FileIdentDesc *fi,
	struct udf_fileident_bh *fibh,
	struct FileIdentDesc *cfi)
{
	cfi->fileCharacteristics |= FILE_DELETED;
	return udf_write_fi(cfi, fi, fibh, NULL, NULL);
}

int udf_create(struct inode *dir, struct dentry *dentry, int mode)
{
	struct udf_fileident_bh fibh;
	struct inode *inode;
	struct FileIdentDesc cfi, *fi;
	int err;

	inode = udf_new_inode(dir, mode, &err);
	if (!inode)
		return err;

	inode->i_op = &udf_file_inode_operations;
	inode->i_mode = mode;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
	{
		udf_debug("udf_add_entry failure!\n");
		inode->i_nlink --;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
		dir->i_version = ++event;
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
	struct FileIdentDesc cfi, *fi;

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
		udf_debug("udf_add_entry failure!\n");
		inode->i_nlink --;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
		dir->i_version = ++event;
	}
	if (S_ISREG(inode->i_mode))
	{
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
	struct FileEntry *fe;
	struct FileIdentDesc cfi, *fi;
	Uint32 loc;

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
	inode->i_size = (sizeof(struct FileIdentDesc) + 3) & ~3;
	if (UDF_I_ALLOCTYPE(inode) == ICB_FLAG_AD_IN_ICB)
	{
		UDF_I_EXT0LEN(inode) = inode->i_size;
		UDF_I_EXT0LOC(inode) = UDF_I_LOCATION(inode);
		UDF_I_LENALLOC(inode) = inode->i_size;
		loc = UDF_I_LOCATION(inode).logicalBlockNum;
		fibh.sbh = udf_tread(inode->i_sb, inode->i_ino, inode->i_sb->s_blocksize);
	}
	else
	{
		fibh.sbh = udf_bread (inode, 0, 1, &err);
		loc = UDF_I_EXT0LOC(inode).logicalBlockNum;
	}

	if (!fibh.sbh)
	{
		inode->i_nlink--;
		mark_inode_dirty(inode);
		iput(inode);
		goto out;
	}
	inode->i_nlink = 2;
	fe = (struct FileEntry *)fibh.sbh->b_data;
	fi = (struct FileIdentDesc *)&(fe->extendedAttr[UDF_I_LENEATTR(inode)]);
	udf_new_tag((char *)&cfi, TID_FILE_IDENT_DESC, 2, 1, loc,
		sizeof(struct FileIdentDesc));
	cfi.fileVersionNum = cpu_to_le16(1);
	cfi.fileCharacteristics = FILE_DIRECTORY | FILE_PARENT;
	cfi.lengthFileIdent = 0;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(dir));
	*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(dir) & 0x00000000FFFFFFFFUL);
	cfi.lengthOfImpUse = cpu_to_le16(0);
	fibh.ebh = fibh.sbh;
	fibh.soffset = sizeof(struct FileEntry);
	fibh.eoffset = sizeof(struct FileEntry) + inode->i_size;
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	udf_release_data(fibh.sbh);
	inode->i_mode = S_IFDIR | (mode & (S_IRWXUGO|S_ISVTX) & ~current->fs->umask);
	if (dir->i_mode & S_ISGID)
		inode->i_mode |= S_ISGID;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
	{
		udf_debug("udf_add_entry failure!\n");
		inode->i_nlink = 0;
		mark_inode_dirty(inode);
		iput(inode);
		goto out;
	}
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
	*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
		cpu_to_le32(UDF_I_UNIQUE(inode) & 0x00000000FFFFFFFFUL);
	cfi.fileCharacteristics |= FILE_DIRECTORY;
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	dir->i_version = ++event;
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
	struct FileIdentDesc *fi, cfi;
	struct udf_fileident_bh fibh;
	int f_pos;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	int block;
	lb_addr bloc, eloc;
	Uint32 extoffset, elen, offset;
	struct buffer_head *bh = NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	fibh.soffset = fibh.eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	if (inode_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2),
		&bloc, &extoffset, &eloc, &elen, &offset, &bh) == EXTENT_RECORDED_ALLOCATED)
	{
		block = udf_get_lb_pblock(dir->i_sb, eloc, offset);
		if (++offset < (elen >> dir->i_sb->s_blocksize_bits))
		{
			if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_SHORT)
				extoffset -= sizeof(short_ad);
			else if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_LONG)
				extoffset -= sizeof(long_ad);
		}
		else
			offset = 0;
	}
	else
	{
		udf_release_data(bh);
		return 0;
	}

	if (!(fibh.sbh = fibh.ebh = udf_tread(dir->i_sb, block, dir->i_sb->s_blocksize)))
		return 0;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, &fibh, &cfi, &bloc, &extoffset, &offset, &bh);

		if (!fi)
		{
			if (fibh.sbh != fibh.ebh)
				udf_release_data(fibh.ebh);
			udf_release_data(fibh.sbh);
			udf_release_data(bh);
			return 0;
		}

		if (cfi.lengthFileIdent && (cfi.fileCharacteristics & FILE_DELETED) == 0)
		{
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
	struct inode * inode;
	struct udf_fileident_bh fibh;
	struct FileIdentDesc *fi, cfi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	inode = dentry->d_inode;

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
		retval = udf_delete_entry(fi, &fibh, &cfi);
		dir->i_version = ++event;
	}
#else
	retval = -ENOTEMPTY;
	if (!empty_dir(inode))
		goto end_rmdir;
	retval = udf_delete_entry(fi, &fibh, &cfi);
	dir->i_version = ++event;
#endif
	if (retval)
		goto end_rmdir;
	if (inode->i_nlink != 2)
		udf_warning(inode->i_sb, "udf_rmdir",
			"empty directory has nlink != 2 (%d)",
			inode->i_nlink);
	inode->i_version = ++event;
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
	struct inode * inode;
	struct udf_fileident_bh fibh;
	struct FileIdentDesc *fi;
	struct FileIdentDesc cfi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,6)
	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;
#endif

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	inode = dentry->d_inode;

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
	retval = udf_delete_entry(fi, &fibh, &cfi);
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
	struct PathComponent *pc;
	struct udf_fileident_bh fibh;
	struct buffer_head *bh = NULL;
	int eoffset, elen = 0;
	struct FileIdentDesc *fi;
	struct FileIdentDesc cfi;
	char *ea;
	int err;

	if (!(inode = udf_new_inode(dir, S_IFLNK, &err)))
		goto out;

	inode->i_mode = S_IFLNK | S_IRWXUGO;
	inode->i_op = &udf_symlink_inode_operations;

	bh = udf_tread(inode->i_sb, inode->i_ino, inode->i_sb->s_blocksize);
	ea = bh->b_data + udf_file_entry_alloc_offset(inode);

	eoffset = inode->i_sb->s_blocksize - (ea - bh->b_data);
	pc = (struct PathComponent *)ea;

	if (*symname == '/')
	{
		do
		{
			symname++;
		} while (*symname == '/');

		pc->componentType = 1;
		pc->lengthComponentIdent = 0;
		pc->componentFileVersionNum = 0;
		pc += sizeof(struct PathComponent);
		elen += sizeof(struct PathComponent);
	}

	while (*symname && eoffset > elen + sizeof(struct PathComponent))
	{
		char *compstart;
		pc = (struct PathComponent *)(ea + elen);

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
			if (elen + sizeof(struct PathComponent) + symname - compstart > eoffset)
				pc->lengthComponentIdent = eoffset - elen - sizeof(struct PathComponent);
			else
				pc->lengthComponentIdent = symname - compstart;

			memcpy(pc->componentIdent, compstart, pc->lengthComponentIdent);
		}

		elen += sizeof(struct PathComponent) + pc->lengthComponentIdent;

		if (*symname)
		{
			do
			{
				symname++;
			} while (*symname == '/');
		}
	}

	udf_release_data(bh);
	UDF_I_LENALLOC(inode) = inode->i_size = elen;
	mark_inode_dirty(inode);

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
		goto out;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(inode));
    if (UDF_SB_LVIDBH(inode->i_sb))
    {
        struct LogicalVolHeaderDesc *lvhd;
        Uint64 uniqueID;
        lvhd = (struct LogicalVolHeaderDesc *)(UDF_SB_LVID(inode->i_sb)->logicalVolContentsUse);
        uniqueID = le64_to_cpu(lvhd->uniqueID);
		*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
			le32_to_cpu(uniqueID & 0x00000000FFFFFFFFUL);
        if (!(++uniqueID & 0x00000000FFFFFFFFUL))
            uniqueID += 16;
        lvhd->uniqueID = cpu_to_le64(uniqueID);
        mark_buffer_dirty(UDF_SB_LVIDBH(inode->i_sb), 1);
	}
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
		dir->i_version = ++event;
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	d_instantiate(dentry, inode);
	err = 0;

out:
	return err;
}

int udf_link(struct dentry * old_dentry, struct inode * dir,
	 struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;
	struct udf_fileident_bh fibh;
	int err;
	struct FileIdentDesc cfi, *fi;

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
        struct LogicalVolHeaderDesc *lvhd;
        Uint64 uniqueID;
        lvhd = (struct LogicalVolHeaderDesc *)(UDF_SB_LVID(inode->i_sb)->logicalVolContentsUse);
        uniqueID = le64_to_cpu(lvhd->uniqueID);
		*(Uint32 *)((struct ADImpUse *)cfi.icb.impUse)->impUse =
			cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
        if (!(++uniqueID & 0x00000000FFFFFFFFUL))
            uniqueID += 16;
        lvhd->uniqueID = cpu_to_le64(uniqueID);
        mark_buffer_dirty(UDF_SB_LVIDBH(inode->i_sb), 1);
	}
	udf_write_fi(&cfi, fi, &fibh, NULL, NULL);
	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
	{
		mark_inode_dirty(dir);
		dir->i_version = ++event;
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
	struct FileIdentDesc *ofi = NULL, *nfi = NULL, *dir_fi = NULL, ocfi, ncfi;
	struct buffer_head *dir_bh = NULL;
	int retval = -ENOENT;

	if (old_dentry->d_name.len > UDF_NAME_LEN)
		goto end_rename;

	old_inode = old_dentry->d_inode;
	ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi);
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
/*
			DQUOT_INIT(new_inode);
*/
		}
	}
	retval = 0;
	if (new_inode == old_inode)
		goto end_rename;
	if (S_ISDIR(old_inode->i_mode))
	{
		Uint32 offset = UDF_I_EXT0OFFS(old_inode);
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
	new_dir->i_version = ++event;

	/*
	 * ok, that's it
	 */
	ncfi.fileVersionNum = ocfi.fileVersionNum;
	ncfi.fileCharacteristics = ocfi.fileCharacteristics;
	memcpy(&(ncfi.icb), &(ocfi.icb), sizeof(long_ad));
	udf_write_fi(&ncfi, nfi, &nfibh, NULL, NULL);

	udf_delete_entry(ofi, &ofibh, &ocfi);

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

	if (dir_bh)
	{
		dir_fi->icb.extLocation = lelb_to_cpu(UDF_I_LOCATION(new_dir));
		udf_update_tag((char *)dir_fi, sizeof(struct FileIdentDesc) +
			cpu_to_le16(dir_fi->lengthOfImpUse));
		if (UDF_I_ALLOCTYPE(old_inode) == ICB_FLAG_AD_IN_ICB)
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
	retval = 0;

end_rename:
	udf_release_data(dir_bh);
	if (ofi)
	{
		if (ofibh.sbh != ofibh.ebh)
			udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}
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
	struct FileIdentDesc *ofi = NULL, *nfi = NULL, *dir_fi = NULL, ocfi, ncfi;
	struct buffer_head *dir_bh = NULL;
	int retval = -ENOENT;

	old_inode = old_dentry->d_inode;
	ofi = udf_find_entry(old_dir, old_dentry, &ofibh, &ocfi);
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
/*
			DQUOT_INIT(new_inode);
*/
		}
	}
	if (S_ISDIR(old_inode->i_mode))
	{
		Uint32 offset = UDF_I_EXT0OFFS(old_inode);

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
	new_dir->i_version = ++event;

	/*
	 * ok, that's it
	 */
	ncfi.fileVersionNum = ocfi.fileVersionNum;
	ncfi.fileCharacteristics = ocfi.fileCharacteristics;
	memcpy(&(ncfi.icb), &(ocfi.icb), sizeof(long_ad));
	udf_write_fi(&ncfi, nfi, &nfibh, NULL, NULL);

	udf_delete_entry(ofi, &ofibh, &ocfi);

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

	if (dir_bh)
	{
		dir_fi->icb.extLocation = lelb_to_cpu(UDF_I_LOCATION(new_dir));
		udf_update_tag((char *)dir_fi, sizeof(struct FileIdentDesc) +
			cpu_to_le16(dir_fi->lengthOfImpUse));
		if (UDF_I_ALLOCTYPE(old_inode) == ICB_FLAG_AD_IN_ICB)
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

	retval = 0;

end_rename:
	udf_release_data(dir_bh);
	if (ofi)
	{
		if (ofibh.sbh != ofibh.ebh)
			udf_release_data(ofibh.ebh);
		udf_release_data(ofibh.sbh);
	}
	if (nfi)
	{
		if (nfibh.sbh != nfibh.ebh)
			udf_release_data(nfibh.ebh);
		udf_release_data(nfibh.sbh);
	}
	return retval;
}
#endif
