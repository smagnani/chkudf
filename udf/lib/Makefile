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
CFLAGS	= $(KERNH)\
	-O2 -Wall -Wstrict-prototypes -I$(UDF_INC) -g
UDF_CONFIG=$(UDF_INC)/config/udf.h
LD		= ld
LD_RFLAGS	=

%.o:	../src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

LIBUDF=libudf.a
LIBUDFO=crc.o unicode.o udftime.o misc.o directory.o

all: $(LIBUDF) 

clean:
	@-/bin/rm -f *.o $(LIBUDF)  *~ *.bak
	@echo "lib cleaned."

$(LIBUDF): $(LIBUDFO)
	ar rcv $@ $(LIBUDFO)

