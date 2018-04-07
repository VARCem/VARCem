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
 * Version:	@(#)win_settings_machine.h	1.0.1	2018/04/05
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
 *			    Machine Dialog				*
 *									*
 ************************************************************************/

static void
machine_recalc_cpu(HWND hdlg)
{
    HWND h;
#ifdef USE_DYNAREC
    int cpu_flags;
#endif
    int cpu_type;
    int rs = 0;

    rs = machine_getromset_ex(temp_machine);

    h = GetDlgItem(hdlg, IDC_COMBO_WS);
    cpu_type = machines[romstomachine[rs]].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
    if ((cpu_type >= CPU_286) && (cpu_type <= CPU_386DX))
	EnableWindow(h, TRUE);
    else
	EnableWindow(h, FALSE);

#ifdef USE_DYNAREC
    h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
    cpu_flags = machines[romstomachine[rs]].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
    if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) && (cpu_flags & CPU_REQUIRES_DYNAREC)) {
	fatal("Attempting to select a CPU that requires the recompiler and does not support it at the same time\n");
    }
    if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC)) {
	if (!(cpu_flags & CPU_SUPPORTS_DYNAREC))
		temp_dynarec = 0;
	if (cpu_flags & CPU_REQUIRES_DYNAREC)
		temp_dynarec = 1;
	SendMessage(h, BM_SETCHECK, temp_dynarec, 0);
	EnableWindow(h, FALSE);
    } else {
	EnableWindow(h, TRUE);
    }
