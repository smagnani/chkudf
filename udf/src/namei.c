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
 * HISTORY
 *
 * 12/12/98 blf  Created. Split out the lookup code from dir.c
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

static int udf_write_fi(struct FileIdentDesc *cfi, struct FileIdentDesc *sfi,
	int soffset, struct buffer_head *sbh,
	int eoffset, struct buffer_head *ebh,
	Uint8 *impuse, Uint8 *fileident)
{
	struct FileIdentDesc *efi;
	Uint16 crclen = eoffset - soffset - sizeof(tag);
	Uint16 crc;
	Uint8 checksum = 0;
	int i;
	int offset, len;

#ifdef VDEBUG
	udf_debug("soffset=%d, eoffset=%d, impuse=%p, fileident=%p\n",
		soffset, eoffset, impuse, fileident);
#endif
	
	crc = udf_crc((Uint8 *)cfi + sizeof(tag), sizeof(struct FileIdentDesc) -
		sizeof(tag), 0);
	efi = (struct FileIdentDesc *)(ebh->b_data + soffset);
	if (sbh == ebh ||
		sizeof(struct FileIdentDesc) + (impuse ? cfi->lengthOfImpUse : 0) +
		(fileident ? cfi->lengthFileIdent : 0) <= -soffset)
	{
		memcpy((Uint8 *)sfi, (Uint8 *)cfi, sizeof(struct FileIdentDesc));

		if (impuse)
			memcpy(&sfi->impUse, impuse, sfi->lengthOfImpUse);

		if (fileident)
			memcpy(&sfi->fileIdent + sfi->lengthOfImpUse, fileident,
				sfi->lengthFileIdent);

		if (sbh == ebh)
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen, 0);
		else
		{
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen - eoffset, 0);
			crc = udf_crc(ebh->b_data, eoffset, crc);
		}

		sfi->descTag.descCRC = cpu_to_le32(crc);
		sfi->descTag.descCRCLength = cpu_to_le16(crclen);

		for (i=0; i<16; i++)
			if (i != 4)
				checksum += ((Uint8 *)&sfi->descTag)[i];

		sfi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(sbh, 1);
	}
	else
	{
		offset = -soffset;
		len = sizeof(struct FileIdentDesc);

		if (sizeof(struct FileIdentDesc) <= -soffset)
			memcpy((Uint8 *)sfi, (Uint8 *)cfi, len);
		else
		{
			memcpy((Uint8 *)sfi, (Uint8 *)cfi, offset);
			memcpy(ebh->b_data, (Uint8 *)cfi + offset, len - offset);
		}

		offset -= len;
		len = cfi->lengthOfImpUse;

		if (impuse)
		{
			if (sizeof(struct FileIdentDesc) + cfi->lengthOfImpUse <= -soffset)
				memcpy(&sfi->impUse, impuse, len);
			else
			{
				memcpy(&sfi->impUse, impuse, offset);
				memcpy(&efi->impUse + offset, impuse + offset, len - offset);
			}
		}

		offset -= len;
		len = cfi->lengthFileIdent;

		if (fileident)
		{
			memcpy(&sfi->fileIdent + cfi->lengthOfImpUse, fileident, offset);
			memcpy(&efi->fileIdent + cfi->lengthOfImpUse + offset,
				fileident + offset, len - offset);
		}

		if (sizeof(tag) < -soffset)
		{
			crc = udf_crc((Uint8 *)sfi + sizeof(tag), crclen - eoffset, 0);
			crc = udf_crc(ebh->b_data, eoffset, crc);
		}
		else
			crc = udf_crc((Uint8 *)ebh->b_data + eoffset - crclen, crclen, 0);

		if (&(cfi->descTag.descCRC) < (Uint16 *)ebh->b_data)
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
				if (&(((Uint8 *)&efi->descTag)[i]) < (Uint8 *)ebh->b_data)
					checksum += ((Uint8 *)&sfi->descTag)[i];
				else
					checksum += ((Uint8 *)&efi->descTag)[i];
			}
		}

		if (&(cfi->descTag.tagChecksum) < (Uint8 *)ebh->b_data)
			sfi->descTag.tagChecksum = checksum;
		else
			efi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(sbh, 1);
		mark_buffer_dirty(ebh, 1);
	}
	return 0;
}

