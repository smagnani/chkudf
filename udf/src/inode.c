/*
 * inode.c
 *
 * PURPOSE
 *	Inode handling routines for the OSTA-UDF(tm) filesystem.
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
 *
 * HISTORY
 *
 * 10/4/98  dgb  Added rudimentary directory functions
 * 10/7/98       Fully working udf_bmap! It works!
 * 11/25/98      bmap altered to better support extents
 * 12/06/98 blf  partition support in udf_iget, udf_bmap and udf_read_inode
 * 12/12/98      rewrote udf_bmap to handle next extents and descs across
 *               block boundaries (which is not actually allowed)
 * 12/20/98      added support for strategy 4096
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/mm.h>

#include "udf_i.h"
#include "udf_sb.h"

static mode_t udf_convert_permissions(struct FileEntry *fe);
static struct semaphore read_semaphore = MUTEX;

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
	time_t modtime, acctime;
	int offset;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf_read_inode: inode=0x%lx\n", (unsigned long)inode);
#endif

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

	UDF_I_FILELEN(inode) = 0x00000000FFFFFFFFULL;
	UDF_I_EXT0LEN(inode)=0;
	UDF_I_EXT0LOC(inode).logicalBlockNum = 0xFFFFFFFF;
	UDF_I_EXT0LOC(inode).partitionReferenceNum = 0xFFFF;
	UDF_I_EXT0OFFS(inode)=0;
	UDF_I_ALLOCTYPE(inode)=0;

	memcpy(&UDF_I_LOCATION(inode), &UDF_SB_LOCATION(inode->i_sb), sizeof(lb_addr));

#ifdef VDEBUG
        printk(KERN_DEBUG "udf: udf_read_inode(%d,%d)\n",
                UDF_I_LOCATION(inode).logicalBlockNum),
                UDF_I_LOCATION(inode).partitionReferenceNum);
#endif
	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, TID_FILE_ENTRY);

	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed !bh\n",
			inode->i_ino);
		make_bad_inode(inode);
		return;
	}

	fe = (struct FileEntry *)bh->b_data;

	if (fe->icbTag.strategyType == 4096)
	{
		struct buffer_head *ibh;
		struct buffer_head *nbh;
		struct IndirectEntry *ie;

		ibh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 1,
			TID_INDIRECT_ENTRY);
		if (ibh)
		{
			lb_addr loc;
			ie = (struct IndirectEntry *)ibh->b_data;

			loc = lelb_to_cpu(ie->indirectICB.extLocation);

			if (ie->indirectICB.extLength && 
				(nbh = udf_read_ptagged(inode->i_sb, loc, 0, TID_FILE_ENTRY)))
			{
				memcpy(&UDF_SB_LOCATION(inode->i_sb), &loc, sizeof(lb_addr));
				udf_release_data(bh);
				udf_release_data(ibh);
				udf_release_data(nbh);
				udf_read_inode(inode);
				return;
			}
			else
				udf_release_data(ibh);
		}
	}
	else if (fe->icbTag.strategyType != 4)
	{
		printk(KERN_ERR "udf: unsupported strategy type: %d\n",
			fe->icbTag.strategyType);
		udf_release_data(bh);
		make_bad_inode(inode);
		return;
	}

	UDF_I_FILELEN(inode) = le64_to_cpu(fe->informationLength);

	if (UDF_I_FILELEN(inode) > 0x00000000FFFFFFFFULL)
	{
		printk(KERN_DEBUG "udf: inode %lu is larger than 4G (%llx)!\n",
			inode->i_ino, UDF_I_FILELEN(inode) );
		UDF_I_FILELEN(inode) = 0x00000000FFFFFFFFULL;
	}

#ifdef VDEBUG
	printk(KERN_DEBUG
		"udf: block: %u (%u,%u) ino %ld FILE_ENTRY: len %lu blocks %lu perm 0x%x link %d type 0x%x flags 0x%x\n",
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
		UDF_I_LOCATION(inode).logicalBlockNum,
		UDF_I_LOCATION(inode).partitionReferenceNum,
		inode->i_ino, 
		UDF_I_FILELEN(inode),
		le64_to_cpu(fe->logicalBlocksRecorded), /* may be zero! */
		le32_to_cpu(fe->permissions),
		le16_to_cpu(fe->fileLinkCount), 
		fe->icbTag.fileType, le16_to_cpu(fe->icbTag.flags));
