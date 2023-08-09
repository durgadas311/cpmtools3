# Crude makefile for building cpmtools3 on linux
# Requires directories $(DEST)/share and $(DEST)/bin to exist
# Requires config.h in top-level directory.

DEST = /usr/local
SRCS = $(filter-out device_win32.c device_libdsk.c,$(wildcard *.c))
OBJS = $(patsubst %.c,%.o,$(SRCS))
EXES = cpmls cpmrm cpmcp
ALLEXES = $(EXES) cpmchmod cpmchattr mkfs.cpm fsck.cpm fsed.cpm

DEVICEOBJ = device_posix.o
CPMAUTOFS = cpmautofs.o
COREOBJ = cpmfs.o $(CPMAUTOFS) getopt.o getopt1.o $(DEVICEOBJ)

CFLAGS = -g -O2 -Wall \
	-Ilinux \
	-DFORMAT='"ibm-3740"' \
	-DDISKDEFS='"$(DEST)/share/diskdefs"'

all: $(EXES)

everything: $(ALLEXES)

install: $(ALLEXES)
	cp $(ALLEXES) $(DEST)/bin
	cp ../diskdefs $(DEST)/share

clean:
	rm -f $(OBJS)

clobber: clean
	rm -f $(ALLEXES)

cpmls: cpmls.o $(COREOBJ)
	$(CC) -o $@ cpmls.o $(COREOBJ)

cpmrm: cpmrm.o $(COREOBJ)
	$(CC) -o $@ cpmrm.o $(COREOBJ)

cpmcp: cpmcp.o $(COREOBJ)
	$(CC) -o $@ cpmcp.o $(COREOBJ)

cpmchmod: cpmchmod.o $(COREOBJ)
	$(CC) -o $@ cpmchmod.o $(COREOBJ)

cpmchattr: cpmchattr.o $(COREOBJ)
	$(CC) -o $@ cpmchattr.o $(COREOBJ)

mkfs.cpm: mkfs.cpm.o $(COREOBJ)
	$(CC) -o $@ mkfs.cpm.o $(COREOBJ)

fsck.cpm: fsck.cpm.o $(COREOBJ)
	$(CC) -o $@ fsck.cpm.o $(COREOBJ)

fsed.cpm: fsed.cpm.o $(COREOBJ) term_curses.o
	$(CC) -o $@ fsed.cpm.o term_curses.o $(COREOBJ) -lcurses
