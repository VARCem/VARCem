/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of server several dialogs.
 *
 * Version:	@(#)win_dialog.c	1.0.14	2018/10/24
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
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../version.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"


DWORD	filterindex = 0;


/* Center a dialog window relative to its parent window. */
void
dialog_center(HWND hdlg)
{
    RECT r, r1, r2;
    HWND parent;

    /* Get handle to parent window. Use desktop if needed. */
    if ((parent = GetParent(hdlg)) == NULL)
	parent = GetDesktopWindow();

    /* Get owner and dialog rects. */
    GetWindowRect(hdlg, &r1); 
    GetWindowRect(parent, &r2);
    CopyRect(&r, &r2); 

    /* Center the dialog within the owner's space. */
    OffsetRect(&r1, -r1.left, -r1.top); 
    OffsetRect(&r, -r.left, -r.top); 
    OffsetRect(&r, -r1.right, -r1.bottom); 

    /* Move the dialog window. */
    SetWindowPos(hdlg, HWND_TOP,
		 r2.left + (r.right / 2), r2.top + (r.bottom / 2),
		 0, 0, SWP_NOSIZE);
}


/* WinHook to allow for centering the messagebox. */
static LRESULT CALLBACK
CenterMsgBoxTextProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = (HWND)wParam;
    HWND hTxt;

    if (nCode == HCBT_ACTIVATE) {
	/* Center dialog. */
	dialog_center(hDlg);

	/* Find the (STATIC) text label in the control. */
	hTxt = FindWindowEx(hDlg, NULL, TEXT("STATIC"), NULL);

	if (hTxt != NULL)
		SetWindowLong(hTxt, GWL_STYLE,
			      GetWindowLong(hTxt, GWL_STYLE) | SS_CENTER);
    }

    return(CallNextHookEx(NULL, nCode, wParam, lParam));
}


int
ui_msgbox(int flags, const void *arg)
{
    WCHAR temp[512];
    HHOOK hHook;
    DWORD fl = 0;
    const WCHAR *str = NULL;
    const WCHAR *cap = NULL;

    switch(flags & 0x1f) {
	case MBX_INFO:		/* just an informational message */
		fl = (MB_OK | MB_ICONINFORMATION);
		cap = TEXT(EMU_NAME);
		break;

	case MBX_WARNING:	/* warning message */
		fl = (MB_YESNO | MB_ICONWARNING);
		cap = get_string(IDS_WARNING);
		break;

	case MBX_ERROR:		/* error message */
		if (flags & MBX_FATAL) {
			fl = (MB_OK | MB_ICONERROR);
			cap = get_string(IDS_ERROR_FATAL);
		} else {
			fl = (MB_OK | MB_ICONWARNING);
			cap = get_string(IDS_ERROR);
		}
		break;

	case MBX_QUESTION:	/* question */
		fl = (MB_YESNOCANCEL | MB_ICONQUESTION);
		cap = TEXT(EMU_NAME);
		break;

	case MBX_CONFIG:	/* configuration */
		fl = (MB_YESNO | MB_ICONERROR);
		cap = get_string(IDS_ERROR_CONF);
		break;
    }

    /* If ANSI string, convert it. */
    str = (WCHAR *)arg;
    if (flags & MBX_ANSI) {
	mbstowcs(temp, (char *)arg, strlen((char *)arg)+1);
	str = temp;
    } else {
	/*
	 * It's a Unicode string.
	 *
	 * Well, no, maybe not. It could also be one of the
	 * strings stored in the Resources. Those are wide,
	 * but referenced by a numeric ID.
	 *
	 * The good news is, that strings are usually stored
	 * in the executable near the end of the code/rodata
	 * segment. This means, that *real* string pointers
	 * usually have a pretty high (numeric) value, much
	 * higher than the numeric ID's.  So, we guesswork
	 * that if the value of 'arg' is low, its an ID..
	 */
#if (defined(_MSC_VER) && defined(_M_X64)) || \
    (defined(__GNUC__) && defined(__amd64__))
	if (((uintptr_t)arg) < ((uintptr_t)65636ULL))
		str = get_string(((intptr_t)arg) & 0xffff);
#else
	if (((uint32_t)arg) < ((uint32_t)65636))
		str = get_string((intptr_t)arg);
#endif
    }

    /* Create a hook for the MessageBox dialog. */
    hHook = SetWindowsHookEx(WH_CBT, (HOOKPROC)CenterMsgBoxTextProc,
			     NULL, GetCurrentThreadId());

    /* At any rate, we do have a valid (wide) string now. */
    fl = MessageBox(hwndMain,		/* our main window */
		    str,		/* error message etc */
		    cap,		/* window caption */
		    fl);

    /* Remove the dialog hook. */
    if (hHook != NULL)
	UnhookWindowsHookEx(hHook);

    /* Convert return values to generic ones. */
    switch(fl) {
	case IDNO:
		fl = 1; break;

	case IDYES:
	case IDOK:
		fl = 0; break;

	case IDCANCEL:
		fl = -1; break;
    }

    return(fl);
}


