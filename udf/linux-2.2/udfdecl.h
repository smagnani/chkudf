#ifndef __UDF_DECL_H
#define __UDF_DECL_H

#include <linux/udf_fs.h>
#include "ecma_167.h"
#include "osta_udf.h"

#include <linux/fs.h>
#include <linux/config.h>
#include <linux/types.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if !defined(CONFIG_UDF_FS) && !defined(CONFIG_UDF_FS_MODULE)
#define CONFIG_UDF_FS_MODULE
#include <linux/udf_fs_i.h>
#include <linux/udf_fs_sb.h>
#endif

#include "udfend.h"

#define udf_fixed_to_variable(x) ( ( ( (x) >> 5 ) * 39 ) + ( (x) & 0x0000001F ) )
#define udf_variable_to_fixed(x) ( ( ( (x) / 39 ) << 5 ) + ( (x) % 39 ) )

#define UDF_EXTENT_LENGTH_MASK	0x3FFFFFFF
#define UDF_EXTENT_FLAG_MASK	0xC0000000

#define UDF_NAME_PAD		4
#define UDF_NAME_LEN		256
#define UDF_PATH_LEN		1023

#define CURRENT_UTIME	(xtime.tv_usec)

#define udf_file_entry_alloc_offset(inode)\
	(UDF_I_USE(inode) ?\
		sizeof(struct unallocSpaceEntry) :\
		((UDF_I_EFE(inode) ?\
			sizeof(struct extendedFileEntry) :\
			sizeof(struct fileEntry)) + UDF_I_LENEATTR(inode)))

#define udf_ext0_offset(inode)\
	(UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB ?\
		udf_file_entry_alloc_offset(inode) : 0)

#define udf_get_lb_pblock(sb,loc,offset) udf_get_pblock((sb), (loc).logicalBlockNum, (loc).partitionReferenceNum, (offset))

struct dentry;
struct inode;
struct task_struct;
struct buffer_head;
struct super_block;

extern struct inode_operations udf_dir_inode_operations;
extern struct inode_operations udf_file_inode_operations;
extern struct inode_operations udf_file_inode_operations_adinicb;
extern struct inode_operations udf_symlink_inode_operations;

struct udf_fileident_bh
{
	struct buffer_head *sbh;
	struct buffer_head *ebh;
	int soffset;
	int eoffset;
};

struct udf_vds_record
{
	uint32_t block;
	uint32_t volDescSeqNum;
};

struct generic_desc
{
	tag		descTag;
	uint32_t	volDescSeqNum;
};

struct ustr
{
	uint8_t u_cmpID;
	uint8_t u_name[UDF_NAME_LEN-2];
	uint8_t u_len;
};

/* super.c */
extern void udf_error(struct super_block *, const char *, const char *, ...);
extern void udf_warning(struct super_block *, const char *, const char *, ...);

/* namei.c */
extern int udf_write_fi(struct inode *, struct fileIdentDesc *, struct fileIdentDesc *, struct udf_fileident_bh *, uint8_t *, uint8_t *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,7)
extern int udf_lookup(struct inode *, struct dentry *);
#else
extern struct dentry * udf_lookup(struct inode *, struct dentry *);
#endif
extern int udf_create(struct inode *, struct dentry *, int);
extern int udf_mknod(struct inode *, struct dentry *, int, int);
extern int udf_mkdir(struct inode *, struct dentry *, int);
extern int udf_rmdir(struct inode *, struct dentry *);
extern int udf_unlink(struct inode *, struct dentry *);
extern int udf_symlink(struct inode *, struct dentry *, const char *);
extern int udf_link(struct dentry *, struct inode *, struct dentry *);
extern int udf_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);

/* file.c */
extern int udf_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

/* inode.c */
extern struct inode *udf_iget(struct super_block *, lb_addr);
extern int udf_sync_inode(struct inode *);
extern void udf_expand_file_adinicb(struct inode *, int, int *);
extern struct buffer_head * udf_expand_dir_adinicb(struct inode *, int *, int *);
extern struct buffer_head * udf_getblk(struct inode *, long, int, int *);
extern struct buffer_head * udf_bread(struct inode *, int, int, int *);
extern void udf_truncate(struct inode *);
extern void udf_read_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern void udf_clear_inode(struct inode *);
extern void udf_write_inode(struct inode *);
extern int8_t inode_bmap(struct inode *, int, lb_addr *, uint32_t *, lb_addr *, uint32_t *, uint32_t *, struct buffer_head **);
extern int udf_bmap(struct inode *, int);
extern int8_t udf_add_aext(struct inode *, lb_addr *, int *, lb_addr, uint32_t, struct buffer_head **, int);
extern int8_t udf_write_aext(struct inode *, lb_addr, int *, lb_addr, uint32_t, struct buffer_head *, int);
extern int8_t udf_insert_aext(struct inode *, lb_addr, int, lb_addr, uint32_t, struct buffer_head *);
extern int8_t udf_delete_aext(struct inode *, lb_addr, int, lb_addr, uint32_t, struct buffer_head *);
extern int8_t udf_next_aext(struct inode *, lb_addr *, int *, lb_addr *, uint32_t *, struct buffer_head **, int);
extern int8_t udf_current_aext(struct inode *, lb_addr *, int *, lb_addr *, uint32_t *, struct buffer_head **, int);

