#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cpmfs.h"
#include "getopt_.h"
#include "term.h"

extern char **environ;

static char *mapbuf;

static struct tm *cpmtime(char lday, char hday, char hour, char min) {
	static struct tm tm;
	unsigned long days = (lday & 0xff) | ((hday & 0xff) << 8);
	int d;
	unsigned int md[12] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	tm.tm_sec = 0;
	tm.tm_min = ((min >> 4) & 0xf) * 10 + (min & 0xf);
	tm.tm_hour = ((hour >> 4) & 0xf) * 10 + (hour & 0xf);
	tm.tm_mon = 0;
	tm.tm_year = 1978;
	tm.tm_isdst = -1;
	if (days) {
		--days;
	}
	while (days >= (d = (((tm.tm_year % 400) == 0 || ((tm.tm_year % 4) == 0 && (tm.tm_year % 100))) ? 366 : 365))) {
		days -= d;
		++tm.tm_year;
	}
	md[1] = ((tm.tm_year % 400) == 0 || ((tm.tm_year % 4) == 0 && (tm.tm_year % 100))) ? 29 : 28;
	while (days >= md[tm.tm_mon]) {
		days -= md[tm.tm_mon];
		++tm.tm_mon;
	}
	tm.tm_mday = days + 1;
	tm.tm_year -= 1900;
	return &tm;
}

static void info(struct cpmSuperBlock *sb, const char *format, const char *image) {
	const char *msg;

	term_clear();
	msg = "File system characteristics";
	term_xy((term_cols() - strlen(msg)) / 2, 0);
	term_printf(msg);
	term_xy(0, 2);
	term_printf("                      Image: %s", image);
	term_xy(0, 3);
	term_printf("                     Format: %s", format);
	term_xy(0, 4);
	term_printf("                File system: ");
	switch (sb->type) {
	case CPMFS_DR22:
		term_printf("CP/M 2.2");
		break;
	case CPMFS_P2DOS:
		term_printf("P2DOS 2.3");
		break;
	case CPMFS_DR3:
		term_printf("CP/M Plus");
		break;
	}

	term_xy(0, 6);
	term_printf("              Sector length: %d", sb->secLength);
	term_xy(0, 7);
	term_printf("           Number of tracks: %d", sb->tracks);
	term_xy(0, 8);
	term_printf("          Sectors per track: %d", sb->sectrk);

	term_xy(0, 10);
	term_printf("                 Block size: %d", sb->blksiz);
	term_xy(0, 11);
	term_printf("Number of directory entries: %d", sb->maxdir);
	term_xy(0, 12);
	term_printf(" Number of directory blocks: %d", sb->dirblks);
	term_xy(0, 13);
	term_printf("        Logical sector skew: %d", sb->skew);
	term_xy(0, 14);
	term_printf("    Number of system tracks: %d", sb->boottrk);
	term_xy(0, 15);
	term_printf(" Logical extents per extent: %d", sb->extents);
	term_xy(0, 16);
	term_printf("    Allocatable data blocks: %d", sb->size - (sb->maxdir * 32 + sb->blksiz - 1) / sb->blksiz);

	msg = "Any key to continue";
	term_xy((term_cols() - strlen(msg)) / 2, 23);
	term_printf(msg);
	term_getch();
}

