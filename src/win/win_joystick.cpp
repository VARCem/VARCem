/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Joystick interface to host device. Since this is a lot more
 *		convienient, we also have the Configuration Dialog in this
 *		file.
 *
 * NOTE:	Hacks currently needed to compile with MSVC; DX needs to
 *		be updated to 11 or 12 or so.  --FvK
 *
 * Version:	@(#)win_joystick.cpp	1.0.18	2018/10/05
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#ifndef DIDEVTYPE_JOYSTICK
  /* TODO: This is a crude hack to fix compilation on MSVC ...
   * it needs a rework at some point
   */
# define DIDEVTYPE_JOYSTICK 4
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../device.h"
#include "../plat.h"
#include "../ui/ui.h"
#include "../devices/ports/game_dev.h"
#include "../devices/input/game/joystick.h"
#include "win.h"
#include "resource.h"


#define MAX_PLAT_JOYSTICKS	8
#define STR_FONTNAME		"Segoe UI"


typedef struct {
    char	name[64];

    int		a[8];
    int		b[32];
    int		p[4];
    struct {
	char	name[32];
	int	id;
    }		axis[8];
    struct {
	char	name[32];
	int	id;
    }		button[32];
    struct {
	char	name[32];
	int	id;
    }		pov[4];
    int		nr_axes;
    int		nr_buttons;
    int		nr_povs;
} plat_joystick_t;


static plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
static LPDIRECTINPUTDEVICE8 lpdi_joystick[2] = {NULL, NULL};
static GUID joystick_guids[MAX_JOYSTICKS];
static LPDIRECTINPUT8 lpdi;


/* Callback for DirectInput. */
static BOOL CALLBACK
enum_callback(LPCDIDEVICEINSTANCE lpddi, UNUSED(LPVOID data))
{
    INFO("JOYSTICK: found game device%i: %s\n",
	 joysticks_present, lpddi->tszProductName);

    /* If we got too many, ignore it. */
    if (joysticks_present >= MAX_JOYSTICKS) return(DIENUM_STOP);

    /* Add to the list of devices found. */
    joystick_guids[joysticks_present++] = lpddi->guidInstance;

    /* Continue scanning. */
    return(DIENUM_CONTINUE);
}


/* Callback for DirectInput. */
static BOOL CALLBACK
obj_callback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
    plat_joystick_t *state = (plat_joystick_t *)pvRef;
	
    if (lpddoi->guidType == GUID_XAxis || lpddoi->guidType == GUID_YAxis ||
	lpddoi->guidType == GUID_ZAxis || lpddoi->guidType == GUID_RxAxis ||
	lpddoi->guidType == GUID_RyAxis || lpddoi->guidType == GUID_RzAxis ||
	lpddoi->guidType == GUID_Slider) {
	strncpy(state->axis[state->nr_axes].name,
		lpddoi->tszName, sizeof(state->axis[state->nr_axes].name));

	DEBUG("Axis%i: %s  %x %x\n",
		state->nr_axes, state->axis[state->nr_axes].name,
		lpddoi->dwOfs, lpddoi->dwType);

	if (lpddoi->guidType == GUID_XAxis)
		state->axis[state->nr_axes].id = 0;
	  else if (lpddoi->guidType == GUID_YAxis)
		state->axis[state->nr_axes].id = 1;
	  else if (lpddoi->guidType == GUID_ZAxis)
		state->axis[state->nr_axes].id = 2;
	  else if (lpddoi->guidType == GUID_RxAxis)
		state->axis[state->nr_axes].id = 3;
	  else if (lpddoi->guidType == GUID_RyAxis)
		state->axis[state->nr_axes].id = 4;
	  else if (lpddoi->guidType == GUID_RzAxis)
		state->axis[state->nr_axes].id = 5;

	state->nr_axes++;
    } else if (lpddoi->guidType == GUID_Button) {
	strncpy(state->button[state->nr_buttons].name,
		lpddoi->tszName, sizeof(state->button[state->nr_buttons].name));
	DEBUG("Button%i: %s  %x %x\n",
		state->nr_buttons, state->button[state->nr_buttons].name,
		lpddoi->dwOfs, lpddoi->dwType);

	state->nr_buttons++;
    } else if (lpddoi->guidType == GUID_POV) {
	strncpy(state->pov[state->nr_povs].name, lpddoi->tszName,
		sizeof(state->pov[state->nr_povs].name));
	DEBUG("POV%i: %s  %x %x\n",
		state->nr_povs, state->pov[state->nr_povs].name,
		lpddoi->dwOfs, lpddoi->dwType);

	state->nr_povs++;
    }	
	
    return(DIENUM_CONTINUE);
}


