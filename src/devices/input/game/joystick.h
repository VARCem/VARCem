/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the joystick devices.
 *
 * Version:	@(#)joystick.h	1.0.1	2018/04/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#ifndef EMU_JOYSTICK_H
# define EMU_JOYSTICK_H


#define JOYSTICK_NONE		0		/* no joystick defined */

#define MAX_JOYSTICKS		4

#define POV_X			0x80000000
#define POV_Y			0x40000000

#define AXIS_NOT_PRESENT	-99999

#define JOYSTICK_PRESENT(n)	(joystick_state[n].plat_joystick_nr != 0)


typedef struct {
    int		axis[8];
    int		button[32];
    int		pov[4];

    int		plat_joystick_nr;
    int		axis_mapping[8];
    int		button_mapping[32];
    int		pov_mapping[4][2];
} joystick_t;


#ifdef __cplusplus
extern "C" {
#endif

extern joystick_t	joystick_state[MAX_JOYSTICKS];
extern int		joysticks_present;


/* Defined in the platform module. */
extern void		joystick_init(void);
extern void		joystick_close(void);
extern void		joystick_process(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_JOYSTICK_H*/
