#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# libudf.a
#
# 10/2/98 dgb: 	pulling some functions out for dual kernel/usermode use
# 11/29/98      re-arranged
#

# uncomment and provide correct directory if /usr/src/linux/include isn't it.
# KERNH=-I/usr/src/linux-2.1.129/include

CC		= gcc
UDF_INC		= ../include
CFLAGS	= -DDEBUG $(KERNH)\
	-O2 -Wall -Wstrict-prototypes -I$(UDF_INC) -g
UDF_CONFIG=$(UDF_INC)/config/udf.h
LD		= ld
LD_RFLAGS	=

%.o:	../src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

LIBUDF=libudf.a
LIBUDFO=crc.o unicode.o udftime.o debug.o misc.o directory.o

all: $(LIBUDF) 

clean:
	/bin/rm -f *.o $(LIBUDF) 

$(LIBUDF): $(LIBUDFO)
	ar rcv $@ $(LIBUDFO)

#crc.o:  crc.c $(UDF_CONFIG) $(UDF_INC)
#	$(CC) $(CFLAGS) -c crc.c -o $@
#
#misc.o:  misc.c $(UDF_CONFIG) $(UDF_INC)
#	$(CC) $(CFLAGS) -c misc.c -o $@
#
#directory.o:  directory.c $(UDF_CONFIG) $(UDF_INC)
#	$(CC) $(CFLAGS) -c directory.c -o $@
#
#getopt.o:	getopt.c $(UDF_CONFIG) $(UDF_INC)
#	$(CC) $(CFLAGS) -c getopt.c -o $@
#
#unicode.o:  unicode.c  $(UDF_INC)
#	$(CC) $(CFLAGS) -c unicode.c -o $@
#
#udftime.o:  udftime.c  $(UDF_INC)
#	$(CC) $(CFLAGS) -c udftime.c -o $@
#
#debug.o:  debug.c  $(UDF_INC)
#	$(CC) $(CFLAGS) -c debug.c -o $@
#
