/*
 * inode.c
 *
 * PURPOSE
 *  Inode handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *  E-mail regarding any portion of the Linux UDF file system should be
 *  directed to the development team mailing list (run by majordomo):
 *    linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *    ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1998 Dave Boynton
 *  (C) 1998-1999 Ben Fennema
 *  (C) 1999 Stelias Computing Inc
 *
 * HISTORY
 *
 *  10/04/98 dgb  Added rudimentary directory functions
 *  10/07/98      Fully working udf_bmap! It works!
 *  11/25/98      bmap altered to better support extents
 *  12/06/98 blf  partition support in udf_iget, udf_bmap and udf_read_inode
 *  12/12/98      rewrote udf_bmap to handle next extents and descs across
 *                block boundaries (which is not actually allowed)
 *  12/20/98      added support for strategy 4096
 *  03/07/99      rewrote udf_bmap (again)
 *                New funcs, block_bmap, udf_next_aext
 *  04/19/99      Support for writing device EA's for major/minor #
 *
 */

#include "udfdecl.h"
#include <linux/udf_fs.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/mm.h>

#include "udf_i.h"
#include "udf_sb.h"

static mode_t udf_convert_permissions(struct FileEntry *);
static int udf_update_inode(struct inode *);
static void udf_fill_inode(struct inode *, struct buffer_head *);

static struct semaphore read_semaphore = MUTEX;

/*
 * udf_put_inode
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel no longer needs the inode.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 *  Called at each iput()
 */
void udf_put_inode(struct inode * inode)
{
	udf_discard_prealloc(inode);
}

/*
 * udf_delete_inode
 *
 * PURPOSE
 *	Clean-up before the specified inode is destroyed.
 *
 * DESCRIPTION
 *	This routine is called when the kernel destroys an inode structure
 *	ie. when iput() finds i_count == 0.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 *  Called at the last iput() if i_nlink is zero.
 */
void udf_delete_inode(struct inode * inode)
{
	udf_debug("ino=%ld, size=%ld, blocks=%ld\n", inode->i_ino, inode->i_size, inode->i_blocks);
	inode->i_size = 0;
	if (inode->i_blocks)
		udf_truncate(inode);
	udf_free_inode(inode);
}

void udf_discard_prealloc(struct inode * inode)
{
#ifdef UDF_PREALLOCATE
	unsigned short total;

	if (UDF_I_PREALLOC_COUNT(inode))
	{
		total = UDF_I_PREALLOC_COUNT(inode);
		UDF_I_PREALLOC_COUNT(inode) = 0;
		udf_free_blocks(inode, UDF_I_PREALLOC_BLOCK(inode), total);
	}
#endif
}

#if 0

