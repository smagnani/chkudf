diff -u --recursive --new-file linux.old/linux/Documentation/Configure.help linux/Documentation/Configure.help
--- linux.old/linux/Documentation/Configure.help	Wed Aug 18 23:27:15 1999
+++ linux/Documentation/Configure.help	Wed Aug 18 23:20:11 1999
@@ -7352,6 +7352,25 @@
   like lynx or netscape). Say Y here if you want to be able to read
   Joliet CDROMs under Linux.
 
+UDF Filesystem support
+CONFIG_UDF_FS
+  This is the new filesystem used by some CDROMS and DVD drivers. 
+  Say Y if you intend to mount DVD discs or CDRWs written in packet mode,
+  or if written to by other UDF utilities, such as DirectCD.
+
+  This filesystem support is also available as a module ( = code which
+  can be inserted in and removed from the running kernel whenever you
+  want). The module is called udf.o. If you want to compile it as a
+  module, say M here and read Documentation/modules.txt. 
+
+  If unsure, say N.
+
+UDF read-write support (EXPERIMENTAL)
+CONFIG_UDF_RW
+  Say Y if you want to test write support for UDF filesystems.
+  Due to lack of support for writing to CDR/CDRW's, this option
+  is only supported for Hard Discs, DVD-RAM, and loopback files.
+
 fat fs support
 CONFIG_FAT_FS
   If you want to use one of the FAT-based filesystems (the MS-DOS,