/* misc.c */
extern int udf_read_tagged_data(char *, int size, int fd, int block, int partref);
extern struct buffer_head *udf_tgetblk(struct super_block *, int);
extern struct buffer_head *udf_tread(struct super_block *, int);
extern struct genericFormat *udf_add_extendedattr(struct inode *, uint32_t, uint32_t, uint8_t);
extern struct genericFormat *udf_get_extendedattr(struct inode *, uint32_t, uint8_t);
extern struct buffer_head *udf_read_tagged(struct super_block *, uint32_t, uint32_t, uint16_t *);
extern struct buffer_head *udf_read_ptagged(struct super_block *, lb_addr, uint32_t, uint16_t *);
extern void udf_release_data(struct buffer_head *);
extern uid_t  udf_convert_uid(int);
extern gid_t  udf_convert_gid(int);
extern void udf_update_tag(char *, int);
extern void udf_new_tag(char *, uint16_t, uint16_t, uint16_t, uint32_t, int);

/* lowlevel.c */
extern unsigned int udf_get_last_session(struct super_block *);
extern unsigned long udf_get_last_block(struct super_block *);

/* partition.c */
extern uint32_t udf_get_pblock(struct super_block *, uint32_t, uint16_t, uint32_t);
extern uint32_t udf_get_pblock_virt15(struct super_block *, uint32_t, uint16_t, uint32_t);
extern uint32_t udf_get_pblock_virt20(struct super_block *, uint32_t, uint16_t, uint32_t);
extern uint32_t udf_get_pblock_spar15(struct super_block *, uint32_t, uint16_t, uint32_t);
extern int udf_relocate_blocks(struct super_block *, long, long *);

/* unicode.c */
extern int udf_get_filename(struct super_block *, uint8_t *, uint8_t *, int);
extern int udf_put_filename(struct super_block *, const uint8_t *, uint8_t *, int);
extern int udf_build_ustr(struct ustr *, dstring *, int);
extern int udf_CS0toUTF8(struct ustr *, struct ustr *);

/* ialloc.c */
extern void udf_free_inode(struct inode *);
extern struct inode * udf_new_inode (const struct inode *, int, int *);

/* truncate.c */
extern void udf_discard_prealloc(struct inode *);
extern void udf_truncate_extents(struct inode *);

/* balloc.c */
extern void udf_free_blocks(struct super_block *, const struct inode *, lb_addr, uint32_t, uint32_t);
extern int udf_prealloc_blocks(struct super_block *, const struct inode *, uint16_t, uint32_t, uint32_t);
extern int udf_new_block(struct super_block *, const struct inode *, uint16_t, uint32_t, int *);

/* fsync.c */
extern int udf_sync_file(struct file *, struct dentry *);
extern int udf_sync_file_adinicb(struct file *, struct dentry *);

/* directory.c */
extern uint8_t * udf_filead_read(struct inode *, uint8_t *, uint8_t, lb_addr, int *, int *, struct buffer_head **, int *);
extern struct fileIdentDesc * udf_fileident_read(struct inode *, loff_t *, struct udf_fileident_bh *, struct fileIdentDesc *, lb_addr *, uint32_t *, lb_addr *, uint32_t *, uint32_t *, struct buffer_head **);
extern struct fileIdentDesc * udf_get_fileident(void * buffer, int bufsize, int * offset);
extern extent_ad * udf_get_fileextent(void * buffer, int bufsize, int * offset);
extern long_ad * udf_get_filelongad(uint8_t *, int, int *, int);
extern short_ad * udf_get_fileshortad(uint8_t *, int, int *, int);
extern uint8_t * udf_get_filead(struct fileEntry *, uint8_t *, int, int, int, int *);

/* crc.c */
extern uint16_t udf_crc(uint8_t *, uint32_t, uint16_t);

/* udftime.c */
extern time_t *udf_stamp_to_time(time_t *, long *, timestamp);
extern timestamp *udf_time_to_stamp(timestamp *, time_t, long);

static inline struct buffer_head * sb_bread(struct super_block *sb, int block)
{
	return bread(sb->s_dev, block, sb->s_blocksize);
}
static inline struct buffer_head * sb_getblk(struct super_block *sb, int block)
{
	return getblk(sb->s_dev, block, sb->s_blocksize);
}

#endif /* __UDF_DECL_H */
