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
 * 10/5/98  dgb  Split directory operations into it's own file
 *               Implemented directory reads via do_udf_readdir
 * 10/6/98       Made directory operations work!
 * 11/17/98      Rewrote directory to support ICB_FLAG_AD_LONG
 * 11/25/98 blf  Rewrote directory handling (readdir+lookup) to support reading
 *               across blocks.
 * 12/12/98      Split out the lookup code to namei.c. bulk of directory
 *               code now in directory.c:udf_fileident_read.
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

/* Prototypes for file operations */
static int udf_readdir(struct file *, void *, filldir_t);
static int do_udf_readdir(struct inode *, struct file *, filldir_t, void *);

/* readdir and lookup functions */

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
 
	result = do_udf_readdir(dir, filp, filldir, dirent);
  
 	return result;
}



static int 
do_udf_readdir(struct inode * dir, struct file *filp, filldir_t filldir, void *dirent)
{
	struct buffer_head *bh;
	struct FileIdentDesc *fi=NULL;
	struct FileIdentDesc *tmpfi;
	int block;
	int offset;
	int nf_pos = filp->f_pos;
	int flen;
	char fname[255];
	Uint32 ino;
	int error = 0;
	int size = (UDF_I_EXT0OFFS(dir) + dir->i_size) >> 2;
	lb_addr loc;

#ifdef VDEBUG
	printk(KERN_DEBUG "do_udf_readdir(%p,%p,%p,%p)\n",
		dir, filp, filldir, dirent);
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

	tmpfi = (struct FileIdentDesc *) __get_free_page(GFP_KERNEL);

	while ( nf_pos < size )
	{
		filp->f_pos = nf_pos;

		fi = udf_fileident_read(dir, tmpfi, &nf_pos, &offset, &bh, &error);

		if (!fi)
		{
			free_page((unsigned long) tmpfi);
			return error;
		}

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

		loc.logicalBlockNum = __le32_to_cpu(fi->icb.extLength.logicalBlockNum);
		loc.partitionReferenceNum = __le16_to_cpu(fi->icb.extLength.partitionRefernceNum);
		ino = udf_get_lb_pblock(dir->i_sb, loc, 0);
 
 		if (fi->lengthFileIdent == 0) /* parent directory */
 		{
			if (filldir(dirent, "..", 2, filp->f_pos, filp->f_dentry->d_parent->d_inode->i_ino) < 0)
			{
				udf_release_data(bh);
				free_page((unsigned long) tmpfi);
 				return 1;
			}
 		}
		if ((flen = udf_get_filename(fi, fname, dir)))
		{
			if (filldir(dirent, fname, flen, filp->f_pos, ino) < 0)
			{
				udf_release_data(bh);
				free_page((unsigned long) tmpfi);
	 			return 1; /* halt enum */
			}
		}
	} /* end while */

	filp->f_pos = nf_pos;

	udf_release_data(bh);

	free_page((unsigned long) tmpfi);

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
