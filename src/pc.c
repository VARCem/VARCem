/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Main emulator module where most things are controlled.
 *
 * Version:	@(#)pc.c	1.0.82	2021/03/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "emu.h"
#include "version.h"
#include "config.h"
#include "timer.h"
#include "cpu/cpu.h"
#ifdef USE_DYNAREC
# include "cpu/x86.h"
# include "cpu/codegen.h"
#endif
#include "machines/machine.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "devices/system/clk.h"
#include "devices/system/dma.h"
#include "devices/system/pic.h"
#include "random.h"
#include "device.h"
#include "nvr.h"
#include "devices/ports/game.h"
#include "devices/ports/serial.h"
#include "devices/ports/parallel.h"
#include "devices/input/game/joystick.h"
#include "devices/input/keyboard.h"
#include "devices/input/mouse.h"
#include "devices/floppy/fdd.h"
#include "devices/floppy/fdd_common.h"
#include "devices/floppy/fdc.h"
#include "devices/floppy/fdc_pii15xb.h"
#include "devices/disk/hdd.h"
#include "devices/disk/hdc.h"
#include "devices/scsi/scsi.h"
#include "devices/scsi/scsi_device.h"
#include "devices/scsi/scsi_disk.h"
#include "devices/disk/zip.h"
#include "devices/cdrom/cdrom.h"
#include "devices/network/network.h"
#include "devices/sound/sound.h"
#include "devices/video/video.h"
#include "devices/misc/bugger.h"
#include "devices/misc/isamem.h"
#include "devices/misc/isartc.h"
#include "devices/misc/amidisk.h"
#include "ui/ui.h"
#include "plat.h"


#define PCLOG_BUFF_SIZE	1024			/* buffer for one line */


/* Commandline options. */
int		dump_on_exit = 0;		/* (O) dump regs on exit */
int		do_dump_config = 0;		/* (O) dump config on load */
int		start_in_fullscreen = 0;	/* (O) start in fullscreen */
#ifdef _WIN32
int		force_debug = 0;		/* (O) force debug output */
#endif
#ifdef USE_WX
int		video_fps = RENDER_FPS;		/* (O) render speed in fps */
#endif
int		settings_only = 0;		/* (O) only the settings dlg */
int		config_ro = 0;			/* (O) dont modify cfg file */
int		config_keep_space = 0;		/* (O) keep spaces in cfg */
int		log_level = LOG_INFO;		/* (O) global logging level */
wchar_t 	log_path[1024] = { L'\0'};	/* (O) full path of logfile */

/* Configuration values. */
config_t	config;				/* (C) active configuration */

/* Global variables. */
char		emu_title[64];			/* full name of application */
char		emu_version[32];		/* short version ID string */
char		emu_fullversion[128];		/* full version ID string */
wchar_t		exe_path[1024];			/* emu executable path */
wchar_t		emu_path[1024];			/* emu installation path */
wchar_t		usr_path[1024];			/* path (dir) of user data */
wchar_t		cfg_path[1024];			/* full path of config file */
int		emu_lang_id;			/* current language ID */
int		scrnsz_x = SCREEN_RES_X,	/* current screen size, X */
		scrnsz_y = SCREEN_RES_Y;	/* current screen size, Y */
int		config_changed,			/* config has changed */
		dopause = 0,			/* system is paused */
		doresize = 0,			/* screen resize requested */
		mouse_capture = 0;		/* mouse is captured in app */
int		AT,				/* machine is AT class */
		MCA,				/* machine has MCA bus */
		PCI;				/* machine has PCI bus */

/* Local variables. */
static int	fps,				/* statistics */
		framecount,
		title_update;			/* we want title updated */
static int	unscaled_size_x = SCREEN_RES_X,	/* current unscaled size X */
		unscaled_size_y = SCREEN_RES_Y,	/* current unscaled size Y */
		efscrnsz_y = SCREEN_RES_Y;

static FILE	*logfp = NULL;			/* logging variables */
static char	logbuff[PCLOG_BUFF_SIZE];
static int	logseen = 0;
static int	logdetect = 1;


/*
 * Log something to the logfile or stdout.
 *
 * To avoid excessively-large logfiles because some
 * module repeatedly logs, we keep track of what is
 * being logged, and catch repeating entries.
 *
 * Note: we need fairly large buffers here, to allow
 *       for the network code dumping packet content
 *       with this.
 */
void
pclog_ex(const char *fmt, va_list ap)
{
    char temp[PCLOG_BUFF_SIZE];
    FILE *fp;

    if (fmt == NULL) {
	/* Initialize. */
	logseen = 0;
	return;
    }

    /* If a logpath was set, override the default. */
    if (log_path[0] != L'\0') {
	fp = plat_fopen(log_path, L"w");
	if (fp != NULL) {
		/* Set the new logging handle. */
		logfp = fp;

		/* Clear the path so we do not try this again. */
		memset(log_path, 0x00, sizeof(log_path));
	}
    }

    if (logdetect) {
	vsprintf(temp, fmt, ap);
	if (strcmp(logbuff, temp)) {
		if (logseen)
			fprintf(logfp, "*** %i repeats ***\n", logseen);
		logseen = 0;
		if (logdetect)
			strcpy(logbuff, temp);
		fprintf(logfp, temp);
	} else
		logseen++;
    } else {
	/* Not detecting duplicates, do not buffer. */
	vfprintf(logfp, fmt, ap);
    }

    fflush(logfp);
}


/* Log something. We only do this in non-release builds. */
void
pclog(int level, const char *fmt, ...)
{
    va_list ap;

    if (fmt == NULL) {
	fflush(logfp);
	fclose(logfp);
	logfp = NULL;
	return;
    }

    if (log_level >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
}


/* Enable or disable detection of repeated info being logged. */
void
pclog_repeat(int enabled)
{
    logdetect = !!enabled;
}


/* Log a block of code around the current CS:IP. */
void
pclog_dump(int num)
{
    char buff[128];
    uint32_t addr;
    char *sp = NULL;
    int i, k;

    /* We make the current PC be in the middle of the block. */
    num >>= 1;
    i = -num;

    /* Disable the repeat-detection. */
    pclog_repeat(0);

    addr = (uint32_t)(_cs.base | cpu_state.pc);

    while (i < num) {
	if (sp == NULL) {
		sp = buff;
		sprintf(sp, "%08x:", addr + i);
		sp += strlen(sp);
	}

	/* Get byte from memory. */
        k = readmembl(addr + i);
	i++;

        sprintf(sp, " %02x", k & 0xff);
	sp += strlen(sp);

	if ((i % 16) == 0) {
#ifdef _DEBUG
		INFO("%s\n", buff);
#else
		DBGLOG(2, "%s\n", buff);
#endif
		sp = NULL;
	}
    }
    if (sp != NULL)
#ifdef _DEBUG
	INFO("%s\n", buff);
#else
	DBGLOG(2, "%s\n", buff);
#endif

    /* Re-enable the repeat-detection. */
    pclog_repeat(1);
}


/* Log a fatal error, and display a UI message before exiting. */
void
fatal(const char *fmt, ...)
{
    char temp[1024];
    va_list ap;
    char *sp;

    va_start(ap, fmt);

    if (logfp == NULL) {
	if (log_path[0] != L'\0') {
		logfp = plat_fopen(log_path, L"w");
		if (logfp == NULL)
			logfp = stdout;
	} else
		logfp = stdout;
    }

    vsprintf(temp, fmt, ap);
    fprintf(logfp, "%s", temp);
    fflush(logfp);
    va_end(ap);

    nvr_save();

    config_save();

    pic_dump();
    cpu_dumpregs(1);

    /*
     * Attempt to perform a clean exit by terminating the
     * main loop of the emulator, which hopefully also do
     * a shutdown of all running threads.
     */
    plat_stop();

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL) *sp = '\0';

    ui_msgbox(MBX_ERROR|MBX_FATAL|MBX_ANSI, temp);

    fflush(logfp);

    exit(-1);
}


/* Set the application version ID string. */
void
pc_version(const char *platform)
{
    char temp[128];

    sprintf(emu_title, "%s for %s", EMU_NAME, platform);

#ifdef EMU_VER_PATCH
    sprintf(emu_version, "v%s", EMU_VERSION_4);
#else
    sprintf(emu_version, "v%s", EMU_VERSION);
#endif
    strcpy(emu_fullversion, emu_version);

#if (defined(_MSC_VER) && defined(_M_X64)) || \
    (defined(__GNUC__) && defined(__amd64__))
    strcat(emu_fullversion, " [64bit]");
#endif

#if defined(_MSC_VER)
    sprintf(temp, " [VC %i]", _MSC_VER);
#elif defined(__clang_major__)
    sprintf(temp, " [Clang %i.%i.%i]",
	__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    sprintf(temp, " [GCC %i.%i.%i]",
	__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
    strcat(emu_fullversion, temp);

#ifdef BUILD
    sprintf(temp, " (Build %i", BUILD);
    strcat(emu_fullversion, temp);
#endif
#ifdef COMMIT
    sprintf(temp, " [Commit #%x]", COMMIT);
    strcat(emu_fullversion, temp);
#endif
#ifdef BUILD
    strcat(emu_fullversion, ")");
#endif

#ifdef UPSTREAM
    sprintf(temp, " [Upstream #%x]", UPSTREAM);
    strcat(emu_fullversion, temp);
#endif
}


/*
 * NOTE:
 *
 * Although we now use relative pathnames in the code,
 * we may still bump into old configuration files that
 * have old, absolute pathnames.
 *
 * We try to "fix" those by stripping the "usr_path"
 * component from them, if they have that.  If another
 * path, well, nothing we can do here.
 */
void
pc_path(wchar_t *dst, int sz, const wchar_t *src)
{
    const wchar_t *str = src;
    wchar_t *ptr = dst;
    int i = (int)wcslen(usr_path);

    /*
     * Fix all the slashes.
     *
     * Since Windows will handle both \ and / paths, we
     * now convert ALL paths to the latter format, so it
     * is always the same.
     */
    if (str == NULL)
	str = ptr;
    while ((sz > 0) && (*str != L'\0')) {
	if (*str == L'\\')
		*ptr = L'/';
	  else
		*ptr = *str;
	str++;
	ptr++;
	sz--;
    }
    *ptr = L'\0';

    if ((src != NULL) && !wcsncasecmp(dst, usr_path, i))
	wcscpy(dst, &dst[i]);
}


/*
 * Perform initial setup of the PC.
 *
 * This is the platform-indepenent part of the startup, where we
 * check commandline arguments and load a configuration file.
 */
int
pc_setup(int argc, wchar_t *argv[])
{
    wchar_t temp[1024];
    char tempA[128];
    wchar_t *cfg = NULL, *p;
    struct tm *info;
    time_t now;
    int c, ret = 0;

    /* Are we doing first initialization? */
    if (argc == 0) {
	/* Grab the executable's full path. */
	plat_get_exe_name(emu_path, sizeof_w(emu_path));
	if ((p = plat_get_basename(emu_path)) != NULL)
		*p = L'\0';
    	wcscpy(exe_path, emu_path);
    	plat_append_slash(exe_path);

	/*
	 * See if we are perhaps in a "bin/" subfolder of the
	 * installation path, in which case the real root of
	 * the installation is one level up. We can test this
	 * by looking for the 'roms' folder.
	 */
	wcscpy(temp, emu_path);
	plat_append_slash(temp);
	wcscat(temp, ROMS_PATH);
	if (! plat_dir_check(temp)) {
		/* No 'roms' folder found, so go up one level. */
		wcscpy(temp, emu_path);
		if ((p = plat_get_basename(temp)) != NULL)
			*p = L'\0';
		plat_append_slash(temp);
		wcscat(temp, ROMS_PATH);
		if (plat_dir_check(temp)) {
			if (p != NULL)
				*p = L'\0';
			wcscpy(emu_path, temp);
		}
	}
	plat_append_slash(emu_path);

	/*
	 * Get the current working directory.
	 *
	 * This is normally the directory from where the
	 * program was run. If we have been started via
	 * a shortcut (desktop icon), however, the CWD
	 * could have been set to something else.
	 */
	plat_getcwd(usr_path, sizeof_w(usr_path));

	/*
	 * Initialize the 'logfp' variable, this is
	 * somewhat platform-specific. On Windows, it
	 * will always be 'stdout', but on UNIX-based
	 * systems, it can be 'stderr' for the console
	 * mode (since 'stdout' is used by the UI) and,
	 * 'stdout' for the GUI versins, etc...
	 */
	logfp = (FILE *)argv;

	return(0);
    }

    memset(temp, 0x00, sizeof(temp));

    for (c = 1; c < argc; c++) {
	if (argv[c][0] != L'-') break;

	if (!wcscasecmp(argv[c], L"--help") || !wcscasecmp(argv[c], L"-?")) {
usage:
#ifdef _WIN32
		plat_console(1);
#endif
		printf("\n%s %s\n", emu_title, emu_fullversion);
		p = plat_get_basename(argv[0]);
		if (*p == L'/' || *p == L'\\') p++;
		printf("\nUsage: %ls [options] [cfg-file]\n\n", p);
		printf("Valid options are:\n\n");
		printf("  -? or --help         - show this information\n");
		printf("  -C or --dumpcfg      - dump config file after loading\n");
		printf("  -D or --debug        - force debug logging\n");
		printf("  -F or --fullscreen   - start in fullscreen mode\n");
		printf("  -L or --logfile path - set 'path' to be the logfile\n");
		printf("  -P or --vmpath path  - set 'path' to be root for vm\n");
		printf("  -q or --quiet        - set logging level to QUIET\n");
#ifdef USE_WX
		printf("  -R or --fps num      - set render speed to 'num' fps\n");
#endif
		printf("  -S or --settings     - show only the settings dialog\n");
		printf("  -W or --readonly     - do not modify the config file\n");
		printf("  -K or --keep_space   - keep whitespace in config file\n");
		printf("\nA config file can be specified. If none is, the default file will be used.\n");
		return(ret);
	} else if (!wcscasecmp(argv[c], L"--dumpcfg") ||
		   !wcscasecmp(argv[c], L"-C")) {
		do_dump_config = 1;
	} else if (!wcscasecmp(argv[c], L"--debug") ||
		   !wcscasecmp(argv[c], L"-D")) {
#ifdef _WIN32
		force_debug = 1;
#endif
		log_level++;
	} else if (!wcscasecmp(argv[c], L"--fullscreen") ||
		   !wcscasecmp(argv[c], L"-F")) {
		start_in_fullscreen = 1;
	} else if (!wcscasecmp(argv[c], L"--logfile") ||
		   !wcscasecmp(argv[c], L"-L")) {
		if ((c+1) == argc) {
			ret = -1;
			goto usage;
		}
		wcscpy(log_path, argv[++c]);
	} else if (!wcscasecmp(argv[c], L"--vmpath") ||
		   !wcscasecmp(argv[c], L"-P")) {
		if ((c+1) == argc) {
			ret = -1;
			goto usage;
		}
		wcsncpy(temp, argv[++c], sizeof_w(temp));
	} else if (!wcscasecmp(argv[c], L"--quiet") ||
		   !wcscasecmp(argv[c], L"-q")) {
		log_level = LOG_DEBUG;
#ifdef USE_WX
	} else if (!wcscasecmp(argv[c], L"--fps") ||
		   !wcscasecmp(argv[c], L"-R")) {
		if ((c+1) == argc) {
			ret = -1;
			goto usage;
		}
		video_fps = wcstol(argv[++c], NULL, 10);
#endif
	} else if (!wcscasecmp(argv[c], L"--settings") ||
		   !wcscasecmp(argv[c], L"-S")) {
		settings_only = 1;
	} else if (!wcscasecmp(argv[c], L"--keep_space") ||
		   !wcscasecmp(argv[c], L"-K")) {
		config_keep_space = 1;
	} else if (!wcscasecmp(argv[c], L"--test")) {
		/* some (undocumented) test function here.. */

		/* .. and then exit. */
		return(0);
	}

	/* Uhm... out of options here.. */
	else goto usage;
    }

    /* One argument (config file) allowed. */
    if (c < argc)
	cfg = argv[c++];
    if (c != argc) {
	ret = -2;
	goto usage;
    }

    /*
     * If the user provided a path for files, use that
     * instead of the current working directory. We do
     * make sure that if that was a relative path, we
     * make it absolute.
     */
    if (temp[0] != L'\0') {
	if (! plat_path_abs(temp)) {
		/*
		 * This looks like a relative path.
		 *
		 * Add it to the current working directory
		 * to convert it (back) to an absolute path.
		 */
		plat_append_slash(usr_path);
		wcscat(usr_path, temp);
	} else {
		/*
		 * The user-provided path seems like an
		 * absolute path, so just use that.
		 */
		wcscpy(usr_path, temp);
	}
    }

    /* Make sure we have a trailing backslash. */
    plat_append_slash(usr_path);

    /* Grab the name of the configuration file. */
    if (cfg == NULL)
	cfg = CONFIG_FILE;

    /*
     * If the configuration file name has (part of)
     * a pathname, consider that to be part of the
     * actual working directory.
     *
     * This can happen when people load a config 
     * file using the UI, for example.
     */
    p = plat_get_filename(cfg);
    if (cfg != p) {
	/*
	 * OK, the configuration file name has a
	 * path component. Separate the two, and
	 * add the path component to the cfg path.
	 */
	*(p-1) = L'\0';

	/*
	 * If this is an absolute path, keep it, as
	 * there is probably have a reason to do so.
	 * Otherwise, assume the pathname given is
	 * relative to whatever the usr_path is.
	 */
	if (plat_path_abs(cfg))
		wcscpy(usr_path, cfg);
	  else
		wcscat(usr_path, cfg);

	/* Make sure we have a trailing backslash. */
	plat_append_slash(usr_path);
    }

    /* Don't forget to clean up the user path. */
    pc_path(usr_path, sizeof_w(usr_path), NULL);

    /* At this point, we can safely create the full path name. */
    plat_append_filename(cfg_path, usr_path, p);

    /* If no extension was given, tack on the default one. */
    if ((cfg = wcschr(p, L'.')) == NULL)
	wcscat(cfg_path, CONFIG_FILE_EXT);

    /*
     * This is where we start outputting to the log file,
     * if there is one. Create a little info header first.
     *
     * For the Windows version, if we do not have a console
     * meaning, not logging to file..), request one if we
     * have force_debug set.
     */
#ifdef _WIN32
    if (force_debug)
	plat_console(1);
#endif
    (void)time(&now);
    info = localtime(&now);
    strftime(tempA, sizeof(temp), "%Y/%m/%d %H:%M:%S", info);
    INFO("#\n# %s %s\n#\n# Logfile created %s\n#\n",
		emu_title, emu_fullversion, tempA);
    INFO("# Emulator path: %ls\n", emu_path);
    INFO("# Userfiles path: %ls\n", usr_path);
    INFO("# Configuration file: %ls\n#\n\n", cfg_path);

    /*
     * We are about to read the configuration file, so
     * clear all the global configuration data now.
     */
    config_init(&config);
    hdd_init();
    cdrom_global_init();
    zip_global_init();
    network_init();

    /* Load the configuration file. */
    if (! config_load(&config)) return(2);

    /* All good. */
    return(1);
}


/*
 * Initialize the configured PC - run once, after pc_setup() is done.
 *
 * At this point, we have loaded a configuration file (or are running
 * with a default one), which may or may not be valid. So, we perform
 * a number of sanity checks here, to avoid running into trouble at a
 * later time..
 */
int
pc_init(void)
{
    wchar_t temp[128];
    char tempA[128];
    const wchar_t *str;
    const char *stransi;

    /* If no machine selected, force user into Setup Wizard. */
    if (config.machine_type < 0) {
	str = get_string(IDS_ERR_NOCONF);

	/* Show the messagebox, and abort if 'No' was selected. */
	if (ui_msgbox(MBX_QUESTION, str) != 0) return(0);

	/* OK, user wants to set up a machine. */
	config.machine_type = 0;

	return(2);
    }

    /* Load the ROMs for the selected machine. */
    if (! machine_available(config.machine_type)) {
	/* Whoops, selected machine not available. */
	stransi = machine_get_name();
	if (stransi == NULL) {
		/* This happens if configured machine is not even in table.. */
		sprintf(tempA, "machine_%i", config.machine_type);
		stransi = (const char *)tempA;
	}

	/* Show the messagebox, and abort if 'No' was selected. */
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOAVAIL), get_string(IDS_3310), stransi);
	if (ui_msgbox(MBX_CONFIG, temp) == 1) return(0);

	/* OK, user wants to (re-)configure.. */
	return(2);
    }

    if ((config.video_card < 0) || !video_card_available(config.video_card)) {
	/* Whoops, selected video not available. */
	stransi = video_card_getname(config.video_card);
	if (stransi == NULL) {
		/* This happens if configured card is not even in table.. */
		sprintf(tempA, "vid_%i", config.video_card);
		stransi = (const char *)tempA;
	}

	/* Show the messagebox, and abort if 'No' was selected. */
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOAVAIL), get_string(IDS_3311), stransi);
	if (ui_msgbox(MBX_CONFIG, temp) == 1) return(0);

	/* OK, user wants to (re-)configure.. */
	return(2);
    }

    /* If we have selected a network provider, make sure it is available. */
    if ((config.network_type < 0) || !network_available(config.network_type)) {
	swprintf(temp, sizeof_w(temp), get_string(IDS_ERR_NOAVAIL),
		 get_string(IDS_3314), network_getname(config.network_type));

	if (ui_msgbox(MBX_CONFIG, temp) == 1) return(0);

	/* OK, user wants to (re-)configure.. */
	return(2);
    }

    /*
     * At this point, we know that the selected machine and
     * video card are available, so we can proceed with the
     * initialization of things.
     */
    random_init();

    mem_init();