static void map(struct cpmSuperBlock *sb) {
	const char *msg;
	char bmap[18 * 80];
	int secmap, sys, directory;
	int pos;

	term_clear();
	msg = "Data map";
	term_xy((term_cols() - strlen(msg)) / 2, 0);
	term_printf(msg);

	secmap = (sb->tracks * sb->sectrk + 80 * 18 - 1) / (80 * 18);
	memset(bmap, ' ', sizeof(bmap));
	sys = sb->boottrk * sb->sectrk;
	memset(bmap, 'S', sys / secmap);
	directory = (sb->maxdir * 32 + sb->secLength - 1) / sb->secLength;
	memset(bmap + sys / secmap, 'D', directory / secmap);
	memset(bmap + (sys + directory) / secmap, '.', sb->sectrk * sb->tracks / secmap);

	for (pos = 0; pos < (sb->maxdir * 32 + sb->secLength - 1) / sb->secLength; ++pos) {
		int entry;

		Device_readSector(&sb->dev, sb->boottrk + pos / (sb->sectrk * sb->secLength), pos / sb->secLength, mapbuf);
		for (entry = 0; entry < sb->secLength / 32 && (pos * sb->secLength / 32) + entry < sb->maxdir; ++entry) {
			int i;

			if (mapbuf[entry * 32] >= 0 && mapbuf[entry * 32] <= (sb->type == CPMFS_P2DOS ? 31 : 15)) {
				for (i = 0; i < 16; ++i) {
					int sector;

					sector = mapbuf[entry * 32 + 16 + i] & 0xff;
					if (sb->size > 256) {
						sector |= (((mapbuf[entry * 32 + 16 + ++i] & 0xff) << 8));
					}
					if (sector > 0 && sector <= sb->size) {
						/* not entirely correct without the last extent record count */
						sector = sector * (sb->blksiz / sb->secLength) + sb->sectrk * sb->boottrk;
						memset(bmap + sector / secmap, '#', sb->blksiz / (sb->secLength * secmap));
					}
				}
			}
		}
	}

	for (pos = 0; pos < (int)sizeof(bmap); ++pos) {
		term_xy(pos / 18, 2 + pos % 18);
		term_putch(bmap[pos]);
	}
	term_xy(0, 21);
	term_printf("S=System area   D=Directory area   #=File data   .=Free");
	msg = "Any key to continue";
	term_xy((term_cols() - strlen(msg)) / 2, 23);
	term_printf(msg);
	term_getch();
}

static void data(struct cpmSuperBlock *sb, const char *buf, unsigned long int pos) {
	int offset = (pos % sb->secLength) & ~0x7f;
	unsigned int i;

	for (i = 0; i < 128; ++i) {
		term_xy((i & 0x0f) * 3 + !!(i & 0x8), 4 + (i >> 4));
		term_printf("%02x", buf[i + offset] & 0xff);
		if (pos % sb->secLength == i + offset) {
			term_reverse(1);
		}
		term_xy(50 + (i & 0x0f), 4 + (i >> 4));
		term_printf("%c", isprint((unsigned char)buf[i + offset]) ? buf[i + offset] : '.');
		term_reverse(0);
	}
	term_xy(((pos & 0x7f) & 0x0f) * 3 + !!((pos & 0x7f) & 0x8) + 1, 4 + ((pos & 0x7f) >> 4));
}


const char cmd[] = "fsed.cpm";

