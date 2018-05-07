/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of a standard joystick.
 *
 * Version:	@(#)js_standard.c	1.0.9	2018/05/06
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
#include "../../../emu.h"
#include "../../../timer.h"
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
    }

    if (JOYSTICK_PRESENT(1)) {
	if (joystick_state[1].button[0])
		ret &= ~0x40;
	if (joystick_state[1].button[1])
		ret &= ~0x80;
    }

    return(ret);
}


static uint8_t
js_read_4button(void *priv)
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
    }

    return(ret);
}


static void
js_write(void *priv)
{
}


static int
js_axis(void *priv, int axis)
{
    int ret = AXIS_NOT_PRESENT;

    switch (axis) {
	case 0:
		if (JOYSTICK_PRESENT(0))
			ret = joystick_state[0].axis[0];
		break;

	case 1:
		if (JOYSTICK_PRESENT(0))
			ret = joystick_state[0].axis[1];
		break;

	case 2:
		if (JOYSTICK_PRESENT(1))
			ret = joystick_state[1].axis[0];
		break;

	case 3:
		if (JOYSTICK_PRESENT(1))
			ret = joystick_state[1].axis[1];
		break;

	default:
		break;
    }

    return(ret);
}


static int
js_axis_4button(void *priv, int axis)
{
    int ret = AXIS_NOT_PRESENT;

    if (! JOYSTICK_PRESENT(0)) return(ret);

    switch (axis) {
	case 0:
		ret = joystick_state[0].axis[0];
		break;

	case 1:
		ret = joystick_state[0].axis[1];
		break;

	case 2:
	case 3:
	default:
		ret = 0;
		break;
    }

    return(ret);
}


static int
js_axis_6button(void *priv, int axis)
{
    int ret = AXIS_NOT_PRESENT;

    if (! JOYSTICK_PRESENT(0)) return(ret);

    switch (axis) {
	case 0:
		ret = joystick_state[0].axis[0];
		break;

	case 1:
		ret = joystick_state[0].axis[1];
		break;

	case 2:
		ret = joystick_state[0].button[4] ? -32767 : 32768;
		break;

	case 3:
		ret = joystick_state[0].button[5] ? -32767 : 32768;
		break;

	default:
		ret = 0;
		break;
    }

    return(ret);
}


static int
js_axis_8button(void *priv, int axis)
{
    int ret = AXIS_NOT_PRESENT;

    if (! JOYSTICK_PRESENT(0)) return(ret);

    switch (axis) {
	case 0:
		ret = joystick_state[0].axis[0];

	case 1:
		ret = joystick_state[0].axis[1];

	case 2:
		ret = 0;
		if (joystick_state[0].button[4])
			ret = -32767;
		if (joystick_state[0].button[6])			
			ret = 32768;
		break;

	case 3:
		ret = 0;
		if (joystick_state[0].button[5])
			ret = -32767;
		if (joystick_state[0].button[7])			
			ret = 32768;
		break;

	default:
		ret = 0;
		break;
    }

    return(ret);
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


const gamedev_t js_standard = {
    "Standard 2-button joystick(s)",
    js_init, js_close,
    js_read, js_write,
    js_axis,
    js_over,
    2,
    2,
    0,
    2,
    { "X axis", "Y axis" },
    { "Button 1", "Button 2" }
};

const gamedev_t js_standard_4btn = {
    "Standard 4-button joystick",
    js_init, js_close,
    js_read_4button, js_write,
    js_axis_4button, js_over,
    2,
    4,
    0,
    1,
    { "X axis", "Y axis" },
    { "Button 1", "Button 2", "Button 3", "Button 4" }
};

const gamedev_t js_standard_6btn = {
    "Standard 6-button joystick",
    js_init, js_close,
    js_read_4button, js_write,
    js_axis_6button, js_over,
    2,
    6,
    0,
    1,
    { "X axis", "Y axis" },
    { "Button 1", "Button 2", "Button 3", "Button 4", "Button 5", "Button 6" }
};

const gamedev_t js_standard_8btn = {
    "Standard 8-button joystick",
    js_init, js_close,
    js_read_4button, js_write,
    js_axis_8button, js_over,
    2,
    8,
    0,
    1,
    { "X axis", "Y axis" },
    { "Button 1", "Button 2", "Button 3", "Button 4",
      "Button 5", "Button 6", "Button 7", "Button 8" }
};
