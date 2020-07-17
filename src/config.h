/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Configuration file handler header.
 *
 * Version:	@(#)config.h	1.0.6	2020/07/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#ifndef EMU_CONFIG_H
# define EMU_CONFIG_H


/* Maximum units for certain devices. */
#define SERIAL_MAX	2			/* two ports supported */
#define PARALLEL_MAX	3			/* three ports supported */
#define ISAMEM_MAX	4			/* max #cards in system */
#define FLOPPY_MAX	4			/* max #drives in system */
#define DISK_MAX	32			/* max #drives in system */


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int		language;			/* language ID */
    int		rctrl_is_lalt;			/* set R-CTRL as L-ALT */

    int		window_remember,		/* window size and position */
		win_w, win_h, win_x, win_y;

    int		machine_type;			/* current machine ID */
    int		cpu_manuf,			/* cpu manufacturer */
		cpu_type,			/* cpu type */
		cpu_use_dynarec,		/* cpu uses/needs Dyna */
		cpu_waitstates,
		enable_ext_fpu;			/* enable external FPU */

    int		mem_size;			/* memory size */

    int		time_sync;			/* enable time sync */

    int		vid_api;			/* video renderer */

    int		video_card,			/* graphics/video card */
		voodoo_enabled;			/* video option */

    int		scale,				/* screen scale factor */
		vid_resize,			/* allow resizing */
		vid_cga_contrast,		/* video */
		vid_fullscreen,			/* video */
		vid_fullscreen_scale,		/* video */
		vid_fullscreen_first,		/* video */
		vid_grayscale,			/* video */
		vid_graytype,			/* video */
		invert_display,			/* invert the display */
		enable_overscan,		/* enable overscans */
		force_43;			/* video */

    int		mouse_type;			/* selected mouse type */
    int		joystick_type;			/* joystick type */

    int		sound_is_float,			/* sound uses FP values */
		sound_gain,			/* sound volume gain */
		sound_card,			/* selected sound card */
		mpu401_standalone_enable,	/* sound option */
		midi_device;			/* selected midi device */

    int		game_enabled,			/* enable game port */
		serial_enabled[SERIAL_MAX],	/* enable serial ports */
		parallel_enabled[PARALLEL_MAX],	/* enable LPT ports */
		parallel_device[PARALLEL_MAX];	/* set up LPT devices */

    struct {
	int	type;				/* drive type */
	int	turbo;				/* drive in turbo mode */
	int	ckbpb;				/* have to check bpb */
	int	wp;				/* drive is write-protected */
 	wchar_t	path[512];			/* path of mounted image */
    }		floppy[FLOPPY_MAX];

    // FIXME: HDD

    // FIXME: CDROM

    // FIXME: ZIP

    int		fdc_type;			/* FDC type (ISA) */

    int		hdc_type,			/* HDC type */
		ide_ter_enabled,
		ide_qua_enabled;

    int		scsi_card;			/* selected SCSI card */

    int		network_type;			/* net provider type */
    int		network_card;			/* net interface num */
    char	network_host[128];		/* host network intf */

    int		bugger_enabled;			/* enable ISAbugger */

    int		isamem_type[ISAMEM_MAX];	/* enable ISA mem cards */

    int		isartc_type;			/* enable ISA RTC card */

#ifdef WALTJE
    int		romdos_enabled;			/* enable ROM DOS */
#endif
} config_t;


/* Global variables. */
extern config_t	config;


/* Functions. */
extern void	config_init(config_t *);
extern int	config_load(config_t *);
extern void	config_save(void);
extern void	config_write(const wchar_t *fn);
extern void	config_dump(void);
extern int	config_compare(const config_t *one, const config_t *two);

extern void	config_delete_var(const char *cat, const char *name);
extern int	config_get_int(const char *cat, const char *name, int def);
extern int	config_get_hex16(const char *cat, const char *name, int def);
extern int	config_get_hex20(const char *cat, const char *name, int def);
extern int	config_get_mac(const char *cat, const char *name, int def);
extern char	*config_get_string(const char *cat, const char *name, const char *def);
extern wchar_t	*config_get_wstring(const char *cat, const char *name, const wchar_t *def);
extern void	config_set_int(const char *cat, const char *name, int val);
extern void	config_set_hex16(const char *cat, const char *name, int val);
extern void	config_set_hex20(const char *cat, const char *name, int val);
extern void	config_set_mac(const char *cat, const char *name, int val);
extern void	config_set_string(const char *cat, const char *name, const char *val);
extern void	config_set_wstring(const char *cat, const char *name, const wchar_t *val);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_CONFIG_H*/
