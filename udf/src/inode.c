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
 * 10/4/98 dgb	Added rudimentary directory functions
 * 10/7/98	Fully working udf_bmap! It works!
 * 11/25/98	bmap altered to better support extents
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>

#include "udf_i.h"
#include "udf_sb.h"

/*
 * udf_read_inode
 *
 * PURPOSE
 *	Read an inode.
 *
 * DESCRIPTION
 *	This routine is called by iget() [replaced by udf_iget()]
 *	when an inode is first read into memory.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
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

	UDF_I_FILELENHIGH(inode)=0;
	UDF_I_FILELENLOW(inode)=0;
	UDF_I_EXT0LEN(inode)=0;
	UDF_I_EXT0LOC(inode).logicalBlockNum = 0xFFFFFFFF;
	UDF_I_EXT0LOC(inode).partitionReferenceNum = 0xFFFF;
	UDF_I_EXT0OFFS(inode)=0;
	UDF_I_ALLOCTYPE(inode)=0;
	memcpy(&UDF_I_LOCATION(inode), &UDF_SB_LOCATION(inode->i_sb), sizeof(lb_addr));

#ifdef VDEBUG
        printk(KERN_DEBUG "udf: udf_read_inode(%d,%d)\n",
                UDF_I_LOCATION(inode).logicalBlockNum,
                UDF_I_LOCATION(inode).partitionReferenceNum);
#endif
	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode));

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: block: %ld partroot: %d part: %d\n",
		inode->i_ino, UDF_SB_PARTROOT(inode->i_sb, UDF_I_PART(inode)), UDF_I_PART(inode));
#endif

	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed !bh\n",
			inode->i_ino);
		return;
	}

	fe = (struct FileEntry *)bh->b_data;
	if (fe->descTag.tagIdent == TID_FILE_ENTRY)
	{
		UDF_I_FILELENHIGH(inode)=udf64_high32(fe->informationLength);
		UDF_I_FILELENLOW(inode)=udf64_low32(fe->informationLength);

#ifdef VDEBUG
		printk(KERN_DEBUG
	"udf: block: %u (%u,%u) ino %ld FILE_ENTRY: len %u/%u blocks %u perm 0x%x link %d type 0x%x flags 0x%x\n",
			udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
			UDF_I_LOCATION(inode).logicalBlockNum,
			UDF_I_LOCATION(inode).partitionReferenceNum,
			inode->i_ino, 
			UDF_I_FILELENHIGH(inode),
			UDF_I_FILELENLOW(inode),
			udf64_low32(fe->logicalBlocksRecorded), /* may be zero! */
			fe->permissions, 
			fe->fileLinkCount, 
			fe->icbTag.fileType, fe->icbTag.flags);
#endif

		inode->i_uid = udf_convert_uid(fe->uid);
		if ( !inode->i_uid ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

		inode->i_gid = udf_convert_gid(fe->gid);
		if ( !inode->i_gid ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

		inode->i_nlink = fe->fileLinkCount;
		inode->i_size = UDF_I_FILELENLOW(inode);

		if ( udf_stamp_to_time(&modtime, &fe->modificationTime) )
		{
			inode->i_mtime = modtime;
			inode->i_ctime = modtime;
		} else {
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
		}

		if ( udf_stamp_to_time(&acctime, &fe->accessTime) ) 
			inode->i_atime = acctime;
		else
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);

		UDF_I_ALLOCTYPE(inode) = fe->icbTag.flags & ICB_FLAG_ALLOC_MASK;

		switch (UDF_I_ALLOCTYPE(inode))
		{
			case ICB_FLAG_AD_SHORT:
 			{
 				short_ad * sa;

				offset = 0;
 			    sa = udf_get_fileshortad(fe, inode->i_sb->s_blocksize, &offset);
 			    if ( (sa) && (sa->extLength & 0x3FFFFFFF) )
				{
 					UDF_I_EXT0LEN(inode) = sa->extLength & 0x3FFFFFFF;
 					UDF_I_EXT0LOC(inode).logicalBlockNum = sa->extPosition;
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
					UDF_I_EXT0LEN(inode) = la->extLength;
					UDF_I_EXT0LOC(inode) = la->extLocation;
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
					UDF_I_EXT0LEN(inode) = ext->extLength;
#if 0
					UDF_I_EXT0LOC(inode) = ext->extLocation;
#endif
				}
				break;
			}
			case ICB_FLAG_AD_IN_ICB: /* short directories */
			{
				UDF_I_EXT0LEN(inode) = fe->lengthAllocDescs;
				UDF_I_EXT0LOC(inode) = UDF_I_LOCATION(inode);
				UDF_I_EXT0OFFS(inode) = (int)fe->extendedAttr - (int)fe +
					fe->lengthExtendedAttr;
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
				inode->i_mode = S_IFDIR|(fe->permissions & 00007)|((fe->permissions >> 2) & 00070)|((fe->permissions >> 4) & 00700);
				break;
			}
			case FILE_TYPE_REGULAR:
			case FILE_TYPE_NONE:
			{
				inode->i_op = &udf_file_inode_operations;
				inode->i_mode = S_IFREG|(fe->permissions & 00007)|((fe->permissions >> 2) & 00070)|((fe->permissions >> 4) & 00700);
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
				inode->i_mode = S_IFLNK|(fe->permissions & 00007)|((fe->permissions >> 2) & 00070)|((fe->permissions >> 4) & 00700);
				break;
			}
		}
	} else {
		printk(KERN_ERR "udf: ino %lu is tag 0x%x, not FILE_ENTRY\n",
			inode->i_ino, ((tag *)bh->b_data)->tagIdent );
	}
	udf_release_data(bh);
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
	COOKIE(("udf_write_inode: inode=0x%lx\n", (unsigned long)inode));
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
 */
