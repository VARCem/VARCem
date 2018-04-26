/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Mouse interface to host device.
 *
 * Version:	@(#)win_mouse.cpp	1.0.4	2018/04/26
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
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <stdio.h>
#include <stdint.h>
#include "../emu.h"
#include "../input/mouse.h"
#include "../plat.h"
#include "win.h"


int mouse_capture;


static LPDIRECTINPUT8 lpdi;
static LPDIRECTINPUTDEVICE8 lpdi_mouse = NULL;
static DIMOUSESTATE mousestate;


void
win_mouse_init(void)
{
    atexit(win_mouse_close);

    mouse_capture = 0;

    if (FAILED(DirectInput8Create(hinstance, DIRECTINPUT_VERSION,
	       IID_IDirectInput8A, (void **) &lpdi, NULL)))
	fatal("plat_mouse_init: DirectInputCreate failed\n"); 

    if (FAILED(lpdi->CreateDevice(GUID_SysMouse, &lpdi_mouse, NULL)))
	fatal("plat_mouse_init: CreateDevice failed\n");

    if (FAILED(lpdi_mouse->SetCooperativeLevel(hwndMain,
	       DISCL_FOREGROUND | (vid_fullscreen ? DISCL_EXCLUSIVE : DISCL_NONEXCLUSIVE))))
	fatal("plat_mouse_init: SetCooperativeLevel failed\n");

    if (FAILED(lpdi_mouse->SetDataFormat(&c_dfDIMouse)))
	fatal("plat_mouse_init: SetDataFormat failed\n");
}


void
win_mouse_close(void)
{
    if (lpdi_mouse != NULL) {
	lpdi_mouse->Release();
	lpdi_mouse = NULL;
    }
}


void
mouse_poll(void)
{
    static int buttons = 0;
    static int x = 0, y = 0, z = 0;
    int b;

    if (FAILED(lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE),
				(LPVOID)&mousestate))) {
	lpdi_mouse->Acquire();
	lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mousestate);
    }                

    if (mouse_capture || vid_fullscreen) {
	if (x != mousestate.lX || y != mousestate.lY || z != mousestate.lZ) {
		mouse_x += mousestate.lX;
		mouse_y += mousestate.lY;
		mouse_z += mousestate.lZ/120;

		x = mousestate.lX;
		y = mousestate.lY;        
		z = mousestate.lZ/120;
	}

	b = 0;
	if (mousestate.rgbButtons[0] & 0x80) b |= 1;
	if (mousestate.rgbButtons[1] & 0x80) b |= 2;
	if (mousestate.rgbButtons[2] & 0x80) b |= 4;
	if (buttons != b) {
		mouse_buttons = b;
		buttons = b;
	}
    }
}
