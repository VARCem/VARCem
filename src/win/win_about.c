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
 * NOTE:	Not quite happy with the Donate button workings, a full
 *		24bit image would be preferred, but we cant use LoadImage
 *		for those (and keep transparency...)
 *
 * Version:	@(#)win_about.c	1.0.12	2018/10/25
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
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
#include "resource.h"


static void
donate_handle(HWND hdlg)
{
    pclog(LOG_ALWAYS,
	  "UI: DONATE button clicked, opening browser to PayPal\n");

    ShellExecute(NULL, L"open", URL_PAYPAL, NULL, NULL , SW_SHOW);
}


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


static void
localize_disp(HWND hdlg, int idx)
{
    WCHAR temp[128];
    lang_t *lang;
    HWND h;

    /* Get the correct language for this index. */
    h = GetDlgItem(hdlg, IDC_LOCALIZE); 
    lang = (lang_t *)SendMessage(h, LB_GETITEMDATA, idx, 0);

    SetDlgItemText(hdlg, IDT_LOCALIZE+1, lang->version); 

    swprintf(temp, sizeof_w(temp), L"%ls, <%ls>", lang->author, lang->email);
    SetDlgItemText(hdlg, IDT_LOCALIZE+2, temp); 
}


static WIN_RESULT CALLBACK
localize_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HBITMAP hBmp;
    lang_t *lang;
    HWND h;
    int c;

    switch (message) {
	case WM_INITDIALOG:
		/* Center in the main window. */
		dialog_center(hdlg);

		/* Load the main icon. */
		hBmp = (HBITMAP) LoadImage(hInstance,MAKEINTRESOURCE(ICON_MAIN),
				 IMAGE_ICON, 64, 64, 0);
		h = GetDlgItem(hdlg, IDC_ABOUT_ICON);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hBmp);

		/* Add the languages. */
		h = GetDlgItem(hdlg, IDC_LOCALIZE);
		for (lang = ui_lang_get()->next; lang != NULL; lang = lang->next) {
			c = (int)SendMessage(h, LB_ADDSTRING, 0, (LPARAM)lang->name);
			SendMessage(h, LB_SETITEMDATA, c, (LPARAM)lang);
		}
		SendMessage(h, LB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		localize_disp(hdlg, 0);
		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;

			case IDC_LOCALIZE:
				switch (HIWORD(wParam)) { 
					case LBN_SELCHANGE:
						h = GetDlgItem(hdlg, IDC_LOCALIZE); 
						c = (int)SendMessage(h, LB_GETCURSEL, 0, 0); 
						localize_disp(hdlg, c);

						return TRUE; 
				} 
				return TRUE;

			default:
				break;
		}
		break;
    }

    return(FALSE);
}


static WIN_RESULT CALLBACK
about_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH brush = NULL;
    wchar_t temp[128];
    HBITMAP hBmp;
    HWND h;

    switch (message) {
	case WM_INITDIALOG:
		/* Center in the main window. */
		dialog_center(hdlg);

		hBmp = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ICON_MAIN),
				 IMAGE_ICON, 64, 64, 0);
		h = GetDlgItem(hdlg, IDC_ABOUT_ICON);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hBmp);
		mbstowcs(temp, emu_title, sizeof_w(temp));
		SendDlgItemMessage(hdlg, IDT_TITLE, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)temp);
		set_font_bold(hdlg, IDT_TITLE);

		mbstowcs(temp, emu_fullversion, sizeof_w(temp));
		SendDlgItemMessage(hdlg, IDT_VERSION, WM_SETTEXT,
				   (WPARAM)NULL, (LPARAM)temp);

		/* Load the Paypal Donate icon. */
		h = GetDlgItem(hdlg, IDC_DONATE);
#if 1
		hBmp = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ICON_DONATE),
				 IMAGE_BITMAP, 105, 50,
				 LR_LOADTRANSPARENT|LR_LOADMAP3DCOLORS);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBmp);
#else
		hBmp = LoadImage(hInstance, MAKEINTRESOURCE(ICON_DONATE),
				 IMAGE_ICON, 128, 128, 0);
		SendMessage(h, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hBmp);
#endif
		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;

			case IDC_LOCALIZE:
				dlg_localize();
				break;

			default:
				switch (HIWORD(wParam)) {
					case STN_CLICKED:
						if ((HWND)lParam == GetDlgItem(hdlg, IDC_DONATE)) {
							donate_handle(hdlg);
							return TRUE;
						}
						break;

					default:
						break;
				}
				break;
		}
		break;

        case WM_CTLCOLORSTATIC:
                if ((HWND)lParam == GetDlgItem(hdlg, IDC_DONATE)) {
			/* Grab background color from dialog window. */
			HDC hDC = GetDC(hdlg);
			COLORREF col = GetBkColor(hDC);

			/* Set as background color for static controli.. */
			hDC = (HDC)wParam;
			SetBkColor(hDC, col);

			/* .. and also return that as paint color. */
			if (brush == NULL)
				brush = CreateSolidBrush(col);
			return (LRESULT)brush;
		}
		return FALSE;
    }

    return(FALSE);
}


void
dlg_about(void)
{
    DialogBox(plat_lang_dll(), (LPCWSTR)DLG_ABOUT, hwndMain, about_proc);
}


void
dlg_localize(void)
{
    DialogBox(plat_lang_dll(), (LPCWSTR)DLG_LOCALIZE, hwndMain, localize_proc);
}
