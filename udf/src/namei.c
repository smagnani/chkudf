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

#ifdef VDEBUG
	udf_debug("fibh->soffset=%d, fibh.eoffset=%d, impuse=%p, fileident=%p\n",
		fibh->soffset, fibh.eoffset, impuse, fileident);
#endif

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

		udf_debug("crclen=%d, crc=%d, descCRC=%p, ebh->b_data=%p\n",
			crclen, crc, &(efi->descTag.descCRC), fibh->ebh->b_data);

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
	int f_pos;
	int flen;
	char fname[255];
	char *nameptr;
	Uint8 lfi;
	Uint16 liu;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	int err = 0;

	if (!dir)
		return NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	fibh->soffset = fibh->eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;

	if (!(fibh->sbh = fibh->ebh = udf_bread(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2), 0, &err)))
		return NULL;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
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
				return fi;
			}
		}
	}
	if (fibh->sbh != fibh->ebh)
		udf_release_data(fibh->ebh);
	udf_release_data(fibh->sbh);
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

#if LINUX_VERSION_CODE < 0x020207
int
#else
struct dentry *
#endif
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct FileIdentDesc cfi, *fi;
	struct udf_fileident_bh fibh;

	if ((fi = udf_find_entry(dir, dentry, &fibh, &cfi)))
	{
		if (fibh.sbh != fibh.ebh)
			udf_release_data(fibh.ebh);
		udf_release_data(fibh.sbh);

		inode = udf_iget(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation));
		if ( !inode )
#if LINUX_VERSION_CODE < 0x020207
			return -EACCES;
#else
			return ERR_PTR(-EACCES);
#endif
	}
	d_add(dentry, inode);
#if LINUX_VERSION_CODE < 0x020207
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
	int block = UDF_I_LOCATION(dir).logicalBlockNum;

	*err = -EINVAL;
	if (!dir || !dir->i_nlink)
		return NULL;
	sb = dir->i_sb;

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

	if (!(fibh->sbh = fibh->ebh = udf_bread(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2), 0, err)))
		return NULL;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, fibh, cfi);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (fibh->sbh != fibh->ebh)
				udf_release_data(fibh->ebh);
			udf_release_data(fibh->sbh);
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
				cfi->descTag.tagSerialNum = cpu_to_le16(1);
				cfi->fileVersionNum = cpu_to_le16(1);
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

		block = le32_to_cpu(cfi->descTag.tagLocation); /* this is a hack */

		if ((flen = udf_get_filename(nameptr, fname, lfi)))
		{
			if (udf_match(flen, fname, &(dentry->d_name)))
			{
				if (fibh->sbh != fibh->ebh)
					udf_release_data(fibh->ebh);
				udf_release_data(fibh->sbh);
				*err = -EEXIST;
				return NULL;
			}
		}
	}

	f_pos += nfidlen;

	if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB &&
		sb->s_blocksize - fibh->eoffset < nfidlen)
	{
		fibh->soffset -= UDF_I_EXT0OFFS(dir);
		fibh->eoffset -= UDF_I_EXT0OFFS(dir);
		f_pos -= (UDF_I_EXT0OFFS(dir) >> 2);
		udf_release_data(fibh->sbh);
		if (!(fibh->sbh = fibh->ebh = udf_expand_adinicb(dir, &block, 1, err)))
			return NULL;
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
		fi = (struct FileIdentDesc *)(fibh->sbh->b_data + fibh->soffset);
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
		*err = -ENOSPC;
		if (!(fibh->ebh = udf_bread(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2), 1, err)))
		{
			udf_release_data(fibh->sbh);
			return NULL;
		}
		udf_debug("sbh=%p, ebh=%p, soffset=%d, eoffset=%d, block=%d, nfidlen=%d\n",
			fibh->sbh->b_data, fibh->ebh->b_data, fibh->soffset, fibh->eoffset, block, nfidlen);
		fi = (struct FileIdentDesc *)(fibh->sbh->b_data + sb->s_blocksize + fibh->soffset);
	}

	memset(cfi, 0, sizeof(struct FileIdentDesc));
	udf_new_tag((char *)cfi, TID_FILE_IDENT_DESC, 2, 1, block, sizeof(tag));
	cfi->fileVersionNum = cpu_to_le16(1);
	cfi->lengthFileIdent = namelen;
	cfi->lengthOfImpUse = cpu_to_le16(0);
	if (!udf_write_fi(cfi, fi, fibh, NULL, name))
	{
		udf_debug("size=%ld, nfidlen=%d\n", dir->i_size, nfidlen);
		dir->i_size += nfidlen;
		if (UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB)
			UDF_I_LENALLOC(dir) += nfidlen;
		else
		{
#if 0
			/* expand extent length */
			UDF_I_EXT0LEN(dir) += nfidlen; /* this is wrong */
#endif
		}
		dir->i_version = ++event;
		mark_inode_dirty(dir);
		return fi;
	}
	else
	{
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

#ifdef VDEBUG
	udf_debug("dir->i_ino=%ld, mode=%d\n", dir->i_ino, mode);
#endif

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
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
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

#ifdef VDEBUG
	udf_debug("dir->ino=%ld, dentry=%s\n", dir->i_ino, dentry->d_name.name);
#endif

	err = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;

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
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
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

#ifdef VDEBUG
	udf_debug("dir->ino=%ld, dentry=%s\n", dir->i_ino, dentry->d_name.name);
#endif

	err = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;

#if 0
	err = -EMLINK;
	if (dir->i_nlink >= UDF_LINK_MAX)
		goto out;
#endif

	err = -EIO;
	inode = udf_new_inode(dir, S_IFDIR, &err);
	if (!inode)
		goto out;

#ifdef VDEBUG
	udf_debug("inode->ino=%ld\n", inode->i_ino);
#endif

	inode->i_op = &udf_dir_inode_operations;
	inode->i_size = (sizeof(struct FileIdentDesc) + 3) & ~3;
	if (UDF_I_ALLOCTYPE(inode) == ICB_FLAG_AD_IN_ICB)
	{
		UDF_I_EXT0LEN(inode) = inode->i_size;
		UDF_I_EXT0LOC(inode) = UDF_I_LOCATION(inode);
		UDF_I_LENALLOC(inode) = inode->i_size;
		fibh.sbh = udf_tread(inode->i_sb, inode->i_ino, inode->i_sb->s_blocksize);
	}
	else
		fibh.sbh = udf_bread (inode, 0, 1, &err);

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
	cfi.descTag.tagIdent = cpu_to_le16(TID_FILE_IDENT_DESC);
	cfi.descTag.descVersion = cpu_to_le16(2);
	cfi.descTag.tagSerialNum = cpu_to_le16(1);
	cfi.descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(inode).logicalBlockNum);
	cfi.fileVersionNum = cpu_to_le16(1);
	cfi.fileCharacteristics = FILE_DIRECTORY | FILE_PARENT;
	cfi.lengthFileIdent = 0;
	cfi.icb.extLength = cpu_to_le32(inode->i_sb->s_blocksize);
	cfi.icb.extLocation = cpu_to_lelb(UDF_I_LOCATION(dir));
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
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
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

