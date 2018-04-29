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
 * Version:	@(#)win_settings_video.h	1.0.3	2018/04/29
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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

static void
recalc_vid_list(HWND hdlg)
{
    WCHAR temp[128];
    const char *stransi;
    HWND h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
    int c = 0, d = 0;
    int found_card = 0;

    SendMessage(h, CB_RESETCONTENT, 0, 0);
    SendMessage(h, CB_SETCURSEL, 0, 0);
	
    while (1) {
	/* Skip "internal" if machine doesn't have it. */
	if (c == 1 && !(machines[temp_machine].flags&MACHINE_VIDEO)) {
		c++;
		continue;
	}

	stransi = video_card_getname(c);
	if (! *stransi)
		break;

	if (video_card_available(c) && vid_present[video_new_to_old(c)] &&
	    device_is_valid(video_card_getdevice(c), machines[temp_machine].flags)) {
		mbstowcs(temp, stransi, sizeof_w(temp));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		if (video_new_to_old(c) == temp_video_card) {
			SendMessage(h, CB_SETCURSEL, d, 0);
			found_card = 1;
		}

		d++;
	}

	c++;
    }
    if (! found_card)
	SendMessage(h, CB_SETCURSEL, 0, 0);

    EnableWindow(h, machines[temp_machine].fixed_vidcard ? FALSE : TRUE);

    h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
    EnableWindow(h, (machines[temp_machine].flags & MACHINE_PCI) ? TRUE:FALSE);

    h = GetDlgItem(hdlg, IDC_BUTTON_VOODOO);
    EnableWindow(h, ((machines[temp_machine].flags & MACHINE_PCI) && temp_voodoo) ? TRUE : FALSE);
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
video_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    char tempA[128];
    int vid;
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		recalc_vid_list(hdlg);

		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO_SPEED);
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2131));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2133));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2134));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2135));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2136));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2137));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2138));
		SendMessage(h, CB_SETCURSEL, temp_video_speed+1, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
		SendMessage(h, BM_SETCHECK, temp_voodoo, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
		SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp);
		wcstombs(tempA, temp, sizeof(tempA));
		vid = video_card_getid(tempA);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_VID);
		if (video_card_has_config(vid))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_VIDEO:
				h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
				SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp);
				wcstombs(tempA, temp, sizeof(tempA));
				vid = video_card_getid(tempA);
				temp_video_card = video_new_to_old(vid);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_VID);
				if (video_card_has_config(vid))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CHECK_VOODOO:
				h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
				temp_voodoo = SendMessage(h, BM_GETCHECK, 0, 0);

				h = GetDlgItem(hdlg, IDC_BUTTON_VOODOO);
				EnableWindow(h, temp_voodoo ? TRUE : FALSE);
				break;

			case IDC_BUTTON_VOODOO:
				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)&voodoo_device);
				break;

			case IDC_CONFIGURE_VID:
				h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
				SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp);
				wcstombs(tempA, temp, sizeof(temp));
				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)video_card_getdevice(video_card_getid(tempA)));

				break;
		}
		return FALSE;

	case WM_SAVESETTINGS:
		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO);
		SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp);
		wcstombs(tempA, temp, sizeof(tempA));
		temp_video_card = video_new_to_old(video_card_getid(tempA));

		h = GetDlgItem(hdlg, IDC_COMBO_VIDEO_SPEED);
		temp_video_speed = SendMessage(h, CB_GETCURSEL, 0, 0) - 1;

		h = GetDlgItem(hdlg, IDC_CHECK_VOODOO);
		temp_voodoo = SendMessage(h, BM_GETCHECK, 0, 0);
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
