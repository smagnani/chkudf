#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := udf.o
O_OBJS   := crc.o inode.o misc.o super.o unicode.o
M_OBJS   := $(O_TARGET)

.SUFFIXES:
.SUFFIXES: .c .o .h .a

IN_KERNEL=n

ifeq (${IN_KERNEL},y)
  ifeq (${CFLAGS},)
    CFLAGS = -D__KERNEL__=1 \
	-DMODULE -Wall -Wstrict-prototypes -I. -I../../include \
	-O2 -fomit-frame-pointer

    include ../../.config

    TOPDIR = ../..
  endif
else
  CC		= gcc
  CFLAGS	= -DDEBUG -D__KERNEL__=1 \
	-O2 -Wall -Wstrict-prototypes -I.. -fomit-frame-pointer 
  MODFLAGS	= -DMODULE 
  LD		= ld
  LD_RFLAGS	=
  TOPDIR 	= .
endif

#all: $(O_TARGET)

udf.o:  crc.o inode.o misc.o super.o unicode.o
	$(LD) $(LD_RFLAGS) -r -o $@ crc.o inode.o misc.o super.o unicode.o

crc.o:  crc.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c crc.c -o $@

inode.o:  inode.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c inode.c -o $@

misc.o:  misc.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c misc.c -o $@

super.o:  super.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c super.c -o $@

unicode.o:  unicode.c 
	$(CC) $(CFLAGS) $(MODFLAGS) -c unicode.c -o $@


