diff -u --recursive --new-file v2.1.129/linux/Documentation/Configure.help linux/Documentation/Configure.help
--- v2.1.129/linux/Documentation/Configure.help	Sun Nov 29 19:13:47 1998
+++ linux/Documentation/Configure.help	Sun Nov 29 16:58:13 1998
@@ -6838,6 +6838,18 @@
 
   If unsure, say N.
 
+UDF filesystem support (EXPERIMENTAL)
+CONFIG_UDF_FS  
+  This is the new filesystem used by some CDROMS and DVD drivers. 
+  Say Y if you intend to mount DVD discs or CDRWs written in packet mode.
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
