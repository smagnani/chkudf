/*
 * dir.c
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
 * 10/5/98 dgb	Split directory operations into it's own file
 *		Implemented directory reads via do_udf_readdir
 * 10/6/98	Made directory operations work!
 * 11/17/98	Rewrote directory to support ICB_FLAG_AD_LONG
 */


#if defined(__linux__) && defined(__KERNEL__)
#include <linux/version.h>
#include <linux/udf_fs.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#endif

#include "udfdecl.h"

struct DirectoryCursor
{
	struct buffer_head *  bh;
	struct buffer_head *  bh_alloc;
	struct inode * inode;
	Uint32  currentBlockNum;   /* of directory data */
	Uint32  dirOffset; 	   /* offset into directory extents */
	Uint32  allocOffset; 	   /* offset into directory alloc desc */
	Uint32  extentLength;
	Uint32  workBufferLength;
	Uint8   workBuffer[0];
};

/* directory enumeration and lookups use this */
static struct DirectoryCursor * udf_fileident_opendir(struct inode *);
static struct FileIdentDesc * udf_fileident_readdir(struct DirectoryCursor *);
static void udf_fileident_closedir(struct DirectoryCursor *);
static int udf_fileident_nextlength(struct DirectoryCursor * );

/* Prototypes for file operations */
static int udf_readdir(struct file *, void *, filldir_t);

/* generic directory enumeration */
typedef int (*udf_enum_callback)(struct inode *, struct FileIdentDesc *, 
	   			void *, void *, void *);
static int udf_enum_directory(struct inode *, udf_enum_callback, 
#ifdef BF_CHANGES
				struct file *, filldir_t, void *,
				struct FileIdentDesc *);
#else
	   			void *, void *, void *);
#endif

/* readdir and lookup functions */
static int udf_lookup_callback(struct inode *, struct FileIdentDesc*, 
	   			void *, void *, void *);

static int udf_readdir_callback(struct inode *, struct FileIdentDesc*, 
#ifdef BF_CHANGES
				struct file *, filldir_t, void *);
#else
	   			void *, void *, void *);
#endif

struct file_operations udf_dir_fops = {
	NULL,			/* llseek */
	NULL,			/* read */
	NULL,
	udf_readdir,	/* readdir */
	NULL,			/* poll */
	NULL,			/* ioctl */
	NULL,			/* mmap */
	NULL,			/* open */
	NULL,			/* flush */
	NULL,			/* release */
	NULL,			/* fsync */
	NULL,			/* fasync */
	NULL,			/* check_media_change */
	NULL,			/* revalidate */
	NULL			/* lock */
};

/*
 * udf_readdir
 *
 * PURPOSE
 *	Read a directory entry.
 *
 * DESCRIPTION
 *	Optional - sys_getdents() will return -ENOTDIR if this routine is not
 *	available.
 *
 *	Refer to sys_getdents() in fs/readdir.c
 *	sys_getdents() -> .
 *
 * PRE-CONDITIONS
 *	filp			Pointer to directory file.
 *	buf			Pointer to directory entry buffer.
 *	filldir			Pointer to filldir function.
 *
 * POST-CONDITIONS
 *	<return>		>=0 on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
#ifdef BF_CHANGES
int udf_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct FileIdentDesc *tmpfi;
	struct inode *dir = filp->f_dentry->d_inode;
	int result;

	if (!dir)
	   return -EBADF;

 	if (!S_ISDIR(dir->i_mode))
	   return -ENOTDIR;

	if ( filp->f_pos == 0 ) 
	{
		if (filldir(dirent, ".", 1, filp->f_pos, dir->i_ino) < 0)
			return 0;
	}
 
	tmpfi = (struct FileIdentDesc *) __get_free_page(GFP_KERNEL);
 
	if (!tmpfi)
		return -ENOMEM;
 
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_readdir(%p, %p) bs-1 %ld bits-2 %d f_pos (%d/%ld=%d), i_ino=%ld\n",
		filp, dirent,
		(dir->i_sb->s_blocksize - 1), (dir->i_sb->s_blocksize_bits - 2),
		((int)filp->f_pos >> (dir->i_sb->s_blocksize_bits - 2)),
		(((int)filp->f_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2),
		(int)filp->f_pos, dir->i_ino);
#endif
  
	result = udf_enum_directory(dir, udf_readdir_callback, 
					filp, filldir, dirent, tmpfi);
  
	free_page((unsigned long) tmpfi);
 	return result;
}

static int 
udf_readdir_callback(struct inode *dir, struct FileIdentDesc*fi, 
 			struct file  *filp, filldir_t filldir, void *dirent)
{
 	struct ustr filename;
 	struct ustr unifilename;
 	long ino;
 	
 	if ( (!fi) || (!filp) || (!filldir) || (!dir) )
 	{
 		return 0;
 	}
 
 	ino = fi->icb.extLocation.logicalBlockNum;
 
 	if (fi->lengthFileIdent == 0) /* parent directory */
 	{
 #ifdef VDEBUG
 		printk(KERN_DEBUG "udf: readdir callback '%s' (%d) ino %ld == f_pos %d\n",
 			"..", 2, filp->f_dentry->d_parent->d_inode->i_ino, (int)filp->f_pos);
 #endif
 		if (filldir(dirent, "..", 2, filp->f_pos, filp->f_dentry->d_parent->d_inode->i_ino) < 0)
 			return -1;
 		return 0;
 	}	
 
 	if ( udf_build_ustr_exact(&unifilename, fi->fileIdent,
 		fi->lengthFileIdent) )
 	{
 		return 0;
 	}
 
 	if ( udf_CS0toUTF8(&filename, &unifilename) )
 	{
 		return 0;
 	}
 
 #ifdef VDEBUG
 	printk(KERN_DEBUG "udf: readdir callback '%s' (%d) ino %ld == f_pos %d\n",
 		filename.u_name, filename.u_len, ino, (int)filp->f_pos);
 #endif
 
 	if (filldir(dirent, filename.u_name, filename.u_len, filp->f_pos, ino) < 0)
 	{
 		return 1; /* halt enum */
 	}
 	return 0;
}