void
joystick_init(void)
{
    int c;

    /* Only initialize if the game port is enabled. */
    if (! game_enabled) return;

    INFO("JOYSTICK: initializing (type=%d)\n", joystick_type);

    atexit(joystick_close);
	
    joysticks_present = 0;
	
    if (FAILED(DirectInput8Create(hInstance,
	       DIRECTINPUT_VERSION, IID_IDirectInput8A,
	       (void **) &lpdi, NULL)))
	fatal("joystick_init : DirectInputCreate failed\n"); 

    if (FAILED(lpdi->EnumDevices(DIDEVTYPE_JOYSTICK,
	       enum_callback, NULL, DIEDFL_ATTACHEDONLY)))
	fatal("joystick_init : EnumDevices failed\n");

    INFO("JOYSTICK: %i game device(s) found.\n", joysticks_present);

    for (c = 0; c < joysticks_present; c++) {		
	LPDIRECTINPUTDEVICE8 lpdi_joystick_temp = NULL;
	DIPROPRANGE joy_axis_range;
	DIDEVICEINSTANCE device_instance;
	DIDEVCAPS devcaps;

	if (FAILED(lpdi->CreateDevice(joystick_guids[c],
				      &lpdi_joystick_temp, NULL)))
		fatal("joystick_init : CreateDevice failed\n");
	if (FAILED(lpdi_joystick_temp->QueryInterface(IID_IDirectInputDevice8,
		   (void **)&lpdi_joystick[c])))
		fatal("joystick_init : CreateDevice failed\n");
	lpdi_joystick_temp->Release();

	memset(&device_instance, 0, sizeof(device_instance));
	device_instance.dwSize = sizeof(device_instance);
	if (FAILED(lpdi_joystick[c]->GetDeviceInfo(&device_instance)))
		fatal("joystick_init : GetDeviceInfo failed\n");

	DEBUG("Joystick%i:\n", c);
	DEBUG(" Name        = %s\n", device_instance.tszInstanceName);
	DEBUG(" ProductName = %s\n", device_instance.tszProductName);
	strncpy(plat_joystick_state[c].name,
		device_instance.tszInstanceName, 64);

	memset(&devcaps, 0, sizeof(devcaps));
	devcaps.dwSize = sizeof(devcaps);
	if (FAILED(lpdi_joystick[c]->GetCapabilities(&devcaps)))
		fatal("joystick_init : GetCapabilities failed\n");
	DEBUG(" Axes        = %i\n", devcaps.dwAxes);
	DEBUG(" Buttons     = %i\n", devcaps.dwButtons);
	DEBUG(" POVs        = %i\n", devcaps.dwPOVs);

	lpdi_joystick[c]->EnumObjects(obj_callback,
				      &plat_joystick_state[c], DIDFT_ALL); 
	if (FAILED(lpdi_joystick[c]->SetCooperativeLevel(hwndMain,
				DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
		fatal("joystick_init : SetCooperativeLevel failed\n");
	if (FAILED(lpdi_joystick[c]->SetDataFormat(&c_dfDIJoystick)))
		fatal("joystick_init : SetDataFormat failed\n");

	joy_axis_range.lMin = -32768;
	joy_axis_range.lMax =  32767;
	joy_axis_range.diph.dwSize = sizeof(DIPROPRANGE);
	joy_axis_range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	joy_axis_range.diph.dwHow = DIPH_BYOFFSET;
	joy_axis_range.diph.dwObj = DIJOFS_X;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
	joy_axis_range.diph.dwObj = DIJOFS_Y;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
	joy_axis_range.diph.dwObj = DIJOFS_Z;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
	joy_axis_range.diph.dwObj = DIJOFS_RX;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
	joy_axis_range.diph.dwObj = DIJOFS_RY;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
	joy_axis_range.diph.dwObj = DIJOFS_RZ;
	lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);

	if (FAILED(lpdi_joystick[c]->Acquire()))
		fatal("joystick_init : Acquire failed\n");
    }
}


void
joystick_close(void)
{
    if (lpdi_joystick[1]) {
	lpdi_joystick[1]->Release();
	lpdi_joystick[1] = NULL;
    }

    if (lpdi_joystick[0]) {
	lpdi_joystick[0]->Release();
	lpdi_joystick[0] = NULL;
    }
}


static int
get_axis(int joystick_nr, int mapping)
{
    if (mapping & POV_X) {
	int pov = plat_joystick_state[joystick_nr].p[mapping & 3];

	if (LOWORD(pov) == 0xFFFF)
		return 0;
	else
		return (int)sin((2*M_PI * (double)pov) / 36000.0) * 32767;
    } else if (mapping & POV_Y) {
	int pov = plat_joystick_state[joystick_nr].p[mapping & 3];

	if (LOWORD(pov) == 0xFFFF)
		return 0;
	else
		return (int)-cos((2*M_PI * (double)pov) / 36000.0) * 32767;
    } else
	return plat_joystick_state[joystick_nr].a[plat_joystick_state[joystick_nr].axis[mapping].id];
}


void
joystick_process(void)
{
    int c, d;

    if (joystick_type == JOYSTICK_NONE) return;

    for (c = 0; c < joysticks_present; c++) {		
	DIJOYSTATE joystate;
	int b;

	if (FAILED(lpdi_joystick[c]->Poll())) {
		lpdi_joystick[c]->Acquire();
		lpdi_joystick[c]->Poll();
	} if (FAILED(lpdi_joystick[c]->GetDeviceState(sizeof(DIJOYSTATE), (LPVOID)&joystate))) {
		lpdi_joystick[c]->Acquire();
		lpdi_joystick[c]->Poll();
		lpdi_joystick[c]->GetDeviceState(sizeof(DIJOYSTATE), (LPVOID)&joystate);
	}

	plat_joystick_state[c].a[0] = joystate.lX;
	plat_joystick_state[c].a[1] = joystate.lY;
	plat_joystick_state[c].a[2] = joystate.lZ;
	plat_joystick_state[c].a[3] = joystate.lRx;
	plat_joystick_state[c].a[4] = joystate.lRy;
	plat_joystick_state[c].a[5] = joystate.lRz;

	for (b = 0; b < 16; b++)
		plat_joystick_state[c].b[b] = joystate.rgbButtons[b] & 0x80;

	for (b = 0; b < 4; b++)
		plat_joystick_state[c].p[b] = joystate.rgdwPOV[b];
    }

    for (c = 0; c < gamedev_get_max_joysticks(joystick_type); c++) {
	if (joystick_state[c].plat_joystick_nr) {
		int joystick_nr = joystick_state[c].plat_joystick_nr - 1;

		for (d = 0; d < gamedev_get_axis_count(joystick_type); d++)
			joystick_state[c].axis[d] = get_axis(joystick_nr, joystick_state[c].axis_mapping[d]);
		for (d = 0; d < gamedev_get_button_count(joystick_type); d++)
			joystick_state[c].button[d] = plat_joystick_state[joystick_nr].b[joystick_state[c].button_mapping[d]];

		for (d = 0; d < gamedev_get_pov_count(joystick_type); d++) {
			int x, y;
			double angle, magnitude;

			x = get_axis(joystick_nr, joystick_state[c].pov_mapping[d][0]);
			y = get_axis(joystick_nr, joystick_state[c].pov_mapping[d][1]);

			angle = (atan2((double)y, (double)x) * 360.0) / (2*M_PI);
			magnitude = sqrt((double)x*(double)x + (double)y*(double)y);

			if (magnitude < 16384)
				joystick_state[c].pov[d] = -1;
			else
				joystick_state[c].pov[d] = ((int)angle + 90 + 360) % 360;
		}
	} else {
		for (d = 0; d < gamedev_get_axis_count(joystick_type); d++)
			joystick_state[c].axis[d] = 0;
		for (d = 0; d < gamedev_get_button_count(joystick_type); d++)
			joystick_state[c].button[d] = 0;
		for (d = 0; d < gamedev_get_pov_count(joystick_type); d++)
			joystick_state[c].pov[d] = -1;
	}
    }
}


#define AXIS_STRINGS_MAX 3
static int joystick_nr;
static int joystick_config_type;
static const char *axis_strings[AXIS_STRINGS_MAX] = {
    "X Axis", "Y Axis", "Z Axis"
};
static uint8_t joystickconfig_changed = 0;


static void
rebuild_selections(HWND hdlg)
{
    int id = IDC_CONFIGURE_DEV + 2;
    int c, d, joystick;
    HWND h;

    h = GetDlgItem(hdlg, IDC_CONFIGURE_DEV);
    joystick = SendMessage(h, CB_GETCURSEL, 0, 0);

    for (c = 0; c < gamedev_get_axis_count(joystick_config_type); c++) {
	int sel = c;

	h = GetDlgItem(hdlg, id);
	SendMessage(h, CB_RESETCONTENT, 0, 0);

	if (joystick) {
		for (d = 0; d < plat_joystick_state[joystick-1].nr_axes; d++) {
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)plat_joystick_state[joystick-1].axis[d].name);
			if (c < AXIS_STRINGS_MAX) {
				if (!stricmp(axis_strings[c], plat_joystick_state[joystick-1].axis[d].name))
					sel = d;
			}
		}
		for (d = 0; d < plat_joystick_state[joystick-1].nr_povs; d++) {
			char s[80];

			sprintf(s, "%s (X axis)", plat_joystick_state[joystick-1].pov[d].name);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
			sprintf(s, "%s (Y axis)", plat_joystick_state[joystick-1].pov[d].name);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
		}
		SendMessage(h, CB_SETCURSEL, sel, 0);
		EnableWindow(h, TRUE);
	} else
		EnableWindow(h, FALSE);
					
	id += 2;
    }

    for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
	h = GetDlgItem(hdlg, id);
	SendMessage(h, CB_RESETCONTENT, 0, 0);

	if (joystick) {
		for (d = 0; d < plat_joystick_state[joystick-1].nr_buttons; d++)
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)plat_joystick_state[joystick-1].button[d].name);
		SendMessage(h, CB_SETCURSEL, c, 0);
		EnableWindow(h, TRUE);
	} else
		EnableWindow(h, FALSE);

	id += 2;
    }

    for (c = 0; c < gamedev_get_pov_count(joystick_config_type)*2; c++) {
	int sel = c;

	h = GetDlgItem(hdlg, id);
	SendMessage(h, CB_RESETCONTENT, 0, 0);

	if (joystick) {
		for (d = 0; d < plat_joystick_state[joystick-1].nr_povs; d++) {
			char s[80];

			sprintf(s, "%s (X axis)", plat_joystick_state[joystick-1].pov[d].name);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
			sprintf(s, "%s (Y axis)", plat_joystick_state[joystick-1].pov[d].name);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
		}
		for (d = 0; d < plat_joystick_state[joystick-1].nr_axes; d++) {
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)plat_joystick_state[joystick-1].axis[d].name);
		}
		SendMessage(h, CB_SETCURSEL, sel, 0);
		EnableWindow(h, TRUE);
	} else
		EnableWindow(h, FALSE);
					
	id += 2;
    }

}


