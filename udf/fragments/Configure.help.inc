UDF Filesystem support (EXPERIMENTAL)
CONFIG_UDF_FS
  This is the new file system used on some CDROMs and DVDs. Say Y if
  you intend to mount DVD discs or CDRW's written in packet mode, or
  if written to by other UDF utilities, such as DirectCD. This UDF
  file system support is read-only. If you want to write to UDF
  file systems on some media, you need to say Y to "UDF read-write
  support" below in addition. Please read
  Documentation/filesystems/udf.txt.

  This file system support is also available as a module ( = code
  which can be inserted in and removed from the running kernel
  whenever you want). The module is called udf.o. If you want to
  compile it as a module, say M here and read
  Documentation/modules.txt.

  If unsure, say N.

UDF read-write support (DANGEROUS)
CONFIG_UDF_FS_WRITE
  Say Y if you want to test write support for UDF filesystems.
  Due to lack of support for writing to CDR/CDRW's, this option
  is only supported for Hard Discs, DVD-RAM, and loopback files.

