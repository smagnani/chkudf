include ../config.mk

all: kernel.patch

DOC_INC=Configure.help.inc
FS_INC=Config.in.inc filesystems.c.inc Makefile.inc
INCLUDE_INC=fs.h.inc
NEW_FILES_INC=udffiles.patch
B_TEMP=build.tmp

#
# diff may return 1, even in normal case
#
.PHONY : udffiles.patch
udffiles.patch:
	-rm -rf $(B_TEMP)
	mkdir -p $(B_TEMP)/linux/fs/udf
	cp ../linux-$(UTS_SHORT)/* $(B_TEMP)/linux/fs/udf
	cp Makefile.udf $(B_TEMP)/linux/fs/udf/Makefile
	mkdir -p $(B_TEMP)/linux.old/linux/fs $(B_TEMP)/linux.old/linux/include
	ln -s ../../../include $(B_TEMP)/linux/include
	-cd $(B_TEMP); diff -u --recursive --new-file linux.old/linux linux > ../udffiles.patch 

kernel.patch:	$(NEW_FILES_INC) $(DOC_INC) $(FS_INC) $(INCLUDE_INC)
	cat $^ >$@

patch.apply: kernel.patch
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

clean:	distclean

distclean:
	@-/bin/rm -rf kernel.patch patch.apply *~ *.bak $(NEW_FILES_INC) $(B_TEMP)
	@echo "patch cleaned."
