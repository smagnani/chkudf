OBJS=chkudf.o read_udf.o errors.o utils.o verifyVRS.o verifyAVDP.o \
	globals.o checkTag.o verifyVDS.o verifyVD.o verifyRegid.o \
        setLastSector.o build_scsi.o utils_read.o init.o \
        cleanup.o volspace.o getVAT.o getMap.o display_dirs.o verifyICB.o \
        readSpMap.o filespace.o icbspace.o linkcount.o setSectorSize.o \
        setFirstSector.o do_scsi.o verifyLVID.o

CFLAGS := -Wall -Wshadow -Wswitch-default -Wswitch-enum -Wuninitialized -Wpointer-arith -g $(EXTRA_CFLAGS)

all:	chkudf

chkudf: $(OBJS)
	@echo "  LD chkudf"
	@$(CC) $(CFLAGS) -o chkudf -g $(OBJS) -lblkid

.c.o:
	@echo "  CC" $*.c
	@$(CC) $(CFLAGS) -c $*.c

clean:
	@-/bin/rm -f chkudf *.o *~ *.bak

chkudf.o: chkudf.c chkudf.h

read_udf.o: read_udf.c chkudf.h

