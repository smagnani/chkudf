--- linux-2.2.5/fs/Makefile	Fri Mar 12 15:50:38 1999
+++ linux/fs/Makefile	Fri Mar 12 15:59:06 1999
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
 
