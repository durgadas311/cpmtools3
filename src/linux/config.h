/* config.h.  Generated from config.h.in by configure.  */
#define HAVE_FCNTL_H 1
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#define HAVE_LIMITS_H 1
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define HAVE_UNISTD_H 1
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* #undef HAVE_WINDOWS_H */
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

/* #undef HAVE_WINIOCTL_H */
#ifdef HAVE_WINIOCTL_H
#include <winioctl.h>
#endif

/* #undef HAVE_LIBDSK_H */
#ifdef HAVE_LIBDSK_H
#include <libdsk.h>
#endif

#define HAVE_SYS_TYPES_H 1
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#define HAVE_SYS_STAT_H 1
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

/* #undef HAVE_SYS_UTIME_H */
#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif

#define HAVE_UTIME_H 1
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

/* #undef NEED_NCURSES */
/* #undef HAVE_NCURSES_NCURSES_H */

/* Define either for large file support, if your OS needs them. */
/* #undef _FILE_OFFSET_BITS */
/* #undef _LARGE_FILES */

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX _MAX_PATH
#endif

/* There is not standard header for this, yet it is needed for
 * timezone changes.
 */
#ifndef environ
extern char **environ;
#endif

/* If types are missing, the script defines.awk contained
 * in configure.status should create definitions here.
 */
