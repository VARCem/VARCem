/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the About dialog.
 *
 * Version:	@(#)win_about.c	1.0.2	2018/02/21
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
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../plat.h"
#include "win.h"


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
AboutDialogProcedure(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		plat_pause(1);
		h = GetDlgItem(hdlg, IDC_ABOUT_ICON);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_ICON,
		  (LPARAM)LoadImage(hinstance,(PCTSTR)100,IMAGE_ICON,64,64,0));
		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, 0);
				plat_pause(0);
				return TRUE;

			default:
				break;
		}
		break;
    }

    return(FALSE);
}


void
AboutDialogCreate(HWND hwnd)
{
    DialogBox(hinstance, (LPCTSTR)DLG_ABOUT, hwnd, AboutDialogProcedure);
}