static int empty_dir(struct inode *inode)
{
	struct FileIdentDesc *fi, cfi;
	struct udf_fileident_bh fibh;
	int f_pos;
	int size = (UDF_I_EXT0OFFS(inode) + inode->i_size) >> 2;
	int err = 0;

	f_pos = (UDF_I_EXT0OFFS(inode) >> 2);

	fibh.soffset = fibh.eoffset = (f_pos & ((inode->i_sb->s_blocksize - 1) >> 2)) << 2;

	if (!(fibh.sbh = fibh.ebh = udf_bread(inode, f_pos >> (inode->i_sb->s_blocksize_bits - 2), 0, &err)))
		return 0;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(inode, &f_pos, &fibh, &cfi);

		if (!fi)
		{
			if (fibh.sbh != fibh.ebh)
				udf_release_data(fibh.ebh);
			udf_release_data(fibh.sbh);
			return 0;
		}

		if (cfi.lengthFileIdent && (cfi.fileCharacteristics & FILE_DELETED) == 0)
			return 0;
	}
	if (fibh.sbh != fibh.ebh)
		udf_release_data(fibh.ebh);
	udf_release_data(fibh.sbh);
	return 1;
}

int udf_rmdir(struct inode * dir, struct dentry * dentry)
{
	int retval;
	struct inode * inode;
	struct udf_fileident_bh fibh;
	struct FileIdentDesc *fi, cfi;

	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	inode = dentry->d_inode;

	retval = -EIO;

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

#ifdef VDEBUG
	udf_debug("dir->i_ino=%ld\n", dir->i_ino);
#endif

	retval = -ENAMETOOLONG;
	if (dentry->d_name.len >= UDF_NAME_LEN)
		goto out;

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &fibh, &cfi);
	if (!fi)
		goto out;

	inode = dentry->d_inode;

	retval = -EIO;

#ifdef VDEBUG
	udf_debug("pblock = %d, i_ino = %lu\n",
		udf_get_lb_pblock(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation), 0),
		inode->i_ino);
#endif

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
	struct buffer_head *bh;
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
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
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

	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return -EPERM;

#if 0
	if (inode->i_nlink >= UDF_LINK_MAX)
		return -EMLINK;
#endif

	if (!(fi = udf_add_entry(dir, dentry, &fibh, &cfi, &err)))
		return err;
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
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
