

all: make-subdirs
	@echo "* ---------- *"
	@echo "* Done! ...  *"
	@echo "* ---------- *"

install: make-subdirs
	$(MAKE) -C module install
	$(MAKE) -C tools install

applypatch:
	make -C patch apply

modules: 
	make -C module

modules_install: 
	make -C module install

make-subdirs: 
	make -C lib
	make -C module
	make -C tools

clean: clean-subdirs
	@-/bin/rm -f *~ src/*~ src/*.bak src/*.o

clean-subdirs:
	@make -C lib clean
	@make -C module clean
	@make -C tools clean
	@make -C patch clean