static int 
udf_enum_directory(struct inode * dir, udf_enum_callback callback, 
			struct file *filp, filldir_t filldir, void *dirent,
			struct FileIdentDesc *tmpfi)
{
	struct buffer_head *bh;
	struct FileIdentDesc *fi=NULL;
	int loffset;
	int block;
	int offset;
	int curtail=0;
	int remainder=0;
	int nf_pos = filp->f_pos;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: first check: nf_pos %d size %d\n", nf_pos, size);
#endif

	if (nf_pos >= size)
		return 1;

	if (nf_pos == 0)
		nf_pos = (UDF_I_EXT0OFFS(dir) >> 2);

	offset = (nf_pos & ((dir->i_sb->s_blocksize - 1) >> 2)) << 2;
	block = udf_bmap(dir, nf_pos >> (dir->i_sb->s_blocksize_bits - 2));

	if (!block)
		return 0;
	if (!(bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
		return 0;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: loop started: os %d block %d fp %d size %d\n",
		 offset, block, nf_pos, size);
#endif

	while ( (nf_pos < size) && (!curtail) )
	{
		filp->f_pos = nf_pos;
		loffset = offset;

		fi = udf_get_fileident(bh->b_data, dir->i_sb->s_blocksize,
			&offset, &remainder);

#ifdef VDEBUG
		printk(KERN_DEBUG "udf: fi %p block %d los %d os %d rem %d fp %d nfp %d\n",
			fi, block, loffset, offset, remainder, nf_pos, (int)nf_pos + ((offset - loffset) >> 2));
#endif

		if (!fi)
		{
			udf_release_data(bh);
			return 1;
		}

		nf_pos += ((offset - loffset) >> 2);

		if (offset == dir->i_sb->s_blocksize)
		{
			udf_release_data(bh);
			block = udf_bmap(dir, nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
			if (!block)
				return 0;
			if (!(bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
				return 0;
		}
		else if (offset > dir->i_sb->s_blocksize)
		{
			int fi_len;

			fi = tmpfi;

			remainder = dir->i_sb->s_blocksize - loffset;
			memcpy((char *)fi, bh->b_data + loffset, remainder);

			udf_release_data(bh);
			block = udf_bmap(dir, nf_pos >> (dir->i_sb->s_blocksize_bits - 2));
			if (!block)
				return 0;
			if (!(bh = bread(dir->i_dev, block, dir->i_sb->s_blocksize)))
				return 0;

			if (sizeof(struct FileIdentDesc) > remainder)
			{
				memcpy((char *)fi + remainder, bh->b_data, sizeof(struct FileIdentDesc) - remainder);

				if (fi->descTag.tagIdent != TID_FILE_IDENT_DESC)
				{
					printk(KERN_DEBUG "udf: (udf_enum_directory) - 0x%x != TID_FILE_IDENT_DESC\n",
						fi->descTag.tagIdent);
					udf_release_data(bh);
					return 1;
				}
				fi_len = sizeof(struct FileIdentDesc) + fi->lengthFileIdent + fi->lengthOfImpUse;
				fi_len += (4 - (fi_len % 4)) % 4;
				nf_pos += ((fi_len - (offset - loffset)) >> 2);
			}
			else
			{
				fi_len = sizeof(struct FileIdentDesc) + fi->lengthFileIdent + fi->lengthOfImpUse;
				fi_len += (4 - (fi_len % 4)) % 4;
			}

			memcpy((char *)fi + remainder, bh->b_data, fi_len - remainder);
			offset = fi_len - remainder;
			remainder = dir->i_sb->s_blocksize - offset;
		}
		/* pre-process ident */

		if ( (fi->fileCharacteristics & FILE_DELETED) != 0 )
		{
			if ( !udf_undelete ) 
				continue;
		}
		
		if ( (fi->fileCharacteristics & FILE_HIDDEN) != 0 )
		{
			if ( !udf_unhide) 
				continue;
		}

		/* callback */
		curtail = callback(dir, fi, filp, filldir, dirent);

	} /* end while */

	if (!curtail)
		filp->f_pos = nf_pos;

	if (bh)
		udf_release_data(bh);

	if ( filp->f_pos >= size)
		return 1;
	else
		return 0;
}

#else

int udf_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
#if 	LINUX_VERSION_CODE > 0x020140
	struct inode *dir = filp->f_dentry->d_inode;
	long ino, parent_ino;
	int result;

	if (!dir)
	   return -EBADF;

 	if (!S_ISDIR(dir->i_mode))
	   return -ENOTDIR;

	if ( (UDF_I_DIRPOS(dir) == 0) &&
	   (filp->f_pos > 0) ) {
	   return 0;
	}
	parent_ino=filp->f_dentry->d_parent->d_inode->i_ino;
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_readdir(%p, %p,) DIRPOS %d f_pos %d, i_ino=%ld, parent=%lu\n",
	   filp, dirent, UDF_I_DIRPOS(dir), (int)filp->f_pos, dir->i_ino, parent_ino);
#endif

	/* procfs used as an example here */
	ino = dir->i_ino;

	if ( filp->f_pos == 0 ) {
	   if (filldir(dirent, ".", 1, filp->f_pos, ino) <0) 
	   	return 0;
	   filp->f_pos++;
	}
	if ( filp->f_pos == 1 ) {
	   if (filldir(dirent, "..", 2, filp->f_pos, parent_ino) <0) 
	   	return 0;
	   filp->f_pos++;
	   UDF_I_DIRPOS(dir)=1;
	}

	result= udf_enum_directory(dir, udf_readdir_callback, 
	   			filp, filldir, dirent);
	if ( result )
	   UDF_I_DIRPOS(dir)=0;
	return result;
#else
	return -1;
#endif
}

static int 
udf_readdir_callback(struct inode *dir, struct FileIdentDesc*fi, 
	   	void *p1, void *p2, void *p3)
{
	struct ustr filename;
	struct ustr unifilename;
	long ino;
	struct file *filp;
	filldir_t filldir;
	
	filp=(struct file*)p1;
	filldir=(filldir_t)p2;

	if ( (!fi) || (!filp) || (!dir) ) {
	   printk(KERN_DEBUG "udf: callback failed, line %u\n", __LINE__);
	   return 0;
	}

	if ( !filldir ) {
	   printk(KERN_DEBUG "udf: callback failed, line %u\n", __LINE__);
	   return 0;
	}
	ino=fi->icb.extLocation.logicalBlockNum;

	if ( udf_build_ustr_exact(&unifilename, fi->fileIdent,
	   fi->lengthFileIdent) ) {
	   	return 0;
	}

	if ( udf_CS0toUTF8(&filename, &unifilename) ) {
	   	return 0;
	}

	UDF_I_DIRPOS(dir)++;
	if ( UDF_I_DIRPOS(dir) == (filp->f_pos) ) {
#ifdef VDEBUG
	   printk(KERN_DEBUG "udf: readdir %lu callback '%s' dir %d == f_pos\n",
	   	ino, filename.u_name, UDF_I_DIRPOS(dir));
#endif
	   if (filldir(p3, filename.u_name, 	
	   		filename.u_len, 
	   		filp->f_pos, ino) <0)
	   	return 0; /* halt enum */
	   filp->f_pos++;
#ifdef VDEBUG
	} else {
	   printk(KERN_DEBUG "udf: readdir callback '%s' dir %d != f_pos %d\n",
	   	filename.u_name, UDF_I_DIRPOS(dir), (int)filp->f_pos);
#endif
	}
	return 0;
}

static int 
udf_enum_directory(struct inode * dir, udf_enum_callback callback, 
	   	void *p1, void *p2, void *p3)
{
	struct FileIdentDesc *fi=NULL;
	struct DirectoryCursor *cursor=NULL;
	int curtail=0;

	cursor=udf_fileident_opendir(dir);
	if (!cursor) {
	   printk(KERN_ERR "udf: enum_directory() failed open, block %lu\n", dir->i_ino);
	   return 1;
	}

	while ( (fi=udf_fileident_readdir(cursor)) != NULL ) {
	   
	   /* pre-process ident */
#ifdef VDEBUG
	   printk(KERN_DEBUG "udf: enum_dir %lu ino %lu fi %p char %x lenident %d\n",
		(long)cursor->inode->i_ino,
		(long)fi->icb.extLocation.logicalBlockNum,
		(void *)fi, fi->fileCharacteristics, fi->lengthFileIdent);
#endif

	   if ( !fi->lengthFileIdent )
	   	continue;

	   if ( (fi->fileCharacteristics & FILE_DELETED) != 0 ) {
	   	if ( !udf_undelete ) 
	   		continue;
	   }
	   
	   if ( (fi->fileCharacteristics & FILE_HIDDEN) != 0 ) {
	   	if ( !udf_unhide) 
	   		continue;
	   }

	   /* callback */
	   curtail=callback(dir, fi, p1, p2, p3);
	} /* end while */

	udf_fileident_closedir(cursor);
	return curtail ? 0 : 1;
}

#if 0
int udf_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct FileIdentDesc *tmpfi;
	struct inode *dir = filp->f_dentry->d_inode;
	long ino, parent_ino;
	int result;

	if (!dir)
	   return -EBADF;

 	if (!S_ISDIR(dir->i_mode))
	   return -ENOTDIR;

	if ( (UDF_I_DIRPOS(dir) == 0) &&
	   (filp->f_pos > 0) ) {
	   return 0;
	}
	parent_ino=filp->f_dentry->d_parent->d_inode->i_ino;
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_readdir(%p, %p,) DIRPOS %d f_pos %d, i_ino=%ld, parent=%lu\n",
	   filp, dirent, UDF_I_DIRPOS(dir), (int)filp->f_pos, dir->i_ino, parent_ino);
#endif

