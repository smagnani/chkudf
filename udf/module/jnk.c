#include <linux/stat.h>

{
	switch (????) {

		case FILE_TYPE_REGULAR:
			inode->i_mode = S_IFREG;
			inode->i_op = &udf_file_iops;
			if (setuid)
				inode->mode |= S_ISUID;
			if (setgid)
				inode->mode |= S_ISGID;
			break;
		
		case FILE_TYPE_DIRECTORY:
			inode->i_mode = S_IFDIR;
			inode->i_op = &udf_dir_iops;
			if (setgid)
				inode->mode |= S_ISGID;
			break;
		
		case FILE_TYPE_SYMLINK:
			inode->i_mode = S_IFLNK;
			inode->i_op = &udf_symlink_iops;
			break;
		
		case FILE_TYPE_BLKDEV:
			inode->i_mode = S_IFBLK;
			inode->i_op = &blkdev_inode_operations;
			break;
		
		case FILE_TYPE_CHARDEV:
			inode->i_mode = S_IFCHR;
			inode->i_op = &chrdev_inode_operations;
			break;

		case FILE_TYPE_FIFO:
			init_fifo(inode);
			break;
	}

	return inode;
}
