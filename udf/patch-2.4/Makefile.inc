diff -u --recursive --new-file linux.old/linux/fs/Makefile linux/fs/Makefile
--- linux.old/linux/fs/Makefile	Wed Aug 18 23:27:39 1999
+++ linux/fs/Makefile	Wed Aug 18 23:21:52 1999
@@ -18,7 +18,7 @@
 MOD_LIST_NAME := FS_MODULES
 ALL_SUB_DIRS = coda minix ext2 fat msdos vfat proc isofs nfs umsdos ntfs \
 		hpfs sysv smbfs ncpfs ufs efs affs romfs autofs hfs lockd \
-		nfsd nls devpts adfs partitions qnx4
+		nfsd nls devpts adfs partitions qnx4 udf
 
 SUB_DIRS := partitions
 
@@ -222,6 +222,14 @@
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
 
