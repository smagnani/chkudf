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
 * 03/07/99		 rewrote udf_bmap (again)
 *				 New funcs, block_bmap, udf_next_aext
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/mm.h>

#include "udf_i.h"
#include "udf_sb.h"

static mode_t udf_convert_permissions(struct FileEntry *);
static int udf_update_inode(struct inode *);
static void udf_fill_inode(struct inode *, struct FileEntry *);

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
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_delete_inode: %ld,%ld,%ld\n", inode->i_ino, inode->i_size, inode->i_blocks);
#endif
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
	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, &ident);

	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed !bh\n",
			inode->i_ino);
		make_bad_inode(inode);
		return;
	}

	if (ident != TID_FILE_ENTRY)
	{
		printk(KERN_DEBUG "ident == %d\n", ident);
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
					if (ident == TID_FILE_ENTRY)
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
	udf_fill_inode(inode, fe);
	udf_release_data(bh);
}

static void udf_fill_inode(struct inode *inode, struct FileEntry *fe)
{
    time_t modtime, acctime;
	int offset, alen;

	UDF_I_FILELEN(inode) = le64_to_cpu(fe->informationLength);

	if (UDF_I_FILELEN(inode) > 0x00000000FFFFFFFFULL)
	{
		printk(KERN_DEBUG "udf: inode %lu is larger than 4G (%llx)!\n",
			inode->i_ino, UDF_I_FILELEN(inode) );
		UDF_I_FILELEN(inode) = 0x00000000FFFFFFFFULL;
	}

#ifdef VDEBUG
	printk(KERN_DEBUG
		"udf: block: %u (%u,%u) ino %ld FILE_ENTRY: len %Lu blocks %Lu perm 0x%x link %d type 0x%x flags 0x%x\n",
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
		UDF_I_LOCATION(inode).logicalBlockNum,
		UDF_I_LOCATION(inode).partitionReferenceNum,
		inode->i_ino, 
		UDF_I_FILELEN(inode),
		le64_to_cpu(fe->logicalBlocksRecorded), /* may be zero! */
		le32_to_cpu(fe->permissions),
		le16_to_cpu(fe->fileLinkCount), 
		fe->icbTag.fileType,
		le16_to_cpu(fe->icbTag.flags));
#endif

	inode->i_uid = udf_convert_uid(le32_to_cpu(fe->uid));
	if ( !inode->i_uid ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

	inode->i_gid = udf_convert_gid(le32_to_cpu(fe->gid));
	if ( !inode->i_gid ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

	inode->i_nlink = le16_to_cpu(fe->fileLinkCount);
	if (!inode->i_nlink)
	{
		printk(KERN_DEBUG "udf: ino %ld, fileLinkCount == 0\n", inode->i_ino);
		inode->i_nlink = 1;
	}
		
	inode->i_size = UDF_I_FILELEN(inode) & 0x00000000FFFFFFFFULL;

	if (inode->i_size)
	{
		inode->i_blocks = ( (inode->i_size - 1) >> 9 ) + 1;
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
	
	offset = sizeof(struct FileEntry) + le32_to_cpu(fe->lengthExtendedAttr);
	alen = offset + le32_to_cpu(fe->lengthAllocDescs);

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

#if 0
	printk(KERN_DEBUG "udf: Ext0len %d, Loc (%d,%d)\n",
		UDF_I_EXT0LEN(inode), UDF_I_EXT0LOC(inode).logicalBlockNum,
		UDF_I_EXT0LOC(inode).partitionReferenceNum);
#endif

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
	Uint32 udfperms;
	int i;
	timestamp cpu_time;
	Uint16 ident;

	bh = bread(inode->i_sb->s_dev,
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
		inode->i_sb->s_blocksize);
	if (bh)
		ident = le16_to_cpu(((tag *)(bh->b_data))->tagIdent);
#if 0
	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, &ident);
#endif
	if (!bh)
	{
		printk(KERN_DEBUG "udf: udf_write_inode: !bh\n");
		return -EIO;
	}
	if (ident != TID_FILE_ENTRY)
	{
		udf_release_data(bh);
		printk(KERN_DEBUG "udf: udf_write_inode: ident == %d\n", ident);
		return -EIO;
	}
	fe = (struct FileEntry *)bh->b_data;

	/* Set fe info from inode */
	udfperms =  ((inode->i_mode & S_IRWXO)     ) |
				((inode->i_mode & S_IRWXG) << 2) |
				((inode->i_mode & S_IRWXU) << 4);

	udfperms |= (le32_to_cpu(fe->permissions) &
		(PERM_O_DELETE | PERM_O_CHATTR |
		 PERM_G_DELETE | PERM_G_CHATTR |
		 PERM_U_DELETE | PERM_U_CHATTR));
	fe->permissions = cpu_to_le32(udfperms);

	if (inode->i_uid != UDF_SB(inode->i_sb)->s_uid)
		fe->uid = cpu_to_le32(inode->i_uid);

	if (inode->i_gid != UDF_SB(inode->i_sb)->s_gid)
		fe->gid = cpu_to_le32(inode->i_gid);

	if (udf_time_to_stamp(&cpu_time, inode->i_atime))
		fe->accessTime = cpu_to_lets(cpu_time);
	if (udf_time_to_stamp(&cpu_time, inode->i_mtime))
		fe->modificationTime = cpu_to_lets(cpu_time);

	fe->descTag.descCRC = cpu_to_le16(udf_crc((char *)fe + sizeof(tag),
		 le16_to_cpu(fe->descTag.descCRCLength), 0));

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

#ifdef VDEBUG
	printk(KERN_DEBUG "udf_iget: ino=%d,%d==%d\n", ino.logicalBlockNum, ino.partitionReferenceNum,
		udf_get_lb_pblock(sb, ino, 0));
#endif
	down(&read_semaphore); /* serialize access to UDF_SB_LOCATION() */
	/* This is really icky.. should fix -- blf */

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

int udf_next_aext(struct inode *inode, lb_addr *bloc, int *ext, lb_addr *eloc, Uint32 *elen)
{
	struct buffer_head *bh;
	int offset, pos, alen;
	Uint16 ident;

	if (!(bh = udf_read_ptagged(inode->i_sb, *bloc, 0, &ident)))
		return 0;

	if (ident == TID_FILE_ENTRY)
	{
		struct FileEntry *fe = (struct FileEntry *)bh->b_data;

		pos = sizeof(struct FileEntry) + fe->lengthExtendedAttr;
		alen = fe->lengthAllocDescs + pos;
	}
	else if (ident == TID_ALLOC_EXTENT_DESC)
	{
		struct AllocExtDesc *aed = (struct AllocExtDesc *)bh->b_data;

		pos = sizeof(struct AllocExtDesc);
		alen = aed->lengthAllocDescs + pos;
	}
	else
	{
		printk(KERN_DEBUG "udf: udf_next_aext, ident == %d\n", ident);
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
			printk(KERN_DEBUG "udf: udf_next_aext: alloc_type = %d unsupported\n",
				UDF_I_ALLOCTYPE(inode));
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
			printk(KERN_DEBUG "udf: udf_bmap: inode: %ld, block >= EOF(%d,%ld)\n",
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

int udf_bmap2(struct inode *, int);

int udf_bmap(struct inode *inode, int block)
{
	lb_addr eloc, bloc;
	Uint32 offset, ext, elen;
	int ret;

	if (block_bmap(inode, block, &bloc, &ext, &eloc, &elen, &offset))
		ret = udf_get_lb_pblock(inode->i_sb, eloc, offset);
	else
		ret = 0;

#if 0
	{
		int ret2 = udf_bmap2(inode, block);

		if (ret != ret2)
		{
			printk(KERN_DEBUG "udf: udf_bmap: (block=%d,ino=%ld) block_bmap (%d) != udf_bmap2 (%d) [%d,%d + %d]\n",
				block, inode->i_ino, ret, ret2, eloc.logicalBlockNum, eloc.partitionReferenceNum, offset);
			return ret2;
		}
		else
			return ret;
	}
#else
	return ret;
#endif
}
	

/*
 * given an inode and block ...
 */
int 
udf_bmap2(struct inode *inode, int block)
{
	off_t b_off, size;
	unsigned int currblk = 0;
	unsigned int currpart = 0;
	int offset = 0;
	int adesclen=0;
	Uint16 ident;

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

#if 0
	printk(KERN_DEBUG "udf: udf_bmap(%d) b_off = %ld\n", block, b_off);
#endif

	/*
	 * If we are beyond the end of this file, don't give out any
	 * blocks.
	 */
#if 0
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
#else
	if (b_off / 512 > inode->i_blocks)
	{
		off_t	max_legal_read_offset;

		max_legal_read_offset = (inode->i_blocks + PAGE_SIZE / 512 - 1)
			& ~(PAGE_SIZE / 512 - 1);

		if (b_off / 512 >= max_legal_read_offset)
		{
			printk(KERN_DEBUG "udf: udf_bmap: inode: %ld, block >= EOF(%ld,%ld)\n",
				inode->i_ino, b_off / 512, inode->i_blocks);
		}
		return 0;
	}
#endif

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
				printk(KERN_ERR "udf: udf_bmap: unsupported alloctype: %d\n",
					UDF_I_ALLOCTYPE(inode));
				return 0;
			}
		}

		bh = udf_read_ptagged(inode->i_sb, loc, 0, &ident);

		if ( !bh )
		{
			printk(KERN_ERR 
				"udf: udf_read_ptagged(%p,(%d,%d)) (%ld) block failed !bh\n",
				inode->i_sb, loc.logicalBlockNum,
				loc.partitionReferenceNum, inode->i_ino);
			free_page((unsigned long) tmpad);
			return 0;
		}

		if (ident != TID_FILE_ENTRY)
		{
			udf_release_data(bh);
			printk(KERN_ERR "udf: udf_bmap: ident == %d\n", ident);
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
					{
						printk(KERN_DEBUG "udf: End of extents!\n");
						return 0;
					}
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
					bh = udf_read_ptagged(inode->i_sb, loc, 0, &ident);
					if (!bh)
					{
						printk(KERN_ERR 
							"udf: udf_read_ptagged(%p,(%d,%d)) (%ld) block failed !bh\n",
							inode->i_sb, loc.logicalBlockNum,
							loc.partitionReferenceNum, inode->i_ino);
						free_page((unsigned long) tmpad);
						return 0;
					}
					if (ident != TID_ALLOC_EXTENT_DESC)
					{
						printk(KERN_ERR "udf: udf_bmap: ident == %d\n", ident);
						udf_release_data(bh);
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

	if (currpart > UDF_SB_NUMPARTS(inode->i_sb))
		printk(KERN_DEBUG "udf: udf_bmap (%ld): get_pblock(%p,%d,%d,%ld)\n", inode->i_ino,
			inode->i_sb, currblk, currpart, b_off >> inode->i_sb->s_blocksize_bits);

#if 0
	printk(KERN_DEBUG "udf: udf_bmap2, currblk=%d, currpart=%d, offset=%ld\n",
		currblk, currpart, b_off >> inode->i_sb->s_blocksize_bits);
#endif

	return udf_get_pblock(inode->i_sb, currblk, currpart, b_off >> inode->i_sb->s_blocksize_bits);
}
