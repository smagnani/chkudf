diff -u --recursive --new-file linux-2.2.9/fs/filesystems.c linux/fs/filesystems.c
--- linux-2.2.9/fs/filesystems.c	Mon Apr 19 15:50:38 1999
+++ linux/fs/filesystems.c	Mon Apr 19 15:59:56 1999
@@ -25,6 +25,7 @@
 #include <linux/romfs_fs.h>
 #include <linux/auto_fs.h>
 #include <linux/qnx4_fs.h>
+#include <linux/udf_fs.h>
 #include <linux/ntfs_fs.h>
 #include <linux/hfs_fs.h>
 #include <linux/devpts_fs.h>
@@ -143,6 +144,10 @@
 
 #ifdef CONFIG_QNX4FS_FS
 	init_qnx4_fs();
+#endif
+
+#ifdef CONFIG_UDF_FS
+	init_udf_fs();
 #endif
    
 #ifdef CONFIG_NLS
