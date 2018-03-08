/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the generic game port handlers.
 *
 * Version:	@(#)gameport.h	1.0.1	2018/02/14
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2017 Sarah Walker.
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
#ifndef EMU_GAMEPORT_H
# define EMU_GAMEPORT_H


#define AXIS_NOT_PRESENT -99999


typedef struct
{
        const char *name;
        void *(*init)(void);
        void (*close)(void *p);
        uint8_t (*read)(void *p);
        void (*write)(void *p);
        int (*read_axis)(void *p, int axis);
        void (*a0_over)(void *p);
        int axis_count, button_count, pov_count;
        int max_joysticks;
        const char *axis_names[8];
        const char *button_names[32];
        const char *pov_names[4];
} joystick_if_t;



#ifdef __cplusplus
extern "C" {
#endif

extern device_t gameport_device;
extern device_t gameport_201_device;

extern int joystick_type;


extern char	*joystick_get_name(int64_t joystick);
extern int64_t	joystick_get_max_joysticks(int64_t joystick);
extern int64_t	joystick_get_axis_count(int64_t joystick);
extern int64_t	joystick_get_button_count(int64_t joystick);
extern int64_t	joystick_get_pov_count(int64_t joystick);
extern char	*joystick_get_axis_name(int64_t joystick, int64_t id);
extern char	*joystick_get_button_name(int64_t joystick, int64_t id);
extern char	*joystick_get_pov_name(int64_t joystick, int64_t id);

extern void	gameport_update_joystick_type(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_GAMEPORT_H*/
