UDF Filesystem support (EXPERIMENTAL)
CONFIG_UDF_FS
  This is the new filesystem used by some CDROMS and DVD drivers. 
  Say Y if you intend to mount DVD discs or CDRWs written in packet mode,
  or if written to by other UDF utilities, such as DirectCD.

  This filesystem support is also available as a module ( = code which
  can be inserted in and removed from the running kernel whenever you
  want). The module is called udf.o. If you want to compile it as a
  module, say M here and read Documentation/modules.txt. 

  If unsure, say N.

UDF read-write support (FOR TESTING ONLY)
CONFIG_UDF_RW
  Say Y if you want to test write support for UDF filesystems.
  Due to lack of support for writing to CDR/CDRW's, this option
  is only supported for Hard Discs, DVD-RAM, and loopback files.

