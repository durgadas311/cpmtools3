#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "getopt_.h"
#include "cpmfs.h"

const char cmd[] = "cpmrm";

int main(int argc, char *argv[]) {
	const char *err;
	const char *image;
	const char *format;
	const char *devopts = NULL;
	int uppercase = 0;
	int c, i, usage = 0, exitcode = 0;
	struct cpmSuperBlock drive;
	struct cpmInode root;
	int gargc;
	char **gargv;

	if (!(format = getenv("CPMTOOLSFMT"))) {
		format = FORMAT;
	}
	while ((c = getopt(argc, argv, "T:f:uh?")) != EOF) {
		switch (c) {
		case 'T':
			devopts = optarg;
			break;
		case 'f':
			format = optarg;
			break;
		case 'u':
			uppercase = 1;
			break;
		case 'h':
		case '?':
			usage = 1;
			break;
		}
	}
	if (optind >= (argc - 1)) {
		usage = 1;
	} else {
		image = argv[optind++];
	}

	if (usage) {
		fprintf(stderr, "Usage: %s [-f format] [-T dsktype] [-u] image pattern ...\n", cmd);
		exit(1);
	}
	err = Device_open(&drive.dev, image, O_RDWR, devopts);
	if (err) {
		fprintf(stderr, "%s: cannot open %s (%s)\n", cmd, image, err);
		exit(1);
	}
	if (cpmReadSuper(&drive, &root, format, uppercase) == -1) {
		fprintf(stderr, "%s: cannot read superblock (%s)\n", cmd, boo);
		exit(1);
	}
	cpmglob(optind, argc, argv, &root, &gargc, &gargv);
	for (i = 0; i < gargc; ++i) {
		if (cpmUnlink(&root, gargv[i]) == -1) {
			fprintf(stderr, "%s: can not erase %s: %s\n", cmd, gargv[i], boo);
			exitcode = 1;
		}
	}
	cpmUmount(&drive);
	exit(exitcode);
}
