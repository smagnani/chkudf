SHELL = /bin/sh

include ../config.mk

.PHONY : help all
help all:
	@echo ""
	@echo "make patch         - rebuild kernel.patch"
	@echo "make apply         - patch kernel source in $(LINUX)"
	@echo "make update        - update fs/udf/* and include/linux/udf*.h only into $(LINUX)"
	@echo "make unapply       - reverse kernel patch in $(LINUX)"
	@echo "make remove	  - remove udf source from kernel, restore preudf files"
	@echo "make clean               - remove kernel.patch and temp files"
	@echo ""

# patch build directory
B_TEMP=build.tmp

# files changed (not replaced) in the source tree
MODFILES  = $(LINUX)/MAINTAINERS 
MODFILES += $(LINUX)/Documentation/filesystems/00-INDEX $(LINUX)/Documentation/Configure.help
MODFILES += $(LINUX)/fs/Makefile $(LINUX)/fs/Config.in $(LINUX)/fs/filesystems.c
MODFILES += $(LINUX)/include/linux/fs.h

#
# dependencies
#
$(LINUX) :
	@echo "Error: can't find linux source - $(LINUX)!"
	@exit 1

$(LINUX)/fs/udf :
	@echo "Error: can't remove what's not there!"
	@exit 1
#
patch: 
	@if [ -r kernel.patch.applied ]; then $(MAKE) build.help1; exit 1; fi
	@if [ ! -r ../module/udf.o ]; then $(MAKE) build.help2; fi
	@-rm patch.build
	@$(MAKE) build.temp
	@echo "* ---------------------------------------- *"
	@echo "* building patch/kernel.patch" 
	@echo "* ---------------------------------------- *"
	@$(MAKE) patch.build
	@echo "* ---------------------------------------- *"
	@echo "* patch/kernel.patch prepared for $(LINUX) *"
	@echo "* ---------------------------------------- *"
	@echo Done.

build.help1 :
	@echo "* ---------------------------------------- *"
	@echo "* A kernel.patch has already been applied."
	@echo "* To ensure a clean patch, you must run"
	@echo "* 'make unapply' before building a new patch."
	@echo "* ---------------------------------------- *"

build.help2 :
	@echo "* ---------------------------------------- *"
	@echo "* You haven't built the module yet."
	@echo "* Please do that now and test it before"
	@echo "* applying the patch to your kernel source."
	@echo "* ---------------------------------------- *"

#
# don't apply if already applied
#
apply:  
	@if [ -r kernel.patch.applied ]; then $(MAKE) apply.help1; exit 1; fi
	@if [ -d $(LINUX)/fs/udf ]; then $(MAKE) apply.help2; exit 2; fi
	@if [ ! -r kernel.patch ]; then $(MAKE) build; fi
	@echo "* ---------------------------------------- *"
	@echo "* Patching kernel source in $(LINUX) *"
	@echo "* ---------------------------------------- *"
	@$(MAKE) patch.prefiles
	@$(MAKE) patch.apply
	@$(MAKE) kernel.help1
	@echo Done.

.PHONY : apply.help1 kernel.help1 unapply.help2
kernel.help1:
	@echo "* ---------------------------------------- *"
	@echo "* Now you must cd $(LINUX); make menuconfig, etc."
	@echo "* Remember to change UDF under filesystems to 'M' or '*'."
	@echo "* ---------------------------------------- *"

apply.help1:
	@echo "* ---------------------------------------- *"
	@echo "* Patch has already been done; please run"
	@echo "* 'make unapply' before trying again."
	@echo "* You can also run 'make update' now."
	@echo "* ---------------------------------------- *"

apply.help2:
	@echo "* ---------------------------------------- *"
	@echo "* Patch has already been done; please run"
	@echo "* 'make unapply' before trying again."
	@echo "* If 'make unapply' fails, try 'make remove'."
	@echo "* You can also run 'make update' now."
	@echo "* ---------------------------------------- *"

