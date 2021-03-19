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
 * Version:	@(#)win_joystick.c	1.0.23	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *              GH Cao, <driver1998.ms@outlook.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2019 GH Cao.
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
#include <windowsx.h>
#include <hidusage.h>
#include <hidsdi.h>
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


typedef struct {
    HANDLE	hdevice;
    PHIDP_PREPARSED_DATA data;

    USAGE	usage_button[128];

    struct raw_axis_t {
	USAGE	usage;
	USHORT	link;
	USHORT	bitsize;
	LONG	max;
	LONG	min;
    }		axis[8];

    struct raw_pov_t {
	USAGE  usage;
	USHORT link;
	LONG   max;
	LONG   min;
    }		pov[4];
} raw_joystick_t;


static plat_joystick_t	plat_joystick_state[MAX_PLAT_JOYSTICKS];
static raw_joystick_t	raw_joystick_state[MAX_PLAT_JOYSTICKS];


/* We only use the first 32 buttons reported, from Usage ID 1-128 */
static void
add_button(raw_joystick_t *raw, plat_joystick_t *joy, USAGE usage)
{
    if (joy->nr_buttons >= 32)
	return;

    if (usage < 1 || usage > 128) return;

    raw->usage_button[usage] = joy->nr_buttons;
    sprintf(joy->button[joy->nr_buttons].name, "Button %i", usage);
    joy->nr_buttons++;
}


static void
add_axis(raw_joystick_t *raw, plat_joystick_t *joy, PHIDP_VALUE_CAPS prop)
{
    if (joy->nr_axes >= 8)
	return;

    switch (prop->Range.UsageMin) {
	case HID_USAGE_GENERIC_X:
		sprintf(joy->axis[joy->nr_axes].name, "X");
		break;

	case HID_USAGE_GENERIC_Y:
		sprintf(joy->axis[joy->nr_axes].name, "Y");
		break;

	case HID_USAGE_GENERIC_Z:
		sprintf(joy->axis[joy->nr_axes].name, "Z");
		break;

	case HID_USAGE_GENERIC_RX:
		sprintf(joy->axis[joy->nr_axes].name, "RX");
		break;

	case HID_USAGE_GENERIC_RY:
		sprintf(joy->axis[joy->nr_axes].name, "RY");
		break;

	case HID_USAGE_GENERIC_RZ:
		sprintf(joy->axis[joy->nr_axes].name, "RZ");
		break;

	default:
		return;
    }

    joy->axis[joy->nr_axes].id = joy->nr_axes;
    raw->axis[joy->nr_axes].usage = prop->Range.UsageMin;
    raw->axis[joy->nr_axes].link = prop->LinkCollection;
    raw->axis[joy->nr_axes].bitsize = prop->BitSize;

    /* Assume unsigned when min >= 0. */
    if (prop->LogicalMin < 0) {
	raw->axis[joy->nr_axes].max = prop->LogicalMax;
    } else {
	/*
	 * Some joysticks will send -1 in LogicalMax, like Xbox Controllers
	 * so we need to mask that to appropriate value (instead of 0xFFFFFFFF) 
	 */
	raw->axis[joy->nr_axes].max = prop->LogicalMax & ((1 << prop->BitSize) - 1);
    }
    raw->axis[joy->nr_axes].min = prop->LogicalMin;

    joy->nr_axes++;
}


static void
add_pov(raw_joystick_t *raw, plat_joystick_t *joy, PHIDP_VALUE_CAPS prop)
{
    if (joy->nr_povs >= 4)
	return;

    sprintf(joy->pov[joy->nr_povs].name, "POV %i", joy->nr_povs+1);
    raw->pov[joy->nr_povs].usage = prop->Range.UsageMin;
    raw->pov[joy->nr_povs].link  = prop->LinkCollection;
    raw->pov[joy->nr_povs].min   = prop->LogicalMin;
    raw->pov[joy->nr_povs].max   = prop->LogicalMax;

    joy->nr_povs++;
}


static void
get_capabilities(raw_joystick_t *raw, plat_joystick_t *joy)
{
    PHIDP_BUTTON_CAPS btn_caps = NULL;
    PHIDP_VALUE_CAPS  val_caps = NULL;
    HIDP_CAPS caps;
    UINT size = 0;
    int b, c, count;

    /* Get preparsed data (HID data format) */
    GetRawInputDeviceInfoW(raw->hdevice, RIDI_PREPARSEDDATA, NULL, &size);
    raw->data = mem_alloc(size);
    if (GetRawInputDeviceInfoW(raw->hdevice, RIDI_PREPARSEDDATA, raw->data, &size) <= 0)
	fatal("joystick_get_capabilities: Failed to get preparsed data.\n");

    HidP_GetCaps(raw->data, &caps);

    /* Buttons. */
    if (caps.NumberInputButtonCaps > 0) {
	btn_caps = calloc(caps.NumberInputButtonCaps, sizeof(HIDP_BUTTON_CAPS));
	if (HidP_GetButtonCaps(HidP_Input, btn_caps, &caps.NumberInputButtonCaps, raw->data) != HIDP_STATUS_SUCCESS) {
		ERRLOG("joystick_get_capabilities: Failed to query input buttons.\n");
		goto end;
	}

	/* We only detect generic stuff. */
	for (c = 0; c < caps.NumberInputButtonCaps; c++) {
		if (btn_caps[c].UsagePage != HID_USAGE_PAGE_BUTTON)
			continue;

		count = btn_caps[c].Range.UsageMax - btn_caps[c].Range.UsageMin + 1;
		for (b = 0; b < count; b++)
			add_button(raw, joy, b + btn_caps[c].Range.UsageMin);
	}
    }

    /* Values (axes and povs) */
    if (caps.NumberInputValueCaps > 0) {
	val_caps = calloc(caps.NumberInputValueCaps, sizeof(HIDP_VALUE_CAPS));
	if (HidP_GetValueCaps(HidP_Input, val_caps, &caps.NumberInputValueCaps, raw->data) != HIDP_STATUS_SUCCESS) {
		ERRLOG("joystick_get_capabilities: Failed to query axes and povs.\n");
		goto end;
	}

 	/* We only detect generic stuff */
	for (c = 0; c < caps.NumberInputValueCaps; c++) {
		if (val_caps[c].UsagePage != HID_USAGE_PAGE_GENERIC)
			continue;

		if (val_caps[c].Range.UsageMin == HID_USAGE_GENERIC_HATSWITCH)
			add_pov(raw, joy, &val_caps[c]);
		else
			add_axis(raw, joy, &val_caps[c]);
	}
    }

end:
    free(btn_caps);
    free(val_caps);
}


static void
get_device_name(raw_joystick_t *raw, plat_joystick_t *joy, PRID_DEVICE_INFO info) {
    WCHAR dev_desc[200] = { 0 };
    char *dev_name = NULL;
    HANDLE hDevObj;
    UINT size = 0;
    int c;

    /* Hrrm. Why do we use ANSI here, and then convert to Unicode later? */
    GetRawInputDeviceInfoA(raw->hdevice, RIDI_DEVICENAME, dev_name, &size);
    dev_name = calloc(size, sizeof(char));
    if (GetRawInputDeviceInfoA(raw->hdevice, RIDI_DEVICENAME, dev_name, &size) <= 0)
	fatal("joystick_get_capabilities: Failed to get device name.\n");

    hDevObj = CreateFile(dev_name, GENERIC_READ | GENERIC_WRITE, 
				   FILE_SHARE_READ | FILE_SHARE_WRITE,
				   NULL, OPEN_EXISTING, 0, NULL);
    if (hDevObj) {
	HidD_GetProductString(hDevObj, dev_desc, sizeof_w(dev_desc));
	CloseHandle(hDevObj);
    }
    free(dev_name);

    c = WideCharToMultiByte(CP_ACP, 0, dev_desc, sizeof_w(dev_desc),
			    joy->name, sizeof(joy->name), NULL, NULL);
    if (c == 0 || strlen(joy->name) == 0) {
	/* That failed, so we do it manually. */
	sprintf(joy->name, 
		"RawInput %s, VID:%04lX PID:%04lX",  
		info->hid.usUsage == HID_USAGE_GENERIC_JOYSTICK ? "Joystick"
								: "Gamepad",
		info->hid.dwVendorId, info->hid.dwProductId);
    }
}


void
joystick_close(void)
{
    RAWINPUTDEVICE ridev[2];

    ridev[0].dwFlags     = RIDEV_REMOVE;
    ridev[0].hwndTarget  = NULL;
    ridev[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    ridev[0].usUsage     = HID_USAGE_GENERIC_JOYSTICK;

    ridev[1].dwFlags     = RIDEV_REMOVE;
    ridev[1].hwndTarget  = NULL;
    ridev[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    ridev[1].usUsage     = HID_USAGE_GENERIC_GAMEPAD;

    (void)RegisterRawInputDevices(ridev, 2, sizeof(RAWINPUTDEVICE));
}


void
joystick_init(void)
{
    RAWINPUTDEVICELIST *deviceList;
    RAWINPUTDEVICE ridev[2];
    RID_DEVICE_INFO *info;
    plat_joystick_t *joy;
    raw_joystick_t *raw;
    UINT raw_devices = 0;
    UINT size = 0;
    int i;

    /* Clear out any leftovers. */
    joysticks_present = 0;
    memset(raw_joystick_state, 0x00, sizeof(raw_joystick_state));

    /* Only initialize if the game port is enabled. */
    if (! config.game_enabled) return;

    INFO("JOYSTICK: initializing (type=%i)\n", config.joystick_type);

    atexit(joystick_close);

    /* Get a list of raw input devices from Windows. */
    GetRawInputDeviceList(NULL, &raw_devices, sizeof(RAWINPUTDEVICELIST));
    deviceList = calloc(raw_devices, sizeof(RAWINPUTDEVICELIST));
    GetRawInputDeviceList(deviceList, &raw_devices, sizeof(RAWINPUTDEVICELIST));

    for (i = 0; i < raw_devices; i++) {
	if (joysticks_present >= MAX_PLAT_JOYSTICKS)
		break;

	if (deviceList[i].dwType != RIM_TYPEHID)
		continue; 

	/* Get device info: hardware IDs and usage IDs. */
	GetRawInputDeviceInfoA(deviceList[i].hDevice, RIDI_DEVICEINFO, NULL, &size);
	info = (RID_DEVICE_INFO *)mem_alloc(size);
	info->cbSize = sizeof(RID_DEVICE_INFO);
	if (GetRawInputDeviceInfoA(deviceList[i].hDevice,
				   RIDI_DEVICEINFO, info, &size) <= 0)
		goto end_loop;

	/* If this is not a joystick/gamepad, skip. */
	if (info->hid.usUsagePage != HID_USAGE_PAGE_GENERIC) goto end_loop;
	if (info->hid.usUsage != HID_USAGE_GENERIC_JOYSTICK && 
		info->hid.usUsage != HID_USAGE_GENERIC_GAMEPAD) goto end_loop;

	joy = &plat_joystick_state[joysticks_present];
	raw = &raw_joystick_state[joysticks_present];
	raw->hdevice = deviceList[i].hDevice;

	get_capabilities(raw, joy);
	get_device_name(raw, joy, info);

	INFO("GAME: Joystick %i: %s - %i buttons, %i axes, %i POVs\n", 
		joysticks_present,
		joy->name, joy->nr_buttons, joy->nr_axes, joy->nr_povs);

	joysticks_present++;

end_loop:
	free(info);
    }
    INFO("JOYSTICK: %i game device(s) found.\n", joysticks_present);

    /* Initialize the RawInput (joystick and gamepad) module. */
    ridev[0].dwFlags     = 0;
    ridev[0].hwndTarget  = NULL;
    ridev[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    ridev[0].usUsage     = HID_USAGE_GENERIC_JOYSTICK;

    ridev[1].dwFlags     = 0;
    ridev[1].hwndTarget  = NULL;
    ridev[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    ridev[1].usUsage     = HID_USAGE_GENERIC_GAMEPAD;

    if (! RegisterRawInputDevices(ridev, 2, sizeof(RAWINPUTDEVICE)))
	fatal("plat_joystick_init: RegisterRawInputDevices failed\n"); 
}


void
joystick_handle(RAWINPUT *raw)
{
    USAGE usage_list[128] = { 0 };
    ULONG usage_length, uvalue, mask;
    LONG value, center;
    struct raw_axis_t *axis;
    struct raw_pov_t *pov;
    HRESULT r;
    int i, j, x;

    /* If the input is not from a known device, we ignore it */
    j = -1;
    for (i = 0; i < joysticks_present; i++) {
	if (raw_joystick_state[i].hdevice == raw->header.hDevice) {
		j = i;
		break;
	}
    }
    if (j == -1)
	return;

    /* Read buttons. */
    usage_length = plat_joystick_state[j].nr_buttons;
    memset(plat_joystick_state[j].b, 0x00, 32 * sizeof(int));

    r = HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON,
		       0, usage_list, &usage_length,
		       raw_joystick_state[j].data,
		       (PCHAR)raw->data.hid.bRawData, raw->data.hid.dwSizeHid);
    if (r == HIDP_STATUS_SUCCESS) {
	for (i = 0; i < usage_length; i++) {
		x = raw_joystick_state[j].usage_button[usage_list[i]];
		plat_joystick_state[j].b[x] = 128;
	}
    }

    /* Read axes. */
    for (x = 0; x < plat_joystick_state[j].nr_axes; x++) {
	axis = &raw_joystick_state[j].axis[x];
	uvalue = 0;
	value  = 0;
	center = (axis->max - axis->min + 1) / 2;

	r = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC,
			       axis->link, axis->usage, &uvalue, 
			       raw_joystick_state[j].data,
			       (PCHAR)raw->data.hid.bRawData,
			       raw->data.hid.dwSizeHid);
	if (r == HIDP_STATUS_SUCCESS) {
		if (axis->min < 0) {
			/* Extend signed uvalue to LONG. */
			if (uvalue & (1 << (axis->bitsize-1))) {
				mask = (1 << axis->bitsize) - 1;
				value = -1U ^ mask;
				value |= uvalue;
			} else
				value = uvalue;
		} else {
			/*
			 * Assume unsigned when min >= 0,
			 * convert to a signed value.
			 */
			value = (LONG)uvalue - center;
		}

		if (abs(value) == 1)
			value = 0;
		value = value * 32768 / center;
	}

	plat_joystick_state[j].a[x] = value;
    }

    /* Read POVs. */
    for (x = 0; x < plat_joystick_state[j].nr_povs; x++) {
	pov = &raw_joystick_state[j].pov[x];
	uvalue = 0;
	value = -1;

	r = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC,
			       pov->link, pov->usage, &uvalue, 
			       raw_joystick_state[j].data,
			       (PCHAR)raw->data.hid.bRawData,
			       raw->data.hid.dwSizeHid);
	if (r == HIDP_STATUS_SUCCESS && (uvalue >= pov->min && uvalue <= pov->max)) {
		value  = (uvalue - pov->min) * 36000;
		value /= (pov->max - pov->min + 1);
		value %= 36000;
	}

	plat_joystick_state[j].p[x] = value;
    }
}


static int
get_axis(int nr, int mapping)
{
    int pov;

    if (mapping & POV_X) {
	pov = plat_joystick_state[nr].p[mapping & 3];
	if (LOWORD(pov) == 0xFFFF)
		return 0;
	else 
		return sin((2*M_PI * (double)pov) / 36000.0) * 32767;
    } else if (mapping & POV_Y) {
	pov = plat_joystick_state[nr].p[mapping & 3];
		
	if (LOWORD(pov) == 0xFFFF)
		return 0;
	else
		return -cos((2*M_PI * (double)pov) / 36000.0) * 32767;
    }

    return plat_joystick_state[nr].a[plat_joystick_state[nr].axis[mapping].id];
}


void
joystick_process(void)
{
    double angle, magnitude;
    int c, d, x, y, type;

    if ((type = config.joystick_type) == JOYSTICK_NONE)
	return;

    for (c = 0; c < gamedev_get_max_joysticks(type); c++) {
	if (joystick_state[c].plat_joystick_nr) {
		int nr = joystick_state[c].plat_joystick_nr - 1;

		for (d = 0; d < gamedev_get_axis_count(type); d++)
			joystick_state[c].axis[d] = get_axis(nr, joystick_state[c].axis_mapping[d]);
		for (d = 0; d < gamedev_get_button_count(type); d++)
			joystick_state[c].button[d] = plat_joystick_state[nr].b[joystick_state[c].button_mapping[d]];

		for (d = 0; d < gamedev_get_pov_count(type); d++) {
			x = get_axis(nr, joystick_state[c].pov_mapping[d][0]);
			y = get_axis(nr, joystick_state[c].pov_mapping[d][1]);

			angle = (atan2((double)y, (double)x) * 360.0) / (2*M_PI);
			magnitude = sqrt((double)x*(double)x + (double)y*(double)y);

			if (magnitude < 16384)
				joystick_state[c].pov[d] = -1;
			else
				joystick_state[c].pov[d] = ((int)angle + 90 + 360) % 360;
		}
	} else {
		for (d = 0; d < gamedev_get_axis_count(type); d++)
			joystick_state[c].axis[d] = 0;
		for (d = 0; d < gamedev_get_button_count(type); d++)
			joystick_state[c].button[d] = 0;
		for (d = 0; d < gamedev_get_pov_count(type); d++)
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
    joystick = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

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
dlg_get_axis(HWND hdlg, int id)
{
    HWND h = GetDlgItem(hdlg, id);
    int axis_sel = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
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
dlg_get_pov(HWND hdlg, int id)
{
    HWND h = GetDlgItem(hdlg, id);
    int axis_sel = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
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


static WIN_RESULT CALLBACK
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
				joystick_state[joystick_nr].plat_joystick_nr = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				
				if (joystick_state[joystick_nr].plat_joystick_nr) {
					for (c = 0; c < gamedev_get_axis_count(joystick_config_type); c++) {
						joystick_state[joystick_nr].axis_mapping[c] = dlg_get_axis(hdlg, id);
						id += 2;
					}			       
					for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].button_mapping[c] = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
						id += 2;
					}
					for (c = 0; c < gamedev_get_button_count(joystick_config_type); c++) {
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].pov_mapping[c][0] = dlg_get_pov(hdlg, id);
						id += 2;
						h = GetDlgItem(hdlg, id);
						joystick_state[joystick_nr].pov_mapping[c][1] = dlg_get_pov(hdlg, id);
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