#endif

	inode->i_uid = udf_convert_uid(le32_to_cpu(fe->uid));
	if ( !inode->i_uid ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

	inode->i_gid = udf_convert_gid(le32_to_cpu(fe->gid));
	if ( !inode->i_gid ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

	inode->i_nlink = le16_to_cpu(fe->fileLinkCount);
	inode->i_size = UDF_I_FILELEN(inode) & 0x00000000FFFFFFFFULL;

	if (inode->i_size)
	{
		inode->i_blocks = ( (inode->i_size - 1) /  512 ) + 1;
		/* blocksize for i_blocks is always 512 */
	}
	else
		inode->i_blocks = 0;

	inode->i_mode = udf_convert_permissions(fe);
	inode->i_mode &= ~UDF_SB(inode->i_sb)->s_umask;

	if ( udf_stamp_to_time(&modtime, lets_to_cpu(fe->modificationTime)) )
	{
		inode->i_mtime = modtime;
		inode->i_ctime = modtime;
	}
	else
	{
		inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
		inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
	}

	if ( udf_stamp_to_time(&acctime, lets_to_cpu(fe->accessTime)) ) 
		inode->i_atime = acctime;
	else
		inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);

	UDF_I_ALLOCTYPE(inode) = le16_to_cpu(fe->icbTag.flags) & ICB_FLAG_ALLOC_MASK;

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICB_FLAG_AD_SHORT:
 		{
 			short_ad * sa;

			offset = 0;
			sa = udf_get_fileshortad(fe, inode->i_sb->s_blocksize, &offset);
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

		    offset=0;
		    la = udf_get_filelongad(fe, inode->i_sb->s_blocksize, &offset);
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

		    offset=0;
		    ext = udf_get_fileextent(fe, inode->i_sb->s_blocksize, &offset);
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
#ifdef VDEBUG
		printk(KERN_DEBUG "udf: ino %lu directory %u len %u loc %u offs %u\n",
			inode->i_ino,
			UDF_I_ALLOCTYPE(inode), UDF_I_EXT0LEN(inode), 
			UDF_I_EXT0LOC(inode), UDF_I_EXT0OFFS(inode));
#endif
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
		case FILE_TYPE_SYMLINK:
		{
			/* untested! */
#ifdef VDEBUG
			printk(KERN_DEBUG "udf: ino %lu symlink %u\n",
				inode->i_ino,
				UDF_I_ALLOCTYPE(inode));
#endif
			inode->i_op = &udf_file_inode_operations;
			inode->i_mode = S_IFLNK|S_IRWXUGO;
			break;
		}
	}
#if 0
	printk(KERN_DEBUG "udf: first extent (%d,%d) len %d offset %d\n",
		UDF_I_EXT0LOC(inode).logicalBlockNum,
		UDF_I_EXT0LOC(inode).partitionReferenceNum,
		UDF_I_EXT0LEN(inode),
		UDF_I_EXT0OFFS(inode));
#endif
	udf_release_data(bh);
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
	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, TID_FILE_ENTRY);
	if (!bh)
	{
		printk(KERN_DEBUG "udf: udf_write_inode: !bh\n");
		return -EIO;
	}
	fe = (struct FileEntry *)bh->b_data;

	/* Set fe info from inode */

	/* write the data blocks */

	mark_buffer_dirty(bh, 1);
	udf_release_data(bh);
	return 0;
}

/*
 * udf_put_inode
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel no longer needs the inode.
 *	Currently this routine is just a placeholder.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
udf_put_inode(struct inode *inode)
{
	COOKIE(("udf_put_inode: inode=0x%lx\n", (unsigned long)inode));

	/* Delete unused inodes */
	if (inode && inode->i_count == 1)
		inode->i_nlink = 0;
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
 */
