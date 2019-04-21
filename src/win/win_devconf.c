/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Imlementation of the Device Configuration dialog.
 *
 *		This module takes a standard 'device_config_t' structure,
 *		and builds a complete Win32 DIALOG resource block in a
 *		buffer in memory, and then passes that to the API handler.
 *
 * Version:	@(#)win_devconf.c	1.0.23	2019/04/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"
#include "resource.h"


#define STR_FONTNAME	"Segoe UI"


static const device_t	*devconf_device;
static int8_t		devconf_changed = 0;


static void
dlg_init(HWND hdlg)
{
    wchar_t temp[512];
    char ansitmp[512];
    const device_config_t *cfg;
    const device_config_selection_t *sel;
    const device_t *dev = devconf_device;
    int c, id, num, val;
    wchar_t* str;
    HWND h;

    id = IDC_CONFIGURE_DEV;
    cfg = dev->config;

    while (cfg->name != NULL) {
	sel = cfg->selection;
	h = GetDlgItem(hdlg, id);

	switch (cfg->type) {
		case CONFIG_BINARY:
			val = config_get_int(dev->name,
					     cfg->name, cfg->default_int);
			SendMessage(h, BM_SETCHECK, val, 0);
			id++;
			break;

		case CONFIG_SELECTION:
			val = config_get_int(dev->name,
					     cfg->name, cfg->default_int);
			c = 0;
			while (sel->description) {
				mbstowcs(temp, sel->description, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)temp);
				if (val == sel->value)
					SendMessage(h, CB_SETCURSEL, c, 0);
				sel++;
				c++;
			}
			id += 2;
			break;

		case CONFIG_MIDI:
			val = config_get_int(dev->name,
					     cfg->name, cfg->default_int);
			num  = plat_midi_get_num_devs();
			for (c = 0; c < num; c++) {
				plat_midi_get_dev_name(c, ansitmp);
				mbstowcs(temp, ansitmp, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)temp);
				if (val == c)
					SendMessage(h, CB_SETCURSEL, c, 0);
			}
			id += 2;
			break;

		case CONFIG_SPINNER:
			val = config_get_int(dev->name,
					     cfg->name, cfg->default_int);
			swprintf(temp, sizeof_w(temp), L"%i", val);
			SendMessage(h, WM_SETTEXT, 0, (LPARAM)temp);
			id += 2;
			break;

		case CONFIG_FNAME:
			str = config_get_wstring(dev->name, cfg->name, NULL);
			if (str != NULL)
				SendMessage(h, WM_SETTEXT, 0, (LPARAM)str);
			id += 3;
			break;

		case CONFIG_HEX16:
			val = config_get_hex16(dev->name,
					       cfg->name, cfg->default_int);
			c = 0;
			while (sel->description) {
				mbstowcs(temp, sel->description, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)temp);
				if (val == sel->value)
					SendMessage(h, CB_SETCURSEL, c, 0);
				sel++;
				c++;
			}
			id += 2;
			break;

		case CONFIG_HEX20:
			val = config_get_hex20(dev->name,
					       cfg->name, cfg->default_int);
			c = 0;
			while (sel->description) {
				mbstowcs(temp, sel->description, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)temp);
				if (val == sel->value)
					SendMessage(h, CB_SETCURSEL, c, 0);
				sel++;
				c++;
			}
			id += 2;
			break;
	}
	cfg++;
    }
}


