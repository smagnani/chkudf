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
#include "udf_i.h"
#include "udf_sb.h"
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

/* Prototypes for file operations */
static int udf_readdir(struct file *, void *, filldir_t);

/* generic directory enumeration */
typedef int (*udf_enum_callback)(struct inode *, struct FileIdentDesc *, 
	   			struct file *, filldir_t, void *);
static int udf_enum_directory(struct inode *, udf_enum_callback, 
				struct file *, filldir_t, void *,
				struct FileIdentDesc *);

/* readdir and lookup functions */
static int udf_lookup_callback(struct inode *, struct FileIdentDesc*, 
	   			void *, void *, void *);

static int udf_readdir_callback(struct inode *, struct FileIdentDesc*, 
				struct file *, filldir_t, void *);

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
 			struct file *filp, filldir_t filldir, void *dirent)
{
 	struct ustr filename;
 	struct ustr unifilename;
 	long ino;
	char name[255];
	int len;
 	
 	if ( (!fi) || (!filp) || (!filldir) || (!dir) )
 	{
 		return 0;
 	}
 
	ino = udf_get_lb_pblock(dir->i_sb, fi->icb.extLocation, 0);
 
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
 
        if (len = udf_translate_to_linux(name, filename.u_name, filename.u_len-1,
                unifilename.u_name, unifilename.u_len))
        {
	 	if (filldir(dirent, name, len, filp->f_pos, ino) < 0)
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
			if ( !IS_UNDELETE(dir->i_sb) )
				continue;
		}
		
		if ( (fi->fileCharacteristics & FILE_HIDDEN) != 0 )
		{
			if ( !IS_UNHIDE(dir->i_sb) )
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

int
udf_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode=NULL;
	lb_addr ino;
	struct file filp;
	struct FileIdentDesc *tmpfi;

	filp.f_pos = 0;
	memset(&ino, 0, sizeof(lb_addr));
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

static int
udf_lookup_callback(struct inode *dir, struct FileIdentDesc *fi, 
		void *unused3, void *parm2, void *parm1)
{
	struct dentry * dentry;
	lb_addr *ino;
	struct ustr filename;
	struct ustr unifilename;
	char name[255];
	int len;

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

	if (len = udf_translate_to_linux(name, filename.u_name, filename.u_len-1,
		unifilename.u_name, unifilename.u_len))
	{
		if ( strncmp(dentry->d_name.name, name, len) == 0)
		{
			*ino = fi->icb.extLocation;
			return 1; /* stop enum */
		}
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
