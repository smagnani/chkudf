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
O_OBJS   := crc.o inode.o misc.o super.o unicode.o debug.o
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
    UDFINC=../../include/linux/udf_fs.h

    include ../../.config

    TOPDIR = ../..
  endif
else
  CC		= gcc
  CFLAGS	= -DDEBUG -D__KERNEL__=1 \
	-O2 -Wall -Wstrict-prototypes -I.. -fomit-frame-pointer 
  MODFLAGS	= -DMODULE 
  CONFIG=../config/udf.h
  UDFINC=../linux/udf_fs.h
  LD		= ld
  LD_RFLAGS	=
  TOPDIR 	= .
endif

all: $(O_TARGET) 

clean:
	/bin/rm -f *.o $(O_TARGET)

# file.c lookup.c and vat.c aren't intergrated in yet, as of 0.2
newobj: file.o lookup.o vat.o

udf.o:  crc.o inode.o misc.o super.o unicode.o debug.o
	$(LD) $(LD_RFLAGS) -r -o $@ crc.o inode.o misc.o super.o unicode.o debug.o

crc.o:  crc.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c crc.c -o $@

inode.o:  inode.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c inode.c -o $@

misc.o:  misc.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c misc.c -o $@

super.o:  super.c $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c super.c -o $@

unicode.o:  unicode.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c unicode.c -o $@

file.o:  file.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c file.c -o $@

lookup.o:  lookup.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c lookup.c -o $@

vat.o:  vat.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c vat.c -o $@

debug.o:  debug.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c debug.c -o $@


