#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>

#include "getopt_.h"
#include "cpmfs.h"

const char cmd[] = "cpmcp";
static int text = 0;
static int preserve = 0;

/**
 * Return the user number.
 * @param s CP/M filename in 0[0]:aaaaaaaa.bbb format.
 * @returns The user number or -1 for no match.
 */
static int userNumber(const char *s) {
	if (isdigit(*s) && *(s + 1) == ':') {
		return (*s - '0');
	}
	if (isdigit(*s) && isdigit(*(s + 1)) && *(s + 2) == ':') {
		return (10 * (*s - '0') + (*(s + 1) - '0'));
	}
	return -1;
}

/**
 * Copy one file from CP/M to UNIX.
 * @param root The inode for the root directory.
 * @param src  The CP/M filename in 00aaaaaaaabbb format.
 * @param dest The UNIX filename.
 * @returns 0 for success, 1 for error.
 */
static int cpmToUnix(const struct cpmInode *root, const char *src, const char *dest) {
	struct cpmInode ino;
	int exitcode = 0;

	if (cpmNamei(root, src, &ino) == -1) {
		fprintf(stderr, "%s: can not open `%s': %s\n", cmd, src, boo);
		exitcode = 1;
	} else {
		struct cpmFile file;
		FILE *ufp;

		cpmOpen(&ino, &file, O_RDONLY);
		ufp = fopen(dest, text ? "w" : "wb");
		if (ufp == NULL) {
			fprintf(stderr, "%s: can not create %s: %s\n", cmd, dest, strerror(errno));
			exitcode = 1;
		} else {
			int crpending = 0;
			int ohno = 0;
			ssize_t res;
			char buf[4096];

			while ((res = cpmRead(&file, buf, sizeof(buf))) > 0) {
				int j;

				for (j = 0; j < res; ++j) {
					if (text) {
						if (buf[j] == '\032') {
							goto endwhile;
						}
						if (crpending) {
							if (buf[j] == '\n') {
								if (putc('\n', ufp) == EOF) {
									fprintf(stderr, "%s: can not write %s: %s\n", cmd, dest, strerror(errno));
									exitcode = 1;
									ohno = 1;
									goto endwhile;
								}
								crpending = 0;
							} else if (putc('\r', ufp) == EOF) {
								fprintf(stderr, "%s: can not write %s: %s\n", cmd, dest, strerror(errno));
								exitcode = 1;
								ohno = 1;
								goto endwhile;
							}
							crpending = (buf[j] == '\r');
						} else {
							if (buf[j] == '\r') {
								crpending = 1;
							} else if (putc(buf[j], ufp) == EOF) {
								fprintf(stderr, "%s: can not write %s: %s\n", cmd, dest, strerror(errno));
								exitcode = 1;
								ohno = 1;
								goto endwhile;
							}
						}
					} else if (putc(buf[j], ufp) == EOF) {
						fprintf(stderr, "%s: can not write %s: %s\n", cmd, dest, strerror(errno));
						exitcode = 1;
						ohno = 1;
						goto endwhile;
					}
				}
			}
endwhile:
			if (res == -1 && !ohno) {
				fprintf(stderr, "%s: can not read %s (%s)\n", cmd, src, boo);
				exitcode = 1;
				ohno = 1;
			}
			if (fclose(ufp) == EOF && !ohno) {
				fprintf(stderr, "%s: can not close %s: %s\n", cmd, dest, strerror(errno));
				exitcode = 1;
				ohno = 1;
			}
			if (preserve && !ohno && (ino.atime || ino.mtime)) {
				struct utimbuf ut;

				if (ino.atime) {
					ut.actime = ino.atime;
				} else {
					time(&ut.actime);
				}
				if (ino.mtime) {
					ut.modtime = ino.mtime;
				} else {
					time(&ut.modtime);
				}
				if (utime(dest, &ut) == -1) {
					fprintf(stderr, "%s: can change timestamps of %s: %s\n", cmd, dest, strerror(errno));
					exitcode = 1;
					ohno = 1;
				}
			}
		}
		cpmClose(&file);
	}
	return exitcode;
}

static void usage(void) {
	fprintf(stderr, "Usage: %s [-f format] [-p] [-t] image user:file file\n", cmd);
	fprintf(stderr, "       %s [-f format] [-p] [-t] image user:file ... directory\n", cmd);
	fprintf(stderr, "       %s [-f format] [-p] [-t] image file user:file\n", cmd);
	fprintf(stderr, "       %s [-f format] [-p] [-t] image file ... user:\n", cmd);
	exit(1);
}

