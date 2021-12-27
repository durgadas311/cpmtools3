#include "config.h"

#include <assert.h>
#include <ctype.h>
#ifdef NEED_NCURSES
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#else
#include <curses.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "term.h"

void term_init(void) {
	initscr();
	noecho();
	raw();
	nonl();
	idlok(stdscr, TRUE);
	idcok(stdscr, TRUE);
	keypad(stdscr, TRUE);
	clear();
}

void term_exit(void) {
	move(LINES - 1, 0);
	refresh();
	echo();
	noraw();
	endwin();
}

void term_clear(void) {
	clear();
}

void term_xy(int x, int y) {
	move(y, x);
}

void term_printf(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vw_printw(stdscr, fmt, ap);
	va_end(ap);
}

void term_putch(char ch) {
	addch(ch);
}

int term_getch(void) {
	return getch();
}

void term_reverse(int on) {
	if (on) {
		attron(A_REVERSE);
	} else {
		attroff(A_REVERSE);
	}
}

int term_lines(void) {
	return LINES;
}

int term_cols(void) {
	return COLS;
}
