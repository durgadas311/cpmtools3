#ifndef TERM_H
#define TERM_H

#ifdef __cplusplus
extern "C" {
#endif

extern void term_init(void);
extern void term_exit(void);
extern void term_clear(void);
extern void term_xy(int x, int y);
extern void term_printf(char const *fmt, ...);
extern void term_putch(char ch);
extern int term_getch(void);
extern void term_reverse(int on);
extern int term_lines(void);
extern int term_cols(void);

#ifdef __cplusplus
}
#endif

#endif