#ifdef USE_DYNAREC
    codegen_init();
#endif

    timer_reset();

    joystick_init();

    video_init();

    sound_init();

#if 0
    fdd_init();
#else
    floppy_init();		//FIXME: fdd_init() now?
#endif

    return(1);
}


/* Close down a running machine. */
void
pc_close(thread_t *ptr)
{
    /* Wait a while so things can shut down. */
    plat_delay_ms(200);

    /* Claim the video blitter. */
    plat_blitter(1);

    /* Terminate the main thread. */
    if (ptr != NULL) {
	thread_kill(ptr);

	/* Wait some more. */
	plat_delay_ms(200);
    }

    nvr_save();

    config_save();

    ui_mouse_capture(0);

    zip_close();

    scsi_disk_close();

//    floppy_close();

    if (dump_on_exit)
	pic_dump();
    cpu_dumpregs(0);

    device_close_all();

    video_close();

    network_close();

    sound_close();

    cdrom_close();
}


void
pc_reset_hard_close(void)
{
    nvr_save();

    mouse_close();

    cdrom_close();

    zip_close();

    scsi_disk_close();

#if 0
    sound_close();
#endif

    device_close_all();
}


/*
 * This is basically the spot where we start up the actual machine,
 * by issuing a 'hard reset' to the entire configuration. Order is
 * somewhat important here. Functions here should be named _reset
 * really, as that is what they do.
 */