static lb_addr udf_alloc_block(struct inode *inode, lb_addr goal, int *err)
{
	lb_addr result;
#ifdef UDF_PREALLOCATE
	struct buffer_head *bh;
#endif

	wait_on_super(inode->i_sb);

#ifdef UDF_PREALLOCATE
	if (UDF_I_PREALLOCCOUNT(inode) &&
		(goal.logicalBlockNum == UDF_I_PREALLOCBLOCK(inode) ||
		 goal.logicalBlockNum + 1 == UDF_I_PREALLOCBLOCK(inode))
	{
		result.logicalBlockNum = UDF_I_PREALLOCBLOCK(inode)++;
			
		UDF_I_PREALLOCCOUNT(inode)--;
		if (!(bh = getblk(inode->i_sb->s_dev, udf_get_lb_pblock(sb, result, 0),
			inode->i_sb->s_blocksize)))
		{
			udf_error(inode->i_sb, "udf_alloc_block",
				"cannot get block %lu", result);
			return 0;
		}
		memset(bh->b_data, 0, inode->i_sb->s_blocksize);
		mark_buffer_uptodate(bh, 1);
		mark_buffer_dirty(bh, 1);
		brelse(bh);
	}
	else
	{
		udf_discard_prealloc(inode);
		if (S_ISREG(inode->i_mode))
			result = udf_new_block(inode, goal,
				&(UDF_I_PREALLOCCOUNT(inode)),
				&(UDF_I_PREALLOCBLOCK(inode)), err);
		else
			result = udf_new_block(inode, goal, 0, 0, err);
	}		
#else
	result = udf_new_block(inode, goal, 0, 0, err);
#endif

	return result;
}

struct buffer_head * udf_getblk(struct inode * inode, long block, int create, int * err)
{
	struct buffer_head * bh;
	
	*err = -EIO;
	if (block < 0)
	{
		udf_warning(inode->i_sb, "udf_getblk", "block < 0");
		return NULL;
	}
	/* block > big check??? */

	/* inode_getblk, block_getblk */
}

struct buffer_head * udf_bread(struct inode * inode, int block, int create, int * err)
{
	struct buffer_head * bh;
	int prev_blocks;

	prev_blocks = inode->i_blocks;

	bh = udf_getblk(inode, block, create, err);
	if (!bh)
		return bh;

	if (create &&
		S_ISDIR(inode->i_mode) &&
		inode->i_blocks > prev_blocks)
	{
		int i;
		struct buffer_head *tmp_bh;

		for (i=1;
			i < UDF_SB_PREALLOC_DIR_BLOCKS(inode->i_sb);
			i++)
		{
			tmp_bh = udf_getblk(inode, block+i, create, err);
			if (!tmp_bh)
			{
				brelse(bh);
				return 0;
			}
			brelse(tmp_bh);
		}
	}

	if (buffer_uptodate(bh))
		return bh;
	ll_rw_block(READ, 1, &bh);
	wait_on_buffer(bh);
	if (buffer_uptodate(bh))
		return bh;
	brelse(bh);
	*err = -EIO;
	return NULL;
}

#endif

/*
 * udf_read_inode
 *
 * PURPOSE
 *	Read an inode.
 *
 * DESCRIPTION
 *	This routine is called by iget() [which is called by udf_iget()]
 *      (clean_inode() will have been called first)
 *	when an inode is first read into memory.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 12/19/98 dgb  Updated to fix size problems.
 */
void
udf_read_inode(struct inode *inode)
{
	struct buffer_head *bh;
	struct FileEntry *fe;
	Uint16 ident;

	/*
	 * Set defaults, but the inode is still incomplete!
	 * Note: get_new_inode() sets the following on a new inode:
	 *      i_sb = sb
	 *      i_dev = sb->s_dev;
	 *      i_no = ino
	 *      i_flags = sb->s_flags
	 *      i_state = 0
	 * clean_inode(): zero fills and sets
	 *      i_count = 1
	 *      i_nlink = 1
	 *      i_op = NULL;
	 */

	inode->i_blksize = inode->i_sb->s_blocksize;
	inode->i_version = 1;

	UDF_I_EXT0LEN(inode)=0;
	UDF_I_EXT0LOC(inode).logicalBlockNum = 0xFFFFFFFF;
	UDF_I_EXT0LOC(inode).partitionReferenceNum = 0xFFFF;
	UDF_I_EXT0OFFS(inode)=0;
	UDF_I_ALLOCTYPE(inode)=0;

	memcpy(&UDF_I_LOCATION(inode), &UDF_SB_LOCATION(inode->i_sb), sizeof(lb_addr));

#ifdef VDEBUG
	udf_debug("ino=%ld, block=%d, partition=%d\n", inode->i_ino,
		UDF_I_LOCATION(inode).logicalBlockNum,
		UDF_I_LOCATION(inode).partitionReferenceNum);
#endif

	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, &ident);

	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed !bh\n",
			inode->i_ino);
		make_bad_inode(inode);
		return;
	}

	if (ident != TID_FILE_ENTRY && ident != TID_EXTENDED_FILE_ENTRY)
	{
		udf_debug("ident == %d\n", ident);
		udf_release_data(bh);
		make_bad_inode(inode);
		return;
	}

	fe = (struct FileEntry *)bh->b_data;

	if (fe->icbTag.strategyType == 4096)
	{
		struct buffer_head *ibh;
		struct buffer_head *nbh;
		struct IndirectEntry *ie;

		ibh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 1, &ident);
		if (ident == TID_INDIRECT_ENTRY)
		{
			if (ibh)
			{
				lb_addr loc;
				ie = (struct IndirectEntry *)ibh->b_data;
	
				loc = lelb_to_cpu(ie->indirectICB.extLocation);
	
				if (ie->indirectICB.extLength && 
					(nbh = udf_read_ptagged(inode->i_sb, loc, 0, &ident)))
				{
					if (ident == TID_FILE_ENTRY ||
						ident == TID_EXTENDED_FILE_ENTRY)
					{
						memcpy(&UDF_SB_LOCATION(inode->i_sb), &loc, sizeof(lb_addr));
						udf_release_data(bh);
						udf_release_data(ibh);
						udf_release_data(nbh);
						udf_read_inode(inode);
						return;
					}
					else
					{
						udf_release_data(nbh);
						udf_release_data(ibh);
					}
				}
				else
					udf_release_data(ibh);
			}
		}
		else
			udf_release_data(ibh);
	}
	else if (fe->icbTag.strategyType != 4)
	{
		printk(KERN_ERR "udf: unsupported strategy type: %d\n",
			fe->icbTag.strategyType);
		udf_release_data(bh);
		make_bad_inode(inode);
		return;
	}
	udf_fill_inode(inode, bh);
	udf_release_data(bh);
}

