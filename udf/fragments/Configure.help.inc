UDF File System support (read only)
CONFIG_UDF_FS
  This is the new file system used on some CDROMs and DVDs. Say Y if
  you intend to mount DVD discs or CDRW's written in packet mode, or
  if written to by other UDF utilities, such as DirectCD.
  Please read Documentation/filesystems/udf.txt.

  This file system support is also available as a module ( = code
  which can be inserted in and removed from the running kernel
  whenever you want). The module is called udf.o. If you want to
  compile it as a module, say M here and read
  Documentation/modules.txt.

  If unsure, say N.