void
pc_reset_hard_init(void)
{
    /* Reset the general machine support modules. */
    io_reset();
    timer_reset();
    device_reset();

    /*
     * Reset the portsi.
     *
     * We do this before the machine is initialized, so its
     * BIOS can override port settings on SIO chips etc.
     */
    game_reset();
    parallel_reset();
    serial_reset();

    /*
     * Reset the actual machine and its basic modules.
     *
     * Note that on PCI-based machines, this will also reset
     * the PCI bus state, so, we MUST reset PCI devices after
     * the machine!
     */
    machine_reset();

    /* Reset the video system. */
    video_reset();

    /* Reset any ISA memory cards. */
    isamem_reset();

    /* Reset any ISA RTC cards. */
    isartc_reset();

    /* Reset some basic devices. */
    keyboard_reset();
    mouse_reset();

    /* Reset sound system. This MAY add a game port, so before joystick! */
    sound_reset();

    /* Reset the Floppy module. */
//FIXME: move to floppy_reset()
    switch(config.fdc_type) {
	case FDC_NONE:
	case FDC_INTERNAL:
		break;

	case 10:
		device_add(&fdc_pii151b_device);
		break;

	case 11:
		device_add(&fdc_pii158b_device);
		break;
    }
    fdd_reset();

    /* Reset the Hard Disk module. */
//FIXME: move to disk_reset()
    hdc_reset();

    /* Enable AMIDISK card if configured. */
    if (config.ami_disk)
	device_add(&amidisk_device);

    /* Reset and reconfigure the SCSI layer. */
//FIXME: move to scsi_reset()
    scsi_card_init();

    cdrom_hard_reset();
    zip_hard_reset();
    scsi_disk_hard_reset();

    /* Reset and reconfigure the Network Card layer. */
    network_reset();

    if (config_changed) {
	ui_sb_reset();

        config_save();

	config_changed = 0;
    }

    /* Needs the status bar... */
    if (config.bugger_enabled)
	device_add(&bugger_device);

    /* Needs the status bar initialized. */
    ui_mouse_capture(-1);

    /*
     * At this point, all configurable hardware has
     * been initialized, and we are about ready to
     * start executing instructions.
     */
#ifdef _DEBUG
    device_dump();
#endif

    /* Reset the CPU module. */
    cpu_reset(1);
    dma_reset();
    pic_reset();

    //FIXME: already done in machine_reset() - why needed here??
    pc_set_speed(1);
}


