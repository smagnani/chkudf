include ../../../config.mk

OBJS=chkudf.o read_udf.o errors.o utils.o verifyVRS.o verifyAVDP.o \
	globals.o checkTag.o verifyVDS.o verifyVD.o verifyRegid.o \
        setLastSector.o build_scsi.o utils_read.o init.o \
        cleanup.o volspace.o getVAT.o getMap.o display_dirs.o verifyICB.o \
        readSpMap.o filespace.o icbspace.o linkcount.o setSectorSize.o \
        setFirstSector.o do_scsi.o verifyLVID.o

all:	chkudf

chkudf: $(OBJS)
	$(CC) -o chkudf -g $(OBJS)

.c.o:
	$(CC) -Wall -Wshadow -Wswitch-default -Wswitch-enum -Wuninitialized -g -c $*.c

clean:
	@-/bin/rm -f chkudf *.o *~ *.bak

chkudf.o: chkudf.c chkudf.h

read_udf.o: read_udf.c chkudf.h

