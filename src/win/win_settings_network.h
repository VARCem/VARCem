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
 * Version:	@(#)win_settings_network.h	1.0.15	2020/06/09
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
static int	net_to_list[8],
		list_to_net[8];
static int	nic_to_list[20],
		list_to_nic[20];
static int	net_ignore_message = 0;


static void
network_recalc_combos(HWND hdlg)
{
    HWND h, h1, h2;

    net_ignore_message = 1;

    h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
    h1 = GetDlgItem(hdlg, IDC_COMBO_NET_CARD);
    h2 = GetDlgItem(hdlg, IDC_CONFIGURE_NET_CARD);

    switch(temp_cfg.network_type) {
	case NET_SLIRP:
		EnableWindow(h, FALSE);
		EnableWindow(h1, TRUE);
		EnableWindow(h2, TRUE);
		break;

	case NET_PCAP:
		EnableWindow(h, TRUE);
		if (network_card_to_id(temp_cfg.network_host) > 0) {
			EnableWindow(h1, TRUE);
			EnableWindow(h2, TRUE);
		} else {
			EnableWindow(h1, FALSE);
			EnableWindow(h2, FALSE);
		}
		break;

	default:
		EnableWindow(h, FALSE);
		EnableWindow(h1, TRUE);
		EnableWindow(h2, TRUE);
		break;
    }

    net_ignore_message = 0;
}


static WIN_RESULT CALLBACK
network_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char tempA[128];
    WCHAR temp[128];
    const char *stransi;
    const device_t *dev;
    const machine_t *m;
    HWND h;
    int c, d;

    switch (message) {
	case WM_INITDIALOG:
		/* Get info about the selected machine. */
		dev = machine_get_device_ex(temp_cfg.machine_type);
		m = (machine_t *)dev->mach_info;

		/* Populate the "network types" box. */
		h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
		c = d = 0;
		for (;;) {
			stransi = network_get_internal_name(c);
			if (stransi == NULL)
				break;

			if (! network_available(c)) {
				c++;
				continue;
			}

			if (c == 0) {
				/* Translate "None". */
				SendMessage(h, CB_ADDSTRING, 0,
					    win_string(IDS_NONE));
			} else {	
				stransi = network_getname(c);
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			}

			net_to_list[c] = d;
			list_to_net[d++] = c++;
		}
		SendMessage(h, CB_SETCURSEL,
			    net_to_list[temp_cfg.network_type], 0);

		EnableWindow(h, d ? TRUE : FALSE);

		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		if (temp_cfg.network_type == NET_PCAP)
			EnableWindow(h, TRUE);
		else
			EnableWindow(h, FALSE);

		/* Populate the "host interfaces" box. */
		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		for (c = 0; c < network_host_ndev; c++) {
			if (c == 0) {
				/* Translate "None". */
				SendMessage(h, CB_ADDSTRING, 0,
					    win_string(IDS_NONE));
			} else {
				mbstowcs(temp, network_host_devs[c].description, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			}
		}
		SendMessage(h, CB_SETCURSEL,
			    network_card_to_id(temp_cfg.network_host), 0);

		/* Populate the "network cards" box. */
		h = GetDlgItem(hdlg, IDC_COMBO_NET_CARD);
		c = d = 0;
		for (;;) {
			stransi = network_card_get_internal_name(c);
			if (stransi == NULL) break;

			dev = network_card_getdevice(c);
			if (!network_card_available(c) ||
			    !device_is_valid(dev, m->flags)) {
				c++;
				continue;
			}

			if (c == 0) {
				/* Translate "None". */
				SendMessage(h, CB_ADDSTRING, 0,
					    win_string(IDS_NONE));
			} else if (c == 1) {
				if (! (m->flags & MACHINE_NETWORK)) {
					c++;
					continue;
				}

				/* Translate "Internal". */
				SendMessage(h, CB_ADDSTRING, 0,
					    win_string(IDS_INTERNAL));
			} else {
				sprintf(tempA, "[%s] %s",
					device_get_bus_name(dev),
					network_card_getname(c));
				mbstowcs(temp, tempA, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			}

			nic_to_list[c] = d;
			list_to_nic[d++] = c++;
		}
		SendMessage(h, CB_SETCURSEL,
			    nic_to_list[temp_cfg.network_card], 0);

		EnableWindow(h, d ? TRUE : FALSE);
		network_recalc_combos(hdlg);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_NET_TYPE:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
				temp_cfg.network_type = list_to_net[SendMessage(h, CB_GETCURSEL, 0, 0)];

				network_recalc_combos(hdlg);
				break;

			case IDC_COMBO_PCAP:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
				memset(temp_cfg.network_host, '\0', sizeof(temp_cfg.network_host));
				strcpy(temp_cfg.network_host,
				       network_host_devs[SendMessage(h, CB_GETCURSEL, 0, 0)].device);

				network_recalc_combos(hdlg);
				break;

			case IDC_COMBO_NET_CARD:
				if (net_ignore_message)
					return FALSE;

				h = GetDlgItem(hdlg, IDC_COMBO_NET_CARD);
				temp_cfg.network_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];

				network_recalc_combos(hdlg);
				break;

			case IDC_CONFIGURE_NET_CARD:
				if (net_ignore_message)
					return FALSE;

				temp_cfg.network_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];
				dev = network_card_getdevice(temp_cfg.network_card);
				if ((temp_cfg.network_card != NET_CARD_NONE) &&
				    (dev != NULL))
					temp_deviceconfig |= dlg_devconf(hdlg, dev);
				break;
		}
		return FALSE;

	case WM_SAVE_CFG:
		h = GetDlgItem(hdlg, IDC_COMBO_NET_TYPE);
		temp_cfg.network_type = list_to_net[SendMessage(h, CB_GETCURSEL, 0, 0)];

		h = GetDlgItem(hdlg, IDC_COMBO_PCAP);
		memset(temp_cfg.network_host, '\0', sizeof(temp_cfg.network_host));
		strcpy(temp_cfg.network_host, network_host_devs[SendMessage(h, CB_GETCURSEL, 0, 0)].device);

		h = GetDlgItem(hdlg, IDC_COMBO_NET_CARD);
		temp_cfg.network_card = list_to_nic[SendMessage(h, CB_GETCURSEL, 0, 0)];
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
