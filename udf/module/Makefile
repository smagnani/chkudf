#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
# 11/29/98      more rearranging
#

#
# uncomment below if your /usr/include/linux isn't a symlink to 
# the kernel source
#
# KERNH		=-I/usr/src/v2.1.129/linux/include 
#
UDF_INC		= ../include
UDF_CONFIG	= $(UDF_INC)/config/udf.h


CFLAGS		= -DDEBUG -D__KERNEL__=1 $(KERNH)\
	-DMODULE -DBF_CHANGES -O2 -Wall -Wstrict-prototypes -I$(UDF_INC) -g
#
# ---------------------------------------------------------
# you shouldn't need to change (much) below this
#

CC		= gcc
LD		= ld
LD_RFLAGS	=

O_TARGET := udf.o

all: $(O_TARGET) 

UDFH    = $(INCDIR)/udf_fs.h  $(INCDIR)/udf_udf.h $(INCDIR)/udf_167.h
UDFH   += $(INCDIR)/udf_fs_sb.h $(INCDIR)/udf_fs_i.h udfdecl.h

O_OBJS   := inode.o super.o dir.o crc.o debug.o directory.o file.o
O_OBJS   += udftime.o unicode.o misc.o

%.o:  	../src/%.c 
	$(CC) -c $(CFLAGS) -o $@ $<

udf.o:  $(O_OBJS) 
	$(LD) $(LD_RFLAGS) -r -o $@ $(O_OBJS)

clean:
	/bin/rm -f *.o $(O_TARGET) $(L_TARGET)

# lookup.c and vat.c aren't intergrated in yet, as of 0.2
newobj: vat.o lookup.o

#super.o:  super.c $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c super.c -o $@
#
#file.o:  file.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c file.c -o $@
#
#dir.o:  dir.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c dir.c -o $@
#
#name.o:	name.c $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c namei.c -o $@
#
#dir.S:  dir.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c dir.c -S -o $@
#
#lookup.o:  lookup.c $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c lookup.c -o $@
#
#vat.o:  vat.c 
#	$(CC) $(CFLAGS) $(MODFLAGS) -c vat.c -o $@
#
#
#$(KLIBUDF): $(KLIBUDFO)
#	ar rcv $@ $(KLIBUDFO)
#
#kcrc.o:  crc.c $(CONFIG) $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c crc.c -o $@
#
#kmisc.o:  misc.c $(CONFIG) $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c misc.c -o $@
#
#kdirectory.o:  directory.c $(CONFIG) $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c directory.c -o $@
#
#kunicode.o:  unicode.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c unicode.c -o $@
#
#kudftime.o:  udftime.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c udftime.c -o $@
#
#kdebug.o:  debug.c  $(UDFINC)
#	$(CC) $(CFLAGS) $(MODFLAGS) -c debug.c -o $@
#
