/*
 * file.c
 *
 * PURPOSE
 *  File handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *  E-mail regarding any portion of the Linux UDF file system should be
 *  directed to the development team mailing list (run by majordomo):
 *    linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *    ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-1999 Dave Boynton
 *  (C) 1998-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  10/02/98 dgb  Attempt to integrate into udf.o
 *  10/07/98      Switched to using generic_readpage, etc., like isofs
 *                And it works!
 *  12/06/98 blf  Added udf_file_read. uses generic_file_read for all cases but
 *                ICBTAG_FLAG_AD_IN_ICB.
 *  04/06/99      64 bit file handling on 32 bit systems taken from ext2 file.c
 *  05/12/99      Preliminary file write support
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/udf_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h> /* memset */
#include <linux/errno.h>
#include <linux/locks.h>

#include "udf_i.h"
#include "udf_sb.h"

#define NBUF	32

typedef void * poll_table; 

int udf_adinicb_readpage (struct file * file, struct page * page)
{
	struct inode * inode;
	struct buffer_head *bh;
	int block;
	int err = 0;

	inode = file->f_dentry->d_inode;

	memset((char *)page_address(page), 0, PAGE_SIZE);
	block = udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0);
	bh = getblk (inode->i_dev, block, inode->i_sb->s_blocksize);
	if (!bh)
	{
		set_bit(PG_error, &page->flags);
		err = -EIO;
		goto out;
	}
	if (!buffer_uptodate(bh))
	{
		ll_rw_block (READ, 1, &bh);
		wait_on_buffer(bh);
	}
	memcpy((char *)page_address(page), bh->b_data + udf_ext0_offset(inode),
		inode->i_size);
	brelse(bh);
	set_bit(PG_uptodate, &page->flags);
out:
	return err;
}

/*
 * Make sure the offset never goes beyond the 32-bit mark..
 */
static long long udf_file_llseek(struct file * file, long long offset, int origin)
{
	struct inode * inode = file->f_dentry->d_inode;

	switch (origin)
	{
		case 2:
		{
			offset += inode->i_size;
			break;
		}
		case 1:
		{
			offset += file->f_pos;
			break;
		}
	}
#if BITS_PER_LONG < 64
	if (((unsigned long long) offset >> 32) != 0)
	{
		return -EINVAL;
	}
#endif
	if (offset != file->f_pos)
	{
		file->f_pos = offset;
		file->f_reada = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		file->f_version = ++event;
#else
		file->f_version = ++global_event;
#endif
	}
	return offset;
}

static inline void remove_suid(struct inode * inode)
{
	unsigned int mode;

	/* set S_IGID if S_IXGRP is set, and always set S_ISUID */
	mode = (inode->i_mode & S_IXGRP)*(S_ISGID/S_IXGRP) | S_ISUID;

	/* was any of the uid bits set? */
	mode &= inode->i_mode;
	if (mode && !capable(CAP_FSETID))
	{
		inode->i_mode &= ~mode;
		mark_inode_dirty(inode);
	}
}

