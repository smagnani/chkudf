include ../config.mk

all: kernel.patch

DOC_INC=Configure.help.inc
FS_INC=Config.in.inc filesystems.c.inc Makefile.inc
INCLUDE_INC=fs.h.inc

kernel.patch:	$(DOC_INC) $(FS_INC) $(INCLUDE_INC)
	cat $^ >$@

patch.apply: $(LINUX)/Documentation/Configure.help
	@echo "* ---------------------------------------- *"
	@echo "* patching kernel source in $(LINUX) *"
	@echo "* ---------------------------------------- *"
	-patch -d $(LINUX) -p1 <kernel.patch
	touch patch.apply

apply:	kernel.patch patch.apply
	@echo "* ------------------------------------------ *"
	@echo "* copying udf source files to $(LINUX) *"
	@echo "* ------------------------------------------ *"
	rm -rf $(LINUX)/fs/udf
	mkdir $(LINUX)/fs/udf
	cp ../src/* $(LINUX)/fs/udf
	cp Makefile.udf $(LINUX)/fs/udf/Makefile
	cp ../include/linux/*.h $(LINUX)/include/linux
	cp ../doc/udf.txt $(LINUX)/Documentation/filesystems

patch.unapply:  kernel.patch patch.apply
	@echo "* -------------------------------------------- *"
	@echo "* reversing the kernel patch in $(LINUX) *"
	@echo "* -------------------------------------------- *"
	-patch -R -d $(LINUX) -p1 < kernel.patch
	rm patch.apply

unapply: patch.unapply
	@echo "* --------------------------------------------- *"
	@echo "* removing udf source files from $(LINUX) *"
	@echo "* --------------------------------------------- *"
	rm -rf $(LINUX)/fs/udf $(LINUX)/include/config/udf
	rm -f $(LINUX)/Documentation/filesystems/udf.txt
	rm -f $(LINUX)/include/linux/udf_*.h

distclean:
	@-/bin/rm -f kernel.patch patch.apply *~ *.bak
	@echo "patch cleaned."