void
udf_delete_inode(struct inode *inode)
{
	COOKIE(("udf_delete_inode: inode=0x%lx\n", (unsigned long)inode));
	inode->i_size = 0;
	clear_inode(inode);
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

#ifdef VDEBUG
	printk(KERN_DEBUG "udf_iget: ino=%d,%d==%d\n", ino.logicalBlockNum, ino.partitionReferenceNum,
		udf_get_lb_pblock(sb, ino, 0));
#endif
	down(&read_semaphore); /* serialize access to UDF_SB_LOCATION() */

	/* put the location where udf_read_inode can find it */
	memcpy(&UDF_SB_LOCATION(sb), &ino, sizeof(lb_addr));

	/* Get the inode */

	inode = iget(sb, udf_get_lb_pblock(sb, ino, 0));
		/* calls udf_read_inode() ! */

	up(&read_semaphore);

	if (!inode)
	{
		printk(KERN_ERR "udf: iget() failed\n");
		return NULL;
	}

	if ( ino.logicalBlockNum >= UDF_SB_PARTLEN(sb, ino.partitionReferenceNum) )
	{
		printk(KERN_DEBUG "udf: iget(%d,%d) out of range\n",
			ino.partitionReferenceNum, ino.logicalBlockNum);
		return NULL;
 	}

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_iget(%d,%d)\n",
		UDF_I_LOCATION(inode).logicalBlockNum,
		UDF_I_LOCATION(inode).partitionReferenceNum);
#endif

	return inode;
}

/*
 * given an inode and block ...
 */
