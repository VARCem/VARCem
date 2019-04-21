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
 * Version:	@(#)win_settings_machine.h	1.0.13	2019/04/11
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
 *			    Machine Dialog				*
 *									*
 ************************************************************************/

static wchar_t	*mach_names[256];
static int	mach_to_list[256],
		list_to_mach[256];


static void
machine_recalc_cpu(HWND hdlg)
{
    HWND h;
    const device_t *dev;
    const machine_t *m;
#ifdef USE_DYNAREC
    int cpu_flags;
#endif
    int cpu_type;

    /* Get info about the selected machine. */
    dev = machine_get_device_ex(temp_machine);
    m = (machine_t *)dev->mach_info;

    cpu_type = m->cpu[temp_cpu_m].cpus[temp_cpu].type;
#ifdef USE_DYNAREC
    cpu_flags = m->cpu[temp_cpu_m].cpus[temp_cpu].flags;
#endif

    h = GetDlgItem(hdlg, IDC_COMBO_WS);
    if ((cpu_type >= CPU_286) && (cpu_type <= CPU_386DX))
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

#ifdef USE_DYNAREC
    h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
    if (cpu_flags & CPU_SUPPORTS_DYNAREC) {
	SendMessage(h, BM_SETCHECK, temp_dynarec, 0);

	/*
	 * If Dynarec is not enabled, see if the CPU requires
	 * it. If it does, the user must have forced it off
	 * manually, and we respect that. We will just issue
	 * a warning so they know.
	 */
	if (cpu_flags & CPU_REQUIRES_DYNAREC) {
#if 0
		if (! temp_dynarec) {
			//FIXME: make this a messagebox with a user warning instead!  --FvK
			fatal("Attempting to select a CPU that requires the recompiler and does not support it at the same time\n");
		}
#endif

#ifdef RELEASE_BUILD
		/* Since it is required, lock the checkbox. */
		EnableWindow(h, FALSE);
#endif
	} else {
		/* Supported but not required, unlock the checkbox. */
		EnableWindow(h, TRUE);
	}
    } else {
	/* CPU does not support Dynarec, un-check it. */
	temp_dynarec = 0;
	SendMessage(h, BM_SETCHECK, temp_dynarec, 0);

	/* Since it is not supported, lock the checkbox. */
	EnableWindow(h, FALSE);
    }
#endif

    h = GetDlgItem(hdlg, IDC_CHECK_FPU);
    if ((cpu_type < CPU_i486DX) && (cpu_type >= CPU_286)) {
	EnableWindow(h, TRUE);
    } else if (cpu_type < CPU_286) {
	temp_fpu = 0;
	EnableWindow(h, FALSE);
    } else {
	temp_fpu = 1;
	EnableWindow(h, FALSE);
    }

    SendMessage(h, BM_SETCHECK, temp_fpu, 0);
}


static void
machine_recalc_cpu_m(HWND hdlg)
{
    WCHAR temp[128];
    const device_t *dev;
    const machine_t *m;
    const char *stransi;
    HWND h;
    int c = 0;

    /* Get info about the selected machine. */
    dev = machine_get_device_ex(temp_machine);
    m = (machine_t *)dev->mach_info;

    h = GetDlgItem(hdlg, IDC_COMBO_CPU);
    SendMessage(h, CB_RESETCONTENT, 0, 0);
    c = 0;
    for (;;) {
	stransi = m->cpu[temp_cpu_m].cpus[c].name;
	if (stransi == NULL)
		break;

	mbstowcs(temp, stransi, sizeof_w(temp));
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
	c++;
    }
    EnableWindow(h, TRUE);

    if (temp_cpu >= c)
	temp_cpu = (c - 1);
    SendMessage(h, CB_SETCURSEL, temp_cpu, 0);

    machine_recalc_cpu(hdlg);
}


static void
machine_recalc_machine(HWND hdlg)
{
    WCHAR temp[128];
    const char *stransi;
    const device_t *dev;
    const machine_t *m;
    UDACCEL accel;
    HWND h;
    int c;

    /* Get info about the selected machine. */
    dev = machine_get_device_ex(temp_machine);
    m = (machine_t *)dev->mach_info;

    h = GetDlgItem(hdlg, IDC_CONFIGURE_MACHINE);
    if (dev != NULL && dev->config != NULL)
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_COMBO_CPU_TYPE);
    SendMessage(h, CB_RESETCONTENT, 0, 0);
    c = 0;
    while (m->cpu[c].cpus != NULL && c < 4) {
	stransi = m->cpu[c].name;
	mbstowcs(temp, stransi, sizeof_w(temp));
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
	c++;
    }
    EnableWindow(h, TRUE);

    if (temp_cpu_m >= c)
	temp_cpu_m = (c - 1);
    SendMessage(h, CB_SETCURSEL, temp_cpu_m, 0);
    if (c == 1)
	EnableWindow(h, FALSE);
      else
	EnableWindow(h, TRUE);

    machine_recalc_cpu_m(hdlg);

    h = GetDlgItem(hdlg, IDC_MEMSPIN);
    SendMessage(h, UDM_SETRANGE, 0, (m->min_ram << 16) | m->max_ram);
    accel.nSec = 0;
    accel.nInc = m->ram_granularity;
    SendMessage(h, UDM_SETACCEL, 1, (LPARAM)&accel);

    if (!(m->flags & MACHINE_AT) || (m->ram_granularity >= 128)) {
	SendMessage(h, UDM_SETPOS, 0, temp_mem_size);
	h = GetDlgItem(hdlg, IDC_TEXT_MB);
	SendMessage(h, WM_SETTEXT, 0, win_string(IDS_3334));
    } else {
	SendMessage(h, UDM_SETPOS, 0, temp_mem_size / 1024);
	h = GetDlgItem(hdlg, IDC_TEXT_MB);
	SendMessage(h, WM_SETTEXT, 0, win_string(IDS_3330));
    }
}


