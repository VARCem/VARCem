/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Main include file for the application.
 *
 * Version:	@(#)emu.h	1.0.38	2021/03/18
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#ifndef EMU_H
# define EMU_H


/* Configuration values. */
#ifdef _WIN32
#define SCREEN_RES_X	640
#define SCREEN_RES_Y	480
#else
#define SCREEN_RES_X	800
#define SCREEN_RES_Y	600
#endif

/* Pre-defined directory names. */
#define LANGUAGE_PATH	L"lang"
#define NVR_PATH	L"nvr"
#define PLUGINS_PATH	L"plugins"
#define ROMS_PATH	L"roms"
# define MOVED_PATH	L".moved"
# define MACHINES_PATH	L"machines"
# define VIDEO_PATH	L"video"
# define PRINTERS_PATH	L"printer"
# define PFONTS_PATH	L"fonts"
#define SCREENSHOT_PATH L"screenshots"
#define PRINTER_PATH	L"printer"

/* Pre-defined file names and extensions. */
#define LANG_FILE	L"VARCem-"
#define BIOS_FILE	L"bios.txt"
#define MOVED_FILE	L"moved.txt"
#define CONFIG_FILE	L"config.varc"
#define CONFIG_FILE_EXT	L".varc"
#define NVR_FILE_EXT	L".nvr"
#define DUMP_FILE_EXT	L".dmp"

/* Pre-defined URLs to websites. */
#define URL_VARCEM	L"https://www.VARCem.com/"
#define URL_PAYPAL	L"https://www.paypal.me/VARCem"


/*
 * Definitions for the logging system.
 *
 * First, let us start by defining the varrious logging
 * levels in use. Some things must always logged (for
 * example, serious errors), while other info is nice
 * to have, but not always.
 *
 * By default, the system starts up in the INFO level,
 * so, messages up to and including LOG_INFO will be
 * written to the logfile. The --quiet commandline
 * option will lower this to LOG_ERROR level, meaning,
 * only real errors will be shown.
 */
#define LOG_ALWAYS	0			// this level always shows
#define LOG_ERR		LOG_ALWAYS		// errors always logged
#define LOG_INFO	1			// informational logging
#define LOG_DEBUG	2			// debug-level info
#define LOG_DETAIL	3			// more detailed info
#define LOG_LOWLEVEL	4			// pretty verbose stuff

/*
 * Now define macros that can be used in the code,
 * and which always react the same way. These are
 * just shorthands to keep the code clean-ish.
 */
#define ERRLOG(...)	pclog(LOG_ERR, __VA_ARGS__)	// always logged
#define INFO(...)	pclog(LOG_INFO, __VA_ARGS__)	// informational

/*
 * Most modules in the program will use the main
 * logging facility provided by the pc.c file, but
 * a module CAN use its own logging function for
 * debugging, for example while it is being worked
 * on. Those modules should define the 'dbglog'
 * macro to their own function before including the
 * main include file, emu.h, to make the logging
 * macros use that local function.
 *
 * By default, if none was defined, we define it
 * to be the main logging function.
 */
#ifndef dbglog
# define dbglog pclog
#endif

/*
 * The debug logging macros are also shorthands to
 * keep the code reasonably clean from #ifdef and
 * other clutter.
 *
 * Macros for several levels are provided.
 */
#ifdef _LOGGING
# define DBGLOG(x, ...)	dbglog(LOG_DEBUG+(x), __VA_ARGS__)
#else
# define DBGLOG(x, ...)	do {/*nothing*/} while(0)
#endif
#define DEBUG(...)	DBGLOG(0, __VA_ARGS__)	/* default-level */


/* For systems that do not provide these. */
#ifndef MIN
# define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef ABS
# define ABS(x)		((x) > 0 ? (x) : -(x))
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Define global types. */
typedef void *priv_t;				// generic "handle" type


/* Commandline option variables. */
extern int	dump_on_exit;			// (O) dump regs on exit
extern int	do_dump_config;			// (O) dump cfg after load
extern int	start_in_fullscreen;		// (O) start in fullscreen
#ifdef _WIN32
extern int	force_debug;			// (O) force debug output
#endif
#ifdef USE_WX
extern int	video_fps;			// (O) render speed in fps
#endif
extern int	config_ro;			// (O) dont modify cfg file
extern int	config_keep_space;		// (O) keep spaces in cfg
extern int	settings_only;			// (O) only the settings dlg
extern int	log_level;			// (O) global logging level
extern wchar_t	log_path[1024];			// (O) full path of logfile

/* Global variables. */
extern char	emu_title[64];			// full name of application
extern char	emu_version[32];		// short version ID string
extern char	emu_fullversion[128];		// full version ID string
extern wchar_t	exe_path[1024];			// emu executable path
extern wchar_t	emu_path[1024];			// emu installation path
extern wchar_t	usr_path[1024];			// path (dir) of user data
extern wchar_t  cfg_path[1024];			// full path of config file
extern int	mem_size;			// configured memory size
extern int	emu_lang_id;			// current language ID
extern int	scrnsz_x,			// current screen size, X
		scrnsz_y;			// current screen size, Y
extern int	config_changed,			// config has changed
		dopause,			// system is paused
		doresize,			// screen resize requested
		mouse_capture;			// mouse is captured in app
extern int	AT,				// machine is AT class
		MCA,				// machine has MCA bus
		PCI;				// machine has PCI bus

#ifdef _LOGGING
extern int	pci_do_log;
extern int	keyboard_do_log;
extern int	mouse_do_log;
extern int	game_do_log;
extern int	parallel_do_log;
extern int	serial_do_log;
extern int	fdc_do_log;
extern int	fdd_do_log;
extern int	d86f_do_log;
extern int	hdc_do_log;
extern int	hdd_do_log;
extern int	zip_do_log;
extern int	cdrom_do_log;
extern int	cdrom_image_do_log;
extern int	cdrom_host_do_log;
extern int	sound_do_log;
extern int	sound_midi_do_log;
extern int	sound_card_do_log;
extern int	network_do_log;
extern int	network_card_do_log;
extern int	scsi_do_log;
extern int	scsi_card_do_log;
extern int	scsi_cdrom_do_log;
extern int	scsi_disk_do_log;
extern int	video_do_log;
extern int	video_card_do_log;
#endif


/* Function prototypes. */
#ifdef HAVE_STDARG_H
extern void		pclog_ex(const char *fmt, va_list);
#endif
extern void		pclog(int level, const char *fmt, ...);
extern void		pclog_repeat(int enabled);
extern void		pclog_dump(int num);
extern void		fatal(const char *fmt, ...);
extern void		pc_version(const char *platform);
extern void		pc_path(wchar_t *dest, int dest_sz, const wchar_t *src);
extern int		pc_setup(int argc, wchar_t *argv[]);
extern int		pc_init(void);
extern void		pc_close(void *threadid);
extern void		pc_reset_hard_close(void);
extern void		pc_reset_hard_init(void);
extern void		pc_reset_hard(void);
extern void		pc_reset(int hard);
extern void		pc_reload(const wchar_t *fn);
extern void		pc_set_speed(int);
extern void		pc_thread(void *param);
extern void		pc_pause(int p);
extern void		pc_onesec(void);
extern void		set_screen_size(int x, int y);
extern void		set_screen_size_natural(void);
extern void		get_screen_size_natural(int *x, int *y);

extern const wchar_t	*get_string(int id);
extern uint32_t		get_val(const char *str);

#ifndef USE_STANDARD_MALLOC
extern void		*mem_alloc(size_t sz);
#endif

#ifdef __cplusplus
}
#endif


#endif	/*EMU_H*/
