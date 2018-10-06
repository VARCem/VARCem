/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the device handler.
 *
 * Version:	@(#)device.h	1.0.8	2018/09/19
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
#ifndef EMU_DEVICE_H
# define EMU_DEVICE_H


#define CONFIG_STRING		0
#define CONFIG_INT		1
#define CONFIG_BINARY		2
#define CONFIG_SELECTION	3
#define CONFIG_MIDI		4
#define CONFIG_FNAME		5
#define CONFIG_SPINNER		6
#define CONFIG_HEX16		7
#define CONFIG_HEX20		8
#define CONFIG_MAC		9


enum {
    DEVICE_ALL = 0x0000,	/* any/all device */
    DEVICE_UNSTABLE = 0x0001,	/* unstable device, be cautious */
    DEVICE_AT = 0x0002,		/* requires an AT-compatible system */
    DEVICE_PS2 = 0x0004,	/* requires a PS/1 or PS/2 system */
    DEVICE_ISA = 0x0100,	/* requires the ISA bus */
    DEVICE_CBUS = 0x0200,	/* requires the C-BUS bus */
    DEVICE_MCA = 0x0400,	/* requires the MCA bus */
    DEVICE_EISA = 0x0800,	/* requires the EISA bus */
    DEVICE_VLB = 0x1000,	/* requires the PCI bus */
    DEVICE_PCI = 0x2000,	/* requires the VLB bus */
    DEVICE_AGP = 0x4000		/* requires the AGP bus */
};
#define DEVICE_SYS_MASK	0x0006
#define DEVICE_BUS_MASK	0xff00


typedef struct {
    const char *description;
    int  value;
} device_config_selection_t;

typedef struct {
    const char *description;
    const char *extensions[5];
} device_config_file_filter_t;

typedef struct {
    int min;
    int max;
    int step;
} device_config_spinner_t;

typedef struct {
    const char *name;
    const char *description;
    int type;
    const char *default_string;
    int default_int;
    device_config_selection_t selection[16];
    device_config_file_filter_t file_filter[16];
    device_config_spinner_t spinner;
} device_config_t;

typedef struct _device_ {
    const char	*name;
    uint32_t	flags;		/* system flags */
    uint32_t	local;		/* flags local to device */

    void	*(*init)(const struct _device_ *);
    void	(*close)(void *priv);
    void	(*reset)(void *priv);
    void	*u1_reuse;
#define ms_poll		u1_reuse
#define dev_available	u1_reuse
    void	(*speed_changed)(void *priv);
    void	(*force_redraw)(void *priv);
    const void	*u2_reuse;
#define vid_timing	u2_reuse
#define mca_reslist	u2_reuse
    const device_config_t *config;
} device_t;


#ifdef __cplusplus
extern "C" {
#endif

extern void		device_init(void);
extern const device_t	*device_clone(const device_t *master);
extern void		*device_add(const device_t *);
extern void		device_add_ex(const device_t *d, void *priv);
extern void		device_close_all(void);
extern void		device_reset_all(int flags);
extern void		*device_get_priv(const device_t *);
extern const char	*device_get_bus_name(const device_t *);
extern int		device_available(const device_t *);
extern void		device_speed_changed(void);
extern void		device_force_redraw(void);

extern int		device_is_valid(const device_t *, int machine_flags);

extern int		device_get_config_int(const char *name);
extern int		device_get_config_int_ex(const char *s, int dflt_int);
extern int		device_get_config_hex16(const char *name);
extern int		device_get_config_hex20(const char *name);
extern int		device_get_config_mac(const char *name, int dflt_int);
extern void		device_set_config_int(const char *s, int val);
extern void		device_set_config_hex16(const char *s, int val);
extern void		device_set_config_hex20(const char *s, int val);
extern void		device_set_config_mac(const char *s, int val);
extern const char	*device_get_config_string(const char *name);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_DEVICE_H*/