static WIN_RESULT CALLBACK
machine_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    char tempA[128];
    const device_t *dev;
    const machine_t *m;
    wchar_t *str;
    HWND h;
    int c, d;

    switch (message) {
	case WM_INITDIALOG:
		h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);

		/*
		 * Populate the Machines combo.
		 *
		 * The necessary data was already built
		 * by the calling code, so we only have
		 * to add it to the combo here.
		 */
		c = 0;
		while(1) {
			str = mach_names[c++];
			if (str == NULL)
				break;

			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)str);
		}

		SendMessage(h, CB_SETCURSEL, mach_to_list[temp_machine], 0);

		h = GetDlgItem(hdlg, IDC_COMBO_WS);
                SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3335));
		for (c = 0; c < 8; c++) {
			swprintf(temp, sizeof_w(temp), L"%i", c);
        	        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		}
		SendMessage(h, CB_SETCURSEL, temp_wait_states, 0);

#ifdef USE_DYNAREC
       	        h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
                SendMessage(h, BM_SETCHECK, temp_dynarec, 0);
#endif

		h = GetDlgItem(hdlg, IDC_MEMSPIN);
		SendMessage(h, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_MEMTEXT), 0);

		h = GetDlgItem(hdlg, IDC_COMBO_SYNC);
               	SendMessage(h, CB_ADDSTRING,
			    TIME_SYNC_DISABLED, win_string(IDS_DISABLED));
               	SendMessage(h, CB_ADDSTRING,
			    TIME_SYNC_ENABLED, win_string(IDS_ENABLED));
               	SendMessage(h, CB_ADDSTRING,
			    TIME_SYNC_ENABLED_UTC, win_string(IDS_3336));
		SendMessage(h, CB_SETCURSEL, temp_sync, 0);

		machine_recalc_machine(hdlg);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_MACHINE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);
					d = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
					temp_machine = list_to_mach[d];
					machine_recalc_machine(hdlg);
				}
				break;

			case IDC_COMBO_CPU_TYPE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_CPU_TYPE);
					temp_cpu_m = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

					temp_cpu = 0;
					machine_recalc_cpu_m(hdlg);
				}
				break;

			case IDC_COMBO_CPU:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_CPU);
					temp_cpu = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

					machine_recalc_cpu(hdlg);
				}
				break;

			case IDC_CONFIGURE_MACHINE:
				h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);
				d = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				temp_machine = list_to_mach[d];
				dev = machine_get_device_ex(temp_machine);
				temp_deviceconfig |= dlg_devconf(hdlg, dev);
				break;

			case IDC_COMBO_SYNC:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_SYNC);
					temp_sync = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				}
				break;
		}

		return FALSE;

	case WM_SAVE_CFG:
		/* Get info about the selected machine. */
		dev = machine_get_device_ex(temp_machine);
		m = (machine_t *)dev->mach_info;

#ifdef USE_DYNAREC
		h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
		temp_dynarec = (int)SendMessage(h, BM_GETCHECK, 0, 0);
#endif

		h = GetDlgItem(hdlg, IDC_CHECK_FPU);
		temp_fpu = (int)SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_WS);
		temp_wait_states = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

		h = GetDlgItem(hdlg, IDC_MEMTEXT);
		SendMessage(h, WM_GETTEXT, sizeof_w(temp), (LPARAM)temp);
		wcstombs(tempA, temp, sizeof(tempA));
		sscanf(tempA, "%i", &temp_mem_size);
		temp_mem_size &= ~(m->ram_granularity - 1);
		if (temp_mem_size < (int)m->min_ram)
			temp_mem_size = m->min_ram;
		else if (temp_mem_size > (int)m->max_ram)
			temp_mem_size = m->max_ram;
		if ((m->flags & MACHINE_AT) && (m->ram_granularity < 128))
			temp_mem_size *= 1024;
		if (m->flags & MACHINE_VIDEO)
			video_card = VID_INTERNAL;
		if (m->flags & MACHINE_MOUSE)
			mouse_type = MOUSE_INTERNAL;
		if (m->flags & MACHINE_SOUND)
			sound_card = SOUND_INTERNAL;
		if (m->flags & MACHINE_HDC)
			hdc_type = HDC_INTERNAL;
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
