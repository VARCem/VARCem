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
 * Version:	@(#)pc.c	1.0.24	2018/04/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include <inttypes.h>
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
#include "cpu/cpu.h"
#ifdef USE_DYNAREC
# include "cpu/codegen.h"
#endif
#include "cpu/x86_ops.h"
#include "machine/machine.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "dma.h"
#include "pic.h"
#include "pit.h"
#include "random.h"
#include "timer.h"
#include "device.h"
#include "nvr.h"
#include "bugger.h"
#include "serial.h"
#include "parallel.h"
#include "keyboard.h"
#include "mouse.h"
#include "game/gameport.h"
#include "floppy/fdd.h"
#include "floppy/fdd_common.h"
#include "disk/hdd.h"
#include "disk/hdc.h"
#include "disk/hdc_ide.h"
#include "disk/zip.h"
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_image.h"
#include "cdrom/cdrom_null.h"
#include "scsi/scsi.h"
#include "network/network.h"
#include "sound/sound.h"
#include "sound/snd_speaker.h"
#include "video/video.h"
#include "ui.h"
#include "plat.h"


/* Commandline options. */
int	dump_on_exit = 0;			/* (O) dump regs on exit */
int	do_dump_config = 0;			/* (O) dump config on load */
int	start_in_fullscreen = 0;		/* (O) start in fullscreen */
#ifdef _WIN32
int	force_debug = 0;			/* (O) force debug output */
#endif
#ifdef USE_WX
int	video_fps = RENDER_FPS;			/* (O) render speed in fps */
#endif
int	settings_only = 0;			/* (O) only the settings dlg */
int	config_ro = 0;				/* (O) dont modify cfg file */
wchar_t log_path[1024] = { L'\0'};		/* (O) full path of logfile */

/* Configuration values. */
int	window_w, window_h,			/* (C) window size and */
	window_x, window_y,			/*     position info */
	window_remember,
	vid_resize,				/* (C) allow resizing */
	invert_display,				/* (C) invert the display */
	suppress_overscan = 0;			/* (C) suppress overscans */
int	scale = 0;				/* (C) screen scale factor */
int	vid_api = 0;				/* (C) video renderer */
int	vid_cga_contrast = 0,			/* (C) video */
	video_fullscreen = 0,			/* (C) video */
	video_fullscreen_scale = 0,		/* (C) video */
	video_fullscreen_first = 0,		/* (C) video */
	enable_overscan = 0,			/* (C) video */
	force_43 = 0,				/* (C) video */
	vid_card = 0,				/* (C) graphics/video card */
	video_speed = 0;			/* (C) video */
int	enable_sync = 0;			/* (C) enable time sync */
int	serial_enabled[] = {0,0},		/* (C) enable serial ports */
	parallel_enabled[] = {0,0,0},		/* (C) enable LPT ports */
	parallel_device[] = {0,0,0},		/* (C) set up LPT devices */
	bugger_enabled = 0;			/* (C) enable ISAbugger */
int	rctrl_is_lalt;				/* (C) set R-CTRL as L-ALT */
int	update_icons;				/* (C) enable icons updates */
#ifdef WALTJE
int	romdos_enabled = 0;			/* (C) enable ROM DOS */
#endif
int	hdc_type = 0;				/* (C) HDC type */
int	sound_is_float = 1,			/* (C) sound uses FP values */
	mpu401_standalone_enable = 0,		/* (C) sound option */
	opl3_type = 0,				/* (C) sound option */
	voodoo_enabled = 0;			/* (C) video option */
int	joystick_type = 0;			/* (C) joystick type */
int	mem_size = 0;				/* (C) memory size */
int	cpu_manufacturer = 0,			/* (C) cpu manufacturer */
	cpu_use_dynarec = 0,			/* (C) cpu uses/needs Dyna */
	cpu = 3,				/* (C) cpu type */
	enable_external_fpu = 0;		/* (C) enable external FPU */
int	network_type;				/* (C) net provider type */
int	network_card;				/* (C) net interface num */
char	network_host[512];			/* (C) host network intf */


/* Statistics. */
extern int
	mmuflush,
	readlnum,
	writelnum;

int	sndcount = 0;
int	sreadlnum,
	swritelnum,
	segareads,
	segawrites,
	scycles_lost;
float	mips, flops;
int	cycles_lost = 0;			/* video */
int	insc = 0;				/* cpu */
int	emu_fps = 0, fps;			/* video */
int	framecount;

int	atfullspeed;
int	cpuspeed2;
int	clockrate;

char	emu_title[64];				/* full name of application */
char	emu_version[32];			/* short version ID string */
char	emu_fullversion[128];			/* full version ID string */
wchar_t	emu_path[1024];				/* emu installation path */
wchar_t	usr_path[1024];				/* path (dir) of user data */
wchar_t	cfg_path[1024];				/* full path of config file */
FILE	*stdlog = NULL;				/* file to log output to */
int	scrnsz_x = SCREEN_RES_X,		/* current screen size, X */
	scrnsz_y = SCREEN_RES_Y;		/* current screen size, Y */
int	unscaled_size_x = SCREEN_RES_X,		/* current unscaled size X */
	unscaled_size_y = SCREEN_RES_Y,		/* current unscaled size Y */
	efscrnsz_y = SCREEN_RES_Y;
int	config_changed;				/* config has changed */
int	romset;					/* current machine ID */
int	title_update;
int64_t	main_time;


/*
 * Log something to the logfile or stdout.
 *
 * To avoid excessively-large logfiles because some
 * module repeatedly logs, we keep track of what is
 * being logged, and catch repeating entries.
 */
void
pclog_ex(const char *fmt, va_list ap)
{
#ifndef RELEASE_BUILD
    static char buff[1024];
    static int seen = 0;
    char temp[1024];

    if (stdlog == NULL) {
	if (log_path[0] != L'\0') {
		stdlog = plat_fopen(log_path, L"w");
		if (stdlog == NULL)
			stdlog = stdout;
	} else {
		stdlog = stdout;
	}
    }

    vsprintf(temp, fmt, ap);
    if (! strcmp(buff, temp)) {
	seen++;
    } else {
	if (seen) {
		fprintf(stdlog, "*** %d repeats ***\n", seen);
	}
	seen = 0;
	strcpy(buff, temp);
	fprintf(stdlog, temp, ap);
    }

    fflush(stdlog);
#endif
}


/* Log something. We only do this in non-release builds. */
void
pclog(const char *fmt, ...)
{
#ifndef RELEASE_BUILD
    va_list ap;

    va_start(ap, fmt);
    pclog_ex(fmt, ap);
    va_end(ap);
#endif
}


/* Log a fatal error, and display a UI message before exiting. */
void
fatal(const char *fmt, ...)
{
    char temp[1024];
    va_list ap;
    char *sp;

    va_start(ap, fmt);

    if (stdlog == NULL) {
	if (log_path[0] != L'\0') {
		stdlog = plat_fopen(log_path, L"w");
		if (stdlog == NULL)
			stdlog = stdout;
	} else {
		stdlog = stdout;
	}
    }

    vsprintf(temp, fmt, ap);
    fprintf(stdlog, "%s", temp);
    fflush(stdlog);
    va_end(ap);

    nvr_save();

    config_save();

    dumppic();
    dumpregs(1);

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL) *sp = '\0';

    ui_msgbox(MBX_ERROR|MBX_FATAL|MBX_ANSI, temp);

    fflush(stdlog);

    exit(-1);
}


