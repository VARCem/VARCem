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
 * Version:	@(#)win_settings_network.h	1.0.3	2018/04/10
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
 *			       Network Dialog				*
 *									*
 ************************************************************************/

/* Global variables for the Network dialog. */
static int	nic_to_list[20],
		list_to_nic[20];
static int	net_ignore_message = 0;


static void
network_recalc_combos(HWND hdlg)
{
    HWND h;

    net_ignore_message = 1;

    h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
    if (temp_net_type == NET_TYPE_PCAP)
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_COMBO_NET);
    if (temp_net_type == NET_TYPE_SLIRP)
	EnableWindow(h, TRUE);
      else if ((temp_net_type == NET_TYPE_PCAP) && (network_dev_to_id(temp_host_dev) > 0))
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_CONFIGURE_NET);
    if (network_card_has_config(temp_net_card) && (temp_net_type == NET_TYPE_SLIRP)) {
	EnableWindow(h, TRUE);
    } else if (network_card_has_config(temp_net_card) &&
		 (temp_net_type == NET_TYPE_PCAP) &&
		  (network_dev_to_id(temp_host_dev) > 0)) {
	EnableWindow(h, TRUE);
    } else {
	EnableWindow(h, FALSE);
    }

    net_ignore_message = 0;
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
network_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    const char *stransi;
    HWND h;
    int c, d;

    switch (message) {
	case WM_INITDIALOG:
		h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
//FIXME: take strings from network.c table.. --FvK
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)L"None");
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)L"PCap");
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)L"SLiRP");
		SendMessage(h, CB_SETCURSEL, temp_net_type, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		if (temp_net_type == NET_TYPE_PCAP)
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		for (c = 0; c < network_ndev; c++) {
			mbstowcs(temp, network_devs[c].description, sizeof_w(temp));
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		}
		SendMessage(h, CB_SETCURSEL, network_dev_to_id(temp_host_dev), 0);

		/*NIC config*/
		h = GetDlgItem(hdlg, IDC_COMBO_NET);
		c = d = 0;
		while (1) {
			stransi = network_card_getname(c);
			if (stransi == NULL)
				break;

			nic_to_list[c] = d;

			if (network_card_available(c) && device_is_valid(network_card_getdevice(c), machines[temp_machine].flags)) {
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);

				list_to_nic[d] = c;
				d++;
			}

			c++;
		}
		SendMessage(h, CB_SETCURSEL, nic_to_list[temp_net_card], 0);

		EnableWindow(h, d ? TRUE : FALSE);
		network_recalc_combos(hdlg);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_NET_TYPE:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
				temp_net_type = SendMessage(h, CB_GETCURSEL, 0, 0);

				network_recalc_combos(hdlg);
				break;

			case IDC_COMBO_PCAP:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
				memset(temp_host_dev, '\0', sizeof(temp_host_dev));
				strcpy(temp_host_dev, network_devs[SendMessage(h, CB_GETCURSEL, 0, 0)].device);

				network_recalc_combos(hdlg);
				break;

			case IDC_COMBO_NET:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_NET);
				temp_net_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];

				network_recalc_combos(hdlg);
				break;

			case IDC_CONFIGURE_NET:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_NET);
				temp_net_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)network_card_getdevice(temp_net_card));
				break;
		}
		return FALSE;

	case WM_SAVESETTINGS:
		h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
		temp_net_type = SendMessage(h, CB_GETCURSEL, 0, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		memset(temp_host_dev, '\0', sizeof(temp_host_dev));
		strcpy(temp_host_dev, network_devs[SendMessage(h, CB_GETCURSEL, 0, 0)].device);

		h = GetDlgItem(hdlg, IDC_COMBO_NET);
		temp_net_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