int main(int argc, char *argv[]) {
	const char *err;
	const char *image;
	const char *format;
	const char *devopts = NULL;
	int uppercase = 0;
	int c, readcpm = -1, todir = -1;
	struct cpmInode root;
	struct cpmSuperBlock super;
	int exitcode = 0;
	int gargc;
	char **gargv;

	if (!(format = getenv("CPMTOOLSFMT"))) {
		format = FORMAT;
	}
	while ((c = getopt(argc, argv, "T:f:ptuh?")) != EOF) {
		switch (c) {
		case 'T':
			devopts = optarg;
			break;
		case 'f':
			format = optarg;
			break;
		case 'p':
			preserve = 1;
			break;
		case 't':
			text = 1;
			break;
		case 'u':
			uppercase = 1;
			break;
		case 'h':
		case '?':
			usage();
			break;
		}
	}
	if ((optind + 2) >= argc) {
		usage();
	}
	image = argv[optind++];

	if (userNumber(argv[optind]) >= 0) /* cpm -> unix? */ {
		int i;
		struct stat statbuf;

		for (i = optind; i < (argc - 1); ++i) {
			if (userNumber(argv[i]) == -1) {
				usage();
			}
		}
		todir = ((argc - optind) > 2);
		if (stat(argv[argc - 1], &statbuf) == -1) {
			if (todir) {
				usage();
			}
		} else if (S_ISDIR(statbuf.st_mode)) {
			todir = 1;
		} else if (todir) {
			usage();
		}
		readcpm = 1;
	} else if (userNumber(argv[argc - 1]) >= 0) /* unix -> cpm */ {
		int i;

		todir = 0;
		for (i = optind; i < (argc - 1); ++i) {
			if (userNumber(argv[i]) >= 0) {
				usage();
			}
		}
		if ((argc - optind) > 2 && *(strchr(argv[argc - 1], ':') + 1) != '\0') {
			usage();
		}
		if (*(strchr(argv[argc - 1], ':') + 1) == '\0') {
			todir = 1;
		}
		readcpm = 0;
	} else {
		usage();
	}
	err = Device_open(&super.dev, image, readcpm ? O_RDONLY : O_RDWR, devopts);
	if (err) {
		fprintf(stderr, "%s: cannot open %s (%s)\n", cmd, image, err);
		exit(1);
	}
	if (cpmReadSuper(&super, &root, format, uppercase) == -1) {
		fprintf(stderr, "%s: cannot read superblock (%s)\n", cmd, boo);
		exit(1);
	}
	if (readcpm) /* copy from CP/M to UNIX */ {
		int i;
		char *last = argv[argc - 1];

		cpmglob(optind, argc - 1, argv, &root, &gargc, &gargv);
		/* trying to copy multiple files to a file? */
		if (gargc > 1 && !todir) {
			usage();
		}
		for (i = 0; i < gargc; ++i) {
			char dest[_POSIX_PATH_MAX];

			if (todir) {
				char *translate;

				strcpy(dest, last);
				strcat(dest, "/");
				translate = dest + strlen(dest);
				strcat(dest, gargv[i] + 2);
				while ((translate = strchr(translate, '/'))) {
					*translate = ',';
				}
			} else {
				strcpy(dest, last);
			}
			if (cpmToUnix(&root, gargv[i], dest)) {
				exitcode = 1;
			}
		}
	} else { /* copy from UNIX to CP/M */
		int i;

		for (i = optind; i < (argc - 1); ++i) {
			char *dest = NULL;
			FILE *ufp;
			struct cpmInode ino;
			char cpmname[2 + 8 + 1 + 3 + 1]; /* 00foobarxy.zzy\0 */
			char *translate;
			struct stat st;

			ufp = fopen(argv[i], "rb");
			if (ufp == NULL) /* cry a little */ {
				fprintf(stderr, "%s: can not open %s: %s\n", cmd, argv[i], strerror(errno));
				exitcode = 1;
				continue;
			}

			stat(argv[i], &st);

			if (todir) {
				dest = strrchr(argv[i], '/');
				if (dest != NULL) {
					++dest;
				} else {
					dest = argv[i];
				}
				snprintf(cpmname, sizeof(cpmname), "%02d%s",
					userNumber(argv[argc - 1]), dest);
			} else {
				snprintf(cpmname, sizeof(cpmname), "%02d%s",
					userNumber(argv[argc - 1]),
					strchr(argv[argc - 1], ':') + 1);
			}

			translate = cpmname;
			while ((translate = strchr(translate, ','))) {
				*translate = '/';
			}

			if (cpmCreat(&root, cpmname, &ino, 0666) == -1) /* just cry */ {
				fprintf(stderr, "%s: can not create %s: %s\n", cmd, cpmname, boo);
				exitcode = 1;
			} else {
				struct cpmFile file;
				int ohno = 0;
				char buf[4096 + 1];

				cpmOpen(&ino, &file, O_WRONLY);
				do {
					int j;

					for (j = 0; j < (sizeof(buf) / 2) && (c = getc(ufp)) != EOF; ++j) {
						if (text && c == '\n') {
							buf[j++] = '\r';
						}
						buf[j] = c;
					}
					if (text && c == EOF) {
						buf[j++] = '\032';
					}
					if (cpmWrite(&file, buf, j) != j) {
						fprintf(stderr, "%s: can not write %s: %s\n", cmd, dest, boo);
						ohno = 1;
						exitcode = 1;
						break;
					}
				} while (c != EOF);
				if (cpmClose(&file) == EOF && !ohno) /* I just can't hold back the tears */ {
					fprintf(stderr, "%s: can not close %s: %s\n", cmd, dest, boo);
					exitcode = 1;
				}
				if (preserve && !ohno) {
					struct utimbuf times;
					times.actime = st.st_atime;
					times.modtime = st.st_mtime;
					cpmUtime(&ino, &times);
				}
			}
			fclose(ufp);
		}
	}
	cpmUmount(&super);
	exit(exitcode);
}
