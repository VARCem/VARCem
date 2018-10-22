/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Sound Gain dialog.
 *
 * Version:	@(#)win_snd_gain.c	1.0.8	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
 *		Copyright 2018 Miran Grca.
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
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "..//emu.h"
#include "..//config.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/sound/sound.h"
#include "win.h"
#include "resource.h"


static uint8_t	old_gain;


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		dialog_center(hdlg);
		old_gain = sound_gain;
		h = GetDlgItem(hdlg, IDC_SLIDER_GAIN);
		SendMessage(h, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(0, 9));
		SendMessage(h, TBM_SETPOS, (WPARAM)1, 9 - (sound_gain >> 1));
		SendMessage(h, TBM_SETTICFREQ, (WPARAM)1, 0);
		SendMessage(h, TBM_SETLINESIZE, (WPARAM)0, 1);
		SendMessage(h, TBM_SETPAGESIZE, (WPARAM)0, 2);
		break;

	case WM_VSCROLL:
		h = GetDlgItem(hdlg, IDC_SLIDER_GAIN);
		sound_gain = (9 - SendMessage(h, TBM_GETPOS, (WPARAM)0, 0)) << 1;
		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
				h = GetDlgItem(hdlg, IDC_SLIDER_GAIN);
				sound_gain = (9 - SendMessage(h, TBM_GETPOS, (WPARAM)0, 0)) << 1;
				config_save();
				EndDialog(hdlg, 0);
				return TRUE;

			case IDCANCEL:
				sound_gain = old_gain;
				config_save();
				EndDialog(hdlg, 0);
				return TRUE;

			default:
				break;
		}
		break;
    }

    return(FALSE);
}


void
dlg_sound_gain(void)
{
    DialogBox(plat_lang_dll(), (LPCTSTR)DLG_SND_GAIN, hwndMain, dlg_proc);
}
