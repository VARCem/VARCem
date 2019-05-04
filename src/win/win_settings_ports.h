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
 * Version:	@(#)win_settings_ports.h	1.0.8	2019/05/03
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
 *			       Ports Dialog				*
 *									*
 ************************************************************************/

static WIN_RESULT CALLBACK
ports_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    const char *stransi;
    HWND h;
    int c, d, i;

    switch (message) {
	case WM_INITDIALOG:
		/* Set up the game port controls. */
		h = GetDlgItem(hdlg, IDC_CHECK_GAME);
		SendMessage(h, BM_SETCHECK, temp_cfg.game_enabled, 0);

		/* Set up the parallel port controls. */
		for (i = 0; i < PARALLEL_MAX; i++) {
			/* Populate the "devices" list and select one. */
			h = GetDlgItem(hdlg, IDC_COMBO_PARALLEL1+i);

			c = d = 0;
			for (;;) {
				stransi = parallel_device_get_internal_name(c);
				if (stransi == NULL) break;

				if (c == 0) {
					SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_NONE));
				} else {
					stransi = parallel_device_get_name(c);
					mbstowcs(temp, stransi, sizeof_w(temp));
					SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
				}

				/* Currently selected device? */
				if (temp_cfg.parallel_device[i] == c)
					d = c;
				c++;
			}
			SendMessage(h, CB_SETCURSEL, d, 0);

			/* Enable or disable this port. */
			d = temp_cfg.parallel_enabled[i];
			EnableWindow(h, d ? TRUE : FALSE);
			h = GetDlgItem(hdlg, IDC_CHECK_PARALLEL1+i);
			SendMessage(h, BM_SETCHECK, d, 0);
		}

		/* Set up the serial port controls. */
		for (i = 0; i < SERIAL_MAX; i++) {
			h = GetDlgItem(hdlg, IDC_CHECK_SERIAL1+i);
			SendMessage(h, BM_SETCHECK, temp_cfg.serial_enabled[i], 0);
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_CHECK_PARALLEL1:
			case IDC_CHECK_PARALLEL2:
			case IDC_CHECK_PARALLEL3:
				h = GetDlgItem(hdlg, LOWORD(wParam));
				d = (int)SendMessage(h, BM_GETCHECK, 0, 0);
				c = (LOWORD(wParam) - IDC_CHECK_PARALLEL1);
				h = GetDlgItem(hdlg, IDC_COMBO_PARALLEL1+c);
				EnableWindow(h, d ? TRUE : FALSE);
				break;
		}
		return FALSE;

	case WM_SAVE_CFG:
		h = GetDlgItem(hdlg, IDC_CHECK_GAME);
		temp_cfg.game_enabled = (int)SendMessage(h, BM_GETCHECK, 0, 0);

		for (i = 0; i < PARALLEL_MAX; i++) {
			h = GetDlgItem(hdlg, IDC_CHECK_PARALLEL1+i);
			temp_cfg.parallel_enabled[i] = (int)SendMessage(h, BM_GETCHECK, 0, 0);

			h = GetDlgItem(hdlg, IDC_COMBO_PARALLEL1+i);
			c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
			temp_cfg.parallel_device[i] = c;
		}

		for (i = 0; i < SERIAL_MAX; i++) {
			h = GetDlgItem(hdlg, IDC_CHECK_SERIAL1+i);
			temp_cfg.serial_enabled[i] = (int)SendMessage(h, BM_GETCHECK, 0, 0);
		}
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
