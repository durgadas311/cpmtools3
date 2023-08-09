This repo does not have a working auto-conf. Until that is fixed, building requires
custom makefiles and config.h. The makefile and config.h that I use on
Ubuntu 22.04 is in the 'linux' subdirectory.

To build on linux, try:

	make -f linux/Makefile everything

To add a custom "autofs" decoder:

	make -f linux/Makefile everything CPMAUTOFS=/path/to/auto/fs/code.o

The config.h file in linux/config.h was built using the old cpmtools.