void
pc_reset_hard(void)
{
    pc_reset_hard_close();

    pc_reset_hard_init();
}


void
pc_reset(int hard)
{
    pc_pause(1);

    plat_delay_ms(100);

    nvr_save();

    config_save();

    if (hard)
        pc_reset_hard();
      else
        keyboard_cad();

    pc_pause(0);
}


/* FIXME: this has to be reviewed! */
void
pc_reload(const wchar_t *fn)
{
    config_write(cfg_path);

    floppy_close();

    cdrom_close();

    pc_reset_hard_close();

    // FIXME: set up new config file path 'fn'... --FvK

    config_load(&config);

    cdrom_hard_reset();

    zip_hard_reset();

    scsi_disk_hard_reset();

    fdd_load(0, floppyfns[0]);
    fdd_load(1, floppyfns[1]);
    fdd_load(2, floppyfns[2]);
    fdd_load(3, floppyfns[3]);

    network_init();

    pc_reset_hard_init();
}


/*
 * The main thread runs the actual emulator code.
 *
 * We basically run until the upper layers terminate us, by
 * setting the variable 'quited' there to 1. We get a pointer
 * to that variable as our function argument.
 */
#define SLICE	10		/* time-slice in msec per frame */
void
pc_thread(void *param)
{
    wchar_t temp[200];
    uint32_t old_time, new_time;
    int *quitp = (int *)param;
    int frm, msec;

    INFO("PC: starting main thread...\n");

    old_time = plat_timer_ms();
    title_update = 1;
    msec = frm = 0;

    while (! *quitp) {
	/* Just so we dont overload the host OS. */
	if (dopause) {
		plat_delay_ms(100);
		continue;
	}

	/* Get the current time (in usec.) */
	new_time = plat_timer_ms();
	msec += (new_time - old_time);
	old_time = new_time;

	if (msec > 0) {
		msec -= SLICE;
		if (msec > 50)
			msec = 0;

		plat_blitter(1);

		/* Run a frame of code. */
		cpu_exec(1000 / SLICE);

		plat_blitter(0);

#ifdef USE_DINPUT
		mouse_poll();
#endif

		joystick_process();

		/* One more frame done! */
		framecount++;
	}

	/*
	 * If the NVR needs to be saved, wait 10 frames before
	 * we actually do, to reduce "nvr file hammering".
	 */
	if (nvr_dosave && (++frm >= 10)) {
		nvr_save();
		nvr_dosave = 0;
		frm = 0;
	}

	/* If needed, update the title bar. */
	if (title_update) {
		if (config.title[0] != L'\0') {
			swprintf(temp, sizeof_w(temp), L"%s %s - %3i%% - %ls",
				 EMU_NAME, emu_version, fps, config.title);
		} else {
			swprintf(temp, sizeof_w(temp),
				 L"%s %s - %3i%% - %s - %s",
				 EMU_NAME, emu_version, fps,
				 machine_get_name(), cpu_get_name());
		}
		ui_window_title(temp);

		title_update = 0;
	}

	/* If needed, handle a screen resize. */
	if (doresize && !config.vid_fullscreen) {
		ui_resize(scrnsz_x, scrnsz_y);

		doresize = 0;
	}
    }

    INFO("PC: main thread done.\n");
}