	/* procfs used as an example here */
	ino = dir->i_ino;

	if ( filp->f_pos == 0 ) {
	   if (filldir(dirent, ".", 1, filp->f_pos, ino) <0) 
	   	return 0;
	   filp->f_pos++;
	}
	if ( filp->f_pos == 1 ) {
	   if (filldir(dirent, "..", 2, filp->f_pos, parent_ino) <0) 
	   	return 0;
	   filp->f_pos++;
	   UDF_I_DIRPOS(dir)=1;
	}

	result= udf_enum_directory(dir, udf_readdir_callback, 
	   			filp, filldir, dirent);
	if ( result )
	   UDF_I_DIRPOS(dir)=0;
	return result;
}

static int 
udf_readdir_callback(struct inode *dir, struct FileIdentDesc*fi, 
	   	void *p1, void *p2, void *p3)
{
	struct ustr filename;
	struct ustr unifilename;
	long ino;
	struct file *filp;
	filldir_t filldir;
	
	filp=(struct file*)p1;
	filldir=(filldir_t)p2;

	if ( (!fi) || (!filp) || (!dir) ) {
	   printk(KERN_DEBUG "udf: callback failed, line %u\n", __LINE__);
	   return 0;
	}

	if ( !filldir ) {
	   printk(KERN_DEBUG "udf: callback failed, line %u\n", __LINE__);
	   return 0;
	}
	ino=fi->icb.extLocation.logicalBlockNum;

	if ( udf_build_ustr_exact(&unifilename, fi->fileIdent,
	   fi->lengthFileIdent) ) {
	   	return 0;
	}

	if ( udf_CS0toUTF8(&filename, &unifilename) ) {
	   	return 0;
	}

	UDF_I_DIRPOS(dir)++;
	if ( UDF_I_DIRPOS(dir) == (filp->f_pos) ) {
#ifdef VDEBUG
	   printk(KERN_DEBUG "udf: readdir %lu callback '%s' dir %d == f_pos\n",
	   	ino, filename.u_name, UDF_I_DIRPOS(dir));
#endif
	   if (filldir(p3, filename.u_name, 	
	   		filename.u_len, 
	   		filp->f_pos, ino) <0)
	   	return 0; /* halt enum */
	   filp->f_pos++;
#ifdef VDEBUG
	} else {
	   printk(KERN_DEBUG "udf: readdir callback '%s' dir %d != f_pos %d\n",
	   	filename.u_name, UDF_I_DIRPOS(dir), (int)filp->f_pos);
#endif
	}
	return 0;
}