unapply: 
	@if [ -r kernel.patch.applied ]; then \
	    $(MAKE) patch.unapply; \
	fi
	@$(MAKE) patch.prefiles.undo
	@if [ -d $(LINUX)/fs/udf ]; then \
	    $(MAKE) remove; \
	fi
	@echo Done.

update: kernel.patch.applied
	@echo "* ------------------------------------------ *"
	@echo "* Updating UDF source files in $(LINUX)/fs and $(LINUX)/include/linux"
	@echo "* ------------------------------------------ *"
	@$(MAKE) patch.update
	@$(MAKE) kernel.help1
	@echo Done.

unapply.help2:
	@echo "* --------------------------------------------- *"
	@echo "* You can't update or unapply if you haven't called make apply"
	@echo "* --------------------------------------------- *"
	@exit 1

remove: 
	@echo "* --------------------------------------------- *"
	@echo "* cleaning all udf source files from $(LINUX) *"
	@echo "* --------------------------------------------- *"
	@$(MAKE) patch.remove
	@$(MAKE) patch.prefiles.remove
	@echo Done.

#
# low-level targets, don't call these directly
#
.PHONY : build.temp
build.temp:
	@-rm -rf $(B_TEMP)
	@mkdir $(B_TEMP)
	@cp Makefile.diff $(B_TEMP)/Makefile
	@$(MAKE) -C $(B_TEMP) 

patch.build: 
	@$(MAKE) -C $(B_TEMP) udffiles.patch
	@cp $(B_TEMP)/udffiles.patch kernel.patch
	@touch patch.build

#
# make backups of changed files
#
patch.prefiles: $(MODFILES)
	@for f in $(MODFILES); do if [ ! -r $${f}.preudf ]; then \
		cp $${f} $${f}.preudf; \
		echo saving $${f} to $${f}.preudf; \
	fi; done

patch.prefiles.undo: $(MODFILES)
	@for f in $(MODFILES); do if [ -r $${f}.preudf ]; then \
		cp $${f}.preudf $${f}; \
		echo $${f} restored from $${f}.preudf; \
	fi; done

patch.prefiles.remove: $(MODFILES)
	@for f in $(MODFILES); do if [ -r $${f}.preudf ]; then \
		rm -f $${f}.preudf; \
	fi; done

# called from apply
# save kernel.patch.applied for restore
patch.apply: kernel.patch
	-patch -d $(LINUX) -p1 <kernel.patch
	@cp kernel.patch kernel.patch.applied
	@touch patch.apply

kernel.patch.applied: unapply.help2

#
# copy udf files, but don't update fs.h, filesystems.c, etc.
# only do if we've patch at least once ... hmm.. complicated.
#
patch.update: 
	@rm -rf $(LINUX)/fs/udf
	@mkdir $(LINUX)/fs/udf
	@cp ../src/* $(LINUX)/fs/udf
	@cp Makefile.udf $(LINUX)/fs/udf/Makefile
	@cp ../include/linux/*.h $(LINUX)/include/linux
	@cp ../doc/udf.txt $(LINUX)/Documentation/filesystems
	@echo Done.


patch.unapply:  
	@if [ ! -r kernel.patch.applied ]; then $(MAKE) unapply.help2; exit 3; fi
	@echo "* -------------------------------------------- *"
	@echo "* reversing the kernel patch in $(LINUX) *"
	@echo "* -------------------------------------------- *"
	-patch -R -d $(LINUX) -p1 < kernel.patch.applied
	@rm -f patch.apply
	@mv kernel.patch.applied kernel.patch.undone

patch.remove: 
	@if [ -d $(LINUX)/fs/udf ]; then rm -rf $(LINUX)/fs/udf; fi
	@if [ -d $(LINUX)/include/config/udf ]; then rm -rf $(LINUX)/include/config/udf; fi
	@rm -f $(LINUX)/Documentation/filesystems/udf.txt
	@rm -f $(LINUX)/include/linux/udf_*.h

#
#
clean:	distclean

distclean:
	@-/bin/rm -rf kernel.patch* patch.* *~ *.bak $(B_TEMP)
	@echo "patch cleaned."
