--- linux-2.2.8/fs/Config.in	Mon Apr 19 15:51:44 1999
+++ linux/fs/Config.in	Mon Apr 19 15:57:29 1999
@@ -26,6 +26,12 @@
   # needed by nls/Config.in
   define_bool CONFIG_JOLIET n
 fi
+if [ "$CONFIG_EXPERIMENTAL" = "y" ]; then
+  tristate 'UDF filesystem support (EXPERIMENTAL)' CONFIG_UDF_FS
+  if [ "$CONFIG_UDF_FS" != "n" ]; then
+    bool '   UDF read-write support (FOR TESTING ONLY)' CONFIG_UDF_RW
+  fi
+fi
 
 tristate 'Minix fs support' CONFIG_MINIX_FS
 tristate 'NTFS filesystem support (read only)' CONFIG_NTFS_FS
