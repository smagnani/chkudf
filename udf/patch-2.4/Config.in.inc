diff -u --recursive --new-file linux.old/linux/fs/Config.in linux/fs/Config.in
--- linux.old/linux/fs/Config.in	Wed Aug 18 23:27:39 1999
+++ linux/fs/Config.in	Wed Aug 18 23:20:35 1999
@@ -31,6 +31,11 @@
   define_bool CONFIG_JOLIET n
 fi
 
+tristate 'UDF filesystem support' CONFIG_UDF_FS
+if [ "$CONFIG_UDF_FS" != "n" -a "$CONFIG_EXPERIMENTAL" = "y" ]; then
+  bool '   UDF read-write support (EXPERIMENTAL)' CONFIG_UDF_RW
+fi
+
 tristate 'Minix fs support' CONFIG_MINIX_FS
 tristate 'NTFS filesystem support (read only)' CONFIG_NTFS_FS
 if [ "$CONFIG_NTFS_FS" != "n" -a "$CONFIG_EXPERIMENTAL" = "y" ]; then