static int 
udf_enum_directory(struct inode * dir, udf_enum_callback callback, 
	   	void *p1, void *p2, void *p3)
{
	struct FileIdentDesc *fi=NULL;
	struct DirectoryCursor *cursor=NULL;
	int curtail=0;

	cursor=udf_fileident_opendir(dir);
	if (!cursor) {
	   printk(KERN_ERR "udf: enum_directory() failed open, block %lu\n", dir->i_ino);
	   return 1;
	}

	while ( (fi=udf_fileident_readdir(cursor)) != NULL ) {
	   
	   /* pre-process ident */
#ifdef VDEBUG
	   printk(KERN_DEBUG "udf: enum_dir %lu ino %lu fi %p char %x lenident %d\n",
		(long)cursor->inode->i_ino,
		(long)fi->icb.extLocation.logicalBlockNum,
		(void *)fi, fi->fileCharacteristics, fi->lengthFileIdent);
#endif

	   if ( !fi->lengthFileIdent )
	   	continue;

	   if ( (fi->fileCharacteristics & FILE_DELETED) != 0 ) {
	   	if ( !udf_undelete ) 
	   		continue;
	   }
	   
	   if ( (fi->fileCharacteristics & FILE_HIDDEN) != 0 ) {
	   	if ( !udf_unhide) 
	   		continue;
	   }

	   /* callback */
	   curtail=callback(dir, fi, p1, p2, p3);
	} /* end while */

	udf_fileident_closedir(cursor);
	return curtail ? 0 : 1;
}
#endif
#endif /* def BF_CHANGES */

