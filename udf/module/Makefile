#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
# 11/29/98      more rearranging
#
include ../config.mk

MODDIR	= $(PREFIX)/lib/modules/$(UTS_RELEASE)

ifndef CFLAGS
KFLAGS	= -O2 -Wall -Wstrict-prototypes -pipe
endif

CPPFLAGS= $(PCDEBUG) -D__KERNEL__ -DMODULE -I../include\
		-I. -I$(LINUX)/include $(SMPFLAG) $(WRITEFLAG) -DCONFIG_UDF_FS_EXT
COFLAGS	= -kv

CFLAGS	= $(KFLAGS) $(MFLAG)

SRCS	:= balloc.c dir.c file.c ialloc.c inode.c lowlevel.c namei.c
SRCS	+= partition.c super.c truncate.c symlink.c fsync.c
SRCS	+= crc.c directory.c misc.c udftime.c unicode.c

OBJS	= $(SRCS:%.c=%.o)

MODULES	= udf.o

all:	../.prereq.ok kcheck $(MODULES)

showvars:
	@echo CFLAGS $(CFLAGS)
	@echo CPPFLAGS $(CPPFLAGS)
	@echo MODDIR $(MODDIR)
	@echo UTS_RELEASE $(UTS_RELEASE)
	@echo SRCS $(SRCS)
	@echo OBJS $(OBJS)

clean:
	rm -f core core.* *.o .*.o *.s *.a *~ .depfiles/*.d
	-rmdir .depfiles

install: $(MODULES) ../.prereq.ok kcheck
	-mkdir -p $(MODDIR)/fs
	install -o root -g root -m 644 $(MODULES) $(MODDIR)/fs

# Stuff to automatically maintain dependency files

%.o:	../linux-$(UTS_SHORT)/%.c
	$(CC) -MD $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@mkdir -p .depfiles ; mv -f $*.d .depfiles

kcheck:
	@. ../config.out ; \
	if [ "##CHECK" != "" ] ; then \
		if [ "`cksum < $$CHECK`" != "$$CKSUM" ] ; then \
			/bin/echo -n "Kernel configuration has changed." ; \
			/bin/echo " Please re-run 'make config'." ; \
			exit 1 ; \
		fi ; \
	fi

udf.o: $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ { print $$4}'`" -r -o $@ $(OBJS)
	@echo Done.

-include $(SRCS:%.c=.depfiles/%.d)
