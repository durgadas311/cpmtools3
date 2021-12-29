#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "getopt_.h"
#include "cpmfs.h"

static const char *const month[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/*
 * namecmp -- compare two entries 
 */
static int namecmp(const void *a, const void *b) {
	if (**((const char *const *)a) == '[') {
		return -1;
	}
	return strcmp(*((const char *const *)a), *((const char *const *)b));
}

/*
 * onlyuser0 -- do all entries belong to user 0? 
 */
static int onlyuser0(char **const dirent, int entries) {
	int i;

	for (i = 0; i < entries; ++i) {
		if (dirent[i][0] != '.' && (dirent[i][0] != '0' || dirent[i][1] != '0')) {
			return 0;
		}
	}

	return 1;
}

/*
 * olddir  -- old style output 
 */
static void olddir(struct cpmSuperBlock *sb, char **dirent, int entries) {
	int i, j, k, l, user, announce, showuser, files;
	int maxu = (sb->type & CPMFS_HAS_XFCBS ? 16 : 32);

	showuser = !onlyuser0(dirent, entries);
	files = 0;
	for (user = 0; user < maxu; ++user) {
		int u10 = '0' + user / 10;
		int u1 = '0' + user % 10;
		announce = 1;
		for (i = l = 0; i < entries; ++i) {
			/* This selects real regular files implicitly, because only those have
			 * the user in their name.  ".", ".." and the password file do not.
			 */
			if (dirent[i][0] == u10 && dirent[i][1] == u1) {
				++files;
				if (announce && showuser) {
					printf("User %d\n", user);
					announce = 0;
				}
				if (l % 4) {
					printf(" : ");
				}
				for (j = 2; dirent[i][j] && dirent[i][j] != '.'; ++j) {
					putchar(toupper(dirent[i][j]));
				}
				k = j;
				while (k < 11) {
					putchar(' ');
					++k;
				}
				if (dirent[i][j] == '.') {
					++j;
				}
				for (k = 0; dirent[i][j]; ++j, ++k) {
					putchar(toupper(dirent[i][j]));
				}
				for (; k < 3; ++k) {
					putchar(' ');
				}
				++l;
			}
			if (l && (l % 4) == 0) {
				l = 0;
				putchar('\n');
			}
		}
		if (l % 4) {
			putchar('\n');
		}
	}

	if (files == 0) {
		printf("No file\n");
	}
}

/* print ASCII time, DD-mmm-YYYY HH:MM */
static void printtime4(time_t ts) {
	struct tm *tmp = localtime(&ts);
	printf("  %02d-%s-%04d %02d:%02d", tmp->tm_mday, month[tmp->tm_mon],
		tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min);
}

/* print ASCII time, MM/DD/YY HH:MM */
static void printtime2(time_t ts) {
	struct tm *tmp = localtime(&ts);
	printf("%02d/%02d/%02d %02d:%02d  ", tmp->tm_mon + 1, tmp->tm_mday,
			tmp->tm_year % 100, tmp->tm_hour, tmp->tm_min);
}

/*
 * oldddir -- old style long output 
 */
static void oldddir(struct cpmSuperBlock *sb, char **dirent, int entries, struct cpmInode *ino) {
	struct cpmStatFS buf;
	struct cpmStat statbuf;
	struct cpmInode file;
	int maxu = (sb->type & CPMFS_HAS_XFCBS ? 16 : 32);

	if (entries > 2) {
		int i, j, k, l, announce, user;

		qsort(dirent, entries, sizeof(char *), namecmp);
		cpmStatFS(ino, &buf);
		printf("     Name    Bytes   Recs  Attr     update             create\n");
		printf("------------ ------ ------ ---- -----------------  -----------------\n");
		announce = 0;
		for (l = user = 0; user < maxu; ++user) {
			int u10 = '0' + user / 10;
			int u1 = '0' + user % 10;
			for (i = 0; i < entries; ++i) {
				if (dirent[i][0] == u10 && dirent[i][1] == u1) {
					if (announce == 1) {
						printf("\nUser %d:\n\n", user);
						printf("     Name    Bytes   Recs  Attr     update             create\n");
						printf("------------ ------ ------ ---- -----------------  -----------------\n");
					}
					announce = 2;
					for (j = 2; dirent[i][j] && dirent[i][j] != '.'; ++j) {
						putchar(toupper(dirent[i][j]));
					}
					k = j;
					while (k < 10) {
						putchar(' ');
						++k;
					}
					putchar('.');
					if (dirent[i][j] == '.') {
						++j;
					}
					for (k = 0; dirent[i][j]; ++j, ++k) {
						putchar(toupper(dirent[i][j]));
					}
					for (; k < 3; ++k) {
						putchar(' ');
					}

					cpmNamei(ino, dirent[i], &file);
					cpmStat(&file, &statbuf);
					printf(" %5.1ldK", (long)(statbuf.size + buf.f_bsize - 1) /
						buf.f_bsize * (buf.f_bsize / 1024));

					printf(" %6.1ld ", (long)(statbuf.size / 128));
					putchar(statbuf.mode & 0200 ? ' ' : 'R');
					putchar(statbuf.mode & 01000 ? 'S' : ' ');
					putchar(' ');
					if (statbuf.mtime) {
						printtime4(statbuf.mtime);
					} else if (statbuf.ctime) {
						printf("                   ");
					}
					if (statbuf.ctime) {
						printtime4(statbuf.ctime);
					}
					putchar('\n');
					++l;
				}
			}
			if (announce == 2) {
				announce = 1;
			}
		}
		printf("%5.1d Files occupying %6.1ldK", l, (buf.f_bused * buf.f_bsize) / 1024);
		printf(", %7.1ldK Free.\n", (buf.f_bfree * buf.f_bsize) / 1024);
	} else {
		printf("No files found\n");
	}
}

/*
 * old3dir -- old CP/M Plus style long output 
 */
static void old3dir(struct cpmSuperBlock *sb, char **dirent, int entries, struct cpmInode *ino) {
	struct cpmStatFS buf;
	struct cpmStat statbuf;
	struct cpmInode file;
	int maxu = (sb->type & CPMFS_HAS_XFCBS ? 16 : 32);

	if (entries > 2) {
		int i, j, k, l, announce, user, attrib;
		int totalBytes = 0, totalRecs = 0;

		qsort(dirent, entries, sizeof(char *), namecmp);
		cpmStatFS(ino, &buf);
		announce = 1;
		for (l = 0, user = 0; user < maxu; ++user) {
			int u10 = '0' + user / 10;
			int u1 = '0' + user % 10;
			for (i = 0; i < entries; ++i) {
				if (dirent[i][0] == u10 && dirent[i][1] == u1) {
					cpmNamei(ino, dirent[i], &file);
					cpmStat(&file, &statbuf);
					cpmAttrGet(&file, &attrib);
					if (announce == 1) {
						if (user) {
							putchar('\n');
						}
						printf("Directory For Drive A:  User %2.1d\n\n", user);
						printf("    Name     Bytes   Recs   Attributes   Prot      Update          %s\n",
							ino->sb->cnotatime ? "Create" : "Access");
						printf("------------ ------ ------ ------------ ------ --------------  --------------\n\n");
					}
					announce = 2;
					for (j = 2; dirent[i][j] && dirent[i][j] != '.'; ++j) {
						putchar(toupper(dirent[i][j]));
					}
					k = j;
					while (k < 10) {
						putchar(' ');
						++k;
					}
					putchar(' ');
					if (dirent[i][j] == '.') {
						++j;
					}
					for (k = 0; dirent[i][j]; ++j, ++k) {
						putchar(toupper(dirent[i][j]));
					}
					for (; k < 3; ++k) {
						putchar(' ');
					}

					totalBytes += statbuf.size;
					totalRecs += (statbuf.size + 127) / 128;
					printf(" %5.1ldk", (long) (statbuf.size + buf.f_bsize - 1) /
						buf.f_bsize * (buf.f_bsize / 1024));
					printf(" %6.1ld ", (long)((statbuf.size + 127) / 128));
					putchar((attrib & CPM_ATTR_F1)   ? '1' : ' ');
					putchar((attrib & CPM_ATTR_F2)   ? '2' : ' ');
					putchar((attrib & CPM_ATTR_F3)   ? '3' : ' ');
					putchar((attrib & CPM_ATTR_F4)   ? '4' : ' ');
					putchar((statbuf.mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ? ' ' : 'R');
					putchar((attrib & CPM_ATTR_SYS)  ? 'S' : ' ');
					putchar((attrib & CPM_ATTR_ARCV) ? 'A' : ' ');
					printf("      ");
					if      (attrib & CPM_ATTR_PWREAD) {
						printf("Read   ");
					} else if (attrib & CPM_ATTR_PWWRITE) {
						printf("Write  ");
					} else if (attrib & CPM_ATTR_PWDEL) {
						printf("Delete ");
					} else {
						printf("None   ");
					}
					if (statbuf.mtime) {
						printtime2(statbuf.mtime);
					} else {
						printf("                ");
					}
					if (ino->sb->cnotatime && statbuf.ctime) {
						printtime2(statbuf.ctime);
					} else if (!ino->sb->cnotatime && statbuf.atime) {
						printtime2(statbuf.atime);
					}
					putchar('\n');
					++l;
				}
			}
			if (announce == 2) {
				announce = 1;
			}
		}
		printf("\nTotal Bytes     = %6.1dk  ", (totalBytes + 1023) / 1024);
		printf("Total Records = %7.1d  ", totalRecs);
		printf("Files Found = %4.1d\n", l);
		printf("Total 1k Blocks = %6.1ld   ", (buf.f_bused * buf.f_bsize) / 1024);
		printf("Used/Max Dir Entries For Drive A: %4.1ld/%4.1ld\n",
				buf.f_files - buf.f_ffree, buf.f_files);
	} else {
		printf("No files found\n");
	}
}

/*
 * ls -- UNIX style output 
 */
static void ls(struct cpmSuperBlock *sb, char **dirent, int entries, struct cpmInode *ino,
			int l, int c, int iflag) {
	int i, user, announce, any;
	time_t now;
	struct cpmStat statbuf;
	struct cpmInode file;
	int maxu = (sb->type & CPMFS_HAS_XFCBS ? 16 : 32);

	time(&now);
	qsort(dirent, entries, sizeof(char *), namecmp);
	announce = 0;
	any = 0;
	for (user = 0; user < maxu; ++user) {
		int u10 = '0' + user / 10;
		int u1 = '0' + user % 10;
		announce = 0;
		for (i = 0; i < entries; ++i) {
			if (dirent[i][0] != '.') {
				if (dirent[i][0] == u10 && dirent[i][1] == u1) {
					if (announce == 0) {
						if (any) {
							putchar('\n');
						}
						printf("%d:\n", user);
						announce = 1;
					}
					any = 1;
					if (iflag || l) {
						cpmNamei(ino, dirent[i], &file);
						cpmStat(&file, &statbuf);
					}
					if (iflag) {
						printf("%4ld ", (long) statbuf.ino);
					}
					if (l) {
						struct tm *tmp;

						putchar(S_ISDIR(statbuf.mode) ? 'd' : '-');
						putchar(statbuf.mode & 0400 ? 'r' : '-');
						putchar(statbuf.mode & 0200 ? 'w' : '-');
						putchar(statbuf.mode & 0100 ? 'x' : '-');
						putchar(statbuf.mode & 0040 ? 'r' : '-');
						putchar(statbuf.mode & 0020 ? 'w' : '-');
						putchar(statbuf.mode & 0010 ? 'x' : '-');
						putchar(statbuf.mode & 0004 ? 'r' : '-');
						putchar(statbuf.mode & 0002 ? 'w' : '-');
						putchar(statbuf.mode & 0001 ? 'x' : '-');
#if 0
						putchar(statbuf.flags & FLAG_PUBLIC ? 'p' : '-');
						putchar(dir[i].flags & FLAG_SYSTEM ? 's' : '-');
						printf(" %-2d ", dir[i].user);
#endif
						printf("%8.1ld ", (long)statbuf.size);
						tmp = localtime(c ? &statbuf.ctime : &statbuf.mtime);
						printf("%s %02d ", month[tmp->tm_mon], tmp->tm_mday);
						if ((c ? statbuf.ctime : statbuf.mtime) < (now - 182 * 24 * 3600)) {
							printf("%04d  ", tmp->tm_year + 1900);
						} else {
							printf("%02d:%02d ", tmp->tm_hour, tmp->tm_min);
						}
					}
					printf("%s\n", dirent[i] + 2);
				}
			}
		}
	}
}

/*
 * lsattr  -- output something like e2fs lsattr 
 */
static void lsattr(struct cpmSuperBlock *sb, char **dirent, int entries, struct cpmInode *ino) {
	int i, user, announce, any;
	struct cpmStat statbuf;
	struct cpmInode file;
	cpm_attr_t attrib;
	int maxu = (sb->type & CPMFS_HAS_XFCBS ? 16 : 32);

	qsort(dirent, entries, sizeof(char *), namecmp);
	announce = 0;
	any = 0;
	/* TODO: don't show XFCBS... or augment w/XFCB data? */
	for (user = 0; user < maxu; ++user) {
		int u10 = '0' + user / 10;
		int u1 = '0' + user % 10;
		announce = 0;
		for (i = 0; i < entries; ++i) {
			if (dirent[i][0] != '.') {
				if (dirent[i][0] == u10 && dirent[i][1] == u1) {
					if (announce == 0) {
						if (any) {
							putchar('\n');
						}
						printf("%d:\n", user);
						announce = 1;
					}
					any = 1;

					cpmNamei(ino, dirent[i], &file);
					cpmStat(&file, &statbuf);
					cpmAttrGet(&file, &attrib);

					putchar ((attrib & CPM_ATTR_F1)      ? '1' : '-');
					putchar ((attrib & CPM_ATTR_F2)      ? '2' : '-');
					putchar ((attrib & CPM_ATTR_F3)      ? '3' : '-');
					putchar ((attrib & CPM_ATTR_F4)      ? '4' : '-');
					putchar ((attrib & CPM_ATTR_SYS)     ? 's' : '-');
					putchar ((attrib & CPM_ATTR_ARCV)    ? 'a' : '-');
					putchar ((attrib & CPM_ATTR_PWREAD)  ? 'r' : '-');
					putchar ((attrib & CPM_ATTR_PWWRITE) ? 'w' : '-');
					putchar ((attrib & CPM_ATTR_PWDEL)   ? 'e' : '-');

					printf(" %s\n", dirent[i] + 2);
				}
			}
		}
	}
}

const char cmd[] = "cpmls";

int main(int argc, char *argv[]) {
	const char *err;
	const char *image;
	const char *format;
	const char *devopts = NULL;
	int c, usage = 0;
	struct cpmSuperBlock super;
	struct cpmInode root;
	int style = 0;
	int changetime = 0;
	int uppercase = 0;
	int inode = 0;
	char **gargv;
	int gargc;
	static char starlit[2] = "*";
	static char *const star[] = { starlit };

	if (!(format = getenv("CPMTOOLSFMT"))) {
		format = FORMAT;
	}
	while ((c = getopt(argc, argv, "cT:f:ih?dDFlAu")) != EOF) {
		switch (c) {
		case 'f':
			format = optarg;
			break;
		case 'T':
			devopts = optarg;
			break;
		case 'h':
		case '?':
			usage = 1;
			break;
		case 'd':
			style = 1;
			break;
		case 'D':
			style = 2;
			break;
		case 'F':
			style = 3;
			break;
		case 'l':
			style = 4;
			break;
		case 'A':
			style = 5;
			break;
		case 'c':
			changetime = 1;
			break;
		case 'i':
			inode = 1;
			break;
		case 'u':
			uppercase = 1;
			break;
		}
	}
	if (optind == argc) {
		usage = 1;
	} else {
		image = argv[optind++];
	}

	if (usage) {
#ifdef HAVE_LIBDSK_H
		fprintf(stderr, "Usage: %s [-f format] [-T libdsk-type] "
				"[-d|-D|-F|-A|[-l][-c][-i]] [-u] image [file ...]\n", cmd);
#else
		fprintf(stderr, "Usage: %s [-f format] "
				"[-d|-D|-F|-A|[-l][-c][-i]] [-u] image [file ...]\n", cmd);
#endif
		exit(1);
	}
	err = Device_open(&super.dev, image, O_RDONLY, devopts);
	if (err) {
		fprintf(stderr, "%s: cannot open %s (%s)\n", cmd, image, err);
		exit(1);
	}
	if (cpmReadSuper(&super, &root, format, uppercase) == -1) {
		fprintf(stderr, "%s: cannot read superblock (%s)\n", cmd, boo);
		exit(1);
	}
	if (optind < argc) {
		cpmglob(optind, argc, argv, &root, &gargc, &gargv);
	} else {
		cpmglob(0, 1, star, &root, &gargc, &gargv);
	}
	if (style == 1) {
		olddir(&super, gargv, gargc);
	} else if (style == 2) {
		oldddir(&super, gargv, gargc, &root);
	} else if (style == 3) {
		old3dir(&super, gargv, gargc, &root);
	} else if (style == 5) {
		lsattr(&super, gargv, gargc, &root);
	} else {
		ls(&super, gargv, gargc, &root, style == 4, changetime, inode);
	}
	cpmglobfree(gargv, gargc);
	cpmUmount(&super);
	exit(0);
}
