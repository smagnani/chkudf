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
#endif

#include "udfdecl.h"

static inline int udf_match(int len, const char * const name, struct qstr *qs)
{
	if (len != qs->len)
		return 0;
	return !memcmp(name, qs->name, len);
}

static struct FileIdentDesc *
udf_find_entry(struct inode *dir, struct dentry *dentry,
	int *soffset, struct buffer_head **sbh,
	int *eoffset, struct buffer_head **ebh)
{
	struct FileIdentDesc *fi=NULL;
	int block;
	int f_pos;
	int flen;
	char fname[255];
	char *nameptr;
	Uint8 filechar;
	Uint8 lfi;
	Uint16 liu;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;

	if (!dir)
		return NULL;

	f_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	*soffset = *eoffset = (f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	block = udf_bmap(dir, f_pos >> (dir->i_sb->s_blocksize_bits - 2));

	if (!block || !(*sbh = *ebh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
		return NULL;

	while ( (f_pos < size) )
	{
		fi = udf_fileident_read(dir, &f_pos, soffset, sbh, eoffset, ebh, &lfi, &liu);

		if (!fi)
		{
			udf_release_data(*sbh);
			if (*sbh != *ebh)
				udf_release_data(*ebh);
			return NULL;
		}

		if (*sbh == *ebh)
		{
			filechar = fi->fileCharacteristics;
			nameptr = fi->fileIdent + liu;
		}
		else
		{
			struct FileIdentDesc *nfi;
			int poffset;	/* Unpaded ending offset */

			nfi = (struct FileIdentDesc *)((*ebh)->b_data + *soffset);

			if (&(nfi->fileCharacteristics) < (Uint8 *)(*ebh)->b_data)
				filechar = fi->fileCharacteristics;
			else
				filechar = nfi->fileCharacteristics;

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

		if ( (filechar & FILE_DELETED) != 0 )
		{
			if ( !IS_UNDELETE(dir->i_sb) )
				continue;
		}
	    
		if ( (filechar & FILE_HIDDEN) != 0 )
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
	udf_release_data(*sbh);
	if (*sbh != *ebh)
		udf_release_data(*ebh);
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
	struct FileIdentDesc *fi;
	struct buffer_head *sbh, *ebh;
	int soffset, eoffset;

	if ((fi = udf_find_entry(dir, dentry, &soffset, &sbh, &eoffset, &ebh)))
	{
		lb_addr ino;

		if (sbh == ebh)
		{
			ino = lelb_to_cpu(fi->icb.extLocation);
			udf_release_data(sbh);
		}
		else
		{
			struct FileIdentDesc *nfi;

			nfi = (struct FileIdentDesc *)(ebh->b_data + soffset);

			if (&(nfi->icb.extLocation.logicalBlockNum) < (Uint32 *)ebh->b_data)
				ino.logicalBlockNum = le32_to_cpu(fi->icb.extLocation.logicalBlockNum);
			else
				ino.logicalBlockNum = le32_to_cpu(nfi->icb.extLocation.logicalBlockNum);

			if (&(nfi->icb.extLocation.partitionReferenceNum) < (Uint16 *)ebh->b_data)
				ino.partitionReferenceNum = le16_to_cpu(fi->icb.extLocation.partitionReferenceNum);
			else
				ino.partitionReferenceNum = le16_to_cpu(nfi->icb.extLocation.partitionReferenceNum);

			udf_release_data(sbh);
			udf_release_data(ebh);
		}

		inode = udf_iget(dir->i_sb, ino);
		if ( !inode )
			return -EACCES;
	}
	d_add(dentry, inode);
	return 0;
}

static int udf_delete_entry(struct FileIdentDesc *fi,
	int soffset, struct buffer_head *sbh,
	int eoffset, struct buffer_head *ebh)
{
	struct FileIdentDesc *nfi;
	Uint16 crclen = eoffset - soffset - sizeof(tag);
	Uint16 crc;
	Uint8 checksum = 0;
	int i;
	
	nfi = (struct FileIdentDesc *)(ebh->b_data + soffset);
	if (sbh == ebh)
	{
		fi->fileCharacteristics |= FILE_DELETED;
		crc = udf_crc((Uint8 *)fi + sizeof(tag), crclen, 0);
		fi->descTag.descCRC = cpu_to_le32(crc);
		fi->descTag.descCRCLength = cpu_to_le16(crclen);

		for (i=0; i<16; i++)
			if (i != 4)
				checksum += ((Uint8 *)&fi->descTag)[i];

		fi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(sbh, 1);
	}
	else if (&(nfi->fileCharacteristics) < (Uint8 *)ebh->b_data)
	{
		fi->fileCharacteristics |= FILE_DELETED;
		crc = udf_crc((Uint8 *)fi + sizeof(tag), crclen - eoffset, 0);
		crc = udf_crc(ebh->b_data, eoffset, crc);
		fi->descTag.descCRC = cpu_to_le16(crc);
		fi->descTag.descCRCLength = cpu_to_le16(crclen);

		for (i=0; i<16; i++)
			if (i != 4)
				checksum += ((Uint8 *)&fi->descTag)[i];

		fi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(sbh, 1);
	}
	else
	{
		nfi->fileCharacteristics |= FILE_DELETED;
		crc = udf_crc((Uint8 *)ebh->b_data + eoffset - crclen, crclen, 0);
		if (&(nfi->descTag.descCRC) < (Uint16 *)ebh->b_data)
		{
			fi->descTag.descCRC = cpu_to_le16(crc);
			fi->descTag.descCRCLength = cpu_to_le16(crclen);
		}
		else
		{
			nfi->descTag.descCRC = cpu_to_le16(crc);
			nfi->descTag.descCRCLength = cpu_to_le16(crclen);
		}

		for (i=0; i<16; i++)
		{
			if (i != 4)
			{
				if (&(((Uint8 *)&nfi->descTag)[i]) < (Uint8 *)ebh->b_data)
					checksum += ((Uint8 *)&fi->descTag)[i];
				else
					checksum += ((Uint8 *)&nfi->descTag)[i];
			}
		}

		if (&(nfi->descTag.tagChecksum) < (Uint8 *)ebh->b_data)
			fi->descTag.tagChecksum = checksum;
		else
			nfi->descTag.tagChecksum = checksum;

		mark_buffer_dirty(sbh, 1);
		mark_buffer_dirty(ebh, 1);
	}
	return 0;
}

int udf_unlink(struct inode * dir, struct dentry * dentry)
{
	int retval;
	struct inode * inode;
	struct buffer_head *sbh, *ebh;
	int soffset, eoffset;
	struct FileIdentDesc *fi;
	lb_addr ino;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_unlink: ino = %ld\n", dir->i_ino);
#endif

	retval = -ENAMETOOLONG;
	if (dentry->d_name.len > UDF_NAME_LEN)
		goto out;

	retval = -ENOENT;
	fi = udf_find_entry(dir, dentry, &soffset, &sbh, &eoffset, &ebh);
	if (!fi)
		goto out;

	if (sbh == ebh)
	{
		ino = lelb_to_cpu(fi->icb.extLocation);
	}
	else
	{
		struct FileIdentDesc *nfi;

		nfi = (struct FileIdentDesc *)(ebh->b_data + soffset);

		if (&(nfi->icb.extLocation.logicalBlockNum) < (Uint32 *)ebh->b_data)
			ino.logicalBlockNum = le32_to_cpu(fi->icb.extLocation.logicalBlockNum);
		else
			ino.logicalBlockNum = le32_to_cpu(nfi->icb.extLocation.logicalBlockNum);

		if (&(nfi->icb.extLocation.partitionReferenceNum) < (Uint16 *)ebh->b_data)
			ino.partitionReferenceNum = le16_to_cpu(fi->icb.extLocation.partitionReferenceNum);
		else
			ino.partitionReferenceNum = le16_to_cpu(nfi->icb.extLocation.partitionReferenceNum);
	}

	inode = dentry->d_inode;

	retval = -EIO;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_unlink: pblock = %d, i_ino = %lu\n",
		udf_get_lb_pblock(dir->i_sb, ino, 0),
		inode->i_ino);
#endif

	if (udf_get_lb_pblock(dir->i_sb, ino, 0) != inode->i_ino)
		goto end_unlink;

	if (!inode->i_nlink)
	{
		printk(KERN_DEBUG "udf: udf_unlink: Deleting nonexistent file (%lu), %d\n",
			inode->i_ino, inode->i_nlink);
		inode->i_nlink = 1;
	}
	retval = udf_delete_entry(fi, soffset, sbh, eoffset, ebh);
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
	udf_release_data(sbh);
	if (sbh != ebh)
		udf_release_data(ebh);
out:
	return retval;
}