static void udf_fill_inode(struct inode *inode, struct buffer_head *bh)
{
	struct FileEntry *fe;
	struct ExtendedFileEntry *efe;
    time_t convtime;
	int offset, alen;

	fe = (struct FileEntry *)bh->b_data;
	efe = (struct ExtendedFileEntry *)bh->b_data;

	if (fe->descTag.tagIdent == TID_EXTENDED_FILE_ENTRY)
		UDF_I_EXTENDED_FE(inode) = 1;
	else /* fe->descTag.tagIdent == TID_FILE_ENTRY */
		UDF_I_EXTENDED_FE(inode) = 0;

	if (fe->icbTag.strategyType == 4)
		UDF_I_STRAT4096(inode) = 0;
	else /* fe->icbTag.strategyType == 4096) */
		UDF_I_STRAT4096(inode) = 1;

	inode->i_uid = udf_convert_uid(le32_to_cpu(fe->uid));
	if ( !inode->i_uid ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

	inode->i_gid = udf_convert_gid(le32_to_cpu(fe->gid));
	if ( !inode->i_gid ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

	inode->i_nlink = le16_to_cpu(fe->fileLinkCount);
	if (!inode->i_nlink)
	{
		udf_debug("ino=%ld, fileLinkCount == 0\n", inode->i_ino);
		inode->i_nlink = 1;
	}
	
	inode->i_size = le64_to_cpu(fe->informationLength);
#if BITS_PER_LONG < 64
	if (le64_to_cpu(fe->informationLength) & 0xFFFFFFFF00000000)
		inode->i_size = (Uint32)-1;
#endif

	inode->i_mode = udf_convert_permissions(fe);
	inode->i_mode &= ~UDF_SB(inode->i_sb)->s_umask;

	UDF_I_ALLOCTYPE(inode) = le16_to_cpu(fe->icbTag.flags) & ICB_FLAG_ALLOC_MASK;

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		inode->i_blocks = le64_to_cpu(fe->logicalBlocksRecorded) <<
			(inode->i_sb->s_blocksize_bits - 9);

		if ( udf_stamp_to_time(&convtime, lets_to_cpu(fe->modificationTime)) )
		{
			inode->i_mtime = convtime;
			inode->i_ctime = convtime;
		}
		else
		{
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
		}

		if ( udf_stamp_to_time(&convtime, lets_to_cpu(fe->accessTime)) ) 
			inode->i_atime = convtime;
		else
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);

		UDF_I_UNIQUE(inode) = le64_to_cpu(fe->uniqueID);
		UDF_I_LENEATTR(inode) = le32_to_cpu(fe->lengthExtendedAttr);
		UDF_I_LENALLOC(inode) = le32_to_cpu(fe->lengthAllocDescs);
		offset = sizeof(struct FileEntry) + UDF_I_LENEATTR(inode);
		alen = offset + UDF_I_LENALLOC(inode);
	}
	else
	{
		inode->i_blocks = le64_to_cpu(efe->logicalBlocksRecorded) << 
			(inode->i_sb->s_blocksize_bits - 9);

		if ( udf_stamp_to_time(&convtime, lets_to_cpu(efe->modificationTime)) )
			inode->i_mtime = convtime;
		else
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);

		if ( udf_stamp_to_time(&convtime, lets_to_cpu(efe->accessTime)) )
			inode->i_atime = convtime;
		else
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);

		if ( udf_stamp_to_time(&convtime, lets_to_cpu(efe->createTime)) )
			inode->i_ctime = convtime;
		else
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);

		UDF_I_UNIQUE(inode) = le64_to_cpu(efe->uniqueID);
		UDF_I_LENEATTR(inode) = le32_to_cpu(efe->lengthExtendedAttr);
		UDF_I_LENALLOC(inode) = le32_to_cpu(efe->lengthAllocDescs);
		offset = sizeof(struct ExtendedFileEntry) + UDF_I_LENEATTR(inode);
		alen = offset + UDF_I_LENALLOC(inode);
	}

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICB_FLAG_AD_SHORT:
 		{
 			short_ad * sa;

			sa = udf_get_fileshortad(fe, alen, &offset);
			if ( (sa) && (sa->extLength) )
			{
				UDF_I_EXT0LEN(inode) = le32_to_cpu(sa->extLength) & UDF_EXTENT_LENGTH_MASK;
 				UDF_I_EXT0LOC(inode).logicalBlockNum = le32_to_cpu(sa->extPosition);
				UDF_I_EXT0LOC(inode).partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
			}
			break;
 		}
		case ICB_FLAG_AD_LONG:
		{
		    long_ad * la;

		    la = udf_get_filelongad(fe, alen, &offset);
		    if ( (la) && (la->extLength) )
			{
				UDF_I_EXT0LEN(inode) = le32_to_cpu(la->extLength) & UDF_EXTENT_LENGTH_MASK;
				UDF_I_EXT0LOC(inode).logicalBlockNum = le32_to_cpu(la->extLocation.logicalBlockNum);
				UDF_I_EXT0LOC(inode).partitionReferenceNum = le16_to_cpu(la->extLocation.partitionReferenceNum);
		    }
			break;
		}
		case ICB_FLAG_AD_EXTENDED:
		{
			extent_ad * ext;

		    ext = udf_get_fileextent(fe, alen, &offset);
		    if ( (ext) && (ext->extLength) )
			{
				UDF_I_EXT0LEN(inode) = le32_to_cpu(ext->extLength) & UDF_EXTENT_LENGTH_MASK;
#if 0
				UDF_I_EXT0LOC(inode) = ext->extLocation;
#endif
			}
			break;
		}
		case ICB_FLAG_AD_IN_ICB: /* short directories */
		{
			UDF_I_EXT0LEN(inode) = le32_to_cpu(fe->lengthAllocDescs);
			UDF_I_EXT0LOC(inode) = UDF_I_LOCATION(inode);
			UDF_I_EXT0OFFS(inode) = sizeof(struct FileEntry) +
				le32_to_cpu(fe->lengthExtendedAttr);
			break;
		}
	} /* end switch ad_type */

	switch (fe->icbTag.fileType)
	{
		case FILE_TYPE_DIRECTORY:
		{
			inode->i_op = &udf_dir_inode_operations;
			inode->i_mode |= S_IFDIR;
			inode->i_nlink ++;
			break;
		}
		case FILE_TYPE_REGULAR:
		case FILE_TYPE_NONE:
		{
			inode->i_op = &udf_file_inode_operations;
			inode->i_mode |= S_IFREG;
			break;
		}
		case FILE_TYPE_BLOCK:
		{
			inode->i_op = &blkdev_inode_operations;
			inode->i_mode |= S_IFBLK;
			break;
		}
		case FILE_TYPE_CHAR:
		{
			inode->i_op = &chrdev_inode_operations;
			inode->i_mode |= S_IFCHR;
			break;
		}
		case FILE_TYPE_FIFO:
		{
			init_fifo(inode);
		}
		case FILE_TYPE_SYMLINK:
		{
			/* untested! */
			inode->i_op = &udf_symlink_inode_operations;
			inode->i_mode = S_IFLNK|S_IRWXUGO;
			break;
		}
	}
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
	{
		struct buffer_head *tbh;
		struct DeviceSpecificationExtendedAttr *dsea =
			(struct DeviceSpecificationExtendedAttr *)
				udf_get_extendedattr(inode, 12, 1, &tbh);

		if (dsea)
		{
			inode->i_rdev = to_kdev_t(
				(le32_to_cpu(dsea->majorDeviceIdent)) << 8) |
				(le32_to_cpu(dsea->minorDeviceIdent) & 0xFF);
			/* Developer ID ??? */
			udf_release_data(tbh);
		}
		else
		{
			make_bad_inode(inode);
		}
	}
}

