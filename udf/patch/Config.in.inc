diff -u --recursive --new-file v2.2.1/linux/fs/Config.in linux/fs/Config.in
--- v2.2.1/linux/fs/Config.in	Tue Jan 19 14:07:53 1999
+++ linux/fs/Config.in	Wed Feb 10 14:45:28 1999
@@ -46,6 +46,7 @@
   if [ "$CONFIG_QNX4FS_FS" != "n" ]; then
     bool '   QNXFS read-write support (FOR TESTING ONLY)' CONFIG_QNX4FS_RW
   fi    
+  tristate 'UDF filesystem support (read only) (EXPERIMENTAL)' CONFIG_UDF_FS
 fi
 tristate 'ROM filesystem support' CONFIG_ROMFS_FS
 tristate 'Second extended fs support' CONFIG_EXT2_FS
