--- linux-2.2.6/include/linux/fs.h	Mon Apr 19 15:51:45 1999
+++ linux/include/linux/fs.h	Mon Apr 19 16:02:54 1999
@@ -285,6 +285,7 @@
 #include <linux/hfs_fs_i.h>
 #include <linux/adfs_fs_i.h>
 #include <linux/qnx4_fs_i.h>
+#include <linux/udf_fs_i.h>
 
 /*
  * Attribute flags.  These should be or-ed together to figure out what
@@ -397,6 +398,7 @@
 		struct hfs_inode_info		hfs_i;
 		struct adfs_inode_info		adfs_i;
 		struct qnx4_inode_info		qnx4_i;	   
+		struct udf_inode_info		udf_i;
 		struct socket			socket_i;
 		void				*generic_ip;
 	} u;
@@ -520,6 +522,7 @@
 #include <linux/hfs_fs_sb.h>
 #include <linux/adfs_fs_sb.h>
 #include <linux/qnx4_fs_sb.h>
+#include <linux/udf_fs_sb.h>
 
 extern struct list_head super_blocks;
 
@@ -562,6 +565,7 @@
 		struct hfs_sb_info	hfs_sb;
 		struct adfs_sb_info	adfs_sb;
 		struct qnx4_sb_info	qnx4_sb;	   
+		struct udf_sb_info	udf_sb;
 		void			*generic_sbp;
 	} u;
	/*