int 
udf_bmap(struct inode *inode, int block)
{
	off_t b_off, size;
	unsigned int currblk = 0;
	unsigned int currpart = 0;
	int offset = 0;
	int adesclen=0;

	if (block < 0)
	{
		printk(KERN_ERR "udf: udf_bmap: block<0\n");
		return 0;
	}

	if (!inode)
	{
		printk(KERN_ERR "udf: udf_bmap: NULL inode\n");
		return 0;
	}

	b_off = block << inode->i_sb->s_blocksize_bits;

	/*
	 * If we are beyond the end of this file, don't give out any
	 * blocks.
	 */
	if( b_off > inode->i_size )
	{
	    off_t	max_legal_read_offset;

	    /*
	     * If we are *way* beyond the end of the file, print a message.
	     * Access beyond the end of the file up to the next page boundary
	     * is normal, however because of the way the page cache works.
	     * In this case, we just return 0 so that we can properly fill
	     * the page with useless information without generating any
	     * I/O errors.
	     */
	    max_legal_read_offset = (inode->i_size + PAGE_SIZE - 1)
	      & ~(PAGE_SIZE - 1);
		if (b_off >= max_legal_read_offset)
		{
			printk(KERN_ERR "udf: udf_bmap: block>= EOF(%d, %ld)\n", block,
			inode->i_size);
	    }
	    return 0;
	}

	/* check for first extent case, so we don't need to reread FileEntry */
	size = UDF_I_EXT0LEN(inode);

	if (b_off >= size)
	{
		/* Bad news:
		 * we'll have to do this the long way 
		 */
		struct buffer_head *bh;
		struct FileEntry *fe;
		Uint32 adsize, pos=0;
		Uint8 *ad;
		Uint8 *tmpad = (Uint8 *) __get_free_page(GFP_KERNEL);
		lb_addr loc;
		int error=0;
		memcpy(&loc, &UDF_I_LOCATION(inode), sizeof(lb_addr));


		switch (UDF_I_ALLOCTYPE(inode))
		{
			case ICB_FLAG_AD_SHORT: adsize = sizeof(short_ad); break;
			case ICB_FLAG_AD_LONG: adsize = sizeof(long_ad); break;
			default:
			{
				printk(KERN_ERR "udf: udf_bmap: unsporrted alloctype: %d\n",
					UDF_I_ALLOCTYPE(inode));
				return 0;
			}
		}


		bh = udf_read_ptagged(inode->i_sb, loc, 0, TID_FILE_ENTRY);

		if ( !bh )
		{
			printk(KERN_ERR 
				"udf: udf_read_ptagged(%p,(%d,%d)) (%ld) block failed !bh\n",
				inode->i_sb, loc.logicalBlockNum,
				loc.partitionReferenceNum, inode->i_ino);
			free_page((unsigned long) tmpad);
			return 0;
		}

		fe = (struct FileEntry *)bh->b_data;
		offset = sizeof(struct FileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + adsize;
		adesclen = le32_to_cpu(fe->lengthAllocDescs);

		do
		{
			b_off -= size;

			ad = udf_filead_read(inode, tmpad, adsize, loc, &pos, &offset, &bh, &error);

			if (!ad)
			{
				free_page((unsigned long) tmpad);
				return error;
			}

			size = le32_to_cpup(ad) & 0x3FFFFFFF;

			switch (le32_to_cpup(ad) >> 30)
			{
				case EXTENT_RECORDED_ALLOCATED:
				{
					if (size == 0)
						printk(KERN_DEBUG "udf: End of extents!\n");
					break;
				}
				case EXTENT_RECORDED_NOT_ALLOCATED:
				case EXTENT_NOT_RECORDED_NOT_ALLOCATED:
				{
					printk(KERN_DEBUG "udf: Unhandled ELEN type (%d) @ (%d,%d) [%d] block %d (ignoring)\n",
						le32_to_cpup(ad) >> 30, loc.logicalBlockNum,
						loc.partitionReferenceNum, udf_get_lb_pblock(inode->i_sb, loc, pos),
						block);
					break;
				}
				case EXTENT_NEXT_EXTENT_ALLOCDECS:
				{
					struct AllocExtDesc *aed;

					size = pos = 0;
					switch (UDF_I_ALLOCTYPE(inode))
					{
						case ICB_FLAG_AD_SHORT:
						{
							short_ad *sa = (short_ad *)ad;
							loc.logicalBlockNum = le32_to_cpu(sa->extPosition);
							break;
						}
						case ICB_FLAG_AD_LONG:
						{
							long_ad *la = (long_ad *)ad;
							loc.logicalBlockNum = le32_to_cpu(la->extLocation.logicalBlockNum);
							loc.partitionReferenceNum = le16_to_cpu(la->extLocation.partitionReferenceNum);
							break;
						}
					}

					udf_release_data(bh);
					bh = udf_read_ptagged(inode->i_sb, loc, 0, TID_ALLOC_EXTENT_DESC);
					if (!bh)
					{
						printk(KERN_ERR 
							"udf: udf_read_ptagged(%p,(%d,%d)) (%ld) block failed !bh\n",
							inode->i_sb, loc.logicalBlockNum,
							loc.partitionReferenceNum, inode->i_ino);
						free_page((unsigned long) tmpad);
						return 0;
					}
					aed = (struct AllocExtDesc *)bh->b_data;
					offset = sizeof(struct AllocExtDesc);
					adesclen = le32_to_cpu(aed->lengthAllocDescs) + adsize;
					break;
				}
			}
			adesclen -= adsize;
		} while (b_off >= size && adesclen > 0);

		if (b_off >= size)
		{
			udf_release_data(bh);
			free_page((unsigned long) tmpad);
			return 0;
		}

		switch (UDF_I_ALLOCTYPE(inode))
		{
			case ICB_FLAG_AD_SHORT:
			{
				short_ad *sa = (short_ad *)ad;
				currblk = le32_to_cpu(sa->extPosition);
				currpart = loc.partitionReferenceNum;
				break;
			}
			case ICB_FLAG_AD_LONG:
			{
				long_ad *la = (long_ad *)ad;
				currblk = le32_to_cpu(la->extLocation.logicalBlockNum);
				currpart = le16_to_cpu(la->extLocation.partitionReferenceNum);
				break;
			}
		}

		udf_release_data(bh);
		free_page((unsigned long) tmpad);
	}
	else
	{
		currblk = UDF_I_EXT0LOC(inode).logicalBlockNum;
		currpart = UDF_I_EXT0LOC(inode).partitionReferenceNum;
	}

	return udf_get_pblock(inode->i_sb, currblk, currpart, b_off >> inode->i_sb->s_blocksize_bits);
}
