/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Main include file for the application.
 *
 * Version:	@(#)emu.h	1.0.1	2018/02/14
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

/* Version info. */
#define EMU_NAME	"VARCem"
#define EMU_NAME_W	L"VARCem"
#define EMU_VERSION	"0.1.0"
#define EMU_VERSION_W	L"0.1.0"

/* Filename and pathname info. */
#define CONFIG_FILE	L"varcem.cfg"
#define NVR_PATH        L"nvr"
#define SCREENSHOT_PATH L"screenshots"


/*FIXME: move to where it's needed (ui) */
#if defined(ENABLE_BUSLOGIC_LOG) || \
    defined(ENABLE_CDROM_LOG) || \
    defined(ENABLE_D86F_LOG) || \
    defined(ENABLE_FDC_LOG) || \
    defined(ENABLE_IDE_LOG) || \
    defined(ENABLE_NIC_LOG)
# define ENABLE_LOG_TOGGLES	1
#endif

/*FIXME: move to where it's needed (ui) */
#if defined(ENABLE_LOG_BREAKPOINT) || defined(ENABLE_VRAM_DUMP)
# define ENABLE_LOG_COMMANDS	1
#endif

#define MIN(a, b)             ((a) < (b) ? (a) : (b))


#ifdef __cplusplus
extern "C" {
#endif

/* Global variables. */
extern int	dump_on_exit;			/* (O) dump regs on exit*/
extern int	do_dump_config;			/* (O) dump cfg after load */
extern int	start_in_fullscreen;		/* (O) start in fullscreen */
#ifdef _WIN32
extern int	force_debug;			/* (O) force debug output */
extern uint64_t	unique_id;			/* (O) -H id */
extern uint64_t	source_hwnd;			/* (O) -H hwnd */
#endif
#ifdef USE_WX
extern int	video_fps;			/* (O) render speed in fps */
#endif
extern int	settings_only;			/* (O) only the settings dlg */
extern wchar_t	log_path[1024];			/* (O) full path of logfile */


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
		video_speed;			/* (C) video */
extern int	serial_enabled[],		/* (C) enable serial ports */
		lpt_enabled,			/* (C) enable LPT ports */
		bugger_enabled;			/* (C) enable ISAbugger */
extern int	rctrl_is_lalt;			/* (C) set R-CTRL as L-ALT */
extern int	update_icons;			/* (C) enable icons updates */
#ifdef WALTJE
extern int	romdos_enabled;			/* (C) enable ROM DOS */
#endif
extern int	gfxcard;			/* (C) graphics/video card */
extern int	sound_is_float,			/* (C) sound uses FP values */
		GAMEBLASTER,			/* (C) sound option */
		GUS,				/* (C) sound option */
		SSI2001,			/* (C) sound option */
		voodoo_enabled;			/* (C) video option */
extern int	mem_size;			/* (C) memory size */
extern int	cpu_manufacturer,		/* (C) cpu manufacturer */
		cpu,				/* (C) cpu type */
		cpu_use_dynarec,		/* (C) cpu uses/needs Dyna */
		enable_external_fpu;		/* (C) enable external FPU */


#ifdef ENABLE_LOG_TOGGLES
extern int	buslogic_do_log;
extern int	cdrom_do_log;
extern int	d86f_do_log;
extern int	fdc_do_log;
extern int	ide_do_log;
extern int	serial_do_log;
extern int	nic_do_log;
#endif

extern wchar_t	exe_path[1024];			/* path (dir) of executable */
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
extern void	set_screen_size(int x, int y);
extern void	set_screen_size_natural(void);
extern void	pc_reload(wchar_t *fn);
extern int	pc_init_modules(void);
extern int	pc_init(int argc, wchar_t *argv[]);
extern void	pc_close(void *threadid);
extern void	pc_reset_hard_close(void);
extern void	pc_reset_hard_init(void);
extern void	pc_reset_hard(void);
extern void	pc_reset(int hard);
extern void	pc_full_speed(void);
extern void	pc_speed_changed(void);
extern void	pc_send_cad(void);
extern void	pc_send_cae(void);
extern void	pc_send_cab(void);
extern void	pc_thread(void *param);
extern void	pc_start(void);
extern void	pc_onesec(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_H*/