static mode_t
udf_convert_permissions(struct FileEntry *fe)
{
	mode_t mode;
	Uint32 permissions;
	Uint32 flags;

	permissions = le32_to_cpu(fe->permissions);
	flags = le16_to_cpu(fe->icbTag.flags);

	mode =	(( permissions      ) & S_IRWXO) |
			(( permissions >> 2 ) & S_IRWXG) |
			(( permissions >> 4 ) & S_IRWXU) |
			(( flags & ICB_FLAG_SETUID) ? S_ISUID : 0) |
			(( flags & ICB_FLAG_SETGID) ? S_ISGID : 0) |
			(( flags & ICB_FLAG_STICKY) ? S_ISVTX : 0);

	return mode;
}

/*
 * udf_write_inode
 *
 * PURPOSE
 *	Write out the specified inode.
 *
 * DESCRIPTION
 *	This routine is called whenever an inode is synced.
 *	Currently this routine is just a placeholder.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

void
udf_write_inode(struct inode *inode)
{
	udf_update_inode(inode);
}

static int
udf_update_inode(struct inode *inode)
{
	struct buffer_head *bh;
	struct FileEntry *fe;
	struct ExtendedFileEntry *efe;
	Uint32 udfperms;
	Uint16 icbflags;
	Uint16 crclen;
	int i;
	timestamp cpu_time;

#ifdef VDEBUG
	udf_debug("ino=%ld\n", inode->i_ino);
#endif

	bh = udf_bread(inode->i_sb,
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
		inode->i_sb->s_blocksize);
	if (!bh)
	{
		udf_debug("bread failure\n");
		return -EIO;
	}
	fe = (struct FileEntry *)bh->b_data;
	efe = (struct ExtendedFileEntry *)bh->b_data;

	if (inode->i_uid != UDF_SB(inode->i_sb)->s_uid)
		fe->uid = cpu_to_le32(inode->i_uid);

	if (inode->i_gid != UDF_SB(inode->i_sb)->s_gid)
		fe->gid = cpu_to_le32(inode->i_gid);

	udfperms =  ((inode->i_mode & S_IRWXO)     ) |
				((inode->i_mode & S_IRWXG) << 2) |
				((inode->i_mode & S_IRWXU) << 4);

	udfperms |= (le32_to_cpu(fe->permissions) &
		(PERM_O_DELETE | PERM_O_CHATTR |
		 PERM_G_DELETE | PERM_G_CHATTR |
		 PERM_U_DELETE | PERM_U_CHATTR));
	fe->permissions = cpu_to_le32(udfperms);

	udf_debug("udf_update_inode: ino=%ld, mode=%d, nlink=%d\n", inode->i_ino, inode->i_mode, inode->i_nlink);

	if (S_ISDIR(inode->i_mode))
		fe->fileLinkCount = cpu_to_le16(inode->i_nlink - 1);
	else
		fe->fileLinkCount = cpu_to_le16(inode->i_nlink);


	fe->informationLength = cpu_to_le64(inode->i_size);

	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
	{
		EntityID *eid;
		struct buffer_head *tbh;
		struct DeviceSpecificationExtendedAttr *dsea =
			(struct DeviceSpecificationExtendedAttr *)
				udf_get_extendedattr(inode, 12, 1, &tbh);	

		if (!dsea)
		{
			dsea = (struct DeviceSpecificationExtendedAttr *)
				udf_add_extendedattr(inode,
					sizeof(struct DeviceSpecificationExtendedAttr) +
					sizeof(EntityID), 12, 0x3, &tbh);
			dsea->attrType = 12;
			dsea->attrSubtype = 1;
			dsea->attrLength = sizeof(struct DeviceSpecificationExtendedAttr) +
				sizeof(EntityID);
			dsea->impUseLength = sizeof(EntityID);
		}
		eid = (EntityID *)dsea->impUse;
		memset(eid, 0, sizeof(EntityID));
		strcpy(eid->ident, UDF_ID_DEVELOPER);
		eid->identSuffix[0] = UDF_OS_CLASS_UNIX;
		eid->identSuffix[1] = UDF_OS_ID_LINUX;
		udf_debug("nr=%d\n", kdev_t_to_nr(inode->i_rdev));
		dsea->majorDeviceIdent = kdev_t_to_nr(inode->i_rdev) >> 8;
		dsea->minorDeviceIdent = kdev_t_to_nr(inode->i_rdev) & 0xFF;
		mark_buffer_dirty(tbh, 1);
		udf_release_data(tbh);
	}

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		fe->logicalBlocksRecorded = cpu_to_le64(
			(inode->i_blocks + (1 << (inode->i_sb->s_blocksize_bits - 9)) - 1) >>
			(inode->i_sb->s_blocksize_bits - 9));

		if (udf_time_to_stamp(&cpu_time, inode->i_atime))
			fe->accessTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_mtime))
			fe->modificationTime = cpu_to_lets(cpu_time);
		memset(&(fe->impIdent), 0, sizeof(EntityID));
		strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
		fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		fe->uniqueID = cpu_to_le64(UDF_I_UNIQUE(inode));
		fe->lengthExtendedAttr = cpu_to_le32(UDF_I_LENEATTR(inode));
		fe->lengthAllocDescs = cpu_to_le32(UDF_I_LENALLOC(inode));
		fe->descTag.tagIdent = le16_to_cpu(TID_FILE_ENTRY);
		crclen = sizeof(struct FileEntry);
	}
	else
	{
		efe->logicalBlocksRecorded = cpu_to_le64(
			(inode->i_blocks + (2 << (inode->i_sb->s_blocksize_bits - 9)) - 1) >>
			(inode->i_sb->s_blocksize_bits - 9));

		if (udf_time_to_stamp(&cpu_time, inode->i_atime))
			efe->accessTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_mtime))
			efe->modificationTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_ctime))
			efe->createTime = cpu_to_lets(cpu_time);
		memset(&(efe->impIdent), 0, sizeof(EntityID));
		strcpy(efe->impIdent.ident, UDF_ID_DEVELOPER);
		efe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		efe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		efe->uniqueID = cpu_to_le64(UDF_I_UNIQUE(inode));
        efe->lengthExtendedAttr = cpu_to_le32(UDF_I_LENEATTR(inode));
        efe->lengthAllocDescs = cpu_to_le32(UDF_I_LENALLOC(inode));
		efe->descTag.tagIdent = le16_to_cpu(TID_EXTENDED_FILE_ENTRY);
		crclen = sizeof(struct ExtendedFileEntry);
	}
	fe->icbTag.strategyType = UDF_I_STRAT4096(inode) ? cpu_to_le16(4096) :
		cpu_to_le16(4);

	if (S_ISDIR(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_DIRECTORY;
	else if (S_ISREG(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_REGULAR;
	else if (S_ISLNK(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_SYMLINK;
	else if (S_ISBLK(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_BLOCK;
	else if (S_ISCHR(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_CHAR;
	else if (S_ISFIFO(inode->i_mode))
		fe->icbTag.fileType = FILE_TYPE_FIFO;

	udf_debug("filetype=%d\n", fe->icbTag.fileType);

	icbflags = UDF_I_ALLOCTYPE(inode) |
			((inode->i_mode & S_ISUID) ? ICB_FLAG_SETUID : 0) |
			((inode->i_mode & S_ISGID) ? ICB_FLAG_SETGID : 0) |
			((inode->i_mode & S_ISVTX) ? ICB_FLAG_STICKY : 0) |
			(le16_to_cpu(fe->icbTag.flags) &
				(ICB_FLAG_ALLOC_MASK | ICB_FLAG_SETUID |
				 ICB_FLAG_SETGID | ICB_FLAG_STICKY));
	
	fe->icbTag.flags = cpu_to_le16(icbflags);
	fe->descTag.descVersion = cpu_to_le16(2);
	fe->descTag.tagSerialNum = cpu_to_le16(UDF_SB_SERIALNUM(inode->i_sb));
	fe->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(inode).logicalBlockNum);
	crclen += UDF_I_LENEATTR(inode) + UDF_I_LENALLOC(inode) - sizeof(tag);
	fe->descTag.descCRCLength = cpu_to_le16(crclen);
	fe->descTag.descCRC = cpu_to_le16(udf_crc((char *)fe + sizeof(tag), crclen, 0));

	fe->descTag.tagChecksum = 0;
	for (i=0; i<16; i++)
		if (i != 4)
			fe->descTag.tagChecksum += ((Uint8 *)&(fe->descTag))[i];

	/* write the data blocks */
	mark_buffer_dirty(bh, 1);
	udf_release_data(bh);
	return 0;
}

