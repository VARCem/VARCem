/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle all the various gameport devices.
 *
 * Version:	@(#)game_dev.c	1.0.2	2018/05/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "game.h"
#include "game_dev.h"


extern const gamedev_t js_standard;
extern const gamedev_t js_standard_4btn;
extern const gamedev_t js_standard_6btn;
extern const gamedev_t js_standard_8btn;

extern const gamedev_t js_ch_fs_pro;

extern const gamedev_t js_sw_pad;

extern const gamedev_t js_tm_fcs;


static const gamedev_t gamedev_none = {
    "Disabled",
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    0, 0, 0
};


static const gamedev_t *devices[] = {
    &gamedev_none,

    &js_standard,
    &js_standard_4btn,
    &js_standard_6btn,
    &js_standard_8btn,
    &js_ch_fs_pro,
    &js_sw_pad,
    &js_tm_fcs,

    NULL
};


const gamedev_t *
gamedev_get_device(int js)
{
    return(devices[js]);
}


const char *
gamedev_get_name(int js)
{
    if (devices[js] == NULL)
	return(NULL);

    return(devices[js]->name);
}


int
gamedev_get_max_joysticks(int js)
{
    return(devices[js]->max_joysticks);
}


int
gamedev_get_axis_count(int js)
{
    return(devices[js]->axis_count);
}


int
gamedev_get_button_count(int js)
{
    return(devices[js]->button_count);
}


int
gamedev_get_pov_count(int js)
{
    return(devices[js]->pov_count);
}


const char *
gamedev_get_axis_name(int js, int id)
{
    return(devices[js]->axis_names[id]);
}


const char *
gamedev_get_button_name(int js, int id)
{
    return(devices[js]->button_names[id]);
}


const char *
gamedev_get_pov_name(int js, int id)
{
    return(devices[js]->pov_names[id]);
}
