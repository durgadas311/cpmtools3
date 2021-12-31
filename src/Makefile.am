bin_PROGRAMS = cpmls cpmrm cpmcp cpmchmod cpmchattr mkfs.cpm fsck.cpm fsed.cpm

DEVICEOBJ = device_posix.o
CPMAUTOFS = cpmautofs.o
COREOBJ = cpmfs.o $(CPMAUTOFS) getopt.o getopt1.o $(DEVICEOBJ)

LDADD = $(COREOBJ)

fsed.cpm_LDADD = term_curses.o $(COREOBJ)
fsed.cpm_LIBADD = -lcurses