/* Handler for the 1-second timer to refresh the window title. */
void
pc_onesec(void)
{
    fps = framecount;
    framecount = 0;

    title_update = 1;
}


/* Pause or unpause the emulator. */
void
pc_pause(int p)
{
    static wchar_t oldtitle[512];
    wchar_t title[512];

    /* If un-pausing, ask the renderer if that's OK. */
    if (p == 0)
	p = vidapi_pause();

    /* If already so, done. */
    if (dopause == p) return;

    if (p) {
	wcscpy(oldtitle, ui_window_title(NULL));
	wcscpy(title, oldtitle);
	wcscat(title, L" - PAUSED -");
	ui_window_title(title);
    } else {
	ui_window_title(oldtitle);
    }

    dopause = p;

    /* Update the actual menu item. */
    menu_set_item(IDM_PAUSE, dopause);
}


/*
 * Set the active processor speed for this machine.
 *
 * The argument is either 0 for 'slowest speed', or an
 * index into the CPU table to select that model' speed.
 *
 * This function handles two things:
 *
 * - it sets up the correct/expected XTAL frequency for
 *   the desired operating speed;
 *
 * - it then tells the CPU module to activate that CPU
 *   and/or speed.
 *
 * In addition, after the speed change, it updates timings.
 */
void
pc_set_speed(int turbo)
{
    uint32_t speed;

    /*
     * Part One - Set up the correct XTAL frequency.
     *
     * Get the selected processor's desired speed.
     *
     * For 286+, this is usually 8 (slow) or max speed.
     * For PC and XT class, this will return max speed.
     */
    speed = machine_get_speed(turbo);
    DEBUG("PC: set_speed(%i) -> speed %lu\n", turbo, speed);

    if (cpu_get_type() >= CPU_286) {
	/* For 286+, we are done. */
	clk_setup(speed);
    } else {
	/*
	 * Not so easy for PC and XT class machines.
	 *
	 * The TURBO setting on these machines (if they had
	 * one at all) basically is the maximum speed of the
	 * selected processor.  The slow speed is, in pretty
	 * much all cases, the original 4.77MHz setting.
	 */
	if (turbo)
		clk_setup(14318184);	// speed * xt_multi ?
	  else
		clk_setup(14318184);
    }

    /*
     * Part Two - set the correct processor type.
     */

    /*
     * Part Three - update several timing constants.
     *
     * This is necessary because the emulator's entire
     * timer system is built around the CPU clock as a
     * base unit. So, if we change that, everything does..
     */
}