static int
get_axis(HWND hdlg, int id)
{
    HWND h = GetDlgItem(hdlg, id);
    int axis_sel = SendMessage(h, CB_GETCURSEL, 0, 0);
    int nr_axes = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr-1].nr_axes;

    if (axis_sel < nr_axes)
	return axis_sel;
	
    axis_sel -= nr_axes;
    if (axis_sel & 1)
	return POV_Y | (axis_sel >> 1);
      else
	return POV_X | (axis_sel >> 1);
}


static int
get_pov(HWND hdlg, int id)
{
    HWND h = GetDlgItem(hdlg, id);
    int axis_sel = SendMessage(h, CB_GETCURSEL, 0, 0);
    int nr_povs = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr-1].nr_povs*2;

    if (axis_sel < nr_povs) {
	if (axis_sel & 1)
		return POV_Y | (axis_sel >> 1);
	else
		return POV_X | (axis_sel >> 1);
    }

    return axis_sel - nr_povs;
}


static void
dlg_init(HWND hdlg)
{
    HWND h;
    int joystick;
    int c, id;
    int mapping;
    int nr_axes;
    int nr_povs;

    h = GetDlgItem(hdlg, IDC_CONFIGURE_DEV);
    id = IDC_CONFIGURE_DEV + 2;
    joystick = joystick_state[joystick_nr].plat_joystick_nr;

    SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"None");

    for (c = 0; c < joysticks_present; c++)
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)plat_joystick_state[c].name);

    SendMessage(h, CB_SETCURSEL, joystick, 0);

    rebuild_selections(hdlg);

    if (joystick_state[joystick_nr].plat_joystick_nr) {
	nr_axes = plat_joystick_state[joystick-1].nr_axes;
	nr_povs = plat_joystick_state[joystick-1].nr_povs;
	for (c = 0; c < gamedev_get_axis_count(joystick_config_type); c++) {
		mapping = joystick_state[joystick_nr].axis_mapping[c];

		h = GetDlgItem(hdlg, id);
		if (mapping & POV_X)
			SendMessage(h, CB_SETCURSEL, nr_axes + (mapping & 3)*2, 0);
		else if (mapping & POV_Y)
			SendMessage(h, CB_SETCURSEL, nr_axes + (mapping & 3)*2 + 1, 0);
		else
			SendMessage(h, CB_SETCURSEL, mapping, 0);
		id += 2;
	} 

	for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
		h = GetDlgItem(hdlg, id);
		SendMessage(h, CB_SETCURSEL, joystick_state[joystick_nr].button_mapping[c], 0);
		id += 2;
	}

	for (c = 0; c < gamedev_get_pov_count(joystick_config_type); c++) {
		h = GetDlgItem(hdlg, id);
		mapping = joystick_state[joystick_nr].pov_mapping[c][0];
		if (mapping & POV_X)
			SendMessage(h, CB_SETCURSEL, (mapping & 3)*2, 0);
		else if (mapping & POV_Y)
			SendMessage(h, CB_SETCURSEL, (mapping & 3)*2 + 1, 0);
		else
			SendMessage(h, CB_SETCURSEL, mapping + nr_povs*2, 0);
		id += 2;

		h = GetDlgItem(hdlg, id);
		mapping = joystick_state[joystick_nr].pov_mapping[c][1];
		if (mapping & POV_X)
			SendMessage(h, CB_SETCURSEL, (mapping & 3)*2, 0);
		else if (mapping & POV_Y)
			SendMessage(h, CB_SETCURSEL, (mapping & 3)*2 + 1, 0);
		else
			SendMessage(h, CB_SETCURSEL, mapping + nr_povs*2, 0);
		id += 2;
	}
    }
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, UNUSED(LPARAM lParam))
{
    HWND h;
    int c, id;

    switch (message) {
	case WM_INITDIALOG:
		dialog_center(hdlg);
		dlg_init(hdlg);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_CONFIGURE_DEV:
				if (HIWORD(wParam) == CBN_SELCHANGE)
					rebuild_selections(hdlg);
				break;
			
			case IDOK:
				id = IDC_CONFIGURE_DEV + 2;
								
				h = GetDlgItem(hdlg, IDC_CONFIGURE_DEV);
				joystick_state[joystick_nr].plat_joystick_nr = SendMessage(h, CB_GETCURSEL, 0, 0);
				
				if (joystick_state[joystick_nr].plat_joystick_nr) {
					for (c = 0; c < gamedev_get_axis_count(joystick_config_type); c++) {
						joystick_state[joystick_nr].axis_mapping[c] = get_axis(hdlg, id);
						id += 2;
					}			       
					for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].button_mapping[c] = SendMessage(h, CB_GETCURSEL, 0, 0);
						id += 2;
					}
					for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].pov_mapping[c][0] = get_pov(hdlg, id);
						id += 2;
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].pov_mapping[c][1] = get_pov(hdlg, id);
						id += 2;
					}
				}
				joystickconfig_changed = 1;
				EndDialog(hdlg, 0);
				return TRUE;

			case IDCANCEL:
				joystickconfig_changed = 0;
				EndDialog(hdlg, 0);
				return TRUE;
		}
		break;
    }

    return FALSE;
}