static WIN_RESULT CALLBACK
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    wchar_t ws[512], temp[512];
    char s[512], *ansistr;
    const device_config_selection_t *sel;
    const device_t *dev = devconf_device;
    const device_config_t *cfg = dev->config;
    int c, cid, changed, d, id, val;
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		dialog_center(hdlg);
		dlg_init(hdlg);
		return TRUE;

	case WM_COMMAND:
		cid = LOWORD(wParam);
		if (cid == IDOK) {
			id = IDC_CONFIGURE_DEV;
			changed = 0;

			while (cfg->name != NULL) {
				sel = cfg->selection;
				h = GetDlgItem(hdlg, id);

				switch (cfg->type) {
					case CONFIG_BINARY:
						val = config_get_int(dev->name, cfg->name, cfg->default_int);

						if (val != (int)SendMessage(h, BM_GETCHECK, 0, 0))
							changed = 1;
						id++;
						break;

					case CONFIG_SELECTION:
						val = config_get_int(dev->name, cfg->name, cfg->default_int);

						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

						for (; c > 0; c--)
							sel++;

						if (val != sel->value)
							changed = 1;
						id += 2;
						break;

					case CONFIG_MIDI:
						val = config_get_int(dev->name, cfg->name, cfg->default_int);

						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

						if (val != c)
							changed = 1;
						id += 2;
						break;

					case CONFIG_FNAME:
						ansistr = config_get_string(dev->name, cfg->name, "");
						SendMessage(h, WM_GETTEXT, sizeof(s), (LPARAM)s);
						if (strcmp(ansistr, s))
							changed = 1;
						id += 3;
						break;

					case CONFIG_SPINNER:
						val = config_get_int(dev->name, cfg->name, cfg->default_int);
						if (val > cfg->spinner.max)
							val = cfg->spinner.max;
						else if (val < cfg->spinner.min)
							val = cfg->spinner.min;

						SendMessage(h, WM_GETTEXT, sizeof_w(temp), (LPARAM)temp);
						wcstombs(s, temp, 79); /*tic*/
						sscanf(s, "%i", &c);

						if (val != c)
							changed = 1;
						id += 2;
						break;

					case CONFIG_HEX16:
						val = config_get_hex16(dev->name, cfg->name, cfg->default_int);

						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

						for (; c > 0; c--)
							sel++;

						if (val != sel->value)
							changed = 1;
						id += 2;
						break;

					case CONFIG_HEX20:
						val = config_get_hex20(dev->name, cfg->name, cfg->default_int);

						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

						for (; c > 0; c--)
							sel++;

						if (val != sel->value)
							changed = 1;
						id += 2;
						break;
				}
				cfg++;
			}

			if (! changed) {
				devconf_changed = 0;
				EndDialog(hdlg, 0);
				return TRUE;
			}

			devconf_changed = 1;

			id = IDC_CONFIGURE_DEV;
			cfg = dev->config;
				
			while (cfg->name != NULL) {
				sel = cfg->selection;
				h = GetDlgItem(hdlg, id);

				switch (cfg->type) {
					case CONFIG_BINARY:
						config_set_int(dev->name, cfg->name, (int)SendMessage(h, BM_GETCHECK, 0, 0));
						
						id++;
						break;

					case CONFIG_SELECTION:
						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
						for (; c > 0; c--)
							sel++;
						config_set_int(dev->name, cfg->name, sel->value);

						id += 2;
						break;

					case CONFIG_MIDI:
						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
						config_set_int(dev->name, cfg->name, c);

						id += 2;
						break;

					case CONFIG_FNAME:
						SendMessage(h, WM_GETTEXT, 511, (LPARAM)ws);
						config_set_wstring(dev->name, cfg->name, ws);

						id += 3;
						break;

					case CONFIG_SPINNER:
                                                SendMessage(h, WM_GETTEXT, 79, (LPARAM)ws);
						wcstombs(s, ws, 79);
						sscanf(s, "%i", &c);
						if (c > cfg->spinner.max)
							c = cfg->spinner.max;
						else if (c < cfg->spinner.min)
							c = cfg->spinner.min;

						config_set_int(dev->name, cfg->name, c);

						id += 2;
						break;

					case CONFIG_HEX16:
						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
						for (; c > 0; c--)
							sel++;
						config_set_hex16(dev->name, cfg->name, sel->value);

						id += 2;
						break;

					case CONFIG_HEX20:
						c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
						for (; c > 0; c--)
							sel++;
						config_set_hex20(dev->name, cfg->name, sel->value);

						id += 2;
						break;
				}
				cfg++;
			}

			EndDialog(hdlg, 0);
			return TRUE;
		} else if (cid == IDCANCEL) {
			devconf_changed = 0;
			EndDialog(hdlg, 0);
			return TRUE;
		} else {
			id = IDC_CONFIGURE_DEV;
			cfg = dev->config;

			while (cfg->name != NULL) {
				switch (cfg->type) {
					case CONFIG_BINARY:
						id++;
						break;

					case CONFIG_SELECTION:
					case CONFIG_MIDI:
					case CONFIG_SPINNER:
						id += 2;
						break;

					case CONFIG_FNAME:
						if (cid == id+1) {
							h = GetDlgItem(hdlg, id);
							SendMessage(h, WM_GETTEXT, 511, (LPARAM)s);
							char file_filter[512];
							file_filter[0] = 0;

							c = 0;
							while (cfg->file_filter[c].description) {
								if (c > 0)
									strcat(file_filter, "|");
								strcat(file_filter, cfg->file_filter[c].description);
								strcat(file_filter, " (");
								d = 0;
								while (cfg->file_filter[c].extensions[d]) {
										if (d > 0)
									strcat(file_filter, ";");
									strcat(file_filter, "*.");
									strcat(file_filter, cfg->file_filter[c].extensions[d]);
									d++;
								}
								strcat(file_filter, ")|");
								d = 0;
								while (cfg->file_filter[c].extensions[d]) {
									if (d > 0)
										strcat(file_filter, ";");
									strcat(file_filter, "*.");
									strcat(file_filter, cfg->file_filter[c].extensions[d]);
									d++;
								}
								c++;
							}
//FIXME: localize
							strcat(file_filter, "|All files (*.*)|*.*|");
							mbstowcs(ws, file_filter, strlen(file_filter) + 1);
							d = (int)strlen(file_filter);

							/* replace | with \0 */
							for (c = 0; c < d; ++c)
								if (ws[c] == L'|')
									ws[c] = 0;

							if (dlg_file_ex(hdlg, ws, NULL, temp, DLG_FILE_LOAD))
								SendMessage(h, WM_SETTEXT, 0, (LPARAM)temp);
						}
						break;
				}
				cfg++;
			}
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


/*
 * Build the full dialog from the template in
 * memory, and data taken from the device config.
 */
#define DLG_MAX_SIZE	16384
uint8_t
dlg_devconf(HWND hwnd, const device_t *device)
{
    char temp[128];
    const device_config_t *cfg = device->config;
    uint16_t *blk, *data;
    DLGITEMTEMPLATE *itm;
    DLGTEMPLATE *dlg;
    int id, y;

    devconf_changed = 0;

    /* Allocate the dialog data block. */
    blk = (uint16_t *)mem_alloc(DLG_MAX_SIZE);
    memset(blk, 0x00, DLG_MAX_SIZE);
    dlg = (DLGTEMPLATE *)blk;

    /* Set up the basic dialog info. */
    dlg->style = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | \
		 WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg->x  = 10;
    dlg->y  = 10;
    dlg->cx = 220;
    dlg->cy = 70;
    y = dlg->y;

    /* Dialog menu bar, title bar, class, etc. */
    data = (uint16_t *)(dlg + 1);
    *data++ = 0; /* no menu bar */
    *data++ = 0; /* predefined dialog box class */
    sprintf(temp, "%s ", device->name);
    data += MultiByteToWideChar(CP_ACP, 0, temp, -1, (LPWSTR)data, 120);
    data--;	/* back over the NUL */
    data = AddWideString(data, IDS_DEVCONF_1);

    /* Font style and size to use. */
    *data++ = 9; /* point size */
    data += MultiByteToWideChar(CP_ACP, 0, STR_FONTNAME, -1, (LPWSTR)data, 120);

    if (((uintptr_t)data) & 2)
	data++;

    /* Now add the items from the configuration. */
    id = IDC_CONFIGURE_DEV;
    while (cfg->name != NULL) {
	/* Align 'data' to DWORD */
	itm = (DLGITEMTEMPLATE *)data;

	switch (cfg->type) {
		case CONFIG_BINARY:
			itm->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
			itm->x = 10;
			itm->y = y;
			itm->cx = 80;
			itm->cy = 15;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0080;    /* button class */
			data += MultiByteToWideChar(CP_ACP, 0,
					cfg->description, -1, (LPWSTR)data, 256);
			*data++ = 0;	      /* no creation data */

			/* Move to next available line. */
			y += 20;
			break;

		case CONFIG_SELECTION:
		case CONFIG_MIDI:
		case CONFIG_HEX16:
		case CONFIG_HEX20:
			/* combo box */
			itm->style = WS_CHILD | WS_VISIBLE | \
				     CBS_DROPDOWNLIST | WS_VSCROLL;
			itm->x = 70;
			itm->y = y;
			itm->cx = 140;
			itm->cy = 150;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0085;	/* combo box class */
			data += MultiByteToWideChar(CP_ACP, 0,
					cfg->description, -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* static text */
			itm = (DLGITEMTEMPLATE *)data;
			itm->style = WS_CHILD | WS_VISIBLE;
			itm->x = 10;
			itm->y = y;
			itm->cx = 60;
			itm->cy = 15;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0082;	/* static class */
			data += MultiByteToWideChar(CP_ACP, 0,
					cfg->description, -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* Move to next available line. */
			y += 20;
			break;

		case CONFIG_SPINNER:
			/* spinner */
			itm->style = WS_CHILD | WS_VISIBLE | \
				     ES_AUTOHSCROLL | ES_NUMBER;
			itm->dwExtendedStyle = WS_EX_CLIENTEDGE;
			itm->x = 70;
			itm->y = y;
			itm->cx = 140;
			itm->cy = 14;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0081;	/* edit text class */
			data += MultiByteToWideChar(CP_ACP, 0,
						    "", -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* TODO: add up down class */
			/* static text */
			itm = (DLGITEMTEMPLATE *)data;
			itm->style = WS_CHILD | WS_VISIBLE;
			itm->x = 10;
			itm->y = y;
			itm->cx = 60;
			itm->cy = 15;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0082;	/* static class */
			data += MultiByteToWideChar(CP_ACP, 0,
					cfg->description, -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* Move to next available line. */
			y += 20;
			break;

		case CONFIG_FNAME:
			/* file */
			itm->style = WS_CHILD | WS_VISIBLE | ES_READONLY;
			itm->dwExtendedStyle = WS_EX_CLIENTEDGE;
			itm->x = 70;
			itm->y = y;
			itm->id = id++;
			itm->cx = 90;
			itm->cy = 14;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0081;	/* edit text class */
			data += MultiByteToWideChar(CP_ACP, 0,
						"", -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* Button */
			itm = (DLGITEMTEMPLATE *)data;
			itm->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
			itm->x = 165;
			itm->y = y;
			itm->cx = 45;
			itm->cy = 14;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0080;	/* button class */
			data = AddWideString(data, IDS_BROWSE);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* static text */
			itm = (DLGITEMTEMPLATE *)data;
			itm->style = WS_CHILD | WS_VISIBLE;
			itm->x = 10;
			itm->y = y;
			itm->cx = 60;
			itm->cy = 15;
			itm->id = id++;

			data = (uint16_t *)(itm + 1);
			*data++ = 0xffff;
			*data++ = 0x0082;	/* static class */
			data += MultiByteToWideChar(CP_ACP, 0,
					cfg->description, -1, (LPWSTR)data, 256);
			*data++ = 0;		/* no creation data */
			if (((uintptr_t)data) & 2)
				data++;		/* align */

			/* Move to next available line. */
			y += 20;
			break;
	}

	/* Align to next 4-byte boundary. */
	if (((uintptr_t)data) & 2)
		data++;

	/* Next item in configuration. */
	cfg++;
    }

    dlg->cdit = (id - IDC_CONFIGURE_DEV) + 2;

    itm = (DLGITEMTEMPLATE *)data;
    itm->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    itm->x = 20;
    itm->y = y;
    itm->cx = 50;
    itm->cy = 14;
    itm->id = IDOK;	/* OK button identifier */

    data = (uint16_t *)(itm + 1);
    *data++ = 0xffff;
    *data++ = 0x0080;	/* button class */
    data = AddWideString(data, IDS_OK);
    *data++ = 0;	/* no creation data */
    if (((uintptr_t)data) & 2)
	data++;		/* align */

    itm = (DLGITEMTEMPLATE *)data;
    itm->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    itm->x = 80;
    itm->y = y;
    itm->cx = 50;
    itm->cy = 14;
    itm->id = IDCANCEL;	/* CANCEL button identifier */
    data = (uint16_t *)(itm + 1);
    *data++ = 0xffff;
    *data++ = 0x0080;	/* button class */
    data = AddWideString(data, IDS_CANCEL);
    *data++ = 0;	      /* no creation data */

    /* Set final height of dialog. */
    dlg->cy = y + 20;

    devconf_device = device;

    DialogBoxIndirect(hInstance, dlg, hwnd, dlg_proc);

    free(blk);

    return(devconf_changed);
}