/* Set the application version ID string. */
void
pc_version(const char *platform)
{
#if defined(BUILD) || defined(COMMIT) || defined(UPSTREAM)
    char temp[128];
#endif

    sprintf(emu_title, "%s for %s", EMU_NAME, platform);

#ifdef EMU_VER_PATCH
    sprintf(emu_version, "v%s", EMU_VERSION_4);
#else
    sprintf(emu_version, "v%s", EMU_VERSION);
#endif
    strcpy(emu_fullversion, emu_version);

#ifdef BUILD
    sprintf(temp, " (Build %d", BUILD);
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
 * still have old, absolute pathnames.
 *
 * We try to "fix" those by stripping the "usr_path"
 * component from them, if they have that.  If another
 * path, well, nothing we can do here.
 */
void
pc_path(wchar_t *dst, int sz, const wchar_t *src)
{
    int i = wcslen(usr_path);

    if ((src != NULL) && !wcsncasecmp(src, usr_path, i))
	src += i;

    /*
     * Fix all the slashes.
     *
     * Since Windows will handle both \ and / paths, we
     * now convert ALL paths to the latter format, so it
     * is always the same.
     */
    if (src == NULL)
	src = dst;
    while ((sz > 0) && (*src != L'\0')) {
	if (*src == L'\\')
		*dst = L'/';
	  else
		*dst = *src;
	src++;
	dst++;
	sz--;
    }
    *dst = L'\0';
}


/*
 * Perform initial startup of the PC.
 *
 * This is the platform-indepenent part of the startup,
 * where we check commandline arguments and load a
 * configuration file.
 */
int
pc_init(int argc, wchar_t *argv[])
{
    wchar_t path[1024];
    wchar_t *cfg = NULL, *p;
    char temp[128];
    struct tm *info;
    time_t now;
    int c;

    /* Grab the executable's full path. */
    plat_get_exe_name(emu_path, sizeof(emu_path)-1);
    if ((p = plat_get_basename(emu_path)) != NULL)
	*p = L'\0';

    /*
     * See if we are perhaps in a "bin/" subfolder of
     * the installation path, in which case the real
     * root of the installation is one level up. We
     * can test this by looking for the 'roms' folder.
     */
    wcscpy(path, emu_path);
    plat_append_slash(path);
    wcscat(path, ROMS_PATH);
    if (! plat_dir_check(path)) {
	/* No 'roms' folder found, so go up one level. */
	wcscpy(path, emu_path);
	if ((p = plat_get_basename(path)) != NULL)
		*p = L'\0';
	plat_append_slash(path);
	wcscat(path, ROMS_PATH);
	if (plat_dir_check(path)) {
		if (p != NULL)
			*p = L'\0';
		wcscpy(emu_path, path);
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
    plat_getcwd(usr_path, sizeof_w(usr_path)-1);
    memset(path, 0x00, sizeof(path));

    for (c=1; c<argc; c++) {
	if (argv[c][0] != L'-') break;

	if (!wcscasecmp(argv[c], L"--help") || !wcscasecmp(argv[c], L"-?")) {
usage:
#ifdef _WIN32
		plat_console(1);
#endif
		printf("\n%s %s\n", emu_title, emu_fullversion);
		printf("\nUsage: varcem [options] [cfg-file]\n\n");
		printf("Valid options are:\n\n");
		printf("  -? or --help         - show this information\n");
		printf("  -C or --dumpcfg      - dump config file after loading\n");
#ifdef _WIN32
		printf("  -D or --debug        - force debug output logging\n");
#endif
		printf("  -F or --fullscreen   - start in fullscreen mode\n");
		printf("  -M or --memdump      - dump memory on exit\n");
		printf("  -L or --logfile path - set 'path' to be the logfile\n");
		printf("  -P or --vmpath path  - set 'path' to be root for vm\n");
#ifdef USE_WX
		printf("  -R or --fps num      - set render speed to 'num' fps\n");
#endif
		printf("  -S or --settings     - show only the settings dialog\n");
		printf("  -W or --readonly     - do not modify the config file\n");
		printf("\nA config file can be specified. If none is, the default file will be used.\n");
		return(0);
	} else if (!wcscasecmp(argv[c], L"--dumpcfg") ||
		   !wcscasecmp(argv[c], L"-C")) {
		do_dump_config = 1;
#ifdef _WIN32
	} else if (!wcscasecmp(argv[c], L"--debug") ||
		   !wcscasecmp(argv[c], L"-D")) {
		force_debug = 1;
#endif
	} else if (!wcscasecmp(argv[c], L"--fullscreen") ||
		   !wcscasecmp(argv[c], L"-F")) {
		start_in_fullscreen = 1;
	} else if (!wcscasecmp(argv[c], L"--logfile") ||
		   !wcscasecmp(argv[c], L"-L")) {
		if ((c+1) == argc) goto usage;

		wcscpy(log_path, argv[++c]);
	} else if (!wcscasecmp(argv[c], L"--memdump") ||
		   !wcscasecmp(argv[c], L"-M")) {
	} else if (!wcscasecmp(argv[c], L"--vmpath") ||
		   !wcscasecmp(argv[c], L"-P")) {
		if ((c+1) == argc) goto usage;

		wcscpy(path, argv[++c]);
#ifdef USE_WX
	} else if (!wcscasecmp(argv[c], L"--fps") ||
		   !wcscasecmp(argv[c], L"-R")) {
		if ((c+1) == argc) goto usage;

		video_fps = wcstol(argv[++c], NULL, 10);
#endif
	} else if (!wcscasecmp(argv[c], L"--settings") ||
		   !wcscasecmp(argv[c], L"-S")) {
		settings_only = 1;
	} else if (!wcscasecmp(argv[c], L"--readonly") ||
		   !wcscasecmp(argv[c], L"-W")) {
		config_ro = 1;
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
    if (c != argc) goto usage;

    /*
     * If the user provided a path for files, use that
     * instead of the current working directory. We do
     * make sure that if that was a relative path, we
     * make it absolute.
     */
    if (path[0] != L'\0') {
	if (! plat_path_abs(path)) {
		/*
		 * This looks like a relative path.
		 *
		 * Add it to the current working directory
		 * to convert it (back) to an absolute path.
		 */
		plat_append_slash(usr_path);
		wcscat(usr_path, path);
	} else {
		/*
		 * The user-provided path seems like an
		 * absolute path, so just use that.
		 */
		wcscpy(usr_path, path);
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
    strftime(temp, sizeof(temp), "%Y/%m/%d %H:%M:%S", info);
    pclog("#\n# %s %s\n#\n# Logfile created %s\n#\n",
		emu_title, emu_fullversion, temp);
    pclog("# Emulator path: %ls\n", emu_path);
    pclog("# Userfiles path: %ls\n", usr_path);
    pclog("# Configuration file: %ls\n#\n\n", cfg_path);

    /*
     * We are about to read the configuration file, which MAY
     * put data into global variables (the hard- and floppy
     * disks are an example) so we have to initialize those
     * modules before we load the config..
     */
    mouse_init();
    hdd_init();
    cdrom_global_init();
    zip_global_init();
    network_init();

    /* Load the configuration file. */
    config_load();

    /* All good! */
    return(1);
}


void
pc_full_speed(void)
{
    cpuspeed2 = cpuspeed;

    if (! atfullspeed) {
	pclog("Set fullspeed - %i %i %i\n", is386, AT, cpuspeed2);
	if (AT)
		setpitclock((float)machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].rspeed);
	  else
		setpitclock(14318184.0);
    }
    atfullspeed = 1;
}


void
pc_speed_changed(void)
{
    if (AT)
	setpitclock((float)machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].rspeed);
      else
	setpitclock(14318184.0);
}


/* Re-load system configuration and restart. */
/* FIXME: this has to be reviewed! */
void
pc_reload(const wchar_t *fn)
{
    int i;

    config_write(cfg_path);

    floppy_close();

    for (i=0; i<CDROM_NUM; i++) {
	cdrom_drives[i].handler->exit(i);
	cdrom_close(i);
    }

    pc_reset_hard_close();

    // FIXME: set up new config file path 'fn'... --FvK

    config_load();

    for (i=0; i<CDROM_NUM; i++) {
	if (cdrom_drives[i].bus_type)
		SCSIReset(cdrom_drives[i].scsi_device_id, cdrom_drives[i].scsi_device_lun);

	if (cdrom_drives[i].host_drive == 200)
		image_open(i, cdrom_image[i].image_path);
	  else if ((cdrom_drives[i].host_drive >= 'A') && (cdrom_drives[i].host_drive <= 'Z'))
		ioctl_open(i, cdrom_drives[i].host_drive);
	  else	
	        cdrom_null_open(i, cdrom_drives[i].host_drive);
    }

    for (i=0; i<ZIP_NUM; i++) {
	if (zip_drives[i].bus_type)
		SCSIReset(zip_drives[i].scsi_device_id, zip_drives[i].scsi_device_lun);

	zip_load(i, zip_drives[i].image_path);
    }

    fdd_load(0, floppyfns[0]);
    fdd_load(1, floppyfns[1]);
    fdd_load(2, floppyfns[2]);
    fdd_load(3, floppyfns[3]);

    network_init();

    pc_reset_hard_init();
}


/* Initialize modules, ran once, after pc_init. */
int
pc_init_modules(void)
{
    wchar_t temp[1024];
    wchar_t name[128];
    wchar_t *str;

    /* Load the ROMs for the selected machine. */
    if ((machine < 0) || !machine_available(machine)) {
	/* Whoops, selected machine not available. */
	str = plat_get_string(IDS_2063);
	mbstowcs(name, machine_getname(), sizeof_w(name));
	swprintf(temp, sizeof_w(temp), str, name);

	/* Show the messagebox, and abort if 'No' was selected. */
	if (ui_msgbox(MBX_CONFIG, temp) == 1) return(0);

	/* OK, user wants to (re-)configure.. */
	return(2);
    }

    if ((vid_card < 0) || !video_card_available(video_old_to_new(vid_card))) {
	/* Whoops, selected video not available. */
	str = plat_get_string(IDS_2064);
	mbstowcs(name, machine_getname(), sizeof_w(name));
	swprintf(temp, sizeof_w(temp), str, name);

	/* Show the messagebox, and abort if 'No' was selected. */
	if (ui_msgbox(MBX_CONFIG, temp) == 1) return(0);

	/* OK, user wants to (re-)configure.. */
	return(2);
    }

    /*
     * At this point, we know that the selected machine and
     * video card are available, so we can proceed with the
     * initialization of things.
     */
    cpuspeed2 = (AT) ? 2 : 1;
    atfullspeed = 0;

    random_init();

    mem_init();

#ifdef USE_DYNAREC
    codegen_init();
#endif

#ifdef WALTJE_SERIAL
    serial_init();
#endif
    keyboard_init();
    joystick_init();
    video_init();

    ide_init_first();

    device_init();

    timer_reset();

    sound_init();

    floppy_init();

    /* FIXME: should be disk_init() */
    cdrom_hard_reset();
    zip_hard_reset();
    ide_reset_hard();

    /* FIXME: should be scsi_init() */
    scsi_card_init();

    pc_full_speed();
    shadowbios = 0;

    return(1);
}


void
pc_reset_hard_close(void)
{
    suppress_overscan = 0;

    nvr_save();

    machine_close();

    mouse_close();

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
    io_init();
    timer_reset();
    device_init();

#ifndef WALTJE_SERIAL
    /* This is needed to initialize the serial timer. */
    serial_init();
#endif

    /* Reset the parallel ports [before machine!] */
    parallel_reset();

    sound_reset();
    speaker_init();

    /* Initialize the actual machine and its basic modules. */
    machine_init();

    fdd_reset();

    /*
     * Once the machine has been initialized, all that remains
     * should be resetting all devices set up for it, to their
     * current configurations !
     *
     * For now, we will call their reset functions here, but
     * that will be a call to device_reset_all() later !
     */

    /* Reset some basic devices. */
    serial_reset();

    shadowbios = 0;

    /*
     * This has to be after the serial initialization so that
     * serial_init() doesn't break the serial mouse by resetting
     * the RCR callback to NULL.
     */
    mouse_reset();

    /* Reset the video card. */
    video_reset(vid_card);

    /* FIXME: these, and hdc_reset, should be in disk_reset(). */
    cdrom_hard_reset();
    zip_hard_reset();

    /* Reset the Hard Disk Controller module. */
    hdc_reset();

    /* Reset and reconfigure the SCSI layer. */
    scsi_card_init();

    /* Reset and reconfigure the Network Card layer. */
    network_reset();

    if (joystick_type != JOYSTICK_TYPE_NONE)
	gameport_update_joystick_type();

    if (config_changed) {
	ui_sb_update_panes();

        config_save();

	config_changed = 0;
    }

    /* Needs the status bar... */
    if (bugger_enabled)
	device_add(&bugger_device);

    /* Reset the CPU module. */
    cpu_set();
    resetx86();
    dma_reset();
    pic_reset();
    cpu_cache_int_enabled = cpu_cache_ext_enabled = 0;

    if (AT)
	setpitclock((float)machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].rspeed);
    else
	setpitclock(14318184.0);
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
    plat_pause(1);

    plat_delay_ms(100);

    nvr_save();

    machine_close();

    config_save();

    if (hard)
        pc_reset_hard();
      else
        keyboard_send_cad();

    plat_pause(0);
}


void
pc_close(thread_t *ptr)
{
    int i;

    /* Wait a while so things can shut down. */
    plat_delay_ms(200);

    /* Claim the video blitter. */
    startblit();

    /* Terminate the main thread. */
    if (ptr != NULL) {
	thread_kill(ptr);

	/* Wait some more. */
	plat_delay_ms(200);
    }

    nvr_save();

    machine_close();

    config_save();

    plat_mouse_capture(0);

    for (i=0; i<ZIP_NUM; i++)
	zip_close(i);

    for (i=0; i<CDROM_NUM; i++)
	cdrom_drives[i].handler->exit(i);

    floppy_close();

    if (dump_on_exit)
	dumppic();
    dumpregs(0);

    video_close();

    device_close_all();

    network_close();

    sound_close();

    ide_destroy_buffers();

    cdrom_destroy_drives();
}


/*
 * The main thread runs the actual emulator code.
 *
 * We basically run until the upper layers terminate us, by
 * setting the variable 'quited' there to 1. We get a pointer
 * to that variable as our function argument.
 */
void
pc_thread(void *param)
{
    wchar_t temp[200];
    uint64_t start_time, end_time;
    uint32_t old_time, new_time;
    int status_update_needed;
    int done, drawits, frames;
    int *quitp = (int *)param;
    int framecountx;

    pclog("PC: starting main thread...\n");

    main_time = 0;
    framecountx = 0;
    status_update_needed = title_update = 1;
    old_time = plat_get_ticks();
    done = drawits = frames = 0;
    while (! *quitp) {
	/* Update the Stat(u)s window with the current info. */
	if (status_update_needed) {
		ui_status_update();
		status_update_needed = 0;
	}

	/* See if it is time to run a frame of code. */
	new_time = plat_get_ticks();
	drawits += (new_time - old_time);
	old_time = new_time;
	if (drawits > 0 && !dopause) {
		/* Yes, so do one frame now. */
		start_time = plat_timer_read();
		drawits -= 10;
		if (drawits > 50)
			drawits = 0;

		/* Run a block of code. */
		startblit();
		clockrate = machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].rspeed;

		if (is386) {
#ifdef USE_DYNAREC
			if (cpu_use_dynarec)
				exec386_dynarec(clockrate/100);
			  else
#endif
				exec386(clockrate/100);
		} else if (AT) {
			exec386(clockrate/100);
		} else {
			execx86(clockrate/100);
		}

		mouse_process();

		joystick_process();

		endblit();

		/* Done with this frame, update statistics. */
		framecount++;
		if (++framecountx >= 100) {
			framecountx = 0;

			/* FIXME: all this should go into a "stats" struct! */
			mips = (float)insc/1000000.0f;
			insc = 0;
			flops = (float)fpucount/1000000.0f;
			fpucount = 0;
			sreadlnum = readlnum;
			swritelnum = writelnum;
			segareads = egareads;
			segawrites = egawrites;
			scycles_lost = cycles_lost;

#ifdef USE_DYNAREC
			cpu_recomp_blocks_latched = cpu_recomp_blocks;
			cpu_recomp_ins_latched = cpu_state.cpu_recomp_ins;
			cpu_recomp_full_ins_latched = cpu_recomp_full_ins;
			cpu_new_blocks_latched = cpu_new_blocks;
			cpu_recomp_flushes_latched = cpu_recomp_flushes;
			cpu_recomp_evicted_latched = cpu_recomp_evicted;
			cpu_recomp_reuse_latched = cpu_recomp_reuse;
			cpu_recomp_removed_latched = cpu_recomp_removed;
			cpu_reps_latched = cpu_reps;
			cpu_notreps_latched = cpu_notreps;

			cpu_recomp_blocks = 0;
			cpu_state.cpu_recomp_ins = 0;
			cpu_recomp_full_ins = 0;
			cpu_new_blocks = 0;
			cpu_recomp_flushes = 0;
			cpu_recomp_evicted = 0;
			cpu_recomp_reuse = 0;
			cpu_recomp_removed = 0;
			cpu_reps = 0;
			cpu_notreps = 0;
#endif

			readlnum = writelnum = 0;
			egareads = egawrites = 0;
			cycles_lost = 0;
			mmuflush = 0;
			emu_fps = frames;
			frames = 0;

			/* We need a Status window update now. */
			status_update_needed = 1;
		}

		if (title_update) {
			swprintf(temp, sizeof_w(temp),
#ifdef _WIN32
				 L"%S %S - %i%% - %S - %S",
#else
				 L"%s %s - %i%% - %s - %s",
#endif
				 EMU_NAME,emu_version,fps,machine_getname(),
				 machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].name);
			if (mouse_type != MOUSE_TYPE_NONE) {
				wcscat(temp, L" - ");
				if (!mouse_capture) {
					wcscat(temp, plat_get_string(IDS_2077));
				} else {
					wcscat(temp, (mouse_get_buttons() > 2) ? plat_get_string(IDS_2078) : plat_get_string(IDS_2079));
				}
			}

			ui_window_title(temp);

			title_update = 0;
		}

		/* One more frame done! */
		done++;

		/* Every 200 frames we save the machine status. */
		if (++frames >= 200 && nvr_dosave) {
			nvr_save();
			nvr_dosave = 0;
			frames = 0;
		}

		end_time = plat_timer_read();
		main_time += (end_time - start_time);
	} else {
		/* Just so we dont overload the host OS. */
		plat_delay_ms(1);
	}

	/* If needed, handle a screen resize. */
	if (doresize && !video_fullscreen) {
		plat_resize(scrnsz_x, scrnsz_y);

		doresize = 0;
	}
    }

    pclog("PC: main thread done.\n");
}


/* Handler for the 1-second timer to refresh the window title. */
void
pc_onesec(void)
{
    fps = framecount;
    framecount = 0;

    title_update = 1;
}


void
set_screen_size(int x, int y)
{
    int owsx = scrnsz_x;
    int owsy = scrnsz_y;
    int temp_overscan_x = overscan_x;
    int temp_overscan_y = overscan_y;
    double dx, dy, dtx, dty;

    /* Make sure we keep usable values. */
#if 0
    pclog("SetScreenSize(%d, %d) resize=%d\n", x, y, vid_resize);
#endif
    if (x < 320) x = 320;
    if (y < 200) y = 200;
    if (x > 2048) x = 2048;
    if (y > 2048) y = 2048;

    /* Save the new values as "real" (unscaled) resolution. */
    unscaled_size_x = x;
    efscrnsz_y = y;

    if (suppress_overscan)
	temp_overscan_x = temp_overscan_y = 0;

    if (force_43) {
	dx = (double)x;
	dtx = (double)temp_overscan_x;

	dy = (double)y;
	dty = (double)temp_overscan_y;

	/* Account for possible overscan. */
	if (!(video_is_ega_vga()) && (temp_overscan_y == 16)) {
		/* CGA */
		dy = (((dx - dtx) / 4.0) * 3.0) + dty;
	} else if (!(video_is_ega_vga()) && (temp_overscan_y < 16)) {
		/* MDA/Hercules */
		dy = (x / 4.0) * 3.0;
	} else {
		if (enable_overscan) {
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

    switch(scale) {
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


int
get_actual_size_x(void)
{
    return(unscaled_size_x);
}


int
get_actual_size_y(void)
{
    return(efscrnsz_y);
}