/*
 * udf_iget
 *
 * PURPOSE
 *	Get an inode.
 *
 * DESCRIPTION
 *	This routine replaces iget() and read_inode().
 *
 * HISTORY
 *	October 3, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 12/19/98 dgb  Added semaphore and changed to be a wrapper of iget
 */
struct inode *
udf_iget(struct super_block *sb, lb_addr ino)
{
	struct inode *inode;
	unsigned long block;

	block = udf_get_lb_pblock(sb, ino, 0);

#ifdef VDEBUG
	udf_debug("ino=%ld, block=%d, partition=%d\n", block,
		ino.logicalBlockNum, ino.partitionReferenceNum);
#endif

	down(&read_semaphore); /* serialize access to UDF_SB_LOCATION() */
	/* This is really icky.. should fix -- blf */

	/* put the location where udf_read_inode can find it */
	memcpy(&UDF_SB_LOCATION(sb), &ino, sizeof(lb_addr));

	/* Get the inode */

	inode = iget(sb, block);
		/* calls udf_read_inode() ! */

	up(&read_semaphore);

	if (!inode)
	{
		printk(KERN_ERR "udf: iget() failed\n");
		return NULL;
	}

	if ( ino.logicalBlockNum >= UDF_SB_PARTLEN(sb, ino.partitionReferenceNum) )
	{
		udf_debug("block=%d, partition=%d out of range\n",
			ino.logicalBlockNum, ino.partitionReferenceNum);
		return NULL;
 	}

	return inode;
}

