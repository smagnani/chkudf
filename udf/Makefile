#
# Makefile 1.00 Peter Braam <braam@cs.cmu.edu>
#

help all:
	@echo "Pick one of the following targets:"
	@echo -e "\tmake config\t\t- configure and check system setup"
	@echo -e "\tmake clean\t\t- clean up the directory"
	@echo -e "\tmake devtools\t\t- build udf devtools"
	@echo -e "Module:"
	@echo -e "\tmake udf.o (or module)\t\t- build Udf module"
	@echo -e "\tmake install\t\t- install Udf module in /lib"
	@echo -e "Patch:"
	@echo -e "\tmake patch\t\t- create kernel patch"
	@echo -e "\tmake apply\t\t- patch kernel source"
	@echo -e "\tmake update\t\t- update kernel source with newer udf"
	@echo -e "\tmake unapply\t\t- reverse patch kernel source"
	@echo -e "\tmake remove\t\t- remove udf files from kernel source"

# alternate names
.PHONY : tools module
tools: devtools
module: udf.o

.PHONY : config
config .prereq.ok:
	@./Configure

kcheck:
	@. config.out ; \
	if [ "$$CHECK" != "" ] ; then \
	    if [ "`cksum < $$CHECK`" != "$$CKSUM" ] ; then \
		/bin/echo -n "Kernel configuration has changed." ; \
		/bin/echo "  Please re-run 'make config'." ; \
		exit 1 ; \
	    fi ; \
	fi

.PHONY : udf.o
udf.o: .prereq.ok
	@. config.out ; \
	$(MAKE) -C "$$BUILD_PATH" modules SUBDIRS="$$BUILD_SUBDIRS"

.PHONY : patch
patch:  .prereq.ok
	@$(MAKE) -C patch patch

.PHONY : apply
apply:  .prereq.ok
	@$(MAKE) -C patch apply

.PHONY : unapply
unapply:  .prereq.ok
	@$(MAKE) -C patch unapply

.PHONY : update
update:  .prereq.ok
	@$(MAKE) -C patch update

.PHONY : remove
remove:  .prereq.ok
	@$(MAKE) -C patch remove

devtools: .prereq.ok
	@$(MAKE) -C tools

install: .prereq.ok
	@. config.out ; \
	$(MAKE) -C "$$BUILD_PATH" modules_install SUBDIRS="$$BUILD_SUBDIRS"

clean: clean-subdirs

clean-subdirs:
	touch config.mk
	-@$(MAKE) -C module clean
	-@$(MAKE) -C tools clean
	-@$(MAKE) -C patch-2.2 clean
	-@$(MAKE) -C patch-2.4 clean

distclean: clean
	-@$(MAKE) -C patch-2.2 distclean
	-@$(MAKE) -C patch-2.4 distclean
	-@rm patch src
	rm -f *~ *.bak .prereq.ok config.out config.mk include/linux/modversions.h
