#ifndef __UDF_DECL_H
#define __UDF_DECL_H

#define UDF_VERSION_NOTICE "v0.7.1b"

#ifdef __linux__

#ifdef MODULE
#define CONFIG_UDF_FS_MODULE
#endif

#define CONFIG_UDF_FS

#ifdef __KERNEL__
#include <linux/types.h>

#include <linux/udf_udf.h>
#include <linux/udf_fs_i.h>
#include <linux/udf_fs_sb.h>

struct dentry;
struct inode;
struct task_struct;
struct buffer_head;
struct super_block;


extern struct file_operations udf_file_fops;
extern struct file_operations udf_dir_fops;
extern struct inode_operations udf_dir_inode_operations;
extern struct inode_operations udf_file_inode_operations;


extern int udf_physical_lookup(struct inode *, struct dentry *);
extern int udf_lookup(struct inode *, struct dentry *);

extern struct inode *udf_iget(struct super_block *, lb_addr);
extern void udf_read_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern void udf_write_inode(struct inode *);
extern int udf_bmap(struct inode *, int block);

/* module parms */
extern int udf_debuglvl;
extern int udf_strict;
extern int udf_undelete;
extern int udf_unhide;

extern int udf_read_tagged_data(char *, int size, int fd, int block, int partref);

extern struct inode_operations udf_inode_operations;
extern void udf_debug_dump(struct buffer_head *);
extern struct buffer_head *udf_read_sector(struct super_block *, unsigned long sec);
extern struct buffer_head *udf_read_tagged(struct super_block *, Uint32, Uint32);
extern struct buffer_head *udf_read_ptagged(struct super_block *, lb_addr);
extern struct buffer_head *udf_read_untagged(struct super_block *, Uint32, Uint32);
extern void udf_release_data(struct buffer_head *);

extern Uint32 udf_get_pblock(struct super_block *, Uint32, Uint16, Uint32);
extern Uint32 udf_get_lb_pblock(struct super_block *, lb_addr, Uint32);
#define UDF_INODE_BLOCK_MASK	0x3FFFFFFFU
#define UDF_INODE_PART_MASK	0xC0000000U
#define UDF_INODE_PART_BITS	30
extern long udf_block_from_inode(struct inode *);
extern long udf_block_from_bmap(struct inode *, int block, int part);
extern long udf_inode_from_block(struct super_block *, long block, int part);
extern int  udf_part_from_inode(struct inode *);
extern struct FileIdentDesc * udf_get_fileident(void * buffer, int bufsize, 
						int * offset, int * remainder);

#define DPRINTK(X,Y)	do { if (udf_debuglvl >= X) printk Y ; } while(0)
#define PRINTK(X)	do { if (udf_debuglvl >= UDF_DEBUG_LVL1) printk X ; } while(0)

#define CRUMB		DPRINTK(UDF_DEBUG_CRUMB, ("udf: file \"%s\" line %d\n", __FILE__, __LINE__))
#define COOKIE(X)	DPRINTK(UDF_DEBUG_COOKIE, X)

#else

#include <sys/types.h>
#include <linux/udf_udf.h>

#define DUMP(X,S)	do { if (udf_debuglvl >= UDF_DEBUG_DUMP) udf_dump((X),(S)); } while(0)
#define DPRINTK(X,Y)	
#define PRINTK(X)	

#define CRUMB		
#define COOKIE(X)	
#endif /* __KERNEL__ */

#endif /* __linux__ */

#if __BYTE_ORDER == __BIG_ENDIAN
#define htofss(x) \
	((Uint16)((((Uint16)(x) & 0x00FFU) << 8) | \
		  (((Uint16)(x) & 0xFF00U) >> 8)))
 
#define htofsl(x) \
	((Uint32)((((Uint32)(x) & 0x000000FFU) << 24) | \
		  (((Uint32)(x) & 0x0000FF00U) <<  8) | \
		  (((Uint32)(x) & 0x00FF0000U) >>  8) | \
		  (((Uint32)(x) & 0xFF000000U) >> 24)))

#define htofsll(x) \
	((Uint64)((((Uint64)(x) & 0x00000000000000FFU) << 56) | \
		  (((Uint64)(x) & 0x000000000000FF00U) << 40) | \
		  (((Uint64)(x) & 0x0000000000FF0000U) << 24) | \
		  (((Uint64)(x) & 0x00000000FF000000U) <<  8) | \
		  (((Uint64)(x) & 0x000000FF00000000U) >>  8) | \
		  (((Uint64)(x) & 0x0000FF0000000000U) >> 24) | \
		  (((Uint64)(x) & 0x00FF000000000000U) >> 40) | \
		  (((Uint64)(x) & 0xFF00000000000000U) >> 56)))		

#define fstohs(x) (htofss(x))
#define fstohl(x) (htofsl(x))
#define fstohll(x) (htofsll(x))
#else /* __BYTE_ORDER == __LITTLE_ENDIAN */
#define htofss(x) (x)
#define htofsl(x) (x)
#define htofsll(x) (x)
#define fstohs(x) (x)
#define fstohl(x) (x)
#define fstohll(x) (x)
#endif

/* structures */
struct udf_directory_record
{
	Uint32	d_parent;
	Uint32	d_inode;
	Uint32	d_name[255];
};

#define VDS_POS_PRIMARY_VOL_DESC	0
#define VDS_POS_UNALLOC_SPACE_DESC	1
#define VDS_POS_LOGICAL_VOL_DESC	2
#define VDS_POS_PARTITION_DESC		3
#define VDS_POS_IMP_USE_VOL_DESC	4
#define VDS_POS_VOL_DESC_PTR		5
#define VDS_POS_TERMINATING_DESC	6
#define VDS_POS_LENGTH				7

struct udf_vds_record
{
	Uint32 block;
	Uint32 volDescSeqNum;
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

struct ustr
{
	Uint8 u_cmpID;
	dstring u_name[UDF_NAME_LEN];
	Uint8 u_len;
	Uint8 padding;
	unsigned long u_hash;
};

/* Miscellaneous UDF Prototypes */
extern Uint16 udf_crc(Uint8 *, Uint32);
extern int udf_translate_to_linux(char *, char *, int, char *, int);
extern int udf_build_ustr(struct ustr *, dstring *ptr, int size);
extern int udf_build_ustr_exact(struct ustr *, dstring *ptr, int size);
extern int udf_CS0toUTF8(struct ustr *, struct ustr *);
extern int udf_UTF8toCS0(dstring *, struct ustr *, int);

extern uid_t  udf_convert_uid(int);
extern gid_t  udf_convert_gid(int);
extern Uint32 udf64_low32(Uint64);
extern Uint32 udf64_high32(Uint64);


extern time_t *udf_stamp_to_time(time_t *, void *);
extern time_t udf_converttime (struct ktm *);

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

extern void udf_dump(char * buffer, int size);


extern extent_ad * udf_get_fileextent(void * buffer, int bufsize, int * offset);
extern long_ad * udf_get_filelongad(void * buffer, int bufsize, int * offset);
extern short_ad * udf_get_fileshortad(void * buffer, int bufsize, int * offset);


#endif
