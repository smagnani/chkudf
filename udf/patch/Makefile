SRC=/usr/src/linux
all: kernel.patch

DOC_INC=Configure.help.inc
FS_INC=Config.in.inc filesystems.c.inc Makefile.inc
INCLUDE_INC=fs.h.inc

kernel.patch:	$(DOC_INC) $(FS_INC) $(INCLUDE_INC)
	cat $^ >$@

patch.apply: $(SRC)/Documentation/Configure.help
	@echo "* -------------------------------------------- *"
	@echo "* patching kernel source in $(SRC) *"
	@echo "* -------------------------------------------- *"
	-patch -d $(SRC) -p1 <kernel.patch
	touch patch.apply

apply:	kernel.patch patch.apply
	@echo "* -------------------------------------------- *"
	@echo "* copying udf source files to $(SRC) *"
	@echo "* -------------------------------------------- *"
	rm -rf $(SRC)/fs/udf $(SRC)/include/config/udf
	mkdir $(SRC)/fs/udf
	cp ../src/* $(SRC)/fs/udf
	cp Makefile.udf $(SRC)/fs/udf/Makefile
	cp ../include/linux/*.h $(SRC)/include/linux
	-mkdir $(SRC)/include/config/udf
	cp ../include/config/udf/*.h $(SRC)/include/config/udf
	cp ../doc/udf.txt $(SRC)/Documentation/filesystems

unapply:  kernel.patch patch.apply
	@echo "* -------------------------------------------- *"
	@echo "* reversing the kernel patch in $(SRC) *"
	@echo "* -------------------------------------------- *"
	-patch -R -d $(SRC) -p1 < kernel.patch
	rm patch.apply
	@echo "* -------------------------------------------- *"
	@echo "* removing udf source files from $(SRC) *"
	@echo "* -------------------------------------------- *"
	rm -rf $(SRC)/fs/udf $(SRC)/include/config/udf
	rm -f $(SRC)/Documentation/filesystems/udf.txt
	rm -f $(SRC)/include/linux/udf_*.h

clean:
	@-/bin/rm -f kernel.patch patch.apply *~ *.bak
	@echo "patch cleaned."