static struct DirectoryCursor * 
udf_fileident_opendir(struct inode *dir)
{
	struct DirectoryCursor * cursor;
	struct FileEntry *fe;
	int buildsize;
	int block;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: opendir %lu\n", dir->i_ino);
#endif

	if ( (!dir) || (!dir->i_sb) || (!S_ISDIR(dir->i_mode)) ) {
	   printk(KERN_ERR "udf: invalid inode sent to opendir\n");
	   return NULL;
	}

	buildsize=sizeof(struct DirectoryCursor) + (dir->i_sb->s_blocksize);
	cursor=(struct DirectoryCursor *) kmalloc( buildsize, GFP_KERNEL);
	if (!cursor) {
	   printk(KERN_ERR "udf: can't kmalloc work buffer\n");
	   return NULL;
	}

	cursor->inode=dir;

#if 0 /* FIX */
	block=udf_block_from_inode(cursor->inode->i_sb, cursor->inode->i_ino);
	cursor->bh=udf_read_tagged(cursor->inode->i_sb, block, 
	   			UDF_BLOCK_OFFSET(cursor->inode->i_sb));
#endif
	if (!cursor->bh) {
	   printk(KERN_ERR "udf: can't read directory block %u\n", block);
	   kfree(cursor);
	   return NULL;
	}
	cursor->currentBlockNum= block;

	/* used by AD_LONG */
	cursor->bh_alloc=NULL; 

	/* workBuffer is for fileentry's that span blocks */
	/* all other entries will be read directly from bh->b_data */
	cursor->workBufferLength= dir->i_sb->s_blocksize;	
	cursor->dirOffset=0;
	cursor->allocOffset=0;

	fe=(struct FileEntry *)cursor->bh->b_data;

	switch (UDF_I_ALLOCTYPE(cursor->inode)) {

	  case ICB_FLAG_AD_SHORT:
	   printk(KERN_DEBUG "udf: unexpected ICB_FLAG_AD_SHORT dir %u\n", 
			block);
	   udf_fileident_closedir(cursor);
	   return NULL;

	  case ICB_FLAG_AD_EXTENDED:
	   printk(KERN_DEBUG "udf: unexpected ICB_FLAG_AD_EXTENTED dir %u\n", 
			block);
	   udf_fileident_closedir(cursor);
	   return NULL;

	  case ICB_FLAG_AD_LONG:
	   { /* setup initial info */
	   	long_ad * la;
	   	la=udf_get_filelongad(fe, 
			cursor->inode->i_sb->s_blocksize, 
			&cursor->allocOffset);
	   	if ( (la) && (la->extLength) ) {
	   		cursor->extentLength=la->extLength;
	   		cursor->currentBlockNum=la->extLocation.logicalBlockNum;
	   		cursor->bh_alloc=cursor->bh;
#if 0 /* FIX */
	   		cursor->bh=udf_read_untagged(cursor->inode->i_sb, 
	   				cursor->currentBlockNum,
	   				UDF_BLOCK_OFFSET(cursor->inode->i_sb));
#endif
	   	} else {
#ifdef DEBUG
		   printk(KERN_DEBUG "udf: readdir %lu AD_LONG no extents\n",
			cursor->inode->i_ino);
	   	   udf_fileident_closedir(cursor);
#endif
		   return NULL;
		}
	   }
#ifdef DEBUG
	   printk(KERN_DEBUG "udf: dir %lu AD_LONG ext[0] -> %d size %u\n", 
	   	cursor->inode->i_ino, cursor->currentBlockNum,
		cursor->extentLength);
#endif
	   break;

	  case ICB_FLAG_AD_IN_ICB:
	   cursor->extentLength=fe->lengthAllocDescs;
	   cursor->dirOffset=(int)fe->extendedAttr - (int)fe +
	   			fe->lengthExtendedAttr;
#ifdef DEBUG
	   printk(KERN_DEBUG "udf: dir %lu AD_IN_ICB size %u\n", 
	   	cursor->inode->i_ino, cursor->extentLength);
#endif
	   break;

	}
	return cursor;
}

static struct FileIdentDesc * 
udf_fileident_readdir(struct DirectoryCursor * cursor)
{
	struct FileEntry * fe;
	struct FileIdentDesc * fi;
	long_ad * la;
	Uint8 * ptr;
	int lengthThisIdent;
	int remainder=0;

	if ( (!cursor) || (!cursor->bh) )
	   return NULL;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: readdir %lu, extlen %d\n", 
		cursor->inode->i_ino, cursor->extentLength);
#endif
	if ( (int)cursor->extentLength < 0 ) {
	   printk(KERN_ERR "udf: programmer malfunction - lengthIdents negative! line %u\n",
	   	__LINE__);
	   return NULL;
	}

	switch ( UDF_I_ALLOCTYPE(cursor->inode) ) {
	   case ICB_FLAG_AD_LONG:
	   	fe=(struct FileEntry *)cursor->bh_alloc->b_data;
	   	break;
	   case ICB_FLAG_AD_IN_ICB:
		if ( !cursor->extentLength ) 
			return NULL;
	   	fe=(struct FileEntry *)cursor->bh->b_data;
	   	break;
	   default:
	   	printk(KERN_DEBUG "udf: huh? line %u\n", __LINE__);
	   	return NULL;
	}

	cursor->workBufferLength=0;
	remainder=cursor->inode->i_sb->s_blocksize - cursor->dirOffset;
	ptr=(Uint8 *)cursor->bh->b_data + cursor->dirOffset;

	lengthThisIdent=udf_fileident_nextlength(cursor);
#ifdef VDEBUG
	printk(KERN_DEBUG 
	   "udf: dirOffset %d lengthThisIdent %d remainder %d extLen %d\n",
	   cursor->dirOffset, lengthThisIdent, remainder, cursor->extentLength);
