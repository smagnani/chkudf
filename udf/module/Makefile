#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
# 11/29/98      more rearranging
#
include ../Make.conf
#
# uncomment below if your /usr/include/linux isn't a symlink to 
# the kernel source
#
# KERNH		=-I/usr/src/v2.1.129/linux/include 
#
UDF_INC		= ../include
UDF_CONFIG	= $(UDF_INC)/config/udf.h
# CFLAGS_DEBUG = -DDEBUG -g

CFLAGS		= -D__KERNEL__=1 $(KERNH) -DUDF_COMPILING\
	-DMODULE -O2 -Wall -Wstrict-prototypes -I$(UDF_INC) 

CFLAGS += $(CFLAGS_DEBUG)
#
# comment out following if you add our include files to linux/fs.h!
#
CFLAGS += -DCONFIG_UDF_FS_EXT
#
# ---------------------------------------------------------
# you shouldn't need to change (much) below this
#

O_TARGET := udf.o

all: $(O_TARGET) 

install: $(O_TARGET)
	@echo "MDEST= $(MODULE_DEST)"
	$(INSTALL) $(O_TARGET) $(MODULE_DEST)

make.log: 
	make clean
	make all >make.log 2>&1

UDFH    = $(INCDIR)/udf_fs.h  $(INCDIR)/udf_udf.h $(INCDIR)/udf_167.h
UDFH   += $(INCDIR)/udf_fs_sb.h $(INCDIR)/udf_fs_i.h udfdecl.h udf_i.h udf_sb.h

O_OBJS   := inode.o super.o dir.o crc.o debug.o directory.o file.o
O_OBJS   += udftime.o unicode.o misc.o partition.o namei.o

%.S:  	../src/%.c 
	$(CC) -S -c $(CFLAGS) -o $@ $<

%.o:  	../src/%.c 
	$(CC) -c $(CFLAGS) -o $@ $<

udf.o:  $(O_OBJS) 
	@echo "* ------------------------------- *"
	@echo "* making UDF module               *"
	@echo "* ------------------------------- *"
	$(LD) $(LD_RFLAGS) -r -o $@ $(O_OBJS)

clean:
	@-/bin/rm -f *.o $(O_TARGET) $(L_TARGET) make.log *~ *.bak
	@echo "module cleaned."

