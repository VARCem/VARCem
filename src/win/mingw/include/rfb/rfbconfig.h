#ifndef _RFB_RFBCONFIG_H
# define _RFB_RFBCONFIG_H
 

/* Enable 24 bit per pixel in native framebuffer */
#ifndef LIBVNCSERVER_ALLOW24BPP 
# define LIBVNCSERVER_ALLOW24BPP  1 
#endif

/* Enable BackChannel communication */
#ifndef LIBVNCSERVER_BACKCHANNEL 
# define LIBVNCSERVER_BACKCHANNEL  1 
#endif

/* Use ffmpeg (for vnc2mpg) */
/* #undef LIBVNCSERVER_FFMPEG */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#ifndef LIBVNCSERVER_HAVE_ARPA_INET_H 
# define LIBVNCSERVER_HAVE_ARPA_INET_H  1 
#endif

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef LIBVNCSERVER_HAVE_DOPRNT */

/* Define to 1 if you have the <fcntl.h> header file. */
#ifndef LIBVNCSERVER_HAVE_FCNTL_H 
# define LIBVNCSERVER_HAVE_FCNTL_H  1 
#endif

/* Define to 1 if you have the `fork' function. */
/* #undef LIBVNCSERVER_HAVE_FORK  */

/* Define to 1 if you have the `ftime' function. */
#ifndef LIBVNCSERVER_HAVE_FTIME 
#define LIBVNCSERVER_HAVE_FTIME  1 
#endif

/* Define to 1 if you have the `gethostbyname' function. */
#ifndef LIBVNCSERVER_HAVE_GETHOSTBYNAME 
#define LIBVNCSERVER_HAVE_GETHOSTBYNAME  1 
#endif

/* Define to 1 if you have the `gethostname' function. */
#ifndef LIBVNCSERVER_HAVE_GETHOSTNAME 
#define LIBVNCSERVER_HAVE_GETHOSTNAME  1 
#endif

/* Define to 1 if you have the `gettimeofday' function. */
#ifndef LIBVNCSERVER_HAVE_GETTIMEOFDAY 
#define LIBVNCSERVER_HAVE_GETTIMEOFDAY  1 
#endif

/* Define to 1 if you have the `inet_ntoa' function. */
#ifndef LIBVNCSERVER_HAVE_INET_NTOA 
#define LIBVNCSERVER_HAVE_INET_NTOA  1 
#endif

/* Define to 1 if you have the <inttypes.h> header file. */
#ifndef LIBVNCSERVER_HAVE_INTTYPES_H 
#define LIBVNCSERVER_HAVE_INTTYPES_H  1 
#endif

/* Define to 1 if you have the `jpeg' library (-ljpeg). */
/* #undef LIBVNCSERVER_HAVE_LIBJPEG */

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #undef LIBVNCSERVER_HAVE_LIBNSL  */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#ifndef LIBVNCSERVER_HAVE_LIBPTHREAD 
#ifndef _WIN32
# define LIBVNCSERVER_HAVE_LIBPTHREAD  1 
#endif
#endif

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef LIBVNCSERVER_HAVE_LIBSOCKET */

/* Define to 1 if you have the `z' library (-lz). */
#ifndef LIBVNCSERVER_HAVE_LIBZ 
#define LIBVNCSERVER_HAVE_LIBZ  1 
#endif

/* Define to 1 if your system has a GNU libc compatible `malloc' function, and
   to 0 otherwise. */
#ifndef LIBVNCSERVER_HAVE_MALLOC 
#define LIBVNCSERVER_HAVE_MALLOC  1 
#endif

/* Define to 1 if you have the `memmove' function. */
#ifndef LIBVNCSERVER_HAVE_MEMMOVE 
#define LIBVNCSERVER_HAVE_MEMMOVE  1 
#endif

/* Define to 1 if you have the <memory.h> header file. */
#ifndef LIBVNCSERVER_HAVE_MEMORY_H 
#define LIBVNCSERVER_HAVE_MEMORY_H  1 
#endif

/* Define to 1 if you have the `memset' function. */
#ifndef LIBVNCSERVER_HAVE_MEMSET 
#define LIBVNCSERVER_HAVE_MEMSET  1 
#endif

/* Define to 1 if you have the `mkfifo' function. */
/* #unndef LIBVNCSERVER_HAVE_MKFIFO  */

