#if !defined(_LINUX_UDF_FS_H)
#define _LINUX_UDF_FS_H
/*
 * udf_fs.h
 *
 * PURPOSE
 *	Global header file for OSTA-UDF(tm) filesystem.
 *
 * DESCRIPTION
 *	This file is included by other header files, so keep it short.
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
 */

#ifdef __linux__
#include <linux/types.h>
#endif

#include "udf_167.h"	/* ECMA 167 */
#include "udf_udf.h"	/* UDF 1.5  */

/* Since UDF 1.50 is ISO 13346 based... */
#define UDF_SUPER_MAGIC	0x15013346

/* Default block size - bigger is better */
#define UDF_BLOCK_SIZE	2048

#define UDF_NAME_PAD 4

/* structures */
struct udf_directory_record {
	Uint32	d_parent;
	Uint32	d_inode;
	Uint32	d_name[255];
};

struct ktm 
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_isdst;
};

struct ustr {
	Uint8 u_cmpID;
	dstring u_name[UDF_NAME_LEN];
	Uint8 u_len;
	Uint8 padding;
	unsigned long u_hash;
};

/* Miscellaneous UDF Prototypes */
extern Uint16 udf_crc(Uint8 *, Uint32);
extern int udf_build_ustr(struct ustr *, dstring *ptr, int size);
extern int udf_build_ustr_exact(struct ustr *, dstring *ptr, int size);
extern int udf_CS0toUTF8(struct ustr *, struct ustr *);
extern int udf_UTF8toCS0(struct ustr *, struct ustr *);
extern time_t * udf_stamp_to_time(time_t *, void *);
extern time_t udf_converttime (struct ktm *);
extern uid_t  udf_convert_uid(int);
extern gid_t  udf_convert_gid(int);

/* --------------------------
 * debug stuff
 * -------------------------- */
/* Debugging levels */
#define UDF_DEBUG_NONE	0
#define UDF_DEBUG_LVL1	1
#define UDF_DEBUG_LVL2	2
#define UDF_DEBUG_LVL3	3
#define UDF_DEBUG_CRUMB	4
#define UDF_DEBUG_LVL5	5
#define UDF_DEBUG_COOKIE	6
#define UDF_DEBUG_LVL7	7
#define UDF_DEBUG_LVL8	8
#define UDF_DEBUG_LVL9	9
#define UDF_DEBUG_DUMP	10

/* module parms */
extern int udf_debuglvl;
extern int udf_strict;
extern int udf_showdeleted;
extern int udf_showhidden;

extern void udf_dump(char * buffer, int size);
extern int udf_read_tagged_data(char *, int size, int fd, int block, int offset);
extern Uint32 udf64_low32(Uint64);
extern Uint32 udf64_high32(Uint64);
extern struct FileIdentDesc * udf_get_fileident(void * buffer, int * offset);
extern extent_ad * udf_get_fileextent(void * buffer, int * offset);
extern long_ad * udf_get_filelongad(void * buffer, int * offset);

#define DUMP(X,S)	do { if (udf_debuglvl >= UDF_DEBUG_DUMP) udf_dump((X),(S)); } while(0)

/* ---------------------------
 * Kernel module definitions
 * --------------------------- */
#if defined(__linux__) && defined(__KERNEL__)
#include <linux/fs.h>
#include "udf_fs_sb.h"
#include "udf_fs_i.h"


/* Prototype for fs/filesystem.c */
extern int init_udf_fs(void);
extern struct inode_operations udf_inode_operations;

extern void udf_debug_dump(struct buffer_head *);
extern struct buffer_head *udf_read_tagged(struct super_block *, Uint32, Uint32);
extern void udf_release_data(struct buffer_head *);
extern long udf_block_from_inode(struct super_block *, long);
extern long udf_inode_from_block(struct super_block *, long);

#define DPRINTK(X,Y)	do { if (udf_debuglvl >= X) printk Y ; } while(0)
#define PRINTK(X)	do { if (udf_debuglvl >= UDF_DEBUG_LVL1) printk X ; } while(0)

#define CRUMB		DPRINTK(UDF_DEBUG_CRUMB, ("udf: file \"%s\" line %d\n", __FILE__, __LINE__))
#define COOKIE(X)	DPRINTK(UDF_DEBUG_COOKIE, X)

#else /* either not __linux__ or not __KERNEL__ */

#define DPRINTK(X,Y)	
#define PRINTK(X)	

#define CRUMB		
#define COOKIE(X)	

#endif /* defined(__linux__) && defined(__KERNEL__) */



#endif /* !defined(_LINUX_UDF_FS_H) */
