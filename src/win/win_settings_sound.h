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
 * Version:	@(#)win_settings_sound.h	1.0.1	2018/04/05
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
 *			      Sound Dialog				*
 *									*
 ************************************************************************/

static int
mpu401_present(void)
{
    char *stransi;

    stransi = sound_card_get_internal_name(temp_sound_card);
    if (stransi != NULL) {
	if (!strcmp(stransi, "sb16") || !strcmp(stransi, "sbawe32"))
		return 1;
    }

    return temp_mpu401 ? 1 : 0;
}


static int
mpu401_standalone_allow(void)
{
    char *n, *md;

    n = sound_card_get_internal_name(temp_sound_card);
    md = midi_device_get_internal_name(temp_midi_device);
    if (n != NULL) {
	if (!strcmp(n, "sb16") || !strcmp(n, "sbawe32"))
		return 0;
    }

    if (md != NULL) {
	if (! strcmp(md, "none"))
		return 0;
    }

    return 1;
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
sound_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    char *stransi;
    const device_t *dev;
    HWND h;
    int c, d;

    switch (message) {
	case WM_INITDIALOG:
		h = GetDlgItem(hdlg, IDC_COMBO_SOUND);
		c = d = 0;
		while (1) {
			stransi = sound_card_getname(c);
			if (stransi == NULL)		
				break;

			settings_sound_to_list[c] = d;

			if (sound_card_available(c)) {
				dev = sound_card_getdevice(c);
				if (device_is_valid(dev, machines[temp_machine].flags)) {
					mbstowcs(temp, stransi, sizeof_w(temp));
					SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
					settings_list_to_sound[d] = c;
					d++;
				}
			}

			c++;
		}
		SendMessage(h, CB_SETCURSEL, settings_sound_to_list[temp_sound_card], 0);

		EnableWindow(h, d ? TRUE : FALSE);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_SND);
		if (sound_card_has_config(temp_sound_card))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDC_COMBO_MIDI);
		c = d = 0;
		while (1) {
			stransi = midi_device_getname(c);
			if (stransi == NULL)
				break;

			settings_midi_to_list[c] = d;

			if (midi_device_available(c)) {
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);

				settings_list_to_midi[d] = c;
				d++;
			}

			c++;
		}
		SendMessage(h, CB_SETCURSEL, settings_midi_to_list[temp_midi_device], 0);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_MIDI);
		if (midi_device_has_config(temp_midi_device))
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDC_CHECK_MPU401);
		SendMessage(h, BM_SETCHECK, temp_mpu401, 0);
		EnableWindow(h, mpu401_standalone_allow() ? TRUE : FALSE);

		h = GetDlgItem(hdlg, IDC_CONFIGURE_MPU401);
		EnableWindow(h, (mpu401_standalone_allow() && temp_mpu401) ? TRUE : FALSE);

		h = GetDlgItem(hdlg, IDC_CHECK_CMS);
		SendMessage(h, BM_SETCHECK, temp_GAMEBLASTER, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_GUS);
		SendMessage(h, BM_SETCHECK, temp_GUS, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_SSI);
		SendMessage(h, BM_SETCHECK, temp_SSI2001, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_NUKEDOPL);
		SendMessage(h, BM_SETCHECK, temp_opl3_type, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_FLOAT);
		SendMessage(h, BM_SETCHECK, temp_float, 0);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_SOUND:
				h = GetDlgItem(hdlg, IDC_COMBO_SOUND);
				temp_sound_card = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_SND);
				if (sound_card_has_config(temp_sound_card))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);

				h = GetDlgItem(hdlg, IDC_CHECK_MPU401);
				SendMessage(h, BM_SETCHECK, temp_mpu401, 0);
				EnableWindow(h, mpu401_standalone_allow() ? TRUE : FALSE);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_MPU401);
				EnableWindow(h, (mpu401_standalone_allow() && temp_mpu401) ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_SND:
				h = GetDlgItem(hdlg, IDC_COMBO_SOUND);
				temp_sound_card = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)sound_card_getdevice(temp_sound_card));
				break;

			case IDC_COMBO_MIDI:
				h = GetDlgItem(hdlg, IDC_COMBO_MIDI);
				temp_midi_device = settings_list_to_midi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				h = GetDlgItem(hdlg, IDC_CONFIGURE_MIDI);
				if (midi_device_has_config(temp_midi_device))
					EnableWindow(h, TRUE);
				else
					EnableWindow(h, FALSE);

				h = GetDlgItem(hdlg, IDC_CHECK_MPU401);
				SendMessage(h, BM_SETCHECK, temp_mpu401, 0);
				EnableWindow(h, mpu401_standalone_allow() ? TRUE : FALSE);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_MPU401);
				EnableWindow(h, (mpu401_standalone_allow() && temp_mpu401) ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_MIDI:
				h = GetDlgItem(hdlg, IDC_COMBO_MIDI);
				temp_midi_device = settings_list_to_midi[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)midi_device_getdevice(temp_midi_device));
				break;

			case IDC_CHECK_MPU401:
				h = GetDlgItem(hdlg, IDC_CHECK_MPU401);
				temp_mpu401 = SendMessage(h, BM_GETCHECK, 0, 0);

				h = GetDlgItem(hdlg, IDC_CONFIGURE_MPU401);
				EnableWindow(h, mpu401_present() ? TRUE : FALSE);
				break;

			case IDC_CONFIGURE_MPU401:
				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)&mpu401_device);
				break;
		}
		return FALSE;

	case WM_SAVESETTINGS:
		h = GetDlgItem(hdlg, IDC_COMBO_SOUND);
		temp_sound_card = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];

		h = GetDlgItem(hdlg, IDC_COMBO_MIDI);
		temp_midi_device = settings_list_to_midi[SendMessage(h, CB_GETCURSEL, 0, 0)];

		h = GetDlgItem(hdlg, IDC_CHECK_MPU401);
		temp_mpu401 = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_CMS);
		temp_GAMEBLASTER = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_GUS);
		temp_GUS = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_SSI);
		temp_SSI2001 = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_NUKEDOPL);
		temp_opl3_type = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_FLOAT);
		temp_float = SendMessage(h, BM_GETCHECK, 0, 0);
		return FALSE;

	default:
	break;
    }

    return FALSE;
}
