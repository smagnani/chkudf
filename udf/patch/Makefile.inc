diff -u --recursive --new-file v2.1.129/linux/fs/Makefile linux/fs/Makefile
--- v2.1.129/linux/fs/Makefile	Mon Aug 31 16:01:35 1998
+++ linux/fs/Makefile	Sun Nov 29 16:35:15 1998
@@ -18,7 +18,7 @@
 MOD_LIST_NAME := FS_MODULES
 ALL_SUB_DIRS = coda minix ext2 fat msdos vfat proc isofs nfs umsdos ntfs \
 		hpfs sysv smbfs ncpfs ufs affs romfs autofs hfs lockd nfsd \
-		nls devpts adfs qnx4
+		nls devpts adfs qnx4 udf
 
 ifeq ($(CONFIG_QUOTA),y)
 O_OBJS += dquot.o
@@ -212,6 +212,14 @@
 else
   ifeq ($(CONFIG_QNX4FS_FS),m)
   MOD_SUB_DIRS += qnx4
+  endif
+endif
+
+ifeq ($(CONFIG_UDF_FS),y)
+SUB_DIRS += udf
+else
+  ifeq ($(CONFIG_UDF_FS),m)
+  MOD_SUB_DIRS += udf
   endif
 endif
 
