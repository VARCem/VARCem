/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the various platform support functions.
 *
 * Version:	@(#)plat.h	1.0.19	2018/10/16
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EMU_PLAT_H
# define EMU_PLAT_H


/* The Win32 API uses _wcsicmp and _stricmp. */
#ifdef _WIN32
# ifndef _MSC_VER
#  undef swprintf
#  define swprintf __swprintf
# endif
# define wcsncasecmp	_wcsnicmp
# define wcscasecmp	_wcsicmp
# define strncasecmp	_strnicmp
# define strcasecmp	_stricmp
#endif

#if defined(UNIX) && defined(FREEBSD)
/* FreeBSD has largefile by default. */
# define fopen64        fopen
# define fseeko64       fseeko
# define ftello64       ftello
# define off64_t        off_t
#elif defined(_MSC_VER)
# define fopen64        fopen
# define fseeko64       _fseeki64
# define ftello64       _ftelli64
# define off64_t        off_t
#endif


#ifdef _MSC_VER
# define UNUSED(arg) arg
#else
  /* A hack (GCC-specific?) to allow us to ignore unused parameters. */
# define UNUSED(arg)	__attribute__((unused))arg
#endif

/* Return the size (in wchar's) of a wchar_t array. */
#define sizeof_w(x)	(sizeof((x)) / sizeof(wchar_t))


#if defined(_WIN32) && !defined(_MSC_VER)
/*
 * MinGW uses the very old Visual Studio 6.0 C runtime (and even ises
 * it..) for licensing reasons. All this means, that it has the old
 * Microsoft-style string specifiers for wide strings, which predate
 * (and are incompatible with) the ISO ones. The (inlined) function
 * below, and the #undef above, are to redirect "swprintf" used in 
 * the code the inlined one, which basically redirects to the ISO
 * variant MinGW also provides.  Yum!  --FvK
 */
static __inline int
__swprintf(wchar_t *__dst, UNUSED(size_t __cnt), const wchar_t *__fmt, ...) \
{									    \
  register int __retval;						    \
  __builtin_va_list __local_argv;					    \
									    \
  __builtin_va_start( __local_argv, __fmt );				    \
  __retval = __mingw_vswprintf( __dst, __fmt, __local_argv );		    \
  __builtin_va_end( __local_argv );					    \
									    \
  return __retval;							    \
}
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Define a "vidapi", or, rather, a Renderer API. */
typedef struct {
    const char	*name;
    int		local;
    int		(*init)(int fs);
    void	(*close)(void);
    void	(*reset)(int fs);
    void	(*resize)(int x, int y);
    int		(*pause)(void);
    void	(*screenshot)(const wchar_t *fn);
    int		(*is_available)(void);
} vidapi_t;


/* Global variables residing in the platform module. */
extern int	quited;				/* system exit requested */
extern const vidapi_t *plat_vidapis[];


/* System-related functions. */
#ifdef _WIN32
extern void	plat_console(int on);
#endif
extern wchar_t	*fix_emu_path(const wchar_t *str);
extern FILE	*plat_fopen(const wchar_t *path, const wchar_t *mode);
extern void	plat_remove(const wchar_t *path);
extern int	plat_getcwd(wchar_t *bufp, int max);
extern int	plat_chdir(const wchar_t *path);
extern void	plat_tempfile(wchar_t *bufp, const wchar_t *prefix, const wchar_t *suffix);
extern void	plat_get_exe_name(wchar_t *path, int size);
extern wchar_t	*plat_get_basename(const wchar_t *path);
extern wchar_t	*plat_get_filename(const wchar_t *path);
extern wchar_t	*plat_get_extension(const wchar_t *path);
extern void	plat_append_filename(wchar_t *dest, const wchar_t *s1, const wchar_t *s2);
extern void	plat_append_slash(wchar_t *path);
extern int	plat_path_abs(const wchar_t *path);
extern int	plat_dir_check(const wchar_t *path);
extern int	plat_dir_create(const wchar_t *path);
extern uint64_t	plat_timer_read(void);
extern uint32_t	plat_get_ticks(void);
extern void	plat_delay_ms(uint32_t count);
extern void	plat_mouse_capture(int on);
extern int	plat_kbd_state(void);
extern void	plat_fullscreen(int on);
#ifdef EMU_UI_H
extern const string_t *plat_lang_load(lang_t *ptr);
#endif
extern void	plat_lang_scan(void);
extern void	plat_lang_set(int id);


/* Dynamic Module Loader interface. */
typedef struct {
    const char	*name;
    void	*func;
} dllimp_t;


extern void	*dynld_module(const char *, const dllimp_t *);
extern void	dynld_close(void *);


/* Emulator start/stop support functions. */
extern void	plat_start(void);
extern void	plat_stop(void);

extern void	plat_midi_init(void);
extern void	plat_midi_close(void);
extern void	plat_midi_play_msg(uint8_t *msg);
extern void	plat_midi_play_sysex(uint8_t *sysex, unsigned int len);
extern int	plat_midi_write(uint8_t val);

extern int	plat_midi_get_num_devs();
extern void	plat_midi_get_dev_name(int num, char *s);


/* Thread support. */
typedef void thread_t;
typedef void event_t;
typedef void mutex_t;

extern thread_t	*thread_create(void (*thread_func)(void *param), void *param);
extern void	thread_kill(thread_t *arg);
extern int	thread_wait(thread_t *arg, int timeout);
extern event_t	*thread_create_event(void);
extern void	thread_set_event(event_t *arg);
extern void	thread_reset_event(event_t *arg);
extern int	thread_wait_event(event_t *arg, int timeout);
extern void	thread_destroy_event(event_t *arg);

extern mutex_t	*thread_create_mutex(const wchar_t *name);
extern void	thread_close_mutex(mutex_t *arg);
extern int	thread_wait_mutex(mutex_t *arg);
extern int	thread_release_mutex(mutex_t *mutex);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_PLAT_H*/
