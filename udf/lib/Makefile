#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
# 11/29/98      more rearranging
#
include ../config.mk

ifndef CFLAGS
KFLAGS	= -O2 -Wall -Wstrict-prototypes -Winline -pipe
endif

CPPFLAGS= $(PCDEBUG) -I../include\
		-I. -I$(LINUX)/include $(SMPFLAG) $(WRITEFLAG) -DCONFIG_UDF_FS_EXT
COFLAGS	= -kv

CFLAGS	= $(KFLAGS) $(MFLAG)

SRCS	:= crc.c directory.c misc.c udftime.c unicode.c

OBJS	= $(SRCS:%.c=%.o)

LIBUDF	= libudf.a

all:	../.prereq.ok kcheck $(LIBUDF)

showvars:
	@echo CFLAGS $(CFLAGS)
	@echo CPPFLAGS $(CPPFLAGS)
	@echo MODDIR $(MODDIR)
	@echo UTS_RELEASE $(UTS_RELEASE)
	@echo SRCS $(SRCS)
	@echo OBJS $(OBJS)

clean:
	rm -f core core.* *.o .*.o *.s *.a *~ .depend .depfiles/*.d

# Stuff to automatically maintain dependency files

%.o:	../linux-$(UTS_SHORT)/%.c
	$(CC) -MD $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@mkdir -p .depfiles ; mv $*.d .depfiles

kcheck:
	@. ../config.out ; \
	if [ "##CHECK" != "" ] ; then \
		if [ "`cksum < $$CHECK`" != "$$CKSUM" ] ; then \
			/bin/echo -n "Kernel configuration has changed." ; \
			/bin/echo " Please re-run 'make config'." ; \
			exit 1 ; \
		fi ; \
	fi

$(LIBUDF): $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ { print $$4}'`" -r -o $@ $(OBJS)

-include $(SRCS:%.c=.depfiles/%.d)
