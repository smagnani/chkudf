--- linux-2.2.5/include/linux/fs.h	Fri Mar 12 15:51:45 1999
+++ linux/include/linux/fs.h	Fri Mar 12 16:02:54 1999
@@ -281,6 +281,7 @@
 #include <linux/hfs_fs_i.h>
 #include <linux/adfs_fs_i.h>
 #include <linux/qnx4_fs_i.h>
+#include <linux/udf_fs_i.h>
 
 /*
  * Attribute flags.  These should be or-ed together to figure out what
@@ -393,6 +394,7 @@
 		struct hfs_inode_info		hfs_i;
 		struct adfs_inode_info		adfs_i;
 		struct qnx4_inode_info		qnx4_i;	   
+		struct udf_inode_info		udf_i;
 		struct socket			socket_i;
 		void				*generic_ip;
 	} u;
@@ -516,6 +518,7 @@
 #include <linux/hfs_fs_sb.h>
 #include <linux/adfs_fs_sb.h>
 #include <linux/qnx4_fs_sb.h>
+#include <linux/udf_fs_sb.h>
 
 extern struct list_head super_blocks;
 
@@ -558,6 +561,7 @@
 		struct hfs_sb_info	hfs_sb;
 		struct adfs_sb_info	adfs_sb;
 		struct qnx4_sb_info	qnx4_sb;	   
+		struct udf_sb_info	udf_sb;
 		void			*generic_sbp;
 	} u;
 };
