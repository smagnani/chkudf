diff -u --recursive --new-file v2.1.129/linux/fs/Config.in linux/fs/Config.in
--- v2.1.129/linux/fs/Config.in	Sun Nov 29 19:13:55 1998
+++ linux/fs/Config.in	Sun Nov 29 14:45:28 1998
@@ -82,6 +82,7 @@
   if [ "$CONFIG_QNX4FS_FS" != "n" ]; then
     bool '   QNXFS read-write support (FOR TESTING ONLY)' CONFIG_QNX4FS_RW
   fi    
+  tristate 'UDF filesystem support (read only) (EXPERIMENTAL)' CONFIG_UDF_FS
 fi
 bool 'Macintosh partition map support' CONFIG_MAC_PARTITION
 endmenu
