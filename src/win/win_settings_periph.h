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
 * Version:	@(#)win_settings_periph.h	1.0.17	2019/01/15
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
 *			(Other) Peripherals Dialog			*
 *									*
 ************************************************************************/

static int	scsi_to_list[20],
		list_to_scsi[20];
static int	hdc_to_list[20],
		list_to_hdc[20];


/* Populate the HDC combo. */
static void
recalc_scsi_list(HWND hdlg)
{
    WCHAR temp[128];
    char tempA[128];
    const char *stransi;
    const device_t *dev;
    HWND h;
    int c, d;

    h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
    SendMessage(h, CB_RESETCONTENT, 0, 0);

    c = d = 0;
    for (;;) {
	stransi = scsi_card_get_internal_name(c);
	if (stransi == NULL) break;

	dev = scsi_card_getdevice(c);

	if (!scsi_card_available(c) ||
	    !device_is_valid(dev, machines[temp_machine].flags)) {
		c++;
		continue;
	}

	if (c == 0) {
		SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_NONE));
	} else {
		stransi = scsi_card_getname(c);
		sprintf(tempA, "[%s] %s",
			device_get_bus_name(dev), stransi);
		mbstowcs(temp, tempA, sizeof_w(temp));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
	}

	scsi_to_list[c] = d;			
	list_to_scsi[d++] = c++;
    }

    SendMessage(h, CB_SETCURSEL, scsi_to_list[temp_scsi_card], 0);
    EnableWindow(h, d ? TRUE : FALSE);
}


/* Populate the HDC combo. */
static void
recalc_hdc_list(HWND hdlg)
{
    WCHAR temp[128];
    char tempA[128];
    char tempB[128];
    const char *stransi;
    const device_t *dev;
    HWND h;
    int c, d;

    h = GetDlgItem(hdlg, IDC_COMBO_HDC);
    SendMessage(h, CB_RESETCONTENT, 0, 0);

    c = d = 0;
    for (;;) {
	stransi = hdc_get_internal_name(c);
	if (stransi == NULL) break;

	dev = hdc_get_device(c);

	if (!hdc_available(c) ||
	    !device_is_valid(dev, machines[temp_machine].flags)) {
		c++;
		continue;
	}

	if (c == 0) {
		SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_NONE));
	} else if (c == 1) {
		if (! (machines[temp_machine].flags&MACHINE_HDC)) {
			/* Skip "Internal" if machine doesn't have one. */
			c++;
			continue;
		}
		SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_INTERNAL));
	} else {
		wcstombs(tempB,
			 hdd_bus_to_ids((dev->local >> 8) & 255),
			 sizeof(tempB));
		stransi = hdc_get_name(c);
		sprintf(tempA, "[%s] %s (%s)",
			device_get_bus_name(dev), stransi, tempB);
		mbstowcs(temp, tempA, sizeof_w(temp));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
	}

	hdc_to_list[c] = d;			
	list_to_hdc[d] = c;
	c++; d++;
    }

    SendMessage(h, CB_SETCURSEL, hdc_to_list[temp_hdc_type], 0);
    EnableWindow(h, d ? TRUE : FALSE);
}


