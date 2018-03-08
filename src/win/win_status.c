/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Status Window dialog.
 *
 * Version:	@(#)win_status.c	1.0.3	2018/03/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../pit.h"
#include "../mem.h"
#include "../cpu/cpu.h"
#include "../cpu/x86_ops.h"
#ifdef USE_DYNAREC
# include "../cpu/codegen.h"
#endif
#include "../device.h"
#include "../plat.h"
#include "win.h"



HWND	hwndStatus = NULL;


extern int sreadlnum, swritelnum, segareads, segawrites, scycles_lost;
extern uint64_t main_time;
static uint64_t status_time;


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
StatusWindowProcedure(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char temp[4096];
    uint64_t new_time;
    uint64_t status_diff;

    switch (message) {
	case WM_INITDIALOG:
		hwndStatus = hdlg;
		/*FALLTHROUGH*/

	case WM_USER:
		new_time = plat_timer_read();
		status_diff = new_time - status_time;
		status_time = new_time;
		sprintf(temp,
			"CPU speed : %f MIPS\n"
			"FPU speed : %f MFLOPS\n\n"

			"Video throughput (read) : %i bytes/sec\n"
			"Video throughput (write) : %i bytes/sec\n\n"
			"Effective clockspeed : %iHz\n\n"
			"Timer 0 frequency : %fHz\n\n"
			"CPU time : %f%% (%f%%)\n"

#ifdef USE_DYNAREC
			"New blocks : %i\nOld blocks : %i\nRecompiled speed : %f MIPS\nAverage size : %f\n"
			"Flushes : %i\nEvicted : %i\nReused : %i\nRemoved : %i"
#endif
			,mips,
			flops,
			segareads,
			segawrites,
			clockrate - scycles_lost,
			pit_timer0_freq(),
			((double)main_time * 100.0) / status_diff,
                        ((double)main_time * 100.0) / timer_freq

#ifdef USE_DYNAREC
			, cpu_new_blocks_latched, cpu_recomp_blocks_latched, (double)cpu_recomp_ins_latched / 1000000.0, (double)cpu_recomp_ins_latched/cpu_recomp_blocks_latched,
			cpu_recomp_flushes_latched, cpu_recomp_evicted_latched,
			cpu_recomp_reuse_latched, cpu_recomp_removed_latched
#endif
		);
		main_time = 0;
		SendDlgItemMessage(hdlg, IDT_SDEVICE, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)temp);

		temp[0] = 0;
		device_add_status_info(temp, 4096);
		SendDlgItemMessage(hdlg, IDT_STEXT, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)temp);
		return(TRUE);
		
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL:
				hwndStatus = NULL;
				EndDialog(hdlg, 0);
				return(TRUE);
		}
		break;
    }

    return(FALSE);
}


void
StatusWindowCreate(HWND hwndParent)
{
    HWND hwnd;

    hwnd = CreateDialog(hinstance, (LPCSTR)DLG_STATUS,
			hwndParent, StatusWindowProcedure);
    ShowWindow(hwnd, SW_SHOW);
}


/* Tell the Status window to update. */
void
ui_status_update(void)
{
    SendMessage(hwndStatus, WM_USER, 0, 0);
}