static ssize_t udf_file_write(struct file * filp, const char * buf,
	size_t count, loff_t *ppos)
{
	struct inode * inode = filp->f_dentry->d_inode;
	off_t pos;
	long block;
	int offset;
	int written, c;
	struct buffer_head * bh, * bufferlist[NBUF];
	struct super_block * sb;
	int err;
	int i, buffercount, write_error, new_buffer;
	unsigned long limit;

	/* POSIX: mtime/ctime may not change for 0 count */
	if (!count)
		return 0;
	/* This makes the bounds-checking arithmetic later on much more
	 * sane. */
	write_error = buffercount = 0;
	if (!inode)
	{
		printk("udf_file_write: inode = NULL\n");
		return -EINVAL;
	}
	sb = inode->i_sb;
	if (sb->s_flags & MS_RDONLY)
	{
		return -ENOSPC;
	}

	if (!S_ISREG(inode->i_mode))
	{
		udf_warning(sb, "udf_file_write", "mode = %07o", inode->i_mode);
		return -EINVAL;
	}
	remove_suid(inode);

	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;
	else
	{
		pos = *ppos;
		if (pos != *ppos)
			return -EINVAL;
	}

#if BITS_PER_LONG < 64
	/* If the fd's pos is already greater than or equal to the file
	 * descriptor's offset maximum, then we need to return EFBIG for
	 * any non-zero count (and we already tested for zero above). */
	if (((off_t) pos) >= 0x7FFFFFFFUL)
		return -EFBIG;

	/* If we are about to overflow the maximum file size, we also
	 * need to return the error, but only if no bytes can be written
	 * successfully. */
	if (((off_t) pos + count) > 0x7FFFFFFFUL)
	{
		count = 0x7FFFFFFFL - pos;
		if (((ssize_t) count) < 0)
			return -EFBIG;
	}
#endif

	limit = current->rlim[RLIMIT_FSIZE].rlim_cur;
	if (limit < RLIM_INFINITY)
	{
		if (((off_t) pos + count) >= limit)
		{
			count = limit - pos;
			if (((ssize_t) count) <= 0)
			{
				send_sig(SIGXFSZ, current, 0);
				return -EFBIG;
			}
		}
	}


	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
	{
		if (inode->i_sb->s_blocksize < (udf_file_entry_alloc_offset(inode) +
			pos + count))
		{
			udf_expand_file_adinicb(inode, pos + count, &err);
			if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
			{
				udf_debug("udf_expand_file_adinicb: err=%d\n", err);
				return err;
			}
		}
		else
		{
			if (pos + count > inode->i_size)
				UDF_I_LENALLOC(inode) = pos + count;
			else
				UDF_I_LENALLOC(inode) = inode->i_size;
			pos += udf_file_entry_alloc_offset(inode);
		}
	}

	if (filp->f_flags & O_SYNC)
		; /* Do something */
	block = pos >> sb->s_blocksize_bits;
	offset = pos & (sb->s_blocksize - 1);
	c = sb->s_blocksize - offset;
	written = 0;

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
		pos -= udf_file_entry_alloc_offset(inode);

	do
	{
		bh = udf_getblk(inode, block, 1, &err);
		if (!bh)
		{
			if (!written)
				written = err;
			break;
		}
		if (c > count)
			c = count;

		new_buffer = (!buffer_uptodate(bh) && !buffer_locked(bh) &&
			c == sb->s_blocksize);

		if (new_buffer)
		{
			set_bit(BH_Lock, &bh->b_state);
			c -= copy_from_user(bh->b_data + offset, buf, c);
			if (c != sb->s_blocksize)
			{
				c = 0;
				unlock_buffer(bh);
				brelse(bh);
				if (!written)
					written = -EFAULT;
				break;
			}
			mark_buffer_uptodate(bh, 1);
			unlock_buffer(bh);
		}
		else
		{
			if (!buffer_uptodate(bh))
			{
				ll_rw_block(READ, 1, &bh);
				wait_on_buffer(bh);
				if (!buffer_uptodate(bh))
				{
					brelse(bh);
					if (!written)
						written = -EIO;
					break;
				}
			}
			c -= copy_from_user(bh->b_data + offset, buf, c);
		}
		if (!c)
		{
			brelse(bh);
			if (!written)
				written = -EFAULT;
			break;
		}
		mark_buffer_dirty(bh, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,14)
		update_vm_cache(inode, pos, bh->b_data + offset, c);
#else
		update_vm_cache_conditional(inode, pos, bh->b_data + offset, c,
			(unsigned long) buf);
#endif
		pos += c;
		written += c;
		buf += c;
		count -= c;

		if (filp->f_flags & O_SYNC)
			bufferlist[buffercount++] = bh;
		else
			brelse(bh);
		if (buffercount == NBUF)
		{
			ll_rw_block(WRITE, buffercount, bufferlist);
			for (i=0; i<buffercount; i++)
			{
				wait_on_buffer(bufferlist[i]);
				if (!buffer_uptodate(bufferlist[i]))
					write_error = 1;
				brelse(bufferlist[i]);
			}
			buffercount = 0;
		}
		if (write_error)
			break;
		block++;
		offset = 0;
		c = sb->s_blocksize;
	} while (count);

	if (buffercount)
	{
		ll_rw_block(WRITE, buffercount, bufferlist);
		for (i=0; i<buffercount; i++)
		{
			wait_on_buffer(bufferlist[i]);
			if (!buffer_uptodate(bufferlist[i]))
				write_error = 1;
			brelse(bufferlist[i]);
		}
	}

	if (pos > inode->i_size)
		inode->i_size = pos;
	if (filp->f_flags & O_SYNC)
		; /* Do something */
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	UDF_I_UCTIME(inode) = UDF_I_UMTIME(inode) = CURRENT_UTIME;
	*ppos = pos;
	mark_inode_dirty(inode);
	return written;
}

