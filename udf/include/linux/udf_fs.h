#if !defined(_LINUX_UDF_FS_H)
#define _LINUX_UDF_FS_H
/*
 * udf_fs.h
 *
 * Included by fs/filesystems.c
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
 *	July 21, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 10/2/98 dgb	rearranged all headers
 * 11/26/98 bf  added byte order macros
 * 12/5/98 dgb  removed other includes to reduce kernel namespace pollution.
 *		This should only be included by the kernel now!
 */

/* Prototype for fs/filesystem.c (the only thing really required in this file) */
extern int init_udf_fs(void);

#endif /* !defined(_LINUX_UDF_FS_H) */