/* Define to 1 if you have the <netdb.h> header file. */
#ifndef LIBVNCSERVER_HAVE_NETDB_H 
#define LIBVNCSERVER_HAVE_NETDB_H  1 
#endif

/* Define to 1 if you have the <netinet/in.h> header file. */
/* #undef LIBVNCSERVER_HAVE_NETINET_IN_H  */

/* Define to 1 if you have the `select' function. */
#ifndef LIBVNCSERVER_HAVE_SELECT 
#define LIBVNCSERVER_HAVE_SELECT  1 
#endif

/* Define to 1 if you have the `setsid' function. */
/* #undef LIBVNCSERVER_HAVE_SETSID */

/* Define to 1 if you have the `socket' function. */
#ifndef LIBVNCSERVER_HAVE_SOCKET 
#define LIBVNCSERVER_HAVE_SOCKET  1 
#endif

/* Define to 1 if `stat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef LIBVNCSERVER_HAVE_STAT_EMPTY_STRING_BUG */

/* Define to 1 if you have the <stdint.h> header file. */
#ifndef LIBVNCSERVER_HAVE_STDINT_H 
#define LIBVNCSERVER_HAVE_STDINT_H  1 
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#ifndef LIBVNCSERVER_HAVE_STDLIB_H 
#define LIBVNCSERVER_HAVE_STDLIB_H  1 
#endif

/* Define to 1 if you have the `strchr' function. */
#ifndef LIBVNCSERVER_HAVE_STRCHR 
#define LIBVNCSERVER_HAVE_STRCHR  1 
#endif

/* Define to 1 if you have the `strcspn' function. */
#ifndef LIBVNCSERVER_HAVE_STRCSPN 
#define LIBVNCSERVER_HAVE_STRCSPN  1 
#endif

/* Define to 1 if you have the `strdup' function. */
#ifndef LIBVNCSERVER_HAVE_STRDUP 
#define LIBVNCSERVER_HAVE_STRDUP  1 
#endif

/* Define to 1 if you have the `strerror' function. */
#ifndef LIBVNCSERVER_HAVE_STRERROR 
#define LIBVNCSERVER_HAVE_STRERROR  1 
#endif

/* Define to 1 if you have the `strftime' function. */
#ifndef LIBVNCSERVER_HAVE_STRFTIME 
#define LIBVNCSERVER_HAVE_STRFTIME  1 
#endif

/* Define to 1 if you have the <strings.h> header file. */
#ifndef LIBVNCSERVER_HAVE_STRINGS_H 
#define LIBVNCSERVER_HAVE_STRINGS_H  1 
#endif

/* Define to 1 if you have the <string.h> header file. */
#ifndef LIBVNCSERVER_HAVE_STRING_H 
#define LIBVNCSERVER_HAVE_STRING_H  1 
#endif

/* Define to 1 if you have the `strstr' function. */
#ifndef LIBVNCSERVER_HAVE_STRSTR 
#define LIBVNCSERVER_HAVE_STRSTR  1 
#endif

/* Define to 1 if you have the <syslog.h> header file. */
/* #undef LIBVNCSERVER_HAVE_SYSLOG_H  */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef LIBVNCSERVER_HAVE_SYS_SOCKET_H  */

/* Define to 1 if you have the <sys/stat.h> header file. */
#ifndef LIBVNCSERVER_HAVE_SYS_STAT_H 
#define LIBVNCSERVER_HAVE_SYS_STAT_H  1 
#endif

/* Define to 1 if you have the <sys/timeb.h> header file. */
#ifndef LIBVNCSERVER_HAVE_SYS_TIMEB_H 
#define LIBVNCSERVER_HAVE_SYS_TIMEB_H  1 
#endif

/* Define to 1 if you have the <sys/time.h> header file. */
#ifndef LIBVNCSERVER_HAVE_SYS_TIME_H 
#define LIBVNCSERVER_HAVE_SYS_TIME_H  1 
#endif

/* Define to 1 if you have the <sys/types.h> header file. */
#ifndef LIBVNCSERVER_HAVE_SYS_TYPES_H 
#define LIBVNCSERVER_HAVE_SYS_TYPES_H  1 
#endif

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#ifndef LIBVNCSERVER_HAVE_SYS_WAIT_H 
#define LIBVNCSERVER_HAVE_SYS_WAIT_H  1 
#endif

