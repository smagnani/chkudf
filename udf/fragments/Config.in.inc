tristate 'UDF file system support (read only)' CONFIG_UDF_FS
if [ "$CONFIG_UDF_FS" != "n" ]; then
  bool '   UDF write support (DANGEROUS)' CONFIG_UDF_RW
fi
