#ifndef __UDF_DECL_H
#define __UDF_DECL_H

#define UDF_VERSION_NOTICE "v0.8.2"

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/udf_udf.h>
#include <linux/config.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < 0x020170
#error "The UDF Module Current Requires Kernel Version 2.1.70 or greater"
#endif

/* if we're not defined, we must be compiling outside of the kernel tree */
#if !defined(CONFIG_UDF_FS) && !defined(CONFIG_UDF_FS_MODULE)
/* ... so override config */
#define CONFIG_UDF_FS_MODULE
#include <linux/fs.h>
/* explicitly include udf_fs_sb.h and udf_fs_i.h */
#include <linux/udf_fs_sb.h>
#include <linux/udf_fs_i.h>
#else
#include <linux/fs.h> /* also gets udf_fs_i.h and udf_fs_sb.h */
#endif

struct dentry;
struct inode;
struct task_struct;
struct buffer_head;
struct super_block;

extern struct inode_operations udf_dir_inode_operations;
extern struct inode_operations udf_file_inode_operations;
extern struct inode_operations udf_symlink_inode_operations;

extern void udf_warning(struct super_block *, const char *, const char *, ...);
extern int udf_lookup(struct inode *, struct dentry *);
extern int udf_create(struct inode *, struct dentry *, int);
extern int udf_mknod(struct inode *, struct dentry *, int, int);
extern int udf_mkdir(struct inode *, struct dentry *, int);
extern int udf_rmdir(struct inode *, struct dentry *);
extern int udf_unlink(struct inode *, struct dentry *);
extern int udf_symlink(struct inode *, struct dentry *, const char *);
extern int udf_link(struct dentry *, struct inode *, struct dentry *);
extern int udf_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
extern struct inode *udf_iget(struct super_block *, lb_addr);
extern void udf_read_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern void udf_write_inode(struct inode *);
extern int udf_bmap(struct inode *, int);
extern int block_bmap(struct inode *, int, lb_addr *, Uint32 *, lb_addr *, Uint32 *, Uint32 *);
extern int udf_next_aext(struct inode *, lb_addr *, int *, lb_addr *, Uint32 *);

extern int udf_read_tagged_data(char *, int size, int fd, int block, int partref);

extern struct buffer_head *udf_bread(struct super_block *, int, int);
extern struct GenericAttrFormat *udf_add_extendedattr(struct inode *, Uint32, Uint32, Uint8, struct buffer_head **);
extern struct GenericAttrFormat *udf_get_extendedattr(struct inode *, Uint32, Uint8, struct buffer_head **);
extern struct buffer_head *udf_read_tagged(struct super_block *, Uint32, Uint32, Uint16 *);
extern struct buffer_head *udf_read_ptagged(struct super_block *, lb_addr, Uint32, Uint16 *);
extern struct buffer_head *udf_read_untagged(struct super_block *, Uint32, Uint32);
extern void udf_release_data(struct buffer_head *);

extern unsigned int udf_get_last_session(kdev_t);
extern unsigned int udf_get_last_block(kdev_t, int *);

extern Uint32 udf_get_pblock(struct super_block *, Uint32, Uint16, Uint32);
extern Uint32 udf_get_lb_pblock(struct super_block *, lb_addr, Uint32);

extern int udf_get_filename(char *, char *, int);

void udf_free_inode(struct inode *);
struct inode * udf_new_inode (const struct inode *, int, int *);
void udf_discard_prealloc(struct inode *);
void udf_truncate(struct inode *);
void udf_free_blocks(const struct inode *, lb_addr, Uint32, Uint32);
lb_addr udf_new_block(const struct inode *, lb_addr, Uint32 *, Uint32 *, int *);

#else

#include <sys/types.h>
#include <linux/udf_udf.h>

#endif /* __KERNEL__ */

#include "udfend.h"

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
	char u_name[UDF_NAME_LEN-1];
	Uint8 u_len;
	Uint8 padding;
	unsigned long u_hash;
};

#define udf_fixed_to_variable(x) ( ( ( x >> 5 ) * 39 ) + ( x & 0x0000001F ) )
#define udf_variable_to_fixed(x) ( ( ( x / 39 ) << 5 ) + ( x % 39 ) )

#ifdef __KERNEL__

#define udf_clear_bit(nr,addr) ext2_clear_bit(nr,addr)
#define udf_set_bit(nr,addr) ext2_set_bit(nr,addr)
#define udf_test_bit(nr, addr) ext2_test_bit(nr, addr)
#define udf_find_first_one_bit(addr, size) find_first_one_bit(addr, size)
#define udf_find_next_one_bit(addr, size, offset) find_next_one_bit(addr, size, offset)

extern inline int find_next_one_bit (void * addr, int size, int offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset / BITS_PER_LONG);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= (BITS_PER_LONG-1);
	if (offset)
	{
		tmp = *(p++);
		tmp |= ~0UL >> (BITS_PER_LONG-offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1))
	{
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;
found_first:
	tmp |= ~0UL << size;
found_middle:
	return result + ffs(tmp);
}

#define find_first_one_bit(addr, size)\
	find_next_one_bit((addr), (size), 0)

#endif

/* Miscellaneous UDF Prototypes */

extern int udf_ustr_to_dchars(char *, const struct ustr *, int);
extern int udf_ustr_to_char(char *, const struct ustr *, int);
extern int udf_ustr_to_dstring(dstring *, const struct ustr *, int);
extern int udf_dchars_to_ustr(struct ustr *, const char *, int);
extern int udf_char_to_ustr(struct ustr *, const char *, int);
extern int udf_dstring_to_ustr(struct ustr *, const dstring *, int);

extern Uint16 udf_crc(Uint8 *, Uint32, Uint16);
extern int udf_translate_to_linux(char *, char *, int, char *, int);
extern int udf_build_ustr(struct ustr *, dstring *ptr, int size);
extern int udf_build_ustr_exact(struct ustr *, dstring *ptr, int size);
extern int udf_CS0toUTF8(struct ustr *, struct ustr *);
extern int udf_UTF8toCS0(dstring *, struct ustr *, int);

extern uid_t  udf_convert_uid(int);
extern gid_t  udf_convert_gid(int);
extern Uint32 udf64_low32(Uint64);
extern Uint32 udf64_high32(Uint64);


extern time_t *udf_stamp_to_time(time_t *, timestamp);
extern timestamp *udf_time_to_stamp(timestamp *dest, time_t src);
extern time_t udf_converttime (struct ktm *);

#ifdef __KERNEL__
extern Uint8 *
udf_filead_read(struct inode *, Uint8 *, Uint8, lb_addr, int *, int *,
	struct buffer_head **, int *);

extern struct FileIdentDesc *
udf_fileident_read(struct inode *, int *,
	int *, struct buffer_head **,
	int *, struct buffer_head **,
	struct FileIdentDesc *);
#endif
extern struct FileIdentDesc * 
udf_get_fileident(void * buffer, int bufsize, int * offset);
extern extent_ad * udf_get_fileextent(void * buffer, int bufsize, int * offset);
extern long_ad * udf_get_filelongad(void * buffer, int bufsize, int * offset);
extern short_ad * udf_get_fileshortad(void * buffer, int bufsize, int * offset);
extern Uint8 * udf_get_filead(struct FileEntry *, Uint8 *, int, int, int, int *);


#endif
