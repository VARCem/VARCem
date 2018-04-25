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
 * Version:	@(#)win_settings_periph.h	1.0.4	2018/04/25
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
 *			(Other) Peripherals Dialog			*
 *									*
 ************************************************************************/

static int	scsi_to_list[20],
		list_to_scsi[20];
static const char	*hdc_names[16];
static const int valid_ide_irqs[11] = { 2, 3, 4, 5, 7, 9, 10, 11, 12, 14, 15 };


static int
find_irq_in_array(int irq, int def)
{
    int i;

    for (i = 0; i < 11; i++) {
	if (valid_ide_irqs[i] == irq)
			return(i + 1);
    }

    return(7 + def);
}


static void
recalc_hdc_list(HWND hdlg, int machine, int use_selected_hdc)
{
    WCHAR temp[128];
    char old_name[32];
    const char *stransi;
    HWND h;
    int valid;
    int c, d;

    h = GetDlgItem(hdlg, IDC_COMBO_HDC);
    valid = 0;

    if (use_selected_hdc) {
	c = SendMessage(h, CB_GETCURSEL, 0, 0);

	if (c != -1 && hdc_names[c])
		strcpy(old_name, hdc_names[c]);
	else
		strcpy(old_name, "none");
    } else {
	strcpy(old_name, hdc_get_internal_name(temp_hdc_type));
    }

    SendMessage(h, CB_RESETCONTENT, 0, 0);
    c = d = 0;
    while (1) {
	stransi = hdc_get_name(c);
	if (stransi == NULL)
		break;

	if (c==1 && !(machines[temp_machine].flags&MACHINE_HDC)) {
		/* Skip "Internal" if machine doesn't have one. */
		c++;
		continue;
	}
	if (!hdc_available(c) || !device_is_valid(hdc_get_device(c), machines[temp_machine].flags)) {
		c++;
		continue;
	}
	mbstowcs(temp, stransi, sizeof_w(temp));
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);

	hdc_names[d] = hdc_get_internal_name(c);
	if (! strcmp(old_name, hdc_names[d])) {
		SendMessage(h, CB_SETCURSEL, d, 0);
		valid = 1;
	}
	c++;
	d++;
    }

    if (! valid)
	SendMessage(h, CB_SETCURSEL, 0, 0);

    EnableWindow(h, d ? TRUE : FALSE);
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
peripherals_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    const char *stransi;
    const device_t *dev;
    int c, d;
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		/*SCSI config*/
		h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
		c = d = 0;
		while (1) {
			stransi = scsi_card_getname(c);
			if (stransi == NULL)
				break;

			scsi_to_list[c] = d;			
			if (scsi_card_available(c)) {
				dev = scsi_card_getdevice(c);

				if (device_is_valid(dev, machines[temp_machine].flags)) {
					if (c == 0) {
						SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2152));
					} else {
						mbstowcs(temp, stransi, sizeof_w(temp));
						SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
					}
					list_to_scsi[d] = c;
					d++;
				}
			}

			c++;
		}
		SendMessage(h, CB_SETCURSEL, scsi_to_list[temp_scsi_card], 0);
		EnableWindow(h, d ? TRUE : FALSE);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_SCSI);
		if (scsi_card_has_config(temp_scsi_card))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_HDC);
		if (hdc_has_config(temp_hdc_type))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		recalc_hdc_list(hdlg, temp_machine, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_IDE_TER);
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_5376));
		for (c = 0; c < 11; c++) {
			swprintf(temp, sizeof_w(temp), plat_get_string(IDS_2155), valid_ide_irqs[c]);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		}
		if (temp_ide_ter)
			SendMessage(h, CB_SETCURSEL, find_irq_in_array(temp_ide_ter_irq, 0), 0);
		else
			SendMessage(h, CB_SETCURSEL, 0, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_IDE_QUA);
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_5376));
		for (c = 0; c < 11; c++) {
			swprintf(temp, sizeof_w(temp), plat_get_string(IDS_2155), valid_ide_irqs[c]);
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		}
		if (temp_ide_qua)
			SendMessage(h, CB_SETCURSEL, find_irq_in_array(temp_ide_qua_irq, 1), 0);
		else
			SendMessage(h, CB_SETCURSEL, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_BUGGER);
		SendMessage(h, BM_SETCHECK, temp_bugger, 0);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_CONFIGURE_SCSI:
				h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
				temp_scsi_card = list_to_scsi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)scsi_card_getdevice(temp_scsi_card));
				break;

			case IDC_CONFIGURE_HDC:
				h = GetDlgItem(hdlg, IDC_COMBO_HDC);
				temp_hdc_type = hdc_get_from_internal_name(hdc_names[SendMessage(h, CB_GETCURSEL, 0, 0)]);

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)hdc_get_device(temp_hdc_type));
				break;

			case IDC_COMBO_SCSI:
				h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
				temp_scsi_card = list_to_scsi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_SCSI);
				if (scsi_card_has_config(temp_scsi_card))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;

			case IDC_COMBO_HDC:
				h = GetDlgItem(hdlg, IDC_COMBO_HDC);
				temp_hdc_type = hdc_get_from_internal_name(hdc_names[SendMessage(h, CB_GETCURSEL, 0, 0)]);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_HDC);
				if (hdc_has_config(temp_hdc_type))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);
				break;
		}
		return FALSE;

	case WM_SAVESETTINGS:
		h = GetDlgItem(hdlg, IDC_COMBO_HDC);
		c = SendMessage(h, CB_GETCURSEL, 0, 0);
		if (hdc_names[c])
			temp_hdc_type = hdc_get_from_internal_name(hdc_names[c]);
		  else
			temp_hdc_type = 0;

		h = GetDlgItem(hdlg, IDC_COMBO_SCSI);
		temp_scsi_card = list_to_scsi[SendMessage(h, CB_GETCURSEL, 0, 0)];

		h = GetDlgItem(hdlg, IDC_COMBO_IDE_TER);
		temp_ide_ter = SendMessage(h, CB_GETCURSEL, 0, 0);
		if (temp_ide_ter > 1) {
			temp_ide_ter_irq = valid_ide_irqs[temp_ide_ter - 1];
			temp_ide_ter = 1;
		}

		h = GetDlgItem(hdlg, IDC_COMBO_IDE_QUA);
		temp_ide_qua = SendMessage(h, CB_GETCURSEL, 0, 0);
		if (temp_ide_qua > 1) {
			temp_ide_qua_irq = valid_ide_irqs[temp_ide_qua - 1];
			temp_ide_qua = 1;
		}

		h = GetDlgItem(hdlg, IDC_CHECK_BUGGER);
		temp_bugger = SendMessage(h, BM_GETCHECK, 0, 0);
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
