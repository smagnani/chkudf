
SUBDIRS=lib module tools

all: make-subdirs

make-subdirs: 
	make -C lib
	make -C module
	make -C tools

clean: clean-subdirs

clean-subdirs:
	make -C lib clean
	make -C module clean
	make -C tools clean
	rm -f *.o *~

