/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Mouse interface to host using DirectInput or RawInput.
 *
 * Version:	@(#)win_mouse.cpp	1.0.9	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		GH Cao, <driver1998.ms@outlook.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifdef USE_DINPUT
# define DIRECTINPUT_VERSION 0x0800
# include <dinput.h>
# define MOUSESTATE	DIMOUSESTATE
#else
# include <windowsx.h>
  typedef struct {
    int		rgbButtons[3];
    int		lX,
		lY;
    int		lZ;
  } MOUSESTATE;
#endif
#include <stdio.h>
#include <stdint.h>
#include "../emu.h"
#include "../config.h"
#include "../plat.h"
#include "../devices/input/mouse.h"
#include "win.h"


#ifdef USE_DINPUT
static LPDIRECTINPUTDEVICE8	lpdi_mouse = NULL;
#endif
static MOUSESTATE		mousestate;


void
win_mouse_close(void)
{
#ifdef USE_DINPUT
    if (lpdi_mouse != NULL) {
	lpdi_mouse->Release();
	lpdi_mouse = NULL;
    }
#else
    RAWINPUTDEVICE ridev;

    ridev.dwFlags = RIDEV_REMOVE;
    ridev.hwndTarget = NULL;
    ridev.usUsagePage = 0x01;
    ridev.usUsage = 0x02;

    RegisterRawInputDevices(&ridev, 1, sizeof(ridev));
#endif
}


void
win_mouse_init(void)
{
#ifdef USE_DINPUT
    LPDIRECTINPUT8 lpdi;
#else
    RAWINPUTDEVICE ridev;
#endif

    atexit(win_mouse_close);

    mouse_capture = 0;

#ifdef USE_DINPUT
    if (FAILED(DirectInput8Create(hInstance, DIRECTINPUT_VERSION,
	       IID_IDirectInput8A, (void **) &lpdi, NULL)))
	fatal("plat_mouse_init: DirectInputCreate failed\n"); 

    if (FAILED(lpdi->CreateDevice(GUID_SysMouse, &lpdi_mouse, NULL)))
	fatal("plat_mouse_init: CreateDevice failed\n");

    if (FAILED(lpdi_mouse->SetCooperativeLevel(hwndMain,
	       DISCL_FOREGROUND | (config.vid_fullscreen ? DISCL_EXCLUSIVE : DISCL_NONEXCLUSIVE))))
	fatal("plat_mouse_init: SetCooperativeLevel failed\n");

    if (FAILED(lpdi_mouse->SetDataFormat(&c_dfDIMouse)))
	fatal("plat_mouse_init: SetDataFormat failed\n");
#else
    /* Initialize the RawInput (mouse) module. */
    ridev.dwFlags = 0;
    ridev.hwndTarget = NULL;
    ridev.usUsagePage = 0x01;
    ridev.usUsage = 0x02;
    if (! RegisterRawInputDevices(&ridev, 1, sizeof(ridev)))
	fatal("plat_mouse_init: RegisterRawInputDevices failed\n"); 
#endif

    memset(&mousestate, 0x00, sizeof(MOUSESTATE));
}


#ifdef USE_DINPUT
void
mouse_process(void)
{
    static int buttons = 0;
    static int x = 0, y = 0, z = 0;
    int b, changed;

    if (FAILED(lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE),
				(LPVOID)&mousestate))) {
	lpdi_mouse->Acquire();
	lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mousestate);
    }                

    if (!mouse_capture && !config.vid_fullscreen) return;

    changed = 0;
    if (x != mousestate.lX || y != mousestate.lY || z != mousestate.lZ) {
	mouse_x = mousestate.lX;
	mouse_y = mousestate.lY;
	mouse_z = mousestate.lZ / 120;

	x = mousestate.lX;
	y = mousestate.lY;        
	z = mousestate.lZ / 120;

	changed++;
    }

    b = 0;
    if (mousestate.rgbButtons[0] & 0x80) b |= 1;
    if (mousestate.rgbButtons[1] & 0x80) b |= 2;
    if (mousestate.rgbButtons[2] & 0x80) b |= 4;
    if (buttons != b) {
	mouse_buttons = b;
	buttons = b;

	changed++;
    }
}