static struct FileIdentDesc *
udf_find_entry(struct inode *dir, struct dentry *dentry,
	int *soffset, struct buffer_head **sbh,
	int *eoffset, struct buffer_head **ebh,
	struct FileIdentDesc *cfi)
{
	struct FileIdentDesc *fi=NULL;
	int block;
	int f_pos;
	int flen;
	char fname[255];
	char *nameptr;
	Uint8 lfi;
	Uint16 liu;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;

	if (!dir)
		return NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	*soffset = *eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	block = udf_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2));

	if (!block || !(*sbh = *ebh = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
		return NULL;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, soffset, sbh, eoffset, ebh, cfi);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (*sbh != *ebh)
				udf_release_data(*ebh);
			udf_release_data(*sbh);
			return NULL;
		}

		if (*sbh == *ebh)
		{
			nameptr = fi->fileIdent + liu;
		}
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = *soffset + sizeof(struct FileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (Uint8 *)((*ebh)->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, (*ebh)->b_data, poffset);
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
	if (*sbh != *ebh)
		udf_release_data(*ebh);
	udf_release_data(*sbh);
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

int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct FileIdentDesc cfi, *fi;
	struct buffer_head *sbh, *ebh;
	int soffset, eoffset;

	if ((fi = udf_find_entry(dir, dentry, &soffset, &sbh, &eoffset, &ebh, &cfi)))
	{
		if (sbh != ebh)
			udf_release_data(ebh);
		udf_release_data(sbh);

		inode = udf_iget(dir->i_sb, lelb_to_cpu(cfi.icb.extLocation));
		if ( !inode )
			return -EACCES;
	}
	d_add(dentry, inode);
	return 0;
}

static struct FileIdentDesc *
udf_add_entry(struct inode *dir, struct dentry *dentry,
	int *soffset, struct buffer_head **sbh,
	int *eoffset, struct buffer_head **ebh,
	struct FileIdentDesc *cfi, int *err)
{
	struct super_block *sb;
	struct FileIdentDesc *fi=NULL;
	struct ustr unifilename;
	char name[UDF_NAME_LEN], fname[UDF_NAME_LEN];
	int namelen;
	int block;
	int f_pos;
	int flen;
	char *nameptr;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	int nfidlen;
	Uint8 lfi;
	Uint16 liu;

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

