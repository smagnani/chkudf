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

#include "udfdecl.h"
#include "udf_sb.h"

#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs.h>

#else

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#endif

struct FileIdentDesc * 
udf_get_fileident(void * buffer, int bufsize, int * offset, int * remainder)
{
	struct FileIdentDesc *fi;
	int lengthThisIdent;
	Uint8 * ptr;
	int padlen;

	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: udf_get_fileident() invalidparms\n");
#endif
		return NULL;
	}

	ptr = buffer;

	if ( (*offset + sizeof(tag)) > bufsize )
	{
		/* This should not be legal, but it is done... */
#if 0
#ifdef __KERNEL__
		printk(KERN_DEBUG 
			"udf: udf_get_fileident() buffer too short (%u + %u > %u)\n", *offset, sizeof(tag), bufsize);
#endif
		return NULL;
#endif
	}

	if ( (*offset > 0) && (*offset < bufsize) ) {
		ptr += *offset;
	}
	fi=(struct FileIdentDesc *)ptr;
	if (fi->descTag.tagIdent != TID_FILE_IDENT_DESC)
	{
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileident - 0x%x != TID_FILE_IDENT_DESC\n",
			fi->descTag.tagIdent);
		printk(KERN_DEBUG "udf: offset: %u sizeof: %u bufsize: %u\n",
			*offset, sizeof(struct FileIdentDesc), bufsize);
		return NULL;
#endif
	}
	if ( (*offset + sizeof(struct FileIdentDesc)) > bufsize )
	{
		lengthThisIdent = sizeof(struct FileIdentDesc);
	}
	else
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
	Uint8 * ptr;

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

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthExtents) ) {
		ptr += *offset;
	}
	ext=(extent_ad *)ptr;

	*offset = *offset + sizeof(extent_ad);
	return ext;
}

short_ad *
udf_get_fileshortad(void * buffer, int bufsize, int * offset)
{
	short_ad * sa;
	struct FileEntry *fe;
	int lengthShortAds;
	Uint8 * ptr;
	if ( (!buffer) || (!offset) ) {
#ifdef __KERNEL__
                printk(KERN_ERR "udf: udf_get_fileshortad() invalidparms\n");
#endif
		return NULL;
	}
	fe=(struct FileEntry *)buffer;
	if ( fe->descTag.tagIdent != TID_FILE_ENTRY ) {
#ifdef __KERNEL__
		printk(KERN_DEBUG "udf: _fileshortad - 0x%x != TID_FILE_ENTRY\n",
			fe->descTag.tagIdent);
#endif
		return NULL;
	}
	lengthShortAds = fe->lengthAllocDescs;

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthShortAds) ) {
		ptr += *offset;
	}
	sa = (short_ad *)ptr;
	*offset = *offset + sizeof(short_ad);
	return sa;
}

long_ad *
udf_get_filelongad(void * buffer, int bufsize, int * offset)
{
	long_ad * la;
	struct FileEntry *fe;
	int lengthLongAds;
	Uint8 * ptr;

	if ( (!buffer) || !(offset) ) 
	{
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

	ptr=(Uint8 *)(fe->extendedAttr);
	ptr += fe->lengthExtendedAttr;

	if ( (*offset > 0) && (*offset < lengthLongAds) ) {
		ptr += *offset;
	}
	la=(long_ad *)ptr;

	*offset = *offset + sizeof(long_ad);
	return la;
}

