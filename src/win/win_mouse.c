/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Mouse interface to host using RawInput.
 *
 * Version:	@(#)win_mouse.c	1.0.10	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		GH Cao, <driver1998.ms@outlook.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *  		Copyright 2019 GH Cao.
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
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdint.h>
#include "../emu.h"
#include "../config.h"
#include "../plat.h"
#include "../devices/input/mouse.h"
#include "win.h"


typedef struct {
    int	buttons;
    int dx;
    int dy;
    int dwheel;
} MOUSESTATE;


static MOUSESTATE		mousestate;


void
win_mouse_close(void)
{
    RAWINPUTDEVICE ridev;

    ridev.dwFlags = RIDEV_REMOVE;
    ridev.hwndTarget = NULL;
    ridev.usUsagePage = 0x01;
    ridev.usUsage = 0x02;

    RegisterRawInputDevices(&ridev, 1, sizeof(ridev));
}


/* Initialize the RawInput (mouse) module. */
int
win_mouse_init(void)
{
    RAWINPUTDEVICE ridev;

    atexit(win_mouse_close);

    mouse_capture = 0;

    ridev.dwFlags = 0;
    ridev.hwndTarget = NULL;
    ridev.usUsagePage = 0x01;
    ridev.usUsage = 0x02;
    if (! RegisterRawInputDevices(&ridev, 1, sizeof(ridev))) {
	ERRLOG("ERROR: RegisterRawInputDevices failed for MOUSE\n"); 
	return(0);
    }

    memset(&mousestate, 0x00, sizeof(MOUSESTATE));

    return(1);
}


void
mouse_handle(RAWINPUT *raw)
{
    static int x, y;
    static int b = 0;
    RAWMOUSE state = raw->data.mouse;

    /* Read mouse buttons and wheel. */
    if (state.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
	mousestate.buttons |= 1;
    else if (state.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
	mousestate.buttons &= ~1;

    if (state.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
	mousestate.buttons |= 4;
    else if (state.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
	mousestate.buttons &= ~4;

    if (state.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
	mousestate.buttons |= 2;
    else if (state.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
	mousestate.buttons &= ~2;

    if (state.usButtonFlags & RI_MOUSE_WHEEL)
	mousestate.dwheel += (SHORT)state.usButtonData / 120;

    if (state.usFlags & MOUSE_MOVE_ABSOLUTE) {
	/*
	 * Absolute mouse, i.e. RDP or VNC seems to work fine for
	 * RDP on Windows 10.  Not sure about other environments.
	 */
	mousestate.dx += (state.lLastX - x)/25;
	mousestate.dy += (state.lLastY - y)/25;
	x = state.lLastX; 
	y = state.lLastY;
    } else {
	/* Relative mouse, i.e. regular mouse. */
	mousestate.dx += state.lLastX;
	mousestate.dy += state.lLastY;
    }

    /* Only update the guest mouse if we have to. */
    if (!mouse_capture && !config.vid_fullscreen)
	return;

    if (mousestate.dx != 0 || mousestate.dy != 0 || mousestate.dwheel != 0) {
	mouse_x += mousestate.dx;
	mouse_y += mousestate.dy;
	mouse_z = mousestate.dwheel;

	mousestate.dx = 0;
	mousestate.dy = 0;
	mousestate.dwheel = 0;
    }

    if (b != mousestate.buttons) {
	mouse_buttons = mousestate.buttons;
	b = mousestate.buttons;
    }
}
