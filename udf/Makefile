#
# Makefile 1.00 Peter Braam <braam@cs.cmu.edu>
#

help all:
	@echo "Pick one of the following targets:"
	@echo -e "\tmake config\t\t- configure and check system setup"
	@echo -e "\tmake udf.o\t\t- build Udf module"
	@echo -e "\tmake install\t\t- install Udf module"
	@echo -e "\tmake clean\t\t- clean up the directory"
	@echo -e "\tmake udftools\t\t- build udf tools"

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

udf.o: .prereq.ok
	@make -C module

libudf: .prereq.ok
	@make -C lib

udftools: libudf .prereq.ok
	@make -C tools

install: .prereq.ok
	@make -C module install

clean: clean-subdirs

clean-subdirs:
	touch config.mk
	-@make -C lib clean
	-@make -C module clean
	-@make -C tools clean
	-@make -C patch distclean

distclean: clean
	-@make -C patch distclean
	rm -f *~ *.bak .prereq.ok config.out config.mk include/linux/modversions.h
