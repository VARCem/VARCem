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
 * Version:	@(#)emu.h	1.0.13	2018/04/02
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
#ifndef EMU_H
# define EMU_H


/* Configuration values. */
#define SERIAL_MAX	2
#define PARALLEL_MAX	1
#define SCREEN_RES_X	640
#define SCREEN_RES_Y	480

/* Filename and pathname info. */
#define NVR_PATH	L"nvr"
#define ROMS_PATH	L"roms"
#define MACHINES_PATH	L"machines"
#define VIDEO_PATH	L"video"
#define SCREENSHOT_PATH L"screenshots"

#define CONFIG_FILE	L"config.varc"
#define CONFIG_FILE_EXT	L".varc"
#define BIOS_FILE	L"bios.txt"


#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define ABS(x)		((x) > 0 ? (x) : -(x))


#ifdef __cplusplus
extern "C" {
#endif

/* Commandline option variables. */
extern int	dump_on_exit;			/* (O) dump regs on exit*/
extern int	do_dump_config;			/* (O) dump cfg after load */
extern int	start_in_fullscreen;		/* (O) start in fullscreen */
#ifdef _WIN32
extern int	force_debug;			/* (O) force debug output */
#endif
#ifdef USE_WX
extern int	video_fps;			/* (O) render speed in fps */
#endif
extern int	config_ro;			/* (O) dont modify cfg file */
extern int	settings_only;			/* (O) only the settings dlg */
extern wchar_t	log_path[1024];			/* (O) full path of logfile */


/* Configuration variables. */
extern int	window_w, window_h,		/* (C) window size and */
		window_x, window_y,		/*     position info */
		window_remember,
		vid_resize,			/* (C) allow resizing */
		invert_display,			/* (C) invert the display */
		suppress_overscan;		/* (C) suppress overscans */
extern int	scale;				/* (C) screen scale factor */
extern int	vid_api;			/* (C) video renderer */
extern int	vid_cga_contrast,		/* (C) video */
		video_fullscreen,		/* (C) video */
		video_fullscreen_first,		/* (C) video */
		video_fullscreen_scale,		/* (C) video */
		enable_overscan,		/* (C) video */
		force_43,			/* (C) video */
		vid_card,			/* (C) graphics/video card */
		video_speed;			/* (C) video */
extern int	serial_enabled[],		/* (C) enable serial ports */
		lpt_enabled,			/* (C) enable LPT ports */
		bugger_enabled;			/* (C) enable ISAbugger */
extern int	rctrl_is_lalt;			/* (C) set R-CTRL as L-ALT */
extern int	update_icons;			/* (C) enable icons updates */
#ifdef WALTJE
extern int	romdos_enabled;			/* (C) enable ROM DOS */
#endif
extern int	sound_is_float,			/* (C) sound uses FP values */
		GAMEBLASTER,			/* (C) sound option */
		GUS,				/* (C) sound option */
		SSI2001,			/* (C) sound option */
		voodoo_enabled;			/* (C) video option */
extern int	joystick_type;			/* (C) joystick type */
extern int	mem_size;			/* (C) memory size */
extern int	cpu_manufacturer,		/* (C) cpu manufacturer */
		cpu,				/* (C) cpu type */
		cpu_use_dynarec,		/* (C) cpu uses/needs Dyna */
		enable_external_fpu;		/* (C) enable external FPU */
extern int	enable_sync;			/* (C) enable time sync */
extern int	network_type;			/* (C) net provider type */
extern int	network_card;			/* (C) net interface num */
extern char	network_host[512];		/* (C) host network intf */


/* Global variables. */
extern char	emu_title[64];			/* full name of application */
extern char	emu_version[32];		/* short version ID string */
extern char	emu_fullversion[128];		/* full version ID string */
extern wchar_t	emu_path[1024];			/* emu installation path */
extern wchar_t	usr_path[1024];			/* path (dir) of user data */
extern wchar_t  cfg_path[1024];			/* full path of config file */
extern FILE	*stdlog;			/* file to log output to */
extern int	scrnsz_x,			/* current screen size, X */
		scrnsz_y;			/* current screen size, Y */
extern int	unscaled_size_x,		/* current unscaled size X */
		unscaled_size_y,		/* current unscaled size Y */
		efscrnsz_y;
extern int	config_changed;			/* config has changed */


/* Function prototypes. */
#ifdef HAVE_STDARG_H
extern void	pclog_ex(const char *fmt, va_list);
#endif
extern void	pclog(const char *fmt, ...);
extern void	fatal(const char *fmt, ...);
extern void	pc_version(const char *platform);
extern void	pc_path(wchar_t *dest, int dest_sz, wchar_t *src);
extern int	pc_init(int argc, wchar_t *argv[]);
extern int	pc_init_modules(void);
extern void	pc_close(void *threadid);
extern void	pc_reset_hard_close(void);
extern void	pc_reset_hard_init(void);
extern void	pc_reset_hard(void);
extern void	pc_reset(int hard);
extern void	pc_reload(wchar_t *fn);
extern void	pc_full_speed(void);
extern void	pc_speed_changed(void);
extern void	pc_thread(void *param);
extern void	pc_start(void);
extern void	pc_onesec(void);
extern void	set_screen_size(int x, int y);
extern void	set_screen_size_natural(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_H*/