static uint16_t *
AddWideString(uint16_t *data, int ids)
{
    const wchar_t *str;
    wchar_t *ptr;

    ptr = (wchar_t *)data;
    str = get_string(ids);
    if (str != NULL) {
	while (*str != L'\0')
		*ptr++ = *str++;
    }
    *ptr++ = L'\0';

    return((uint16_t *)ptr);
}


/* Create an in-memory dialog template for the Joystick Configuration. */
uint8_t
dlg_jsconf(HWND hwnd, int joy_nr, int type)
{
    uint16_t *data_block = (uint16_t *)mem_alloc(16384);
    uint16_t *data;
    DLGTEMPLATE *dlg = (DLGTEMPLATE *)data_block;
    DLGITEMTEMPLATE *item;
    int c, y = 10;
    int id = IDC_CONFIGURE_DEV;

    joystickconfig_changed = 0;
    joystick_nr = joy_nr;
    joystick_config_type = type;

    memset(data_block, 0x00, 16384);

    dlg->style = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | \
		 WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg->x  = 10;
    dlg->y  = 10;
    dlg->cx = 220;
    dlg->cy = 70;

    data = (uint16_t *)(dlg + 1);
    *data++ = 0; /*no menu*/
    *data++ = 0; /*predefined dialog box class*/
    data = AddWideString(data, IDS_DEVCONF_1);

    *data++ = 8; /*Point*/
    data += MultiByteToWideChar(CP_ACP, 0, STR_FONTNAME, -1,
				(wchar_t *)data, 128);
    if (((uintptr_t)data) & 2)
	data++;

    /*Combo box*/
    item = (DLGITEMTEMPLATE *)data;
    item->style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
    item->x = 70;
    item->y = y;
    item->id = id++;
    item->cx = 140;
    item->cy = 150;
    data = (uint16_t *)(item + 1);
    *data++ = 0xFFFF;
    *data++ = 0x0085;    /* combo box class */
    data += MultiByteToWideChar(CP_ACP, 0, "Device", -1, (wchar_t *)data, 256);
    *data++ = 0;	      /* no creation data */
    if (((uintptr_t)data) & 2)
	data++;

    /*Static text*/
    item = (DLGITEMTEMPLATE *)data;
    item->style = WS_CHILD | WS_VISIBLE;
    item->id = id++;
    item->x = 10;
    item->y = y;
    item->cx = 60;
    item->cy = 15;
    data = (uint16_t *)(item + 1);
    *data++ = 0xFFFF;
    *data++ = 0x0082;    /* static class */
    data = AddWideString(data, IDS_DEVCONF_2);
    *data++ = 0;	      /* no creation data */
    if (((uintptr_t)data) & 2)
	data++;

    y += 20;

    for (c = 0; c < gamedev_get_axis_count(type); c++) {
	/*Combo box*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
	item->id = id++;
	item->x = 70;
	item->y = y;
	item->cx = 140;
	item->cy = 150;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0085;    /* combo box class */
	data += MultiByteToWideChar(CP_ACP, 0,
			    gamedev_get_axis_name(type, c), -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	/*Static text*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE;
	item->id = id++;
	item->x = 10;
	item->y = y;
	item->cx = 60;
	item->cy = 15;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0082;    /* static class */
	data += MultiByteToWideChar(CP_ACP, 0,
		gamedev_get_axis_name(type, c), -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	y += 20;
    }		

    for (c = 0; c < gamedev_get_button_count(type); c++) {
	/*Combo box*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
	item->id = id++;
	item->x = 70;
	item->y = y;
	item->cx = 140;
	item->cy = 150;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0085;    /* combo box class */
	data += MultiByteToWideChar(CP_ACP, 0,
		gamedev_get_button_name(type, c), -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	/*Static text*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE;
	item->id = id++;
	item->x = 10;
	item->y = y;
	item->cx = 60;
	item->cy = 15;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0082;    /* static class */
	data += MultiByteToWideChar(CP_ACP, 0,
		gamedev_get_button_name(type, c), -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	y += 20;
    }
 
    for (c = 0; c < gamedev_get_pov_count(type)*2; c++) {
	char s[80];

	/*Combo box*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
	item->id = id++;
	item->x = 70;
	item->y = y;
	item->cx = 140;
	item->cy = 150;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0085;    /* combo box class */
	if (c & 1)
		sprintf(s, "%s (Y axis)", gamedev_get_pov_name(type, c/2));
	else
		sprintf(s, "%s (X axis)", gamedev_get_pov_name(type, c/2));
	data += MultiByteToWideChar(CP_ACP, 0, s, -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	/*Static text*/
	item = (DLGITEMTEMPLATE *)data;
	item->style = WS_CHILD | WS_VISIBLE;
	item->id = id++;
	item->x = 10;
	item->y = y;
	item->cx = 60;
	item->cy = 15;
	data = (uint16_t *)(item + 1);
	*data++ = 0xFFFF;
	*data++ = 0x0082;    /* static class */
	data += MultiByteToWideChar(CP_ACP, 0, s, -1, (wchar_t *)data, 256);
	*data++ = 0;	      /* no creation data */
	if (((uintptr_t)data) & 2)
		data++;

	y += 20;
    }

    dlg->cdit = (id - IDC_CONFIGURE_DEV) + 2;

    item = (DLGITEMTEMPLATE *)data;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    item->id = IDOK;
    item->x = 20;
    item->y = y;
    item->cx = 50;
    item->cy = 14;
    data = (uint16_t *)(item + 1);
    *data++ = 0xFFFF;
    *data++ = 0x0080;    /* button class */
    data = AddWideString(data, IDS_OK);
    *data++ = 0;	      /* no creation data */
    if (((uintptr_t)data) & 2)
	data++;
		
    item = (DLGITEMTEMPLATE *)data;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    item->id = IDCANCEL;
    item->x = 80;
    item->y = y;
    item->cx = 50;
    item->cy = 14;
    data = (uint16_t *)(item + 1);
    *data++ = 0xFFFF;
    *data++ = 0x0080;    /* button class */
    data = AddWideString(data, IDS_CANCEL);
    *data++ = 0;	      /* no creation data */

    dlg->cy = y + 20;

    DialogBoxIndirect(hInstance, dlg, hwnd, dlg_proc);

    free(data_block);

    return joystickconfig_changed;
}