struct inode *
udf_iget(struct super_block *sb, lb_addr ino)
{
	struct inode *inode;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf_iget: ino=%d,%d==%d\n", ino.logicalBlockNum, ino.partitionReferenceNum,
		udf_get_lb_pblock(sb, ino, 0));
#endif
	/* Get the inode */
	memcpy(&UDF_SB_LOCATION(sb), &ino, sizeof(lb_addr));

	inode = iget(sb, udf_get_lb_pblock(sb, ino, 0));
	if (!inode) {
		printk(KERN_ERR "udf: iget() failed\n");
		return NULL;
	}

	if ( ino.logicalBlockNum >= UDF_SB_PARTLEN(sb, ino.partitionReferenceNum) )
	{
		printk(KERN_DEBUG "udf: iget(%d,%d) out of range\n",
			ino.partitionReferenceNum, ino.logicalBlockNum);
		return NULL;
 	}

	/* Cached inode - nothing to do */
	if (inode->i_op && inode->i_nlink)
		return inode;

#if 0
	/* Sanity check */
	if (inode->i_op) {
		printk(KERN_WARNING "udf: i_op != NULL\n");
		inode->i_op = NULL;
	}
#endif
#ifdef VDEBUG	
	if (inode->i_nlink)
		printk(KERN_DEBUG "udf: i_nlink != 0, %d\n",
			inode->i_nlink);
#endif

	/*
	 * Set defaults, but the inode is still incomplete!
	 * Note: iget() sets the following on a new inode:
	 *	i_sb = sb
	 *	i_dev = sb->s_dev;
	 *	i_no = ino
	 *	i_flags = sb->s_flags
	 *	i_count = 1
	 *	i_state = 0
	 * and udf_read_inode() sets these:
	 *	(!)i_op = NULL
	 *	i_nlink = 0
	 */
	inode->i_blksize = sb->s_blocksize;
	inode->i_version = 1;
#if 0
	inode->i_mode = UDF_SB(sb)->s_mode;
	inode->i_gid = UDF_SB(sb)->s_gid;
	inode->i_uid = UDF_SB(sb)->s_uid;

	/* Mark the inode as empty */
	inode->i_op = NULL; 
	inode->i_nlink = 0; 

	memcpy(&UDF_SB_LOCATION(inode), &ino, sizeof(lb_addr));
#endif
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_iget(%d,%d)\n",
		UDF_I_LOCATION(inode).logicalBlockNum,
		UDF_I_LOCATION(inode).partitionReferenceNum);
#endif

#if 0
	udf_read_inode(inode);
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

	if (block < 0)
	{
		printk(KERN_ERR "udf: udf_bmap: block<0\n");
		return 0;
	}

	if (!inode) {
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

		bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode));

		if ( !bh )
		{
			printk(KERN_ERR 
				"udf: udf_read_ptagged(%p,(%d,%d)) (%ld) block failed !bh\n",
				inode->i_sb, UDF_I_LOCATION(inode).logicalBlockNum,
				UDF_I_LOCATION(inode).partitionReferenceNum, inode->i_ino);
			return 0;
		}

		fe = (struct FileEntry *)bh->b_data;

		switch (UDF_I_ALLOCTYPE(inode))
		{
			case ICB_FLAG_AD_SHORT:
			{
				short_ad *sa;
				offset = sizeof(short_ad);
				do
				{
					b_off -= size;
					sa = udf_get_fileshortad(fe, inode->i_sb->s_blocksize, &offset);
					size = sa->extLength & 0x3FFFFFFF;
				} while ( (sa) && (sa->extLength & 0x3FFFFFFF) &&
					(offset < UDF_I_EXT0LEN(inode)) &&
					(b_off >= size) );
				currblk = sa->extPosition & 0x3FFFFFFF;
				currpart = UDF_I_LOCATION(inode).partitionReferenceNum;
				break;
			}
			case ICB_FLAG_AD_LONG:
			{
				long_ad *la;
				offset = sizeof(long_ad);
				do
				{
					b_off -= size;
					la = udf_get_filelongad(fe, inode->i_sb->s_blocksize, &offset);
					size = la->extLength;
				}
				while ( (la) && (la->extLength) &&
					 (offset < UDF_I_EXT0LEN(inode)) &&
					 (b_off >= size) );

				currblk = la->extLocation.logicalBlockNum;
				currpart = la->extLocation.partitionReferenceNum;
				break;
			}
		} /* end switch */
		udf_release_data(bh);
	}
	else
	{
		currblk = UDF_I_EXT0LOC(inode).logicalBlockNum;
		currpart = UDF_I_EXT0LOC(inode).partitionReferenceNum;
	}

	return udf_get_pblock(inode->i_sb, currblk, currpart, b_off >> inode->i_sb->s_blocksize_bits);
}
