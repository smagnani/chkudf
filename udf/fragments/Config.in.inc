tristate 'UDF filesystem support' CONFIG_UDF_FS
if [ "$CONFIG_UDF_FS" != "n" ]; then
  bool '   UDF filesystem write support (DANGEROUS)' CONFIG_UDF_FS_WRITE
fi