int udf_next_aext(struct inode *inode, lb_addr *bloc, int *ext,
	lb_addr *eloc, Uint32 *elen)
{
	struct buffer_head *bh;
	int offset, pos, alen;
	Uint16 ident;

	if (!(bh = udf_read_ptagged(inode->i_sb, *bloc, 0, &ident)))
		return 0;

	if (ident == TID_FILE_ENTRY)
	{
		pos = sizeof(struct FileEntry) + UDF_I_LENEATTR(inode);
		alen = UDF_I_LENALLOC(inode) + pos;
	}
	else if (ident == TID_EXTENDED_FILE_ENTRY)
	{
		pos = sizeof(struct ExtendedFileEntry) + UDF_I_LENEATTR(inode);
		alen = UDF_I_LENALLOC(inode) + pos;
	}
	else if (ident == TID_ALLOC_EXTENT_DESC)
	{
		struct AllocExtDesc *aed = (struct AllocExtDesc *)bh->b_data;

		pos = sizeof(struct AllocExtDesc);
		alen = aed->lengthAllocDescs + pos;
	}
	else
	{
		udf_debug("ident == %d\n", ident);
		udf_release_data(bh);
		return 0;
	}

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICB_FLAG_AD_SHORT:
		{
			short_ad *sad;

			offset = pos + (*ext * sizeof(short_ad));
			if (!(sad = udf_get_fileshortad(bh->b_data, alen, &offset)))
			{
				udf_release_data(bh);
				return 0;
			}

			eloc->logicalBlockNum = le32_to_cpu(sad->extPosition);
			eloc->partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
			*elen = le32_to_cpu(sad->extLength) & UDF_EXTENT_LENGTH_MASK;

			sad = udf_get_fileshortad(bh->b_data, alen, &offset);
			if (sad && le32_to_cpu(sad->extLength) >> 30 == EXTENT_NEXT_EXTENT_ALLOCDECS)
			{
				bloc->logicalBlockNum = le32_to_cpu(sad->extPosition);
				*ext = 0;
			}
			else
				(*ext)++;
			break;
		}
		case ICB_FLAG_AD_LONG:
		{
			long_ad *lad;

			offset = pos + (*ext * sizeof(long_ad));
			if (!(lad = udf_get_filelongad(bh->b_data, alen, &offset)))
			{
				udf_release_data(bh);
				return 0;
			}

			*eloc = lelb_to_cpu(lad->extLocation);
			*elen = le32_to_cpu(lad->extLength) & UDF_EXTENT_LENGTH_MASK;

			lad = udf_get_filelongad(bh->b_data, alen, &offset);
			if (lad && le32_to_cpu(lad->extLength) >> 30 == EXTENT_NEXT_EXTENT_ALLOCDECS)
			{
				*bloc = lelb_to_cpu(lad->extLocation);
				*ext = 0;
			}
			else
				(*ext)++;
			break;
		}
		default:
		{
			udf_debug("alloc_type = %d unsupported\n", UDF_I_ALLOCTYPE(inode));
			udf_release_data(bh);
			return 0;
		}
	}
	udf_release_data(bh);
	if (!(*elen))
		return 0;
	else
		return 1;
}