#endif

    h = GetDlgItem(hdlg, IDC_CHECK_FPU);
    cpu_type = machines[romstomachine[rs]].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
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
    const char *stransi;
    HWND h;
    int c = 0;
    int rs = 0;

    rs = machine_getromset_ex(temp_machine);

    h = GetDlgItem(hdlg, IDC_COMBO_CPU);
    SendMessage(h, CB_RESETCONTENT, 0, 0);
    c = 0;
    while (machines[romstomachine[rs]].cpu[temp_cpu_m].cpus[c].cpu_type != -1) {
	stransi = machines[romstomachine[rs]].cpu[temp_cpu_m].cpus[c].name;
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
    HWND h;
    int c = 0;
    int rs = 0;
    UDACCEL accel;

    rs = machine_getromset_ex(temp_machine);

    h = GetDlgItem(hdlg, IDC_CONFIGURE_MACHINE);
    if (machine_getdevice(temp_machine))
	EnableWindow(h, TRUE);
    else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_COMBO_CPU_TYPE);
    SendMessage(h, CB_RESETCONTENT, 0, 0);
    c = 0;
    while (machines[romstomachine[rs]].cpu[c].cpus != NULL && c < 4) {
	stransi = machines[romstomachine[rs]].cpu[c].name;
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
    SendMessage(h, UDM_SETRANGE, 0, (machines[romstomachine[rs]].min_ram << 16) | machines[romstomachine[rs]].max_ram);
    accel.nSec = 0;
    accel.nInc = machines[romstomachine[rs]].ram_granularity;
    SendMessage(h, UDM_SETACCEL, 1, (LPARAM)&accel);

    if (!(machines[romstomachine[rs]].flags & MACHINE_AT) || (machines[romstomachine[rs]].ram_granularity >= 128)) {
	SendMessage(h, UDM_SETPOS, 0, temp_mem_size);
	h = GetDlgItem(hdlg, IDC_TEXT_MB);
	SendMessage(h, WM_SETTEXT, 0, (LPARAM)plat_get_string(IDS_2094));
    } else {
	SendMessage(h, UDM_SETPOS, 0, temp_mem_size / 1024);
	h = GetDlgItem(hdlg, IDC_TEXT_MB);
	SendMessage(h, WM_SETTEXT, 0, (LPARAM)plat_get_string(IDS_2087));
    }
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
machine_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    char tempA[128];
    const char *stransi;
    HWND h;
    int c;
    int d = 0;

    switch (message) {
	case WM_INITDIALOG:
		h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);
		for (c = 0; c < ROM_MAX; c++)
			romstolist[c] = 0;
		c = d = 0;
		while (machines[c].id != -1) {
			if (romspresent[machines[c].id]) {
				stransi = machines[c].name;
				mbstowcs(temp, stransi, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
				machinetolist[c] = d;
				listtomachine[d] = c;
				romstolist[machines[c].id] = d;
				romstomachine[machines[c].id] = c;
				d++;
			}
			c++;
		}
		SendMessage(h, CB_SETCURSEL, machinetolist[temp_machine], 0);

		h = GetDlgItem(hdlg, IDC_COMBO_WS);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(IDS_2131));

		for (c = 0; c < 8; c++) {
			swprintf(temp, sizeof_w(temp), plat_get_string(2132), c);
        	        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
		}

		SendMessage(h, CB_SETCURSEL, temp_wait_states, 0);

#ifdef USE_DYNAREC
       	        h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
                SendMessage(h, BM_SETCHECK, temp_dynarec, 0);
#endif

		h = GetDlgItem(hdlg, IDC_MEMSPIN);
		SendMessage(h, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_MEMTEXT), 0);

       	        h = GetDlgItem(hdlg, IDC_CHECK_SYNC);
                SendMessage(h, BM_SETCHECK, temp_sync, 0);

		machine_recalc_machine(hdlg);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_MACHINE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);
					temp_machine = listtomachine[SendMessage(h, CB_GETCURSEL, 0, 0)];

					machine_recalc_machine(hdlg);
				}
				break;

			case IDC_COMBO_CPU_TYPE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_CPU_TYPE);
					temp_cpu_m = SendMessage(h, CB_GETCURSEL, 0, 0);

					temp_cpu = 0;
					machine_recalc_cpu_m(hdlg);
				}
				break;

			case IDC_COMBO_CPU:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					h = GetDlgItem(hdlg, IDC_COMBO_CPU);
					temp_cpu = SendMessage(h, CB_GETCURSEL, 0, 0);

					machine_recalc_cpu(hdlg);
				}
				break;

			case IDC_CONFIGURE_MACHINE:
				h = GetDlgItem(hdlg, IDC_COMBO_MACHINE);
				temp_machine = listtomachine[SendMessage(h, CB_GETCURSEL, 0, 0)];

				temp_deviceconfig |= deviceconfig_open(hdlg, (void *)machine_getdevice(temp_machine));
				break;
		}

		return FALSE;

	case WM_SAVESETTINGS:
#ifdef USE_DYNAREC
		h = GetDlgItem(hdlg, IDC_CHECK_DYNAREC);
		temp_dynarec = SendMessage(h, BM_GETCHECK, 0, 0);
#endif

		h = GetDlgItem(hdlg, IDC_CHECK_SYNC);
		temp_sync = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_CHECK_FPU);
		temp_fpu = SendMessage(h, BM_GETCHECK, 0, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_WS);
		temp_wait_states = SendMessage(h, CB_GETCURSEL, 0, 0);

		h = GetDlgItem(hdlg, IDC_MEMTEXT);
		SendMessage(h, WM_GETTEXT, sizeof_w(temp), (LPARAM)temp);
		wcstombs(tempA, temp, sizeof(tempA));
		sscanf(tempA, "%i", &temp_mem_size);
		temp_mem_size &= ~(machines[temp_machine].ram_granularity - 1);
		if (temp_mem_size < machines[temp_machine].min_ram)
			temp_mem_size = machines[temp_machine].min_ram;
		else if (temp_mem_size > machines[temp_machine].max_ram)
			temp_mem_size = machines[temp_machine].max_ram;
		if ((machines[temp_machine].flags & MACHINE_AT) && (machines[temp_machine].ram_granularity < 128))
			temp_mem_size *= 1024;
		if (machines[temp_machine].flags & MACHINE_VIDEO)
			vid_card = VID_INTERNAL;
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
