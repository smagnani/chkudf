SHELL = /bin/sh

include ../config.mk

.PHONY : help all
help all:
	@echo ""
	@echo "This driver is in kernel 2.3.17 and higher."
	@echo "The only target supported on 2.3 is now 'update'."
	@echo ""
	@echo "make update        - update fs/udf/* and include/linux/udf*.h only into $(LINUX)"
	@echo "make clean               - remove temp files"
	@echo ""

# files changed (not replaced) in the source tree
MODFILES  = $(LINUX)/MAINTAINERS 
MODFILES += $(LINUX)/Documentation/filesystems/00-INDEX $(LINUX)/Documentation/Configure.help
MODFILES += $(LINUX)/fs/Makefile $(LINUX)/fs/Config.in $(LINUX)/fs/filesystems.c
MODFILES += $(LINUX)/include/linux/fs.h

# unsupported
.PHONY : patch apply unapply remove
patch:
	@echo "* ---------------------------------------- *"
	@echo "Error: patch is only supported for kernel 2.2;"
	@echo "kernel 2.3.17 and higher already has this driver."
	@echo "* ---------------------------------------- *"
	exit 1

apply:
	@echo "* ---------------------------------------- *"
	@echo "Error: apply is only supported for kernel 2.2;"
	@echo "kernel 2.3.17 and higher already has this driver."
	@echo "* ---------------------------------------- *"
	exit 1

unapply:
	@echo "* ---------------------------------------- *"
	@echo "Error: unapply is only supported for kernel 2.2;"
	@echo "kernel 2.3.17 and higher already has this driver."
	@echo "* ---------------------------------------- *"
	exit 1

remove:
	@echo "* ---------------------------------------- *"
	@echo "Error: remove is only supported for kernel 2.2;"
	@echo "kernel 2.3.17 and higher already has this driver."
	@echo "* ---------------------------------------- *"
	exit 1

#
# dependencies
#
$(LINUX) :
	@echo "Error: can't find linux source - $(LINUX)!"
	@exit 1

#
.PHONY : kernel.help1 kernel.help2
kernel.help1:
	@echo "* ---------------------------------------- *"
	@echo "* Now you must cd $(LINUX); make menuconfig, etc."
	@echo "* Remember to change UDF under filesystems to 'M' or '*'."
	@echo "* ---------------------------------------- *"

kernel.help2:
	@echo "* ---------------------------------------- *"
	@echo "Error: update is only supported on 2.2 or 2.3.17 or higher."



update:
	@if [ ! -d $(LINUX)/fs/udf ]; then $(MAKE) kernel.help2; exit 1; fi
	@echo "* ------------------------------------------ *"
	@echo "* Updating UDF source files in $(LINUX)/fs and $(LINUX)/include/linux"
	@echo "* ------------------------------------------ *"
	@$(MAKE) patch.update
	@$(MAKE) kernel.help1
	@echo Done.

#
# low-level targets, don't call these directly
#

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

#
#
clean:	distclean

distclean:
	@-/bin/rm -rf kernel.patch* patch.* *~ *.bak $(B_TEMP)
	@echo "patch cleaned."
