/*
 * directory.c
 *
 * PURPOSE
 *	Directory related functions
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
 */


#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <linux/udf_fs.h>

#else

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/udf_fs.h>

#endif

struct FileIdentDesc * 
udf_get_fileident(void * buffer, int bufsize, int * offset, int * remainder)
{
	struct FileIdentDesc *fi;
	int lengthThisIdent;
	__u8 * ptr;
	int padlen;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: udf_get_fileident() invalidparms\n");
#endif
		return NULL;
	}

	ptr = buffer;

	if ( (*offset + sizeof(struct FileIdentDesc)) > bufsize ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG 
			"udf: udf_get_fileident() buffer too short\n");
#endif
		return NULL;
	}

	if ( (*offset > 0) && (*offset < bufsize) ) {
		ptr += *offset;
	}
	fi=(struct FileIdentDesc *)ptr;

	lengthThisIdent=sizeof(struct FileIdentDesc) +
		fi->lengthFileIdent + fi->lengthOfImpUse;

	/* we need to figure padding, too! */
	padlen=lengthThisIdent % UDF_NAME_PAD;
	if (padlen)
		lengthThisIdent+= (UDF_NAME_PAD - padlen);
	*offset = *offset + lengthThisIdent;
	if ( remainder ) {
		*remainder=bufsize - *offset;
	}

	return fi;
}

extent_ad *
udf_get_fileextent(void * buffer, int bufsize, int * offset)
{
	extent_ad * ext;
	struct FileEntry *fe;
	int lengthExtents;
	__u8 * ptr;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_fileextent() invalidparms\n");
#endif
		return NULL;
	}

	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileextent - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthExtents = fe->lengthAllocDescs;

	ptr=(__u8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthExtents) ) {
		ptr += *offset;
	}
	ext=(extent_ad *)ptr;

	*offset = *offset + sizeof(extent_ad);
	return ext;
}

long_ad *
udf_get_filelongad(void * buffer, int bufsize, int * offset)
{
	long_ad * la;
	struct FileEntry *fe;
	int lengthLongAds;
	__u8 * ptr;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_ERR "udf: udf_get_filelongad() invalidparms\n");
#endif
		return NULL;
	}

	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _filelongad - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthLongAds = fe->lengthAllocDescs;

	ptr=(__u8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthLongAds) ) {
		ptr += *offset;
	}
	la=(long_ad *)ptr;

	*offset = *offset + sizeof(long_ad);
	return la;
}

