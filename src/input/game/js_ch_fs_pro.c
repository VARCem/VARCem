/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Flight Stick Pro.
 *
 * Version:	@(#)js_fs_pro.c	1.0.6	2018/04/26
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
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
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../ports/game_dev.h"
#include "joystick.h"


static uint8_t
js_read(void *priv)
{
    uint8_t ret = 0xf0;
	
    if (JOYSTICK_PRESENT(0)) {
	if (joystick_state[0].button[0])
		ret &= ~0x10;
	if (joystick_state[0].button[1])
		ret &= ~0x20;
	if (joystick_state[0].button[2])
		ret &= ~0x40;
	if (joystick_state[0].button[3])
		ret &= ~0x80;
	if (joystick_state[0].pov[0] != -1) {
		if (joystick_state[0].pov[0] > 315 || joystick_state[0].pov[0] < 45)
			ret &= ~0xf0;
		else if (joystick_state[0].pov[0] >= 45 && joystick_state[0].pov[0] < 135)
			ret &= ~0xb0;
		else if (joystick_state[0].pov[0] >= 135 && joystick_state[0].pov[0] < 225)
			ret &= ~0x70;
		else if (joystick_state[0].pov[0] >= 225 && joystick_state[0].pov[0] < 315)
			ret &= ~0x30;
	}
    }

    return(ret);
}


static void
js_write(void *priv)
{
}


static int
js_read_axis(void *priv, int axis)
{
    if (! JOYSTICK_PRESENT(0)) return(AXIS_NOT_PRESENT);

    switch (axis) {
	case 0:
		return(joystick_state[0].axis[0]);

	case 1:
		return(joystick_state[0].axis[1]);

	case 2:
		return(0);

	case 3:
		return(joystick_state[0].axis[2]);

	default:
		return(0);
    }
}


static void
js_over(void *priv)
{
}


static void *
js_init(void)
{
    return(NULL);
}


static void
js_close(void *priv)
{
}


const gamedev_t js_ch_fs_pro = {
    "CH Flightstick Pro",
    js_init, js_close,
    js_read, js_write,
    js_read_axis,
    js_over,
    3,
    4,
    1,
    1,
    { "X axis", "Y axis", "Throttle" },
    { "Button 1", "Button 2", "Button 3", "Button 4" },
    { "POV" }
};