int block_bmap(struct inode *inode, int block, lb_addr *bloc, Uint32 *ext,
	lb_addr *eloc, Uint32 *elen, Uint32 *offset)
{
	int lbcount = 0;
	int b_off;

	*ext = 1;

	if (block < 0)
	{
		printk(KERN_ERR "udf: block_bmap: block < 0\n");
		return 0;
	}
	if (!inode)
	{
		printk(KERN_ERR "udf: block_bmap: NULL inode\n");
		return 0;
	}
	if (block >> (inode->i_sb->s_blocksize_bits - 9) > inode->i_blocks)
	{
		off_t   max_legal_read_offset;

		max_legal_read_offset = (inode->i_blocks + PAGE_SIZE / 512 - 1)
			& ~(PAGE_SIZE / 512 - 1);

        if (block >> (inode->i_sb->s_blocksize_bits - 9) >= max_legal_read_offset)
		{
			udf_debug("inode=%ld, block >= EOF(%d,%ld)\n",
				inode->i_ino, block >> (inode->i_sb->s_blocksize_bits - 9), inode->i_blocks);
		}
		return 0;
	}

	b_off = block << inode->i_sb->s_blocksize_bits;
	*bloc = UDF_I_LOCATION(inode);
	*eloc = UDF_I_EXT0LOC(inode);
	*elen = UDF_I_EXT0LEN(inode);

	while (lbcount + *elen <= b_off)
	{
		lbcount += *elen;
		if (!(udf_next_aext(inode, bloc, ext, eloc, elen)))
			return 0;
	}

	*offset = (b_off - lbcount) >> inode->i_sb->s_blocksize_bits;

	return 1;
}

int udf_bmap(struct inode *inode, int block)
{
	lb_addr eloc, bloc;
	Uint32 offset, ext, elen;
	int ret;

#ifdef VDEBUG
	udf_debug("ino=%ld, block=%d\n", inode->i_ino, block);
#endif

	if (block_bmap(inode, block, &bloc, &ext, &eloc, &elen, &offset))
		ret = udf_get_lb_pblock(inode->i_sb, eloc, offset);
	else
		ret = 0;

    if (UDF_SB(inode->i_sb)->s_flags & UDF_FLAG_VARCONV)
		return udf_fixed_to_variable(ret);
	else
		return ret;
}
