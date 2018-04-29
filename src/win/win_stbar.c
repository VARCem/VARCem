/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Status Bar module.
 *
 * Version:	@(#)win_stbar.c	1.0.13	2018/04/29
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
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"

#ifndef GWL_WNDPROC
#define GWL_WNDPROC GWLP_WNDPROC
#endif


#define SBAR_HEIGHT		17			/* for 16x16 icons */


HWND		hwndSBAR;


static LONG_PTR	OriginalProcedure;
static HMENU	menuSBAR;
static HMENU	*sb_menu;
static int	sb_nparts;


static VOID APIENTRY
PopupMenu(HWND hwnd, POINT pt, int part)
{
    if (part >= (sb_nparts - 1)) return;

    pt.x = part * SB_ICON_WIDTH;	/* justify to the left */
    pt.y = 0;				/* justify to the top */

    ClientToScreen(hwnd, (LPPOINT)&pt);

    TrackPopupMenu(sb_menu[part],
		   TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
		   pt.x, pt.y, 0, hwndSBAR, NULL);
}


/* Handle messages for the Status Bar window. */
#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
dlg_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    POINT pt;
    int idm, tag;

    switch (message) {
	case WM_COMMAND:
		idm = LOWORD(wParam) & 0xff00;		/* low 8 bits */
		tag = LOWORD(wParam) & 0x00ff;		/* high 8 bits */
		ui_sb_menu_command(idm, tag);
		return(0);

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		GetClientRect(hwnd, (LPRECT)&r);
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		if (PtInRect((LPRECT)&r, pt))
			PopupMenu(hwnd, pt, (pt.x / SB_ICON_WIDTH));
		break;

	case WM_LBUTTONDBLCLK:
		GetClientRect(hwnd, (LPRECT)&r);
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		tag = (pt.x / SB_ICON_WIDTH);
		if (PtInRect((LPRECT)&r, pt))
			ui_sb_click(tag);
		break;

	default:
		return(CallWindowProc((WNDPROC)OriginalProcedure,
				      hwnd, message, wParam, lParam));
    }

    return(0);
}


/* Create and set up the Status Bar window. */
void
StatusBarCreate(HWND hwndParent, uintptr_t idStatus, HINSTANCE hInst)
{
    intptr_t i;
    int dw, dh;
    RECT r;

    /* Load our icons into the cache for faster access. */
    for (i = 128; i < 130; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 144; i < 146; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 160; i < 162; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 176; i < 178; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 192; i < 194; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 208; i < 210; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 224; i < 226; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 259; i < 260; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 384; i < 386; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 400; i < 402; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 416; i < 418; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 432; i < 434; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);
    for (i = 448; i < 450; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);

    GetWindowRect(hwndParent, &r);
    dw = r.right - r.left;
    dh = r.bottom - r.top;

    /* Load the Common Controls DLL if needed. */
    InitCommonControls();

    /* Create the window, and make sure it's using the STATUS class. */
    hwndSBAR = CreateWindowEx(0,
			      STATUSCLASSNAME, 
			      (LPCTSTR)NULL,
			      SBARS_SIZEGRIP|WS_CHILD|WS_VISIBLE|SBT_TOOLTIPS,
			      0, dh-SBAR_HEIGHT, dw, SBAR_HEIGHT,
			      hwndParent,
			      (HMENU)idStatus, hInst, NULL);

    /* Replace the original procedure with ours. */
    OriginalProcedure = GetWindowLongPtr(hwndSBAR, GWLP_WNDPROC);
    SetWindowLongPtr(hwndSBAR, GWL_WNDPROC, (LONG_PTR)&dlg_proc);

    SendMessage(hwndSBAR, SB_SETMINHEIGHT, (WPARAM)SBAR_HEIGHT, (LPARAM)0);

    /* Align the window with the parent (main) window. */
    GetWindowRect(hwndSBAR, &r);
    SetWindowPos(hwndSBAR,
		 HWND_TOPMOST,
		 r.left, r.top, r.right-r.left, r.bottom-r.top,
		 SWP_SHOWWINDOW);

    /* Load the dummu menu for this window. */
    menuSBAR = LoadMenu(hInst, SB_MENU_NAME);

    /* Clear the menus, just in case.. */
    sb_nparts = 0;
    sb_menu = NULL;
}


/* Also used by win_settings.c */
int
sb_fdd_icon(int type)
{
    int ret = 512;

    switch(type) {
	case 0:
		break;

	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		ret = 128;
		break;

	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
		ret = 144;
		break;

	default:
		break;
    }

    return(ret);
}


void
sb_setup(int parts, const int *widths)
{
    SendMessage(hwndSBAR, SB_SETPARTS, (WPARAM)parts, (LPARAM)widths);

    if (sb_menu != NULL)
	sb_menu_destroy();

    sb_menu = (HMENU *)malloc(parts * sizeof(HMENU));
    memset(sb_menu, 0x00, parts * sizeof(HMENU));

    sb_nparts = parts;
}


void
sb_menu_destroy(void)
{
    int i;

    if (!sb_nparts || (sb_menu == NULL)) return;

    for (i = 0; i < sb_nparts; i++) {
	if (sb_menu[i] != NULL)
		DestroyMenu(sb_menu[i]);
    }

    free(sb_menu);

    sb_menu = NULL;
}


/* Create a menu for a status bar part. */
void
sb_menu_create(int part)
{
    HMENU h;

    h = CreatePopupMenu();

    sb_menu[part] = h;
}


/* Add an item to a (status bar) menu. */
void
sb_menu_add_item(int part, int idm, const wchar_t *str)
{
    if (idm >= 0)
	AppendMenu(sb_menu[part], MF_STRING, idm, str);
      else
	AppendMenu(sb_menu[part], MF_SEPARATOR, 0, NULL);
}


void
sb_menu_enable_item(int part, int idm, int val)
{
    EnableMenuItem(sb_menu[part], idm,
	val ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
}


void
sb_menu_set_item(int part, int idm, int val)
{
    CheckMenuItem(sb_menu[part], idm, val ? MF_CHECKED : MF_UNCHECKED);
}


void
sb_set_icon(int part, int icon)
{
    HANDLE ptr;

    if (icon == -1) ptr = NULL;
      else ptr = hIcon[(intptr_t)icon];

    SendMessage(hwndSBAR, SB_SETICON, part, (LPARAM)ptr);
}


void
sb_set_text(int part, const wchar_t *str)
{
    SendMessage(hwndSBAR, SB_SETTEXT, part | SBT_NOBORDERS, (LPARAM)str);
}


void
sb_set_tooltip(int part, const wchar_t *str)
{
    SendMessage(hwndSBAR, SB_SETTIPTEXT, part, (LPARAM)str);
}
