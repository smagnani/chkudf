#ifndef __UDF_DECL_H
#define __UDF_DECL_H


extern struct file_operations udf_file_fops;
extern struct file_operations udf_dir_fops;
extern struct inode_operations udf_dir_inode_operations;
extern struct inode_operations udf_file_inode_operations;

extern int udf_physical_lookup(struct inode *, struct dentry *);
extern int udf_lookup(struct inode *, struct dentry *);

extern void udf_read_inode(struct inode *);
extern void udf_put_inode(struct inode *);
extern void udf_delete_inode(struct inode *);
extern void udf_write_inode(struct inode *);
extern struct inode *udf_iget(struct super_block *, unsigned long);

#endif
