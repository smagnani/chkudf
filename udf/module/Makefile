#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_TARGET := udf.o
O_OBJS   := inode.o super.o 
M_OBJS   := $(O_TARGET)

.SUFFIXES:
.SUFFIXES: .c .o .h .a

IN_KERNEL=n

ifeq (${IN_KERNEL},y)
  ifeq (${CFLAGS},)
    CFLAGS = -D__KERNEL__=1 \
	-DMODULE -Wall -Wstrict-prototypes -I. -I../../include \
	-O2 -fomit-frame-pointer
    CONFIG=../../include/config/udf.h
    INCDIR=../../include/linux
    LIBKUDF=libkudf.a
    include ../../.config

    TOPDIR = ../..
  endif
else
  CC		= gcc
  CFLAGS	= -DDEBUG -D__KERNEL__=1 \
	-O2 -Wall -Wstrict-prototypes -I.. -fomit-frame-pointer 
  MODFLAGS	= -DMODULE 
  LIBKUDF=../lib/libkudf.a
  CONFIG=../config/udf.h
  INCDIR=../linux
  LD		= ld
  LD_RFLAGS	=
  TOPDIR 	= .
endif

O_OBJS += $(LIBUDF)
UDFINC=$(INCDIR)/udf_fs.h $(INCDIR)/udf_fs_sb.h $(INCDIR)/udf_fs_i.h udfdecl.h

all: $(O_TARGET) 

clean:
	/bin/rm -f *.o $(O_TARGET)

UDFOBJ= inode.o super.o file.o dir.o

# lookup.c and vat.c aren't intergrated in yet, as of 0.2
newobj: vat.o lookup.o

udf.o:  $(UDFOBJ) $(LIBKUDF)
	$(LD) $(LD_RFLAGS) -r -o $@ $(UDFOBJ) $(LIBKUDF)

inode.o:  inode.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c inode.c -o $@

super.o:  super.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c super.c -o $@

file.o:  file.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c file.c -o $@

dir.o:  dir.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c dir.c -o $@

dir.S:  dir.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c dir.c -S -o $@

lookup.o:  lookup.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c lookup.c -o $@

vat.o:  vat.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c vat.c -o $@