/* Re-load system configuration and restart. */
void
set_screen_size(int x, int y)
{
    int owsx = scrnsz_x;
    int owsy = scrnsz_y;
    int temp_overscan_x = overscan_x;
    int temp_overscan_y = overscan_y;
    double dx, dy, dtx, dty;
    int vid;

    DEBUG("SetScreenSize(%i, %i) resize=%i\n", x, y, config.vid_resize);

    /* Make sure we keep usable values. */
    if (x < 320) x = 320;
    if (y < 200) y = 200;
    if (x > 2048) x = 2048;
    if (y > 2048) y = 2048;

    /* Save the new values as "real" (unscaled) resolution. */
    unscaled_size_x = x;
    efscrnsz_y = y;

    if (suppress_overscan)
	temp_overscan_x = temp_overscan_y = 0;

    if (config.force_43) {
	dx = (double)x;
	dtx = (double)temp_overscan_x;

	dy = (double)y;
	dty = (double)temp_overscan_y;

	/* Account for possible overscan. */
	vid = video_type();
	if ((vid == VID_TYPE_CGA) && (temp_overscan_y == 16)) {
		/* CGA */
		dy = (((dx - dtx) / 4.0) * 3.0) + dty;
	} else if ((vid == VID_MDA) && (temp_overscan_y < 16)) {
		/* MDA/Hercules */
		dy = (x / 4.0) * 3.0;
	} else {
		if (config.enable_overscan) {
			/* EGA/(S)VGA with overscan */
			dy = (((dx - dtx) / 4.0) * 3.0) + dty;
		} else {
			/* EGA/(S)VGA without overscan */
			dy = (x / 4.0) * 3.0;
		}
	}
	unscaled_size_y = (int)dy;
    } else {
	unscaled_size_y = efscrnsz_y;
    }

    switch(config.scale) {
	case 0:		/* 50% */
		scrnsz_x = (unscaled_size_x>>1);
		scrnsz_y = (unscaled_size_y>>1);
		break;

	case 1:		/* 100% */
		scrnsz_x = unscaled_size_x;
		scrnsz_y = unscaled_size_y;
		break;

	case 2:		/* 150% */
		scrnsz_x = ((unscaled_size_x*3)>>1);
		scrnsz_y = ((unscaled_size_y*3)>>1);
		break;

	case 3:		/* 200% */
		scrnsz_x = (unscaled_size_x<<1);
		scrnsz_y = (unscaled_size_y<<1);
		break;
    }

    /* If the resolution has changed, let the main thread handle it. */
    if ((owsx != scrnsz_x) || (owsy != scrnsz_y))
	doresize = 1;
      else
	doresize = 0;
}


void
set_screen_size_natural(void)
{
    set_screen_size(unscaled_size_x, unscaled_size_y);
}


void
get_screen_size_natural(int *x, int *y)
{
    if (x != NULL)
	*x = unscaled_size_x;
    if (y != NULL)
	*y = efscrnsz_y;
}