#endif

	if ( lengthThisIdent < 1 ) {
	   /* refill our buffers */
	   remainder=cursor->inode->i_sb->s_blocksize - cursor->dirOffset;
#ifdef VDEBUG
	   printk(KERN_DEBUG "udf: refill needed %lu len %d type %u\n",
		cursor->inode->i_ino, remainder,
		UDF_I_ALLOCTYPE(cursor->inode));
#endif

	   if ( UDF_I_ALLOCTYPE(cursor->inode) == ICB_FLAG_AD_LONG ) {
	   	/* copy fragment into workBuffer */
	   	memcpy(cursor->workBuffer, ptr, remainder);
	   	cursor->workBufferLength=remainder;

	   	/* release old buffer */
	   	udf_release_data(cursor->bh);
	   	cursor->bh=NULL;

	   	/* calculate new block number */
	   	if ( cursor->extentLength > cursor->inode->i_sb->s_blocksize) {
	   		cursor->extentLength -= cursor->inode->i_sb->s_blocksize;
	   		cursor->currentBlockNum++;
#ifdef VDEBUG
			printk(KERN_DEBUG "udf: refill %lu ext %u len %d\n",
				cursor->inode->i_ino, 
				cursor->currentBlockNum,
				cursor->extentLength);
#endif
	   	} else { /* need next extent */
	   		la=udf_get_filelongad(fe, 
	   			cursor->inode->i_sb->s_blocksize, 
	   			&cursor->allocOffset);
	   		if ( (la) && (la->extLength) ) {
	   			cursor->currentBlockNum=la->extLocation.logicalBlockNum;
	   			cursor->extentLength=la->extLength;
#ifdef DEBUG
				printk(KERN_DEBUG 
				  "udf: dir %lu AD_LONG ext[] -> %u len %d\n",
				cursor->inode->i_ino, 
				cursor->currentBlockNum,
				cursor->extentLength);
#endif
	   		} else {
#ifdef DEBUG
		   	   printk(KERN_DEBUG 
				"udf: dir %lu AD_LONG ext[] end\n",
				cursor->inode->i_ino);
#endif
				return NULL;
			}
	   	}

	   	/* get new buffer */
#if 0 /* FIX */
	   	cursor->bh=udf_read_untagged(cursor->inode->i_sb,
	   			cursor->currentBlockNum,
	   			UDF_BLOCK_OFFSET(cursor->inode->i_sb));
#endif
	   	if (!cursor->bh) {
	   	   printk(KERN_ERR "udf: can't read directory block %lu\n",
			(long)cursor->currentBlockNum);
	   	   return NULL;
	   	}

	   	/* merge other fragment into workBuffer */
	   	memcpy(cursor->workBuffer+cursor->workBufferLength,
	   		cursor->bh->b_data, 
	   		(cursor->inode->i_sb->s_blocksize - remainder));

	   	/* use workBuffer instead of b_data for this one */
	   	ptr=cursor->workBuffer;
	   	cursor->dirOffset=0;
	   	lengthThisIdent=udf_fileident_nextlength(cursor);

	   	/* adjust dirOffset for fragment */
	   	cursor->dirOffset = lengthThisIdent - cursor->workBufferLength;
		cursor->extentLength -= cursor->dirOffset;
	   	cursor->workBufferLength= 0;

#ifdef VDEBUG
	   	printk(KERN_DEBUG "udf: merged fi size %d, dirOffset %d\n", 
			lengthThisIdent, cursor->dirOffset);
#endif
	   } else {
	   	printk(KERN_DEBUG "udf: alloc type? line %u rem %d type %d\n", 
	   		__LINE__, remainder, UDF_I_ALLOCTYPE(cursor->inode));
	   	return NULL;
	   }
	} else {
	   /* not a fragment */
	   cursor->dirOffset += lengthThisIdent;	
	   cursor->extentLength -= lengthThisIdent;
	}

	if ( lengthThisIdent < 0 ) {
		printk(KERN_DEBUG "udf: readdir len %d??\n", lengthThisIdent);
		return NULL;
	}

	fi=(struct FileIdentDesc *)ptr;
	if ( fi->descTag.tagIdent != TID_FILE_IDENT_DESC )
		return NULL;

	return fi;
}

static void 
udf_fileident_closedir(struct DirectoryCursor * cursor)
{
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: closedir %lu\n",
		cursor->inode->i_ino);
#endif
	if ( cursor->bh )
	   udf_release_data(cursor->bh);
	if ( cursor->bh_alloc )
	   udf_release_data(cursor->bh_alloc);
	kfree(cursor);
}

/*
 * returns: -1 if refill needed, otherwise size of ident
 */
static int
udf_fileident_nextlength(struct DirectoryCursor * cursor)
{
	struct FileIdentDesc * fi;
	Uint8 * ptr;
	int lengthThisIdent;
	int padlen;
	int remainder;

	if ( !cursor->bh )
	   return -1;

	if ( cursor->extentLength < 1 ) 
	   return -1;

	if ( !cursor->workBufferLength ) {
	   /* check for refill */
	   remainder=cursor->inode->i_sb->s_blocksize - cursor->dirOffset;
	   if ( remainder < sizeof(struct FileIdentDesc) )
	   	return -1;

	   ptr=(Uint8 *)cursor->bh->b_data + cursor->dirOffset;
	} else {
	   ptr=cursor->workBuffer;
	}
	fi=(struct FileIdentDesc *)ptr;

	lengthThisIdent=sizeof(struct FileIdentDesc) +
	   	fi->lengthFileIdent + fi->lengthOfImpUse;

	padlen=lengthThisIdent % UDF_NAME_PAD;  /* should be 4 ? */
	if ( padlen )
	   lengthThisIdent += (UDF_NAME_PAD - padlen);

	if ( (cursor->workBufferLength) ||
	     ( (cursor->dirOffset+lengthThisIdent) < cursor->inode->i_sb->s_blocksize) )
	   return lengthThisIdent;
	else
	   return -1;
}