/*
 * udf_ioctl
 *
 * PURPOSE
 *	Issue an ioctl.
 *
 * DESCRIPTION
 *	Optional - sys_ioctl() will return -ENOTTY if this routine is not
 *	available, and the ioctl cannot be handled without filesystem help.
 *
 *	sys_ioctl() handles these ioctls that apply only to regular files:
 *		FIBMAP [requires udf_bmap()], FIGETBSZ, FIONREAD
 *	These ioctls are also handled by sys_ioctl():
 *		FIOCLEX, FIONCLEX, FIONBIO, FIOASYNC
 *	All other ioctls are passed to the filesystem.
 *
 *	Refer to sys_ioctl() in fs/ioctl.c
 *	sys_ioctl() -> .
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode that ioctl was issued on.
 *	filp			Pointer to file that ioctl was issued on.
 *	cmd			The ioctl command.
 *	arg			The ioctl argument [can be interpreted as a
 *				user-space pointer if desired].
 *
 * POST-CONDITIONS
 *	<return>		Success (>=0) or an error code (<=0) that
 *				sys_ioctl() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int result = -EINVAL;
	struct buffer_head *bh = NULL;
	long_ad eaicb;
	uint8_t *ea = NULL;

	if ( permission(inode, MAY_READ) != 0 )
	{
		udf_debug("no permission to access inode %lu\n",
						inode->i_ino);
		return -EPERM;
	}

	if ( !arg )
	{
		udf_debug("invalid argument to udf_ioctl\n");
		return -EINVAL;
	}

	/* first, do ioctls that don't need to udf_read */
	switch (cmd)
	{
		case UDF_GETVOLIDENT:
			return copy_to_user((char *)arg,
				UDF_SB_VOLIDENT(inode->i_sb), 32) ? -EFAULT : 0;
		case UDF_RELOCATE_BLOCKS:
		{
			long old, new;

			if (!capable(CAP_SYS_ADMIN)) return -EACCES;
			if (get_user(old, (long *)arg)) return -EFAULT;
			if ((result = udf_relocate_blocks(inode->i_sb,
					old, &new)) == 0)
				result = put_user(new, (long *)arg);

			return result;
		}
	}

	/* ok, we need to read the inode */
	bh = udf_tread(inode->i_sb,
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0),
		inode->i_sb->s_blocksize);

	if (!bh)
	{
		udf_debug("bread failed (inode=%ld)\n", inode->i_ino);
		return -EIO;
	}

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)bh->b_data;
		eaicb = lela_to_cpu(fe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = fe->extendedAttr;
	}
	else
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)bh->b_data;
		eaicb = lela_to_cpu(efe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = efe->extendedAttr;
	}

	switch (cmd) 
	{
		case UDF_GETEASIZE:
			result = put_user(UDF_I_LENEATTR(inode), (int *)arg);
			break;

		case UDF_GETEABLOCK:
			result = copy_to_user((char *)arg, ea,
				UDF_I_LENEATTR(inode)) ? -EFAULT : 0;
			break;
	}

	udf_release_data(bh);
	return result;
}

/*
 * udf_release_file
 *
 * PURPOSE
 *  Called when all references to the file are closed
 *
 * DESCRIPTION
 *  Discard prealloced blocks
 *
 * HISTORY
 *
 */
static int udf_release_file(struct inode * inode, struct file * filp)
{
	if (filp->f_mode & FMODE_WRITE)
		udf_discard_prealloc(inode);
	return 0;
}

#if BITS_PER_LONG < 64
/*
 * udf_open_file
 *
 * PURPOSE
 *  Called when an inode is about to be open.
 *
 * DESCRIPTION
 *  Use this to disallow opening RW large files on 32 bit systems.
 *
 * HISTORY
 *
 */
static int udf_open_file(struct inode * inode, struct file * filp)
{
	if (inode->i_size == (uint32_t)-1 && (filp->f_mode & FMODE_WRITE))
		return -EFBIG;
	return 0;
}
#endif

static struct file_operations udf_file_operations = {
	udf_file_llseek,		/* llseek */
	generic_file_read,		/* read */
	udf_file_write,			/* write */
	NULL,					/* readdir */
	NULL,					/* poll */
	udf_ioctl,				/* ioctl */
	generic_file_mmap,		/* mmap */
#if BITS_PER_LONG < 64
	udf_open_file,			/* open */
#else
	NULL, 					/* open */
#endif
	NULL,					/* flush */
	udf_release_file,		/* release */
	udf_sync_file,			/* fsync */
	NULL,					/* fasync */
	NULL,					/* check_media_change */
	NULL,					/* revalidate */
	NULL					/* lock */
};

struct inode_operations udf_file_inode_operations = {
	&udf_file_operations,
	NULL,					/* create */
	NULL,					/* lookup */
	NULL,					/* link */
	NULL,					/* unlink */
	NULL,					/* symlink */
	NULL,					/* mkdir */
	NULL,					/* rmdir */
	NULL,					/* mknod */
	NULL,					/* rename */
	NULL,					/* readlink */
	NULL,					/* follow_link */
	generic_readpage,		/* readpage */
	NULL,					/* writepage */
	udf_bmap,				/* bmap */
	udf_truncate,			/* truncate */
	NULL,					/* permission */
	NULL,					/* smap */
	NULL,					/* updatepage */
	NULL					/* revalidate */
};

struct inode_operations udf_file_inode_operations_adinicb = {
	&udf_file_operations,
	NULL,					/* create */
	NULL,					/* lookup */
	NULL,					/* link */
	NULL,					/* unlink */
	NULL,					/* symlink */
	NULL,					/* mkdir */
	NULL,					/* rmdir */
	NULL,					/* mknod */
	NULL,					/* rename */
	NULL,					/* readlink */
	NULL,					/* follow_link */
	udf_adinicb_readpage,	/* readpage */
	NULL,					/* writepage */
	NULL,					/* bmap */
	udf_truncate,			/* truncate */
	NULL,					/* permission */
	NULL,					/* smap */
	NULL,					/* updatepage */
	NULL					/* revalidate */
};
