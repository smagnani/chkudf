#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
# 11/29/98      more rearranging
#
include ../config.mk

ifeq ($(UTS_SHORT),2.2)
MODDIR	= $(PREFIX)/lib/modules/$(UTS_RELEASE)/fs
else
MODDIR	= $(PREFIX)/lib/modules/$(UTS_RELEASE)/kernel/fs/udf
endif

ifndef CFLAGS
KFLAGS	= -O2 -Wall -Wstrict-prototypes -pipe
endif

CPPFLAGS= $(PCDEBUG) -D__KERNEL__ -DMODULE -I../include\
		-I. -I$(LINUX)/include -I$(LINUX)/arch/$(ARCH)/mach-generic $(SMPFLAG) -DCONFIG_UDF_FS_EXT -DKBUILD_MODNAME=udf
COFLAGS	= -kv

CFLAGS	= $(KFLAGS) $(MFLAG) $(ACFLAG)

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
	-mkdir -p $(MODDIR)
	install -o root -g root -m 644 $(MODULES) $(MODDIR)

# Stuff to automatically maintain dependency files

%.o:	../linux-$(UTS_SHORT)/%.c
	$(CC) -MD $(CFLAGS) $(CPPFLAGS) -DKBUILD_BASENAME=$* -c -o $@ $<
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
	$(LD) -r -o $@ $(OBJS)
	@echo Done.

-include $(SRCS:%.c=.depfiles/%.d)