int main(int argc, char *argv[]) {
	/* variables */
	const char *devopts = (const char *)0;
	int uppercase = 0;
	char *image;
	const char *err;
	struct cpmSuperBlock drive;
	struct cpmInode root;
	const char *format;
	int c, usage = 0;
	off_t pos;
	int ch;
	int reload;
	char *buf;


	/* parse options */
	if (!(format = getenv("CPMTOOLSFMT"))) {
		format = FORMAT;
	}
	while ((c = getopt(argc, argv, "T:f:uh?")) != EOF) {
		switch (c) {
		case 'f':
			format = optarg;
			break;
		case 'T':
			devopts = optarg;
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
	if (optind != (argc - 1)) {
		usage = 1;
	} else {
		image = argv[optind++];
	}

	if (usage) {
		fprintf(stderr, "Usage: fsed.cpm [-f format] image\n");
		exit(1);
	}

	/* open image */
	if ((err = Device_open(&drive.dev, image, O_RDONLY, devopts))) {
		fprintf(stderr, "%s: cannot open %s (%s)\n", cmd, image, err);
		exit(1);
	}
	if (cpmReadSuper(&drive, &root, format, uppercase) == -1) {
		fprintf(stderr, "%s: cannot read superblock (%s)\n", cmd, boo);
		exit(1);
	}

	/* alloc sector buffers */
	if ((buf = malloc(drive.secLength)) == (char *)0 || (mapbuf = malloc(drive.secLength)) == (char *)0) {
		fprintf(stderr, "fsed.cpm: can not allocate sector buffer (%s).\n", strerror(errno));
		exit(1);
	}

	term_init();

	pos = 0;
	reload = 1;
	do {
		/* display position and load data */
		term_clear();
		term_xy(0, 2);
		term_printf("Byte %8lu (0x%08lx)  ", pos, pos);
		if (pos < (drive.boottrk * drive.sectrk * drive.secLength)) {
			term_printf("Physical sector %3lu  ", ((pos / drive.secLength) % drive.sectrk) + 1);
		} else {
			term_printf("Sector %3lu ", ((pos / drive.secLength) % drive.sectrk) + 1);
			term_printf("(physical %3d)  ", drive.skewtab[(pos / drive.secLength) % drive.sectrk] + 1);
		}
		term_printf("Offset %5lu  ", pos % drive.secLength);
		term_printf("Track %5lu", pos / (drive.secLength * drive.sectrk));
		term_xy(0, term_lines() - 3);
		term_printf("N)ext track    P)revious track");
		term_xy(0, term_lines() - 2);
		term_printf("n)ext record   p)revious record     f)orward byte      b)ackward byte");
		term_xy(0, term_lines() - 1);
		term_printf("i)nfo          q)uit");
		if (reload) {
			if (pos < (drive.boottrk * drive.sectrk * drive.secLength)) {
				err = Device_readSector(&drive.dev, pos / (drive.secLength * drive.sectrk), (pos / drive.secLength) % drive.sectrk, buf);
			} else {
				err = Device_readSector(&drive.dev, pos / (drive.secLength * drive.sectrk), drive.skewtab[(pos / drive.secLength) % drive.sectrk], buf);
			}
			if (err) {
				term_xy(0, 4);
				term_printf("Data can not be read: %s", err);
			} else {
				reload = 0;
			}
		}


		if /* position before end of system area */
		(pos < (drive.boottrk * drive.sectrk * drive.secLength)) {
			const char *msg;

			msg = "System area";
			term_xy((term_cols() - strlen(msg)) / 2, 0);
			term_printf(msg);
			term_xy(36, term_lines() - 3);
			term_printf("F)orward 16 byte   B)ackward 16 byte");
			if (!reload) {
				data(&drive, buf, pos);
			}
			switch (ch = term_getch()) {
		case 'F': /* next 16 byte */ {
				if (pos + 16 < (drive.sectrk * drive.tracks * (off_t)drive.secLength)) {
					if (pos / drive.secLength != (pos + 16) / drive.secLength) {
						reload = 1;
					}
					pos += 16;
				}
				break;
			}

		case 'B': /* previous 16 byte */ {
				if (pos >= 16) {
					if (pos / drive.secLength != (pos - 16) / drive.secLength) {
						reload = 1;
					}
					pos -= 16;
				}
				break;
			}

			}
		} else if /* position before end of directory area */
		(pos < (drive.boottrk * drive.sectrk * drive.secLength + drive.maxdir * 32)) {
			const char *msg;
			unsigned long entrystart = (pos & ~0x1f) % drive.secLength;
			int entry = (pos - (drive.boottrk * drive.sectrk * drive.secLength)) >> 5;
			int offset = pos & 0x1f;

			msg = "Directory area";
			term_xy((term_cols() - strlen(msg)) / 2, 0);
			term_printf(msg);
			term_xy(36, term_lines() - 3);
			term_printf("F)orward entry     B)ackward entry");

			term_xy(0, 13);
			term_printf("Entry %3d: ", entry);
			if /* free or used directory entry */
			((buf[entrystart] >= 0 && buf[entrystart] <= (drive.type == CPMFS_P2DOS ? 31 : 15)) || buf[entrystart] == (char)0xe5) {
				int i;

				if (buf[entrystart] == (char)0xe5) {
					if (offset == 0) {
						term_reverse(1);
					}
					term_printf("Free");
					term_reverse(0);
				} else {
					term_printf("Directory entry");
				}
				term_xy(0, 15);
				if (buf[entrystart] != (char)0xe5) {
					term_printf("User: ");
					if (offset == 0) {
						term_reverse(1);
					}
					term_printf("%2d", buf[entrystart]);
					term_reverse(0);
					term_printf(" ");
				}
				term_printf("Name: ");
				for (i = 0; i < 8; ++i) {
					if (offset == 1 + i) {
						term_reverse(1);
					}
					term_printf("%c", buf[entrystart + 1 + i] & 0x7f);
					term_reverse(0);
				}
				term_printf(" Extension: ");
				for (i = 0; i < 3; ++i) {
					if (offset == 9 + i) {
						term_reverse(1);
					}
					term_printf("%c", buf[entrystart + 9 + i] & 0x7f);
					term_reverse(0);
				}
				term_xy(0, 16);
				term_printf("Extent: %3d", ((buf[entrystart + 12] & 0xff) + ((buf[entrystart + 14] & 0xff) << 5)) / drive.extents);
				term_printf(" (low: ");
				if (offset == 12) {
					term_reverse(1);
				}
				term_printf("%2d", buf[entrystart + 12] & 0xff);
				term_reverse(0);
				term_printf(", high: ");
				if (offset == 14) {
					term_reverse(1);
				}
				term_printf("%2d", buf[entrystart + 14] & 0xff);
				term_reverse(0);
				term_printf(")");
				term_xy(0, 17);
				term_printf("Last extent record count: ");
				if (offset == 15) {
					term_reverse(1);
				}
				term_printf("%3d", buf[entrystart + 15] & 0xff);
				term_reverse(0);
				term_xy(0, 18);
				term_printf("Last record byte count: ");
				if (offset == 13) {
					term_reverse(1);
				}
				term_printf("%3d", buf[entrystart + 13] & 0xff);
				term_reverse(0);
				term_xy(0, 19);
				term_printf("Data blocks:");
				for (i = 0; i < 16; ++i) {
					unsigned int block = buf[entrystart + 16 + i] & 0xff;
					if (drive.size > 256) {
						term_printf(" ");
						if (offset == 16 + i || offset == 16 + i + 1) {
							term_reverse(1);
						}
						term_printf("%5d", block | (((buf[entrystart + 16 + ++i] & 0xff) << 8)));
						term_reverse(0);
					} else {
						term_printf(" ");
						if (offset == 16 + i) {
							term_reverse(1);
						}
						term_printf("%3d", block);
						term_reverse(0);
					}
				}
			} else if /* disc label */
			(buf[entrystart] == 0x20 && drive.type == CPMFS_DR3) {
				int i;
				const struct tm *tm;
				char s[30];

				if (offset == 0) {
					term_reverse(1);
				}
				term_printf("Disc label");
				term_reverse(0);
				term_xy(0, 15);
				term_printf("Label: ");
				for (i = 0; i < 11; ++i) {
					if (i + 1 == offset) {
						term_reverse(1);
					}
					term_printf("%c", buf[entrystart + 1 + i] & 0x7f);
					term_reverse(0);
				}
				term_xy(0, 16);
				term_printf("Bit 0,7: ");
				if (offset == 12) {
					term_reverse(1);
				}
				term_printf("Label %s", buf[entrystart + 12] & 1 ? "set" : "not set");
				term_printf(", password protection %s", buf[entrystart + 12] & 0x80 ? "set" : "not set");
				term_reverse(0);
				term_xy(0, 17);
				term_printf("Bit 4,5,6: ");
				if (offset == 12) {
					term_reverse(1);
				}
				term_printf("Time stamp ");
				if (buf[entrystart + 12] & 0x10) {
					term_printf("on create, ");
				} else {
					term_printf("not on create, ");
				}
				if (buf[entrystart + 12] & 0x20) {
					term_printf("on modification, ");
				} else {
					term_printf("not on modifiction, ");
				}
				if (buf[entrystart + 12] & 0x40) {
					term_printf("on access");
				} else {
					term_printf("not on access");
				}
				term_reverse(0);
				term_xy(0, 18);
				term_printf("Password: ");
				for (i = 0; i < 8; ++i) {
					char printable;

					if (offset == 16 + (7 - i)) {
						term_reverse(1);
					}
					printable = (buf[entrystart + 16 + (7 - i)] ^ buf[entrystart + 13]) & 0x7f;
					term_printf("%c", isprint(printable) ? printable : ' ');
					term_reverse(0);
				}
				term_printf(" XOR value: ");
				if (offset == 13) {
					term_reverse(1);
				}
				term_printf("0x%02x", buf[entrystart + 13] & 0xff);
				term_reverse(0);
				term_xy(0, 19);
				term_printf("Created: ");
				tm = cpmtime(buf[entrystart + 24], buf[entrystart + 25], buf[entrystart + 26], buf[entrystart + 27]);
				if (offset == 24 || offset == 25) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 26) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 27) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
				term_printf(" Updated: ");
				tm = cpmtime(buf[entrystart + 28], buf[entrystart + 29], buf[entrystart + 30], buf[entrystart + 31]);
				if (offset == 28 || offset == 29) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 30) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 31) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
			} else if /* time stamp */
			(buf[entrystart] == 0x21 && (drive.type == CPMFS_P2DOS || drive.type == CPMFS_DR3)) {
				const struct tm *tm;
				char s[30];

				if (offset == 0) {
					term_reverse(1);
				}
				term_printf("Time stamps");
				term_reverse(0);
				term_xy(0, 15);
				term_printf("3rd last extent: Created/Accessed ");
				tm = cpmtime(buf[entrystart + 1], buf[entrystart + 2], buf[entrystart + 3], buf[entrystart + 4]);
				if (offset == 1 || offset == 2) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 3) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 4) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
				term_printf(" Modified ");
				tm = cpmtime(buf[entrystart + 5], buf[entrystart + 6], buf[entrystart + 7], buf[entrystart + 8]);
				if (offset == 5 || offset == 6) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 7) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 8) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);

				term_xy(0, 16);
				term_printf("2nd last extent: Created/Accessed ");
				tm = cpmtime(buf[entrystart + 11], buf[entrystart + 12], buf[entrystart + 13], buf[entrystart + 14]);
				if (offset == 11 || offset == 12) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 13) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 14) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
				term_printf(" Modified ");
				tm = cpmtime(buf[entrystart + 15], buf[entrystart + 16], buf[entrystart + 17], buf[entrystart + 18]);
				if (offset == 15 || offset == 16) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 17) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 18) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);

				term_xy(0, 17);
				term_printf("    Last extent: Created/Accessed ");
				tm = cpmtime(buf[entrystart + 21], buf[entrystart + 22], buf[entrystart + 23], buf[entrystart + 24]);
				if (offset == 21 || offset == 22) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 23) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 24) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
				term_printf(" Modified ");
				tm = cpmtime(buf[entrystart + 25], buf[entrystart + 26], buf[entrystart + 27], buf[entrystart + 28]);
				if (offset == 25 || offset == 26) {
					term_reverse(1);
				}
				strftime(s, sizeof(s), "%x", tm);
				term_printf("%s", s);
				term_reverse(0);
				term_printf(" ");
				if (offset == 27) {
					term_reverse(1);
				}
				term_printf("%2d", tm->tm_hour);
				term_reverse(0);
				term_printf(":");
				if (offset == 28) {
					term_reverse(1);
				}
				term_printf("%02d", tm->tm_min);
				term_reverse(0);
			} else if /* password */
			(buf[entrystart] >= 16 && buf[entrystart] <= 31 && drive.type == CPMFS_DR3) {
				int i;

				if (offset == 0) {
					term_reverse(1);
				}
				term_printf("Password");
				term_reverse(0);

				term_xy(0, 15);
				term_printf("Name: ");
				for (i = 0; i < 8; ++i) {
					if (offset == 1 + i) {
						term_reverse(1);
					}
					term_printf("%c", buf[entrystart + 1 + i] & 0x7f);
					term_reverse(0);
				}
				term_printf(" Extension: ");
				for (i = 0; i < 3; ++i) {
					if (offset == 9 + i) {
						term_reverse(1);
					}
					term_printf("%c", buf[entrystart + 9 + i] & 0x7f);
					term_reverse(0);
				}

				term_xy(0, 16);
				term_printf("Password required for: ");
				if (offset == 12) {
					term_reverse(1);
				}
				if (buf[entrystart + 12] & 0x80) {
					term_printf("Reading ");
				}
				if (buf[entrystart + 12] & 0x40) {
					term_printf("Writing ");
				}
				if (buf[entrystart + 12] & 0x20) {
					term_printf("Deleting ");
				}
				term_reverse(0);

				term_xy(0, 17);
				term_printf("Password: ");
				for (i = 0; i < 8; ++i) {
					char printable;

					if (offset == 16 + (7 - i)) {
						term_reverse(1);
					}
					printable = (buf[entrystart + 16 + (7 - i)] ^ buf[entrystart + 13]) & 0x7f;
					term_printf("%c", isprint(printable) ? printable : ' ');
					term_reverse(0);
				}
				term_printf(" XOR value: ");
				if (offset == 13) {
					term_reverse(1);
				}
				term_printf("0x%02x", buf[entrystart + 13] & 0xff);
				term_reverse(0);
			} else /* bad status */ {
				term_printf("Bad status ");
				if (offset == 0) {
					term_reverse(1);
				}
				term_printf("0x%02x", buf[entrystart]);
				term_reverse(0);
			}

			if (!reload) {
				data(&drive, buf, pos);
			}
			switch (ch = term_getch()) {
		case 'F': /* next entry */ {
				if (pos + 32 < (drive.sectrk * drive.tracks * (off_t)drive.secLength)) {
					if (pos / drive.secLength != (pos + 32) / drive.secLength) {
						reload = 1;
					}
					pos += 32;
				}
				break;
			}

		case 'B': /* previous entry */ {
				if (pos >= 32) {
					if (pos / drive.secLength != (pos - 32) / drive.secLength) {
						reload = 1;
					}
					pos -= 32;
				}
				break;
			}

			}
		} else /* data area */ {
			const char *msg;

			msg = "Data area";
			term_xy((term_cols() - strlen(msg)) / 2, 0);
			term_printf(msg);
			if (!reload) {
				data(&drive, buf, pos);
			}
			ch = term_getch();
		}


		/* process common commands */
		switch (ch) {
	case 'n': /* next record */ {
			if (pos + 128 < (drive.sectrk * drive.tracks * (off_t)drive.secLength)) {
				if (pos / drive.secLength != (pos + 128) / drive.secLength) {
					reload = 1;
				}
				pos += 128;
			}
			break;
		}

	case 'p': /* previous record */ {
			if (pos >= 128) {
				if (pos / drive.secLength != (pos - 128) / drive.secLength) {
					reload = 1;
				}
				pos -= 128;
			}
			break;
		}

	case 'N': /* next track */ {
			if ((pos + drive.sectrk * drive.secLength) < (drive.sectrk * drive.tracks * drive.secLength)) {
				pos += drive.sectrk * drive.secLength;
				reload = 1;
			}
			break;
		}

	case 'P': /* previous track */ {
			if (pos >= drive.sectrk * drive.secLength) {
				pos -= drive.sectrk * drive.secLength;
				reload = 1;
			}
			break;
		}

	case 'b': /* byte back */ {
			if (pos) {
				if (pos / drive.secLength != (pos - 1) / drive.secLength) {
					reload = 1;
				}
				--pos;
			}
			break;
		}

	case 'f': /* byte forward */ {
			if (pos + 1 < drive.tracks * drive.sectrk * drive.secLength) {
				if (pos / drive.secLength != (pos + 1) / drive.secLength) {
					reload = 1;
				}
				++pos;
			}
			break;
		}

		case 'i':
			info(&drive, format, image);
			break;
		case 'm':
			map(&drive);
			break;
		}

	} while (ch != 'q');

	term_exit();
	exit(0);
}