/* Define to 1 if you have the <unistd.h> header file. */
#ifndef LIBVNCSERVER_HAVE_UNISTD_H 
#define LIBVNCSERVER_HAVE_UNISTD_H  1 
#endif

/* Define to 1 if you have the `vfork' function. */
/* #undef LIBVNCSERVER_HAVE_VFORK  */

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef LIBVNCSERVER_HAVE_VFORK_H */

/* Define to 1 if you have the `vprintf' function. */
#ifndef LIBVNCSERVER_HAVE_VPRINTF 
#define LIBVNCSERVER_HAVE_VPRINTF  1 
#endif

/* Define to 1 if `fork' works. */
/* #undef LIBVNCSERVER_HAVE_WORKING_FORK  */

/* Define to 1 if `vfork' works. */
/* #undef LIBVNCSERVER_HAVE_WORKING_VFORK  */

/* XKEYBOARD extension build environment present */
/* #undef LIBVNCSERVER_HAVE_XKEYBOARD */

/* MIT-SHM extension build environment present */
/* #undef LIBVNCSERVER_HAVE_XSHM */

/* XTEST extension build environment present */
/* #undef LIBVNCSERVER_HAVE_XTEST */

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
/* #undef LIBVNCSERVER_LSTAT_FOLLOWS_SLASHED_SYMLINK */

/* Name of package */
#ifndef LIBVNCSERVER_PACKAGE 
#define LIBVNCSERVER_PACKAGE  "LibVNCServer" 
#endif

/* Define to the address where bug reports for this package should be sent. */
#ifndef LIBVNCSERVER_PACKAGE_BUGREPORT 
#define LIBVNCSERVER_PACKAGE_BUGREPORT  "http://sourceforge.net/projects/libvncserver" 
#endif

/* Define to the full name of this package. */
#ifndef LIBVNCSERVER_PACKAGE_NAME 
#define LIBVNCSERVER_PACKAGE_NAME  "LibVNCServer" 
#endif

/* Define to the full name and version of this package. */
#ifndef LIBVNCSERVER_PACKAGE_STRING 
#define LIBVNCSERVER_PACKAGE_STRING  "LibVNCServer 0.7" 
#endif

/* Define to the one symbol short name of this package. */
#ifndef LIBVNCSERVER_PACKAGE_TARNAME 
#define LIBVNCSERVER_PACKAGE_TARNAME  "libvncserver" 
#endif

/* Define to the version of this package. */
#ifndef LIBVNCSERVER_PACKAGE_VERSION 
#define LIBVNCSERVER_PACKAGE_VERSION  "0.7" 
#endif

/* The number of bytes in type char */
/* #undef LIBVNCSERVER_SIZEOF_CHAR */

/* The number of bytes in type int */
/* #undef LIBVNCSERVER_SIZEOF_INT */

/* The number of bytes in type long */
/* #undef LIBVNCSERVER_SIZEOF_LONG */

/* The number of bytes in type short */
/* #undef LIBVNCSERVER_SIZEOF_SHORT */

/* The number of bytes in type void* */
/* #undef LIBVNCSERVER_SIZEOF_VOIDP */

/* Define to 1 if you have the ANSI C header files. */
#ifndef LIBVNCSERVER_STDC_HEADERS 
#define LIBVNCSERVER_STDC_HEADERS  1 
#endif

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#ifndef LIBVNCSERVER_TIME_WITH_SYS_TIME 
#define LIBVNCSERVER_TIME_WITH_SYS_TIME  1 
#endif

/* Version number of package */
#ifndef LIBVNCSERVER_VERSION 
#define LIBVNCSERVER_VERSION  "0.7" 
#endif

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef LIBVNCSERVER_WORDS_BIGENDIAN */

/* Define to 1 if the X Window System is missing or not being used. */
#ifndef LIBVNCSERVER_X_DISPLAY_MISSING 
#define LIBVNCSERVER_X_DISPLAY_MISSING  1 
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef _libvncserver_const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef _libvncserver_inline */
#endif

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef _libvncserver_malloc */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef _libvncserver_pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef _libvncserver_size_t */

/* The type for socklen */
/* #undef _libvncserver_socklen_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef _libvncserver_vfork */
 
/* once: _RFB_RFBCONFIG_H */
#endif