/* Inode Operations */

#ifdef CONFIG_UDF_WRITE
static int udf_link(struct inode *, struct inode *, struct dentry *);
static int udf_unlink(struct inode *, struct dentry *);
static int udf_symlink(struct inode *, struct dentry *, const char *);
static int udf_mkdir(struct inode *, struct dentry *, int);
static int udf_rmdir(struct inode *, struct dentry *);
static int udf_mknod(struct inode *, struct dentry *, ints, int);
static int udf_rename(struct inode *, struct dentry *, struct inode *,
	struct dentry *);
#endif

#ifdef CONFIG_UDF_FULL_FS
static int udf_readlink(struct inode *, char *, int);
static struct dentry * udf_follow_link(struct inode *, struct dentry *);
static int udf_permission(struct inode *, int);
static int udf_smap(struct inode *, int);
#endif

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
#ifdef BF_CHANGES

int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode=NULL;
	lb_addr ino;
	struct file filp;
	struct FileIdentDesc *tmpfi;

	filp.f_pos = 0;
	tmpfi = (struct FileIdentDesc *) __get_free_page(GFP_KERNEL);
	if (!tmpfi)
		return -ENOMEM;

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_lookup(%lu, '%s')\n",
		dir->i_ino, dentry->d_name.name);
#endif

	udf_enum_directory(dir, udf_lookup_callback, &filp, &ino, (void *)dentry, tmpfi);

	free_page((unsigned long) tmpfi);

	if ( ino.logicalBlockNum )
	{
		inode = udf_iget(dir->i_sb, ino);
		if ( !inode )
			return -EACCES;
	}
	d_add(dentry, inode);
	return 0;
}

#else

int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode=NULL;
	long ino=0;

	/* Temporary - name doesn't exist, but it is okay to create it */
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: udf_lookup(%lu, '%s')\n",
	   dir->i_ino, dentry->d_name.name);
#endif
#ifdef DEBUG
	/* temporary shorthand for specifying files by inode number */
	if ( !strncmp(dentry->d_name.name, ".I=", 3) ) {
	   ino=simple_strtoul(dentry->d_name.name+3, NULL, 0);
	}
#endif
	udf_enum_directory(dir, udf_lookup_callback, (void *)dentry, &ino, NULL);
	if ( ino ) {
	   inode = udf_iget(dir->i_sb, ino);
	   if ( !inode )
	   	return -EACCES;
	}
	d_add(dentry, inode);
	return 0;
}
#endif

static int
udf_lookup_callback(struct inode *dir, struct FileIdentDesc *fi, 
#ifdef BF_CHANGES
		void *unused3, void *parm2, void *parm1)
#else
	   	void *parm1, void *parm2, void *unused3)
#endif
{
	struct dentry * dentry;
	lb_addr *ino;
	struct ustr filename;
	struct ustr unifilename;

	dentry=(struct dentry *)parm1;
	ino=(lb_addr *)parm2;

	if ( (!dir) || (!fi) || (!dentry) || (!ino) )
	   return 0;

	if (!fi->lengthFileIdent)
		return 0;

	if ( udf_build_ustr_exact(&unifilename, fi->fileIdent,
	   fi->lengthFileIdent) ) {
	   	return 0;
	}

	if ( udf_CS0toUTF8(&filename, &unifilename) ) {
	   	return 0;
	}

	if ( strcmp(dentry->d_name.name, filename.u_name) == 0) {
	   *ino = fi->icb.extLocation;
	   return 1; /* stop enum */
	}
	return 0; /* continue enum */
}

#ifdef CONFIG_UDF_WRITE
/*
 * udf_link
 *
 * PURPOSE
 *	Create a hard link.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of file to link to.
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new link.
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_link(struct inode *inode, struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_unlink
 *
 * PURPOSE
 *	Unlink (remove) an inode.
 *
 * DESCRIPTION
 *	Call d_delete(dentry) when ready to delete the dentry and inode.
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to unlink.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_unlink(struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_symlink
 *
 * PURPOSE
 *	Create a symbolic link.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new symlink.
 *	symname			Pointer to symbolic name of new symlink.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	return -1;
}

/*
 * udf_mkdir
 *
 * PURPOSE
 *	Create a directory.
 *
 * DESCRIPTION
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Refer to sys_mkdir() in fs/namei.c
 *	sys_mkdir() -> do_mkdir() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer toinode of parent directory.
 *	dentry			Pointer to dentry of new directory.
 *	mode			Mode of new directory.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	return -1;
}

/*
 * udf_rmdir
 *
 * PURPOSE
 *	Remove a directory.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of directory to remove.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_rmdir(struct inode *dir, struct dentry *dentry)
{
	return -1;
}

/*
 * udf_mknod
 *
 * PURPOSE
 *	Make a special node.
 *
 * DESCRIPTION
 *	Optional - sys_mknod() will retrun -EPERM if this routine is not
 *	available.
 *
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Note that only root is allowed to create sockets and device files.
 *
 *	Refer to sys_mknod() in fs/namei.c
 *	sys_mknod() -> do_mknod() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry of new node.
 *	mode			Mode of new node.
 *	rdev			Real device of new node.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_mknod(struct inode *dir, struct dentry *dentry, int mode, int rdev)
{
	return -1;
}

/*
 * udf_rename
 *
 * PURPOSE
 *	Rename an existing inode.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	old_dir			Pointer to inode of old parent directory.
 *	old_dentry		Pointer to old dentry.
 *	new_dir			Pointer to inode of new parent directory.
 *	new_dentry		Pointer to new dentry.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_rename(struct inode *inode, struct dentry *dentry, struct inode *, struct dentry *)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_FULL_FS
/*
 * udf_readlink
 *
 * PURPOSE
 *	Read the contents of a symbolic link.
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of link.
 *	buf			Pointer to buffer for name.
 *	buf_size		Size of buffer.
 *	
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_readlink(struct inode *inode, char *buf, int buf_size)
{
	return -1;
}

/*
 * udf_follow_link
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static struct dentry *
udf_follow_link(struct inode *inode, struct dentry *dentry)
{
	return ERR_PTR(-1);
}
#endif


/* Inode Operations */
#ifdef CONFIG_UDF_WRITE
static int udf_create(struct inode *, struct dentry *, int);
static int udf_link(struct inode *, struct inode *, struct dentry *);
static int udf_unlink(struct inode *, struct dentry *);
static int udf_symlink(struct inode *, struct dentry *, const char *);
static int udf_mkdir(struct inode *, struct dentry *, int);
static int udf_rmdir(struct inode *, struct dentry *);
static int udf_mknod(struct inode *, struct dentry *, ints, int);
static int udf_rename(struct inode *, struct dentry *, struct inode *,
	struct dentry *);
