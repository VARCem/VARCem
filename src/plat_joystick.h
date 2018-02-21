/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define platform joystick interface.
 *
 * NOTE:	This should be worked into platform code and joystick.h.
 *
 * Version:	@(#)plat_joystick.h	1.0.1	2018/02/14
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
#ifndef PLAT_JOYSTICK_H
# define PLAT_JOYSTICK_H


#define MAX_PLAT_JOYSTICKS	8
#define MAX_JOYSTICKS		4

#define POV_X	0x80000000
#define POV_Y	0x40000000


typedef struct {
    char	name[64];

    int		a[8];
    int		b[32];
    int		p[4];

    struct {
	char name[32];
	int id;
    }		axis[8];

    struct {
	char name[32];
	int id;
    }		button[32];

    struct {
	char name[32];
	int id;
    }		pov[4];

    int		nr_axes;
    int		nr_buttons;
    int		nr_povs;
} plat_joystick_t;

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

extern plat_joystick_t	plat_joystick_state[MAX_PLAT_JOYSTICKS];
extern joystick_t	joystick_state[MAX_JOYSTICKS];
extern int		joysticks_present;


#define JOYSTICK_PRESENT(n) (joystick_state[n].plat_joystick_nr != 0)


extern void	joystick_init(void);
extern void	joystick_close(void);
extern void	joystick_process(void);

#ifdef __cplusplus
}
#endif


#endif	/*PLAT_JOYSTICK_H*/