#else

void
mouse_handle(LPARAM lParam, int focus)
{
    static int buttons = 0;
    static int x = 0, y = 0, z = 0;
    uint32_t ri_size;
    UINT size = 0;
    RAWINPUT *raw;
    RAWMOUSE *st;
    int b, changed;

    if (! focus)
	return;

    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL,
		    &size, sizeof(RAWINPUTHEADER));
    raw = (RAWINPUT *)mem_alloc(size);

    /* Read the raw input data for the mouse. */
    ri_size = GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
			      raw, &size, sizeof(RAWINPUTHEADER));

    /* If wrong size, or input is not a mouse, ignore it. */	
    if ((ri_size != size) || (raw->header.dwType != RIM_TYPEMOUSE)) {
        ERRLOG("MOUSE: bad event buffer %i/%i\n", size, ri_size);
	free(raw);
	return;
    }

    /* Read mouse buttons and wheel. */
    st = &raw->data.mouse;
    if (st->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
	mousestate.rgbButtons[0] |= 0x80;
    else if (st->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
	mousestate.rgbButtons[0] &= ~0x80;
    if (st->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
	mousestate.rgbButtons[1] |= 0x80;
    else if (st->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
	mousestate.rgbButtons[1] &= ~0x80;
    if (st->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
	mousestate.rgbButtons[2] |= 0x80;
    else if (st->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
	mousestate.rgbButtons[2] &= ~0x80;

    if (st->usButtonFlags & RI_MOUSE_WHEEL)
	mousestate.lZ += (SHORT)st->usButtonData;

    if (st->usFlags & MOUSE_MOVE_ABSOLUTE) {
	/* absolute mouse, i.e. RDP or VNC 
	 * seems to work fine for RDP on Windows 10
	 * Not sure about other environments.
	 */
	static int old_x = st->lLastX, old_y = st->lLastY;

	mousestate.lX += (st->lLastX - old_x) / 100;
	mousestate.lY += (st->lLastY - old_y) / 100;
	old_x = st->lLastX; 
	old_y = st->lLastY;
INFO("MOUSE: abs, %i/%i\n", mousestate.lX, mousestate.lY);
    } else {
	/* Relative mouse, i.e. regular mouse. */
	mousestate.lX += st->lLastX / 100;
	mousestate.lY += st->lLastY / 100;
INFO("MOUSE: rel, %i/%i\n", mousestate.lX, mousestate.lY);
    }

    /* No longer needed. */
    free(raw);

    if (!mouse_capture && !config.vid_fullscreen) return;

    changed = 0;
INFO("MOUSE: handle(focus=%i) dX=%i dY=%i dZ=%i B=(%i,%i,%i)\n", focus,
	mousestate.lX, mousestate.lY, mousestate.lZ, !!mousestate.rgbButtons[0],
	!!mousestate.rgbButtons[1], !!mousestate.rgbButtons[2]);
    if (x != mousestate.lX || y != mousestate.lY || z != mousestate.lZ) {
	mouse_x = mousestate.lX;
	mouse_y = mousestate.lY;
	mouse_z = mousestate.lZ / 120;

	x = mousestate.lX;
	y = mousestate.lY;        
	z = mousestate.lZ / 120;

	changed++;
    }

    b = 0;
    if (mousestate.rgbButtons[0] & 0x80) b |= 1;
    if (mousestate.rgbButtons[1] & 0x80) b |= 2;
    if (mousestate.rgbButtons[2] & 0x80) b |= 4;
    if (buttons != b) {
	mouse_buttons = b;
	buttons = b;

	changed++;
    }

    /* Handle the machine end of things. */
    if (changed)
	mouse_poll();
}

#endif
