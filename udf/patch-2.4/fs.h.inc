diff -u --recursive --new-file linux.old/linux/include/linux/fs.h linux/include/linux/fs.h
--- linux.old/linux/include/linux/fs.h	Mon Aug  9 12:42:06 1999
+++ linux/include/linux/fs.h	Tue Aug 10 12:43:52 1999
@@ -269,6 +269,7 @@
 #include <linux/hfs_fs_i.h>
 #include <linux/adfs_fs_i.h>
 #include <linux/qnx4_fs_i.h>
+#include <linux/udf_fs_i.h>
 
 /*
  * Attribute flags.  These should be or-ed together to figure out what
@@ -381,6 +382,7 @@
 		struct hfs_inode_info		hfs_i;
 		struct adfs_inode_info		adfs_i;
 		struct qnx4_inode_info		qnx4_i;
+		struct udf_inode_info		udf_i;
 		struct socket			socket_i;
 		void				*generic_ip;
 	} u;
@@ -510,6 +512,7 @@
 #include <linux/hfs_fs_sb.h>
 #include <linux/adfs_fs_sb.h>
 #include <linux/qnx4_fs_sb.h>
+#include <linux/udf_fs_sb.h>
 
 extern struct list_head super_blocks;
 
@@ -554,6 +557,7 @@
 		struct hfs_sb_info	hfs_sb;
 		struct adfs_sb_info	adfs_sb;
 		struct qnx4_sb_info	qnx4_sb;
+		struct udf_sb_info	udf_sb;
 		void			*generic_sbp;
 	} u;
 	/*