	*soffset = *eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	block = udf_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2));

	if (!block || !(*sbh = *ebh = udf_bread(dir->i_sb, block, dir->i_sb->s_blocksize)))
		return NULL;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, soffset, sbh, eoffset, ebh, cfi);
		liu = le16_to_cpu(cfi->lengthOfImpUse);
		lfi = cfi->lengthFileIdent;

		if (!fi)
		{
			if (*sbh != *ebh)
				udf_release_data(*ebh);
			udf_release_data(*sbh);
			return NULL;
		}

		if (*sbh == *ebh)
			nameptr = fi->fileIdent + liu;
		else
		{
			int poffset;	/* Unpaded ending offset */

			poffset = *soffset + sizeof(struct FileIdentDesc) + liu + lfi;

			if (poffset >= lfi)
				nameptr = (char *)((*ebh)->b_data + poffset - lfi);
			else
			{
				nameptr = fname;
				memcpy(nameptr, fi->fileIdent + liu, lfi - poffset);
				memcpy(nameptr + lfi - poffset, (*ebh)->b_data, poffset);
			}
		}

		if ( (cfi->fileCharacteristics & FILE_DELETED) != 0 )
		{
			if (((sizeof(struct FileIdentDesc) + liu + lfi + 3) & ~3) == nfidlen)
			{
				memset(cfi, 0, sizeof(struct FileIdentDesc));
				cfi->descTag.tagIdent = cpu_to_le16(TID_FILE_IDENT_DESC);
				cfi->descTag.descVersion = cpu_to_le16(2);
				cfi->descTag.tagSerialNum = cpu_to_le16(1);
				cfi->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(dir).logicalBlockNum);
				cfi->fileVersionNum = cpu_to_le16(1);
				cfi->lengthFileIdent = namelen;
				cfi->lengthOfImpUse = cpu_to_le16(0);
				if (!udf_write_fi(cfi, fi, *soffset, *sbh, *eoffset, *ebh, NULL, name))
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
				*err = -EEXIST;
				return NULL;
			}
		}
	}
	if (sb->s_blocksize - *eoffset > nfidlen)
	{
		*soffset = *eoffset;
		*eoffset += nfidlen;
		if (*sbh != *ebh)
		{
			udf_release_data(*sbh);
			*sbh = *ebh;
		}
		fi = (struct FileIdentDesc *)((*sbh)->b_data + *soffset);
		memset(cfi, 0, sizeof(struct FileIdentDesc));
		cfi->descTag.tagIdent = cpu_to_le16(TID_FILE_IDENT_DESC);
		cfi->descTag.descVersion = cpu_to_le16(2);
		cfi->descTag.tagSerialNum = cpu_to_le16(1);
		cfi->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(dir).logicalBlockNum);
		cfi->fileVersionNum = cpu_to_le16(1);
		cfi->lengthFileIdent = namelen;
		cfi->lengthOfImpUse = cpu_to_le16(0);
		if (!udf_write_fi(cfi, fi, *soffset, *sbh, *eoffset, *ebh, NULL, name))
		{
			dir->i_size += nfidlen;
			mark_inode_dirty(dir);
			return fi;
		}
		else
			return NULL;
	}
	else
	{
		/* Allocate new block */
		return NULL;
	}
}

static int udf_delete_entry(struct FileIdentDesc *fi,
	int soffset, struct buffer_head *sbh,
	int eoffset, struct buffer_head *ebh,
	struct FileIdentDesc *cfi)
{
	cfi->fileCharacteristics |= FILE_DELETED;
	return udf_write_fi(cfi, fi, soffset, sbh, eoffset, ebh, NULL, NULL);
}

int udf_create(struct inode *dir, struct dentry *dentry, int mode)
{
    struct buffer_head *sbh, *ebh;
    int soffset, eoffset;
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

	if (!(fi = udf_add_entry(dir, dentry, &soffset, &sbh, &eoffset, &ebh, &cfi, &err)))
	{
		udf_debug("udf_add_entry failure!\n");
		inode->i_nlink --;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	cfi.icb.extLocation = UDF_I_LOCATION(inode);
	cfi.icb.extLength = inode->i_sb->s_blocksize;
	udf_write_fi(&cfi, fi, soffset, sbh, eoffset, ebh, NULL, NULL);
	dir->i_version = ++event;
	if (sbh != ebh)
		udf_release_data(ebh);
	udf_release_data(sbh);
	d_instantiate(dentry, inode);
	return 0;
}

int udf_unlink(struct inode * dir, struct dentry * dentry)
{
	int retval;
	struct inode * inode;
	struct buffer_head *sbh, *ebh;
	int soffset, eoffset;
	struct FileIdentDesc *fi;
	struct FileIdentDesc cfi;

#ifdef VDEBUG
	udf_debug("dir->i_ino=%ld\n", dir->i_ino);
#endif

	retval = -ENAMETOOLONG;
	if (dentry->d_name.len > UDF_NAME_LEN)
		goto out;

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &soffset, &sbh, &eoffset, &ebh, &cfi);
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
	retval = udf_delete_entry(fi, soffset, sbh, eoffset, ebh, &cfi);
	if (retval)
		goto end_unlink;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);
	inode->i_nlink--;
	mark_inode_dirty(inode);
	inode->i_ctime = dir->i_ctime;
	retval = 0;
	d_delete(dentry);	/* This also frees the inode */

end_unlink:
	if (sbh != ebh)
		udf_release_data(ebh);
	udf_release_data(sbh);
out:
	return retval;
}
