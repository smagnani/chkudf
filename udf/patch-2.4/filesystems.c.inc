diff -u --recursive --new-file linux.old/linux/fs/filesystems.c linux/fs/filesystems.c
--- linux.old/linux/fs/filesystems.c	Thu May 13 23:50:15 1999
+++ linux/fs/filesystems.c	Tue Aug 10 12:37:45 1999
@@ -26,6 +26,7 @@
 #include <linux/romfs_fs.h>
 #include <linux/auto_fs.h>
 #include <linux/qnx4_fs.h>
+#include <linux/udf_fs.h>
 #include <linux/ntfs_fs.h>
 #include <linux/hfs_fs.h>
 #include <linux/devpts_fs.h>
@@ -148,6 +149,10 @@
 
 #ifdef CONFIG_QNX4FS_FS
 	init_qnx4_fs();
+#endif
+
+#ifdef CONFIG_UDF_FS
+	init_udf_fs();
 #endif
    
 #ifdef CONFIG_NLS
