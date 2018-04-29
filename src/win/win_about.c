/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the About dialog.
 *
 * Version:	@(#)win_about.c	1.0.6	2018/04/27
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../version.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"


static void
set_font_bold(HWND hdlg, int item)
{
    LOGFONT logFont;
    HFONT font;
    HWND h;

    /* Get a handle to the speficied control in the dialog. */
    h = GetDlgItem(hdlg, item);

    /* Get that control's current font, and create a LOGFONT for it. */
    font = (HFONT)SendMessage(h, WM_GETFONT, 0, 0);
    GetObject(font, sizeof(logFont), &logFont);
 
    /* Modify the font to have BOLD enabled. */
    logFont.lfWeight = FW_BOLD;
    logFont.lfHeight *= 2;
    logFont.lfWidth *= 2;
    font = CreateFontIndirect(&logFont);
 
    /* Set the new font for the control. */
    SendMessage(h, WM_SETFONT, (WPARAM)font, 0);
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char temp[128];
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		/* Center in the main window. */
		dialog_center(hdlg);

		h = GetDlgItem(hdlg, IDC_ABOUT_ICON);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_ICON,
		  (LPARAM)LoadImage(hinstance,(PCTSTR)100,IMAGE_ICON,64,64,0));
		SendDlgItemMessage(hdlg, IDT_TITLE, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)emu_title);
		set_font_bold(hdlg, IDT_TITLE);

		sprintf(temp, "%s", emu_fullversion);
		SendDlgItemMessage(hdlg, IDT_VERSION, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)temp);
		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
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
dlg_about(void)
{
    plat_pause(1);

    DialogBox(hinstance, (LPCTSTR)DLG_ABOUT, hwndMain, dlg_proc);

    plat_pause(0);
}
