#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# libudf.a
#
# 10/2/98 dgb: 	pulling some functions out for dual kernel/usermode use
#

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
  CFLAGS	= -DDEBUG \
	-O2 -Wall -Wstrict-prototypes -I.. -fomit-frame-pointer 
  MODFLAGS	= -D__KERNEL__=1 -DMODULE
  CONFIG=../config/udf.h
  UDFINC=../linux/udf_fs.h
  LD		= ld
  LD_RFLAGS	=
  TOPDIR 	= .
endif

KLIBUDF=libkudf.a
KLIBUDFO=kcrc.o kunicode.o kudftime.o kdebug.o kmisc.o kdirectory.o
LIBUDF=libudf.a
LIBUDFO=crc.o unicode.o udftime.o debug.o misc.o directory.o

all: $(LIBUDF) $(KLIBUDF)

clean:
	/bin/rm -f *.o $(LIBUDF) $(KLIBUDF)

$(KLIBUDF): $(KLIBUDFO)
	ar rcv $@ $(KLIBUDFO)

$(LIBUDF): $(LIBUDFO)
	ar rcv $@ $(LIBUDFO)

crc.o:  crc.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) -c crc.c -o $@

kcrc.o:  crc.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c crc.c -o $@

misc.o:  misc.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) -c misc.c -o $@

kmisc.o:  misc.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c misc.c -o $@

directory.o:  directory.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) -c directory.c -o $@

kdirectory.o:  directory.c $(CONFIG) $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c directory.c -o $@

unicode.o:  unicode.c  $(UDFINC)
	$(CC) $(CFLAGS) -c unicode.c -o $@

kunicode.o:  unicode.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c unicode.c -o $@

udftime.o:  udftime.c  $(UDFINC)
	$(CC) $(CFLAGS) -c udftime.c -o $@

kudftime.o:  udftime.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c udftime.c -o $@

debug.o:  debug.c  $(UDFINC)
	$(CC) $(CFLAGS) -c debug.c -o $@

kdebug.o:  debug.c  $(UDFINC)
	$(CC) $(CFLAGS) $(MODFLAGS) -c debug.c -o $@

