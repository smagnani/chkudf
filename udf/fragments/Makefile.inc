ifeq ($(CONFIG_UDF_FS),y)
SUB_DIRS += udf
else
  ifeq ($(CONFIG_UDF_FS),m)
  MOD_SUB_DIRS += udf
  endif
endif
 
