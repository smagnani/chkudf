diff -u --recursive --new-file v2.2.1/linux/Documentation/Configure.help linux/Documentation/Configure.help
--- v2.2.1/linux/Documentation/Configure.help	Tue Jan 19 14:07:53 1999
+++ linux/Documentation/Configure.help	Wed Feb 10 14:45:28 1999
@@ -7016,6 +7016,19 @@
 
   If unsure, say N.
 
+UDF filesystem support (EXPERIMENTAL)
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
 Kernel automounter support
 CONFIG_AUTOFS_FS
   The automounter is a tool to automatically mount remote filesystems