static int udf_writepage(struct inode *, struct page *);
static void udf_truncate(struct inode *);
static int udf_updatepage(struct inode *, struct page *, const char *,
	unsigned long, unsigned int, int);
static int udf_revalidate(struct inode *);
#endif

#ifdef CONFIG_UDF_FULL_FS
static int udf_readlink(struct inode *, char *, int);
static struct dentry * udf_follow_link(struct inode *, struct dentry *);
static int udf_readpage(struct inode *, struct page *);
static int udf_permission(struct inode *, int);
static int udf_smap(struct inode *, int);
#endif


struct inode_operations udf_dir_inode_operations= {
	&udf_dir_fops,
#ifdef CONFIG_UDF_WRITE
	NULL,			/* create */
	udf_lookup,		/* lookup */
	udf_link,		/* link */
	udf_unlink,		/* unlink */
	udf_symlink,		/* symlink */
	udf_mkdir,		/* mkdir */
	udf_rmdir,		/* rmdir */
	udf_mknod,		/* mknod */
	udf_rename,		/* rename */
	udf_readlink,		/* readlink */
	udf_follow_link,	/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	udf_bmap,		/* bmap */
	NULL,			/* truncate */
	udf_permission,		/* permission */
	NULL,			/* smap */
	NULL,			/* updatepage */
	NULL,			/* revalidate */
#else
	NULL,			/* create */
	udf_lookup,		/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,		/* bmap */
	NULL,			/* truncate */
#ifdef CONFIG_UDF_FULL_FS
	udf_permission,		/* permission */
#else
	NULL,
#endif
	NULL,			/* smap */
	NULL,			/* updatepage */
	NULL			/* revalidate */
#endif
};

#ifdef CONFIG_UDF_WRITE
/*
 * udf_create
 *
 * PURPOSE
 *	Create an inode.
 *
 * DESCRIPTION
 *	Optional - sys_open() will return -EACCESS if not available.
 *
 *	This routine is passed an incomplete dentry - it doesn't have an
 *	assigned inode, so one must be assigned by calling:
 *		d_instantiate(dentry, inode)
 *
 *	Refer to sys_open() in fs/open.c and open_namei() in fs/namei.c
 *	sys_open() -> do_open() -> open_namei() -> .
 *
 * PRE-CONDITIONS
 *	dir			Pointer to inode of parent directory.
 *	dentry			Pointer to dentry to complete.
 *	mode			Mode of new inode.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_create(struct inode *dir, struct dentry *dentry, int mode)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_readpage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	Used for memory mapping. See generic_readpage() in fs/buffer.c for
 *	filesystems that support the bmap() operation.
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_readpage(struct inode *inode, struct page *)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_writepage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	Used for memory mapping. Required if readpage() is defined.
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_writepage(struct inode *inode, struct page *)
{
	return -1;
}


/*
 * udf_truncate
 *
 * PURPOSE
 *	Truncate a file.
 *
 * DESCRIPTION
 *	This routine is called by sys_truncate() to reduce the size of a file.
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode of file to truncate.
 *
 * POST-CONDITIONS
 *	None.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static void
udf_truncate(struct inode *inode)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_FULL_FS
/*
 * udf_permissions
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_permission(struct inode *inode, int)
{
	return -1;
}

/*
 * udf_smap
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_smap(struct inode *inode, int)
{
	return -1;
}
#endif

#ifdef CONFIG_UDF_WRITE
/*
 * udf_updatepage
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_updatepage(struct inode *inode, struct page *, const char *, unsigned long, unsigned int, int)
{
	return -1;
}

/*
 *
 *
 * PURPOSE
 *
 * DESCRIPTION
 *
 * PRE-CONDITIONS
 *
 * POST-CONDITIONS
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static int
udf_revalidate(struct inode *inode)
{
	return -1;
}
#endif