/* Use a hook to center the OFN dialog. */
static UINT_PTR CALLBACK
dlg_file_hook(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    OFNOTIFY *np;

    switch (uiMsg) {
	case WM_NOTIFY:
		np = (OFNOTIFY *)lParam;
		if (np->hdr.code == CDN_INITDONE)
			dialog_center(GetParent(hdlg));
		break;
    }

    return(TRUE);
}


/* Implement the main GetFileName dialog. */
int
dlg_file_ex(HWND h, const wchar_t *f, const wchar_t *ifn, wchar_t *fn, int fl)
{
    wchar_t temp[512], *str;
    wchar_t path[512];
    OPENFILENAME ofn;
    DWORD err; 
    BOOL r;
    int ret;

    /* Initialize OPENFILENAME. */
    memset(&ofn, 0x00, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = h;
    ofn.lpfnHook = dlg_file_hook;

    /* Tell the dialog where to go initially. */
    memset(temp, 0x00, sizeof(temp));
    if ((ifn != NULL) && (*ifn != L'\0')) {
	/* We seem to have a valid path, use that. */
	if (! plat_path_abs(ifn))
		wcscpy(temp, usr_path);
	wcscat(temp, ifn);

	/* Re-slash the path, OpenFileName does not like forward slashes. */
	str = temp;
	while (*str != L'\0') {
		if (*str == L'/')
			*str = L'\\';
		str++;
	}

	ofn.lpstrInitialDir = NULL;
    } else {
	/* Re-slash the path, OpenFileName does not like forward slashes. */
	wcscpy(path, usr_path);
    	str = path;
    	while (*str != L'\0') {
		if (*str == L'/')
			*str = L'\\';
		str++;
	}

	/* No initial path, use the usr_path value. */
	ofn.lpstrInitialDir = path;
    }
    ofn.lpstrFile = temp;
    ofn.nMaxFile = sizeof_w(temp);

    /* Set up the "file types" filter. */
    ofn.lpstrFilter = f;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;

    /* Set up the flags for this dialog. */
    r = (fl & DLG_FILE_RO) ? TRUE : FALSE;
    ofn.Flags = OFN_ENABLESIZING | OFN_ENABLEHOOK | OFN_EXPLORER | \
		OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_DONTADDTORECENT;

    if (! (fl & DLG_FILE_SAVE)) {
	ofn.Flags |= OFN_FILEMUSTEXIST;
	if (r == TRUE)
		ofn.Flags |= OFN_READONLY;
    }

    /* Display the Open dialog box. */
    if (fl & DLG_FILE_SAVE)
	r = GetSaveFileName(&ofn);
      else
	r = GetOpenFileName(&ofn);

    /* As OFN_NOCHANGEDIR does not work (see MSDN), just make sure. */
    plat_chdir(usr_path);

    if (r) {
	/* All good, see if we can make this a relative path. */
	pc_path(fn, sizeof_w(temp), temp);

	/* Remember the file type for next time. */
	filterindex = ofn.nFilterIndex;

	ret = 1;
	if (ofn.Flags & OFN_READONLY)
		ret |= DLG_FILE_RO;
    } else {
	/* If an error occurred, log this. */
	if ((err = CommDlgExtendedError()) != NO_ERROR) {
		sprintf((char *)temp,
			"%sFile('%ls', %02x):\n\n    error 0x%08lx",
			(fl & DLG_FILE_SAVE)?"Save":"Open", temp, fl, err);
		ERRLOG("%s\n", (char *)temp);
		(void)ui_msgbox(MBX_ERROR|MBX_ANSI, (char *)temp);
	}

	ret = 0;
    }

    return(ret);
}


int
dlg_file(const wchar_t *filt, const wchar_t *ifn, wchar_t *ofn, int flags)
{
    return(dlg_file_ex(hwndMain, filt, ifn, ofn, flags));
}
