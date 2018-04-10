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
 * Version:	@(#)win_dialog.c	1.0.6	2018/04/09
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
#include "../plat.h"
#include "../ui.h"
#include "win.h"


WCHAR	wopenfilestring[1024];
char	openfilestring[1024];
DWORD	filterindex = 0;


/* Center a dialog window with respect to the main window. */
void
dialog_center(HWND hdlg)
{
    RECT r, r1, r2;
    HWND owner;

    /* Get handle to owner window. Use desktop if needed. */
    if ((owner = GetParent(hdlg)) == NULL)
	owner = GetDesktopWindow();

    /* Get owner and dialog rects. */
    GetWindowRect(hdlg, &r1); 
    GetWindowRect(owner, &r2);
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


int
ui_msgbox(int flags, void *arg)
{
    WCHAR temp[512];
    DWORD fl = 0;
    WCHAR *str = NULL;
    WCHAR *cap = NULL;

    switch(flags & 0x1f) {
	case MBX_INFO:		/* just an informational message */
		fl = (MB_OK | MB_ICONINFORMATION);
		cap = TEXT(EMU_NAME);
		break;

	case MBX_WARNING:	/* warning message */
		fl = (MB_YESNO | MB_ICONWARNING);
		cap = plat_get_string(IDS_2051);	/* "Warning" */
		break;

	case MBX_ERROR:		/* error message */
		if (flags & MBX_FATAL) {
			fl = (MB_OK | MB_ICONERROR);
			cap = plat_get_string(IDS_2049);    /* "Fatal Error"*/
		} else {
			fl = (MB_OK | MB_ICONWARNING);
			cap = plat_get_string(IDS_2048);    /* "Error" */
		}
		break;

	case MBX_QUESTION:	/* question */
		fl = (MB_YESNOCANCEL | MB_ICONQUESTION);
		cap = TEXT(EMU_NAME);
		break;

	case MBX_CONFIG:	/* configuration */
		fl = (MB_YESNO | MB_ICONERROR);
		cap = plat_get_string(IDS_2050); /* "Configuration Error" */
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
	if (((uintptr_t)arg) < ((uintptr_t)65636))
		str = plat_get_string((intptr_t)arg);
    }

    /* At any rate, we do have a valid (wide) string now. */
    fl = MessageBox(hwndMain,		/* our main window */
		    str,		/* error message etc */
		    cap,		/* window caption */
		    fl);

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


int
file_dlg(HWND hwnd, WCHAR *filt, WCHAR *ifn, int save)
{
    OPENFILENAME ofn;
    wchar_t fn[1024];
    char temp[1024];
    BOOL r;
    DWORD err; 

    /* Clear out the ("initial") pathname. */
    memset(fn, 0x00, sizeof(fn));

    /* Initialize OPENFILENAME. */
    memset(&ofn, 0x00, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    /* This is the buffer in which to place the resulting filename. */
    ofn.lpstrFile = fn;
    ofn.nMaxFile = sizeof_w(fn);

    /* Set up the "file types" filter. */
    ofn.lpstrFilter = filt;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;

    /* Tell the dialog where to go initially. */
    ofn.lpstrInitialDir = usr_path;

    /* Set up the flags for this dialog. */
    ofn.Flags = OFN_PATHMUSTEXIST;
    if (! save)
	ofn.Flags |= OFN_FILEMUSTEXIST;

    /* Display the Open dialog box. */
    if (save)
	r = GetSaveFileName(&ofn);
      else
	r = GetOpenFileName(&ofn);

    /* OK, just to make sure the dialog did not change our CWD. */
    plat_chdir(usr_path);

    if (r) {
	/* All good, see if we can make this a relative path. */
	pc_path(wopenfilestring, sizeof_w(wopenfilestring), fn);

	/* All good, create an ASCII copy of the string as well. */
	wcstombs(openfilestring, wopenfilestring, sizeof(openfilestring));

	/* Remember the file type for next time. */
	filterindex = ofn.nFilterIndex;

	return(0);
    }

    /* If an error occurred, log this. */
    err = CommDlgExtendedError();
    if (err != NO_ERROR) {
	sprintf(temp, "OpenFile(%ls, %d):\n\n    error 0x%08lx",
					usr_path, save, err);
	pclog("%s\n", temp);
	(void)ui_msgbox(MBX_ERROR|MBX_ANSI, temp);
    }

    return(1);
}


int
file_dlg_st(HWND hwnd, int id, WCHAR *fn, int save)
{
    return(file_dlg(hwnd, plat_get_string(id), fn, save));
}
