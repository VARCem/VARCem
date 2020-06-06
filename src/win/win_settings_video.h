/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Settings dialog.
 *
 * Version:	@(#)win_settings_video.h	1.0.12	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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


/************************************************************************
 *									*
 *			      Video Dialog				*
 *									*
 ************************************************************************/

static int	list_to_vid[100],
		vid_to_list[100];


static WIN_RESULT CALLBACK
video_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char tempA[128];
    WCHAR temp[128];
    const char *stransi;
    const device_t *dev;
    const machine_t *m;
    int c, d;
    int vid;
    HWND h;

    /* Get info about the selected machine. */
    dev = machine_get_device_ex(temp_cfg.machine_type);
    m = (machine_t *)dev->mach_info;
    
    switch (message) {
	case WM_INITDIALOG:
		/* Clear the video cards combo. */
		h = GetDlgItem(hdlg, IDC_CONFIGURE_VIDEO);
		EnableWindow(h, FALSE);
		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
		SendMessage(h, CB_RESETCONTENT, 0, 0);
		SendMessage(h, CB_SETCURSEL, 0, 0);

		/* Populate the video cards combo. */
		c = d = 0;
		for (;;) {
			stransi = video_get_internal_name(c);
			if (stransi == NULL) break;

			/* Skip "internal" if machine doesn't have it. */
			if (c == VID_INTERNAL && !(m->flags & MACHINE_VIDEO)) {
				c++;
				continue;
			}

			dev = video_card_getdevice(c);

			if (c == 0) {
				/* Translate "None". */
				wcscpy(temp, get_string(IDS_NONE));
			} else if (c == 1) {
				/* Translate "Internal". */
				wcscpy(temp, get_string(IDS_INTERNAL));
			} else if (video_card_available(c) &&
				   device_is_valid(dev, m->flags)) {
				sprintf(tempA, "[%s] %s",
					device_get_bus_name(dev),
					video_card_getname(c));
				mbstowcs(temp, tempA, sizeof_w(temp));
			} else {
				c++;
				continue;
			}

			/* Add entry to combo. */
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);

			vid_to_list[c] = d;
			list_to_vid[d++] = c++;
		}
		INFO("A total of %i video cards are available.\n", d);

		/* Select the current card. */
		vid = vid_to_list[temp_cfg.video_card];
		if (vid < d) {
			SendMessage(h, CB_SETCURSEL, vid, 0);
			if (video_card_has_config(temp_cfg.video_card)) {
				h = GetDlgItem(hdlg, IDC_CONFIGURE_VIDEO);
				EnableWindow(h, TRUE);
			}
		}

		/* Machine with fixed video card have this disabled. */
		EnableWindow(h, (m->flags_fixed&MACHINE_VIDEO) ? FALSE : TRUE);

		/* Disable Voodoo on non-PCI machine? */
		h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
		EnableWindow(h, (m->flags & MACHINE_PCI) ? TRUE:FALSE);
		SendMessage(h, BM_SETCHECK, temp_cfg.voodoo_enabled, 0);
		h = GetDlgItem(hdlg, IDC_CONFIGURE_VOODOO);
		EnableWindow(h, ((m->flags & MACHINE_PCI) && temp_cfg.voodoo_enabled) ? TRUE : FALSE);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_VIDEO:
				h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
				c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_cfg.video_card = list_to_vid[c];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_VIDEO);
				if (video_card_has_config(temp_cfg.video_card))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CONFIGURE_VIDEO:
				h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
				temp_cfg.video_card = list_to_vid[SendMessage(h, CB_GETCURSEL, 0, 0)];
				temp_deviceconfig |= dlg_devconf(hdlg, video_card_getdevice(temp_cfg.video_card));

				break;

			case IDC_CHECK_VOODOO:
				h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
				temp_cfg.voodoo_enabled = (int)SendMessage(h, BM_GETCHECK, 0, 0);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_VOODOO);
				EnableWindow(h, temp_cfg.voodoo_enabled ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_VOODOO:
				temp_deviceconfig |= dlg_devconf(hdlg, &voodoo_device);
				break;
		}
		return FALSE;

	case WM_SAVE_CFG:
		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
		c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
		temp_cfg.video_card = list_to_vid[c];

		h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
		temp_cfg.voodoo_enabled = (int)SendMessage(h, BM_GETCHECK, 0, 0);
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
