diff -u --recursive --new-file v2.1.129/linux/fs/filesystems.c linux/fs/filesystems.c
--- v2.1.129/linux/fs/filesystems.c	Sun Sep  6 15:55:38 1998
+++ linux/fs/filesystems.c	Sun Nov 29 14:45:28 1998
@@ -25,6 +25,7 @@
 #include <linux/romfs_fs.h>
 #include <linux/auto_fs.h>
 #include <linux/qnx4_fs.h>
+#include <linux/udf_fs.h>
 #include <linux/ntfs_fs.h>
 #include <linux/hfs_fs.h>
 #include <linux/devpts_fs.h>
@@ -145,6 +146,10 @@
 	init_qnx4_fs();
 #endif
    
+#ifdef CONFIG_UDF_FS
+	init_udf_fs();
+#endif
+
 #ifdef CONFIG_NLS
 	init_nls();
 #endif
