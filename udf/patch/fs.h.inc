diff -u --recursive --new-file v2.1.129/linux/include/linux/fs.h linux/include/linux/fs.h
--- v2.1.129/linux/include/linux/fs.h	Sun Nov 29 19:13:42 1998
+++ linux/include/linux/fs.h	Sun Nov 29 16:54:24 1998
@@ -281,6 +281,7 @@
 #include <linux/hfs_fs_i.h>
 #include <linux/adfs_fs_i.h>
 #include <linux/qnx4_fs_i.h>
+#include <linux/udf_fs_i.h>
 
 /*
  * Attribute flags.  These should be or-ed together to figure out what
@@ -392,6 +393,7 @@
 		struct hfs_inode_info		hfs_i;
 		struct adfs_inode_info		adfs_i;
 		struct qnx4_inode_info		qnx4_i;	   
+		struct udf_inode_info		udf_i;
 		struct socket			socket_i;
 		void				*generic_ip;
 	} u;
@@ -513,6 +515,7 @@
 #include <linux/hfs_fs_sb.h>
 #include <linux/adfs_fs_sb.h>
 #include <linux/qnx4_fs_sb.h>
+#include <linux/udf_fs_sb.h>
 
 extern struct list_head super_blocks;
 
@@ -555,6 +558,7 @@
 		struct hfs_sb_info	hfs_sb;
 		struct adfs_sb_info	adfs_sb;
 		struct qnx4_sb_info	qnx4_sb;	   
+		struct udf_sb_info	udf_sb;
 		void			*generic_sbp;
 	} u;
 };
