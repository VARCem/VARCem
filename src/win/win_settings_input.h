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
 * Version:	@(#)win_settings_input.h	1.0.7	2018/05/02
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
 *			      Input Dialog				*
 *									*
 ************************************************************************/

static int	mouse_to_list[20],
		list_to_mouse[20];


static int
mouse_valid(int num, int m)
{
    const device_t *dev;

    if ((num == MOUSE_INTERNAL) &&
	!(machines[m].flags & MACHINE_MOUSE)) return(0);

    dev = mouse_get_device(num);

    return(device_is_valid(dev, machines[m].flags));
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
input_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    const char *stransi;
    HWND h;
    int c;
    int d = 0;

    switch (message) {
	case WM_INITDIALOG:
		h = GetDlgItem(hdlg, IDC_COMBO_MOUSE);
		d = 0;
		for (c = 0; c < mouse_get_ndev(); c++) {
			mouse_to_list[c] = d;

			if (mouse_valid(c, temp_machine)) {
				stransi = mouse_get_name(c);
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);

				list_to_mouse[d] = c;
				d++;
			}
		}

		SendMessage(h, CB_SETCURSEL, mouse_to_list[temp_mouse], 0);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_MOUSE);
		if (mouse_has_config(temp_mouse))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
		c = 0;
		while ((stransi = gamedev_get_name(c)) != NULL) {
			mbstowcs(temp, stransi, sizeof_w(temp));
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			c++;
		}
		SendMessage(h, CB_SETCURSEL, temp_joystick, 0);
		EnableWindow(h, (temp_game)?TRUE:FALSE);

		h = GetDlgItem(hdlg, IDC_JOY1);
		EnableWindow(h, (temp_game && (gamedev_get_max_joysticks(temp_joystick) >= 1)) ? TRUE : FALSE);
		h = GetDlgItem(hdlg, IDC_JOY2);
		EnableWindow(h, (temp_game && (gamedev_get_max_joysticks(temp_joystick) >= 2)) ? TRUE : FALSE);
		h = GetDlgItem(hdlg, IDC_JOY3);
		EnableWindow(h, (temp_game && (gamedev_get_max_joysticks(temp_joystick) >= 3)) ? TRUE : FALSE);
		h = GetDlgItem(hdlg, IDC_JOY4);
		EnableWindow(h, (temp_game && (gamedev_get_max_joysticks(temp_joystick) >= 4)) ? TRUE : FALSE);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_MOUSE:
				h = GetDlgItem(hdlg, IDC_COMBO_MOUSE);
				temp_mouse = list_to_mouse[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_MOUSE);
				if (mouse_has_config(temp_mouse))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CONFIGURE_MOUSE:
				h = GetDlgItem(hdlg, IDC_COMBO_MOUSE);
				temp_mouse = list_to_mouse[SendMessage(h, CB_GETCURSEL, 0, 0)];
				temp_deviceconfig |= dlg_devconf(hdlg, (void *)mouse_get_device(temp_mouse));
				break;

			case IDC_COMBO_JOYSTICK:
				h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
				temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);

				h = GetDlgItem(hdlg, IDC_JOY1);
				EnableWindow(h, (gamedev_get_max_joysticks(temp_joystick) >= 1) ? TRUE : FALSE);
				h = GetDlgItem(hdlg, IDC_JOY2);
				EnableWindow(h, (gamedev_get_max_joysticks(temp_joystick) >= 2) ? TRUE : FALSE);
				h = GetDlgItem(hdlg, IDC_JOY3);
				EnableWindow(h, (gamedev_get_max_joysticks(temp_joystick) >= 3) ? TRUE : FALSE);
				h = GetDlgItem(hdlg, IDC_JOY4);
				EnableWindow(h, (gamedev_get_max_joysticks(temp_joystick) >= 4) ? TRUE : FALSE);
				break;

			case IDC_JOY1:
				h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
				temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_deviceconfig |= dlg_jsconf(hdlg, 0, temp_joystick);
				break;

			case IDC_JOY2:
				h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
				temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_deviceconfig |= dlg_jsconf(hdlg, 1, temp_joystick);
				break;

			case IDC_JOY3:
				h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
				temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_deviceconfig |= dlg_jsconf(hdlg, 2, temp_joystick);
				break;

			case IDC_JOY4:
				h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
				temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_deviceconfig |= dlg_jsconf(hdlg, 3, temp_joystick);
				break;
		}
		return FALSE;

	case WM_SAVESETTINGS:
		h = GetDlgItem(hdlg, IDC_COMBO_MOUSE);
		temp_mouse = list_to_mouse[SendMessage(h, CB_GETCURSEL, 0, 0)];

		h = GetDlgItem(hdlg, IDC_COMBO_JOYSTICK);
		temp_joystick = SendMessage(h, CB_GETCURSEL, 0, 0);
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