static WIN_RESULT CALLBACK
peripherals_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    const char *stransi;
    const device_t *dev;
    int c, d;
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		recalc_scsi_list(hdlg);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_SCSI);
		if (scsi_card_has_config(temp_scsi_card))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		recalc_hdc_list(hdlg);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_HDC);
		if (hdc_has_config(temp_hdc_type))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		if (machines[temp_machine].flags & MACHINE_AT) {
			h = GetDlgItem(hdlg, IDC_CHECK_IDE_TER);
			EnableWindow(h, TRUE);
			SendMessage(h, BM_SETCHECK, temp_ide_ter, 0);
			h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_TER);
			EnableWindow(h, TRUE);

			h = GetDlgItem(hdlg, IDC_CHECK_IDE_QUA);
			EnableWindow(h, TRUE);
			SendMessage(h, BM_SETCHECK, temp_ide_qua, 0);
			h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_QUA);
			EnableWindow(h, TRUE);
		} else {
			h = GetDlgItem(hdlg, IDC_CHECK_IDE_TER);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_TER);
			EnableWindow(h, FALSE);

			h = GetDlgItem(hdlg, IDC_CHECK_IDE_QUA);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_QUA);
			EnableWindow(h, FALSE);
		}

		h = GetDlgItem(hdlg, IDC_CHECK_BUGGER);
		SendMessage(h, BM_SETCHECK, temp_bugger, 0);

		/* Populate the ISA RTC card dropdown. */
		h = GetDlgItem(hdlg, IDC_COMBO_ISARTC);
		for (d = 0; ; d++) {
			stransi = isartc_get_internal_name(d);
			if (stransi == NULL)
				break;

			if (d == 0) {
				/* Translate "None". */
				SendMessage(h, CB_ADDSTRING, 0,
					    win_string(IDS_NONE));
			} else {
				stransi = isartc_get_name(d);
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			}
		}
		SendMessage(h, CB_SETCURSEL, temp_isartc, 0);
		h = GetDlgItem(hdlg, IDC_CONFIGURE_ISARTC);
		if (temp_isartc != 0)
			EnableWindow(h, TRUE);
		  else
			EnableWindow(h, FALSE);

		/* Populate the ISA memory card dropdowns. */
		for (c = 0; c < ISAMEM_MAX; c++) {
			h = GetDlgItem(hdlg, IDC_COMBO_ISAMEM_1 + c);
			for (d = 0; ; d++) {
				stransi = isamem_get_internal_name(d);
				if (stransi == NULL)
					break;

				if (d == 0) {
					/* Translate "None". */
					SendMessage(h, CB_ADDSTRING, 0,
						    win_string(IDS_NONE));
				} else {
					stransi = isamem_get_name(d);
					mbstowcs(temp, stransi, sizeof_w(temp));
					SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
				}
			}
			SendMessage(h, CB_SETCURSEL, temp_isamem[c], 0);
			h = GetDlgItem(hdlg, IDC_CONFIGURE_ISAMEM_1 + c);
			if (temp_isamem[c] != 0)
				EnableWindow(h, TRUE);
			  else
				EnableWindow(h, FALSE);
		}

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_SCSI:
				h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
				temp_scsi_card = list_to_scsi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_SCSI);
				if (scsi_card_has_config(temp_scsi_card))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CONFIGURE_SCSI:
				h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
				temp_scsi_card = list_to_scsi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= dlg_devconf(hdlg, scsi_card_getdevice(temp_scsi_card));
				break;

			case IDC_COMBO_HDC:
				h = GetDlgItem(hdlg, IDC_COMBO_HDC);
				temp_hdc_type = list_to_hdc[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_HDC);
				if (hdc_has_config(temp_hdc_type))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CONFIGURE_HDC:
				h = GetDlgItem(hdlg, IDC_COMBO_HDC);
				temp_hdc_type = list_to_hdc[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= dlg_devconf(hdlg, hdc_get_device(temp_hdc_type));
				break;

			case IDC_CHECK_IDE_TER:
        		        h = GetDlgItem(hdlg, IDC_CHECK_IDE_TER);
				temp_ide_ter = (int)SendMessage(h, BM_GETCHECK, 0, 0);
        		        h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_TER);
				EnableWindow(h, temp_ide_ter ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_IDE_TER:
				temp_deviceconfig |= dlg_devconf(hdlg, &ide_ter_device);
				break;

			case IDC_CHECK_IDE_QUA:
	       		        h = GetDlgItem(hdlg, IDC_CHECK_IDE_QUA);
				temp_ide_qua = (int)SendMessage(h, BM_GETCHECK, 0, 0);
	       		        h = GetDlgItem(hdlg, IDC_CONFIGURE_IDE_QUA);
				EnableWindow(h, temp_ide_qua ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_IDE_QUA:
				temp_deviceconfig |= dlg_devconf(hdlg, &ide_qua_device);
				break;

			case IDC_COMBO_ISARTC:
				h = GetDlgItem(hdlg, IDC_COMBO_ISARTC);
				temp_isartc = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				h = GetDlgItem(hdlg, IDC_CONFIGURE_ISARTC);
				if (temp_isartc != 0)
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_COMBO_ISAMEM_1:
			case IDC_COMBO_ISAMEM_2:
			case IDC_COMBO_ISAMEM_3:
			case IDC_COMBO_ISAMEM_4:
				c = LOWORD(wParam) - IDC_COMBO_ISAMEM_1;
				h = GetDlgItem(hdlg, LOWORD(wParam));
				temp_isamem[c] = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_ISAMEM_1 + c);
				if (temp_isamem[c] != 0)
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_CONFIGURE_ISARTC:
				dev = isartc_get_device(temp_isartc);
				if (dev != NULL)
					temp_deviceconfig |= dlg_devconf(hdlg, dev);
				break;

			case IDC_CONFIGURE_ISAMEM_1:
			case IDC_CONFIGURE_ISAMEM_2:
			case IDC_CONFIGURE_ISAMEM_3:
			case IDC_CONFIGURE_ISAMEM_4:
				c = LOWORD(wParam) - IDC_CONFIGURE_ISAMEM_1;
				dev = isamem_get_device(c);
				if (dev != NULL)
					temp_deviceconfig |= dlg_devconf(hdlg, dev);
				  else
					ui_msgbox(MBX_INFO, (wchar_t *)IDS_ERR_SAVEIT);
				break;
		}
		return FALSE;

	case WM_SAVE_CFG:
		h = GetDlgItem(hdlg, IDC_COMBO_HDC);
		c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
		temp_hdc_type = list_to_hdc[c];

		h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
		c = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
		temp_scsi_card = list_to_scsi[c];

		h = GetDlgItem(hdlg, IDC_CHECK_BUGGER);
		temp_bugger = (int)SendMessage(h, BM_GETCHECK, 0, 0);

		return FALSE;

	default:
		break;
    }

    return FALSE;
}
