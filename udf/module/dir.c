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
 */

#include <config/udf.h>

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/version.h>
#include <linux/udf_fs.h>
#include <linux/string.h>
#include <linux/errno.h>
#endif

#include "udfdecl.h"

/* Prototypes for file operations */
static int udf_readdir(struct file *, void *, filldir_t);

/* generic directory enumeration */
typedef int (*udf_enum_callback)(struct inode *, struct FileIdentDesc *, 
					void *, void *, void *);
static int udf_enum_directory(struct inode *, udf_enum_callback, 
					void *, void *, void *);

/* readdir and lookup functions */
static int udf_lookup_callback(struct inode *, struct FileIdentDesc*, 
					void *, void *, void *);

static int udf_readdir_callback(struct inode *, struct FileIdentDesc*, 
					void *, void *, void *);

struct file_operations udf_dir_fops = {
	NULL,			/* llseek */
	NULL,			/* read */
	NULL,
	udf_readdir,		/* readdir */
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

	if ( (!fi) || (!filp) || (!filldir) || (!dir) ) {
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
		printk(KERN_DEBUG "udf: readdir callback '%s' dir %d == f_pos\n",
			filename.u_name, UDF_I_DIRPOS(dir));
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
	struct buffer_head *bh;
	struct FileIdentDesc *fi=NULL;
	int block;
	int lengthIdents;
	int offset;
	int curtail=0;
	int result=0;
	int remainder;

	offset=UDF_I_EXT0OFFS(dir);
	block=UDF_I_EXT0LOC(dir);
	lengthIdents=UDF_I_EXT0LEN(dir);
	
#ifdef VDEBUG
	printk(KERN_DEBUG "udf: enum type %u block %u len %u offset %u\n",
		UDF_I_ALLOCTYPE(dir), UDF_I_EXT0LOC(dir),
		UDF_I_EXT0LEN(dir), UDF_I_EXT0OFFS(dir));
#endif

	bh = bread(dir->i_sb->s_dev, block+UDF_BLOCK_OFFSET(dir->i_sb), dir->i_sb->s_blocksize);
	if (!bh) {
		printk(KERN_DEBUG "udf: udf_enum_directory(%ld) !bh\n",
			dir->i_ino);
		return -1;
	}

	if ( UDF_I_ALLOCTYPE(dir) == ICB_FLAG_AD_IN_ICB )
		lengthIdents += offset;

	while ( (lengthIdents > offset) && (!curtail) ) {

		/* getting next ident depends on alloc_type */

		fi=udf_get_fileident(bh->b_data, dir->i_sb->s_blocksize, 
			&offset, &remainder);
		if ( !fi ) {
			printk(KERN_DEBUG "udf: get_fileident(,,%d) failed\n",
				offset);
			udf_release_data(bh);
			return 1;
		}

		/* pre-process ident */

		if ( !fi->lengthFileIdent ) /* len=0 means parent directory */
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

		/* setup for next read */
		if ( remainder < sizeof(struct FileIdentDesc) ) {
			/* we need to add a refill-buffer function here */
			printk(KERN_INFO "udf: ino %lu long directory truncated\n",
				dir->i_ino);
			udf_release_data(bh);
			return 1;
		}
	} /* end while */
	if ( offset >= lengthIdents )
		result=1;

	if ( bh )
		udf_release_data(bh);
	return result;
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

static int
udf_lookup_callback(struct inode *dir, struct FileIdentDesc *fi, 
			void *parm1, void *parm2, void *unused3)
{
	struct dentry * dentry;
	long *ino;
	struct ustr filename;
	struct ustr unifilename;

	dentry=(struct dentry *)parm1;
	ino=(long *)parm2;

	if ( (!dir) || (!fi) || (!dentry) || (!ino) )
		return 0;

	if ( udf_build_ustr_exact(&unifilename, fi->fileIdent,
		fi->lengthFileIdent) ) {
			return 0;
	}

	if ( udf_CS0toUTF8(&filename, &unifilename) ) {
			return 0;
	}

#ifdef VDEBUG
	printk(KERN_DEBUG "udf: lookup_callback(%s) %s found \n",
			dentry->d_name.name, filename.u_name);
#endif
	if ( strcmp(dentry->d_name.name, filename.u_name) == 0) {
		*ino = fi->icb.extLocation.logicalBlockNum;
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
