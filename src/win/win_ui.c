/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the user Interface module.
 *
 * Version:	@(#)win_ui.c	1.0.34	2019/03/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#ifdef USE_HOST_CDROM
# include <dbt.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../version.h"
#include "../config.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/video/video.h"
#include "../devices/cdrom/cdrom.h"
#include "win.h"
#include "resource.h"


#define TIMER_1SEC	1		/* ID of the one-second timer */
#define ICONS_MAX	256		/* number of icons we can cache */


/* Platform Public data, specific. */
HWND		hwndMain = NULL,	/* application main window */
		hwndRender = NULL;	/* machine render window */
HICON		hIcon[ICONS_MAX];	/* icon data loaded from resources */
RECT		oldclip;		/* mouse rect */


/* Local data. */
static wchar_t	wTitle[512];
static HHOOK	hKeyboardHook;
static LONG_PTR	input_orig_proc,
		stbar_orig_proc;
static HWND	input_orig_hwnd = NULL,
		hwndSBAR = NULL;	/* application status bar */
static HMENU	menuMain = NULL,	/* application menu bar */
		menuSBAR = NULL,	/* status bar menu bar */
		*sb_menu = NULL;
static int	sb_nparts = 0;
static const sbpart_t *sb_parts = NULL;
static int	infocus = 1;
static int	hook_enabled = 0;
static int	save_window_pos = 0;
static int	cruft_x = 0,
		cruft_y = 0,
		cruft_sb = 0;


static VOID APIENTRY
PopupMenu(HWND hwnd, POINT pt, int part)
{
    if (part >= (sb_nparts - 1)) return;

    pt.x = part * SB_ICON_WIDTH;	/* justify to the left */
    pt.y = 1;				/* justify to the top */

    ClientToScreen(hwnd, (LPPOINT)&pt);

    TrackPopupMenu(sb_menu[part],
		   TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
		   pt.x, pt.y, 0, hwndSBAR, NULL);
}


/* Handle messages for the Status Bar window. */
static WIN_RESULT CALLBACK
sb_dlg_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
		return(CallWindowProc((WNDPROC)stbar_orig_proc,
				      hwnd, message, wParam, lParam));
    }

    return(0);
}


/* Create and set up the Status Bar window. */
static void
StatusBarCreate(uintptr_t id)
{
    int borders[3];
    intptr_t i;
    int dw, dh;
    RECT r;

    /* Load our icons into the cache for faster access. */
    for (i = 0; i < ICONS_MAX; i++)
	hIcon[i] = LoadIconEx((PCTSTR)i);

    GetWindowRect(hwndMain, &r);
    dw = r.right - r.left;
    dh = r.bottom - r.top;

    /* Create the window. */
    hwndSBAR = CreateWindow(STATUSCLASSNAME,
			    NULL,
			    WS_CHILD|WS_VISIBLE|SBT_TOOLTIPS|SBARS_SIZEGRIP,
			    0, dh,
			    dw, SB_HEIGHT,
			    hwndMain,
			    (HMENU)id,
			    hInstance,
			    NULL);

    /* Retrieve the width of the border the status bar got. */
    memset(borders, 0x00, sizeof(borders));
    SendMessage(hwndSBAR, SB_GETBORDERS, 0, (LPARAM)borders);
    if (borders[1] == 0)
	borders[1] = 2;
    cruft_sb = SB_HEIGHT + (2 * borders[1]) + (2 * SB_PADDING);

    /* Replace the original procedure with ours. */
    stbar_orig_proc = GetWindowLongPtr(hwndSBAR, GWLP_WNDPROC);
    SetWindowLongPtr(hwndSBAR, GWLP_WNDPROC, (LONG_PTR)sb_dlg_proc);

    SendMessage(hwndSBAR, SB_SETMINHEIGHT, (WPARAM)SB_HEIGHT, (LPARAM)0);

    /* Load the dummy menu for this window. */
    menuSBAR = LoadMenu(hInstance, MENU_SB_NAME);

    /* Clear the menus, just in case.. */
    sb_nparts = 0;
    sb_menu = NULL;
}


/*
 * Calculate the edges of all status bar parts.
 *
 * We do it here, and in the platform module, because not all
 * systems implement them the same way Microsoft did. For that
 * reason, the UI code actually specifies widths for all the
 * parts, which we "convert" here.
 */
static void
StatusBarResize(int w)
{
    int i, *edges;
    int j, k, x;
    int grippy;

    /* If no parts yet, bail out. */
    if (sb_nparts == 0) return;

    /* Create local 'edges' array and populate it. */
    edges = (int *)mem_alloc(sb_nparts * sizeof(int));
    memset(edges, 0x00, sb_nparts * sizeof(int));

    /* If we have a grippy on the status bar... */
    //FIXME: how to determine this programmatically?  --FvK
    grippy = 8;

    /*
     * We start at the leftmost edge of the window, and
     * then move towards the right. The parameter given
     * to Windows is the *right* side edge!
     *
     * No variable-part offset yet.
     */
    k = x = 0;
    for (i = 0; i < sb_nparts; i++) {
	if (sb_parts[i].width == 0) {
		/*
		 * This is the variable-length part, which
		 * extends to the right edge of the window.
		 * So, we must get the window length, and
		 * then calculate length for this part, and
		 * then the position for all parts following.
		 *
		 * First, see how much space we need for the
		 * parts following this one.
		 */
		k = w - x - grippy;
		for (j = i + 1; j < sb_nparts; j++)
			k -= sb_parts[j].width;

		/* OK, now we know how wide this part can be. */
		edges[i] = x + k;
	} else {
		x += sb_parts[i].width;
		if (i == (sb_nparts - 1))
			x += grippy;
		edges[i] = k + x;
	}
    }

    /* Send the list to the status bar window. */
    SendMessage(hwndSBAR, SB_SETPARTS, (WPARAM)sb_nparts, (LPARAM)edges);

    free(edges);
}


static LRESULT CALLBACK
LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    BOOL bControlKeyDown;
    KBDLLHOOKSTRUCT *p;

    if (nCode < 0 || nCode != HC_ACTION || (!mouse_capture && !vid_fullscreen))
	return(CallNextHookEx(hKeyboardHook, nCode, wParam, lParam));
	
    p = (KBDLLHOOKSTRUCT*)lParam;

    /* disable alt-tab */
    if (p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN) return(1);

    /* disable alt-space */
    if (p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN) return(1);

    /* disable alt-escape */
    if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN) return(1);

    /* disable windows keys */
    if((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) return(1);

    /* checks ctrl key pressed */
    bControlKeyDown = GetAsyncKeyState(VK_CONTROL)>>((sizeof(SHORT)*8)-1);

    /* disable ctrl-escape */
    if (p->vkCode == VK_ESCAPE && bControlKeyDown) return(1);

    return(CallNextHookEx(hKeyboardHook, nCode, wParam, lParam));
}


#ifdef USE_HOST_CDROM
static void
HandleMediaEvent(WPARAM wParam, LPARAM lParam)
{
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
    PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
    int f = -1, i;
    char d;

    switch (wParam) {
	case DBT_DEVICEARRIVAL:
		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME) {
			if (lpdbv->dbcv_flags & DBTF_MEDIA)
				f = 1;
		}
		break;

	case DBT_DEVICEREMOVECOMPLETE:
		if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME) {
			if (lpdbv->dbcv_flags & DBTF_MEDIA)
				f = 0;
		}
		break;

	default:
		/* We don't care.. */
		break;
    }

    /* Now report all 'changed' drives to the CD-ROM handler. */
    if (f != -1) {
	for (i = 0; i < 26; i++) {
		if (lpdbv->dbcv_unitmask & 1) {
			/* Get the drive letter. */
			d = 'A' + i;

			cdrom_notify(&d, f);
		}
		lpdbv->dbcv_unitmask >>= 1;
	}
    }
}
#endif


static LRESULT CALLBACK
MainWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DWORD flags;
    RECT rect;
    int x, y;
    int idm;

    switch (message) {
	case WM_CREATE:
		SetTimer(hwnd, TIMER_1SEC, 1000, NULL);
		hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL,
						 LowLevelKeyboardProc,
						 GetModuleHandle(NULL), 0);
		hook_enabled = 1;
		break;

	case WM_COMMAND:
		UpdateWindow(hwnd);
		idm = LOWORD(wParam);

		/* Let the general UI handle it first, and then we do. */
		if (! ui_menu_command(idm)) switch(idm) {
			case IDM_EXIT:
				PostQuitMessage(0);
				break;

			case IDM_RESIZE:
				/* Set up for resizing if configured. */
				flags = WS_OVERLAPPEDWINDOW;
				if (! vid_resize)
					flags &= ~(WS_SIZEBOX | WS_THICKFRAME |
						   WS_MAXIMIZEBOX);
				SetWindowLongPtr(hwnd, GWL_STYLE, flags);

				/* Main Window. */
				GetWindowRect(hwnd, &rect);
				get_screen_size_natural(&x, &y);
                        	MoveWindow(hwnd, rect.left, rect.top,
					   x + cruft_x, y + cruft_y + cruft_sb,
					   TRUE);

				break;

			case IDM_REMEMBER:
				GetWindowRect(hwnd, &rect);
				if (window_remember) {
					window_x = rect.left;
					window_y = rect.top;
					window_w = rect.right - rect.left;
					window_h = rect.bottom - rect.top;
				}
				config_save();
				break;
		}
		return(0);

	case WM_ENTERMENULOOP:
		break;

	case WM_SIZE:
		/* Note: this is the *client area* size!! */
		x = (int)(lParam & 0xffff);
		y = (int)(lParam >> 16);
		if (cruft_x == 0) {
			/* Determine the window cruft. */
			cruft_x = (scrnsz_x - x);
			cruft_y = (scrnsz_y - y);

			/* Update window size with cruft. */
			x += cruft_x;
			y += (cruft_y + cruft_sb);
		}
		y -= cruft_sb;

		/* Request a re-size if needed. */
		if ((x != scrnsz_x) || (y != scrnsz_y)) doresize = 1;

		/* Set the new panel size. */
 		scrnsz_x = x;
		scrnsz_y = y;

		/* Update the render window. */
		if (hwndRender != NULL)
			MoveWindow(hwndRender, 0, 0, scrnsz_x, scrnsz_y, TRUE);

		/* Update the Status bar. */
		MoveWindow(hwndSBAR, 0, scrnsz_y, scrnsz_x, cruft_sb, TRUE);
		StatusBarResize(scrnsz_x);

		/* Update the renderer if needed. */
		vidapi_resize(scrnsz_x, scrnsz_y);

		/* Re-clip the mouse area if needed. */
		if (mouse_capture) {
			GetWindowRect(hwndRender, &rect);

			ClipCursor(&rect);
		}

		if (window_remember) {
			GetWindowRect(hwndMain, &rect);

			window_x = rect.left;
			window_y = rect.top;
			window_w = rect.right - rect.left;
			window_h = rect.bottom - rect.top;
			save_window_pos = 1;

			config_save();
		}
		break;

	case WM_MOVE:
		/*
		 * If window is not resizable, then tell the main thread			 * to resize it, as sometimes, moves can mess up the window
		 * size.
		 */
		if (! vid_resize)
			doresize = 1;

		if (window_remember) {
			GetWindowRect(hwndMain, &rect);

			window_x = rect.left;
			window_y = rect.top;
			window_w = rect.right - rect.left;
			window_h = rect.bottom - rect.top;
			save_window_pos = 1;
		}
		break;
                
	case WM_TIMER:
		if (wParam == TIMER_1SEC)
			pc_onesec();
		break;

	case WM_RESET_D3D:
		plat_startblit();
		vidapi_reset();
		plat_endblit();
		break;

	case WM_LEAVE_FS:
		ui_fullscreen(0);
		config_save();
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		return(0);

	case WM_DESTROY:
		UnhookWindowsHookEx(hKeyboardHook);
		KillTimer(hwnd, TIMER_1SEC);
		PostQuitMessage(0);
		break;

	case WM_SHOW_CFG:
		pc_pause(1);
		if (dlg_settings(1) == 2)
			pc_reset_hard_init();
		pc_pause(0);
		break;

	case WM_PAUSE:
		pc_pause(dopause ^ 1);
		menu_set_item(IDM_PAUSE, dopause);
		break;

	case WM_HARD_RESET:
		pc_reset(1);
		break;

	case WM_CTRLALTDEL:
		pc_reset(0);
		break;

	case WM_SHUTDOWN:
		PostQuitMessage(0);
		break;

#ifdef USE_HOST_CDROM
	case WM_DEVICECHANGE:
		HandleMediaEvent(wParam, lParam);
		break;
#endif

	case WM_SYSCOMMAND:
		/*
		 * Disable ALT key *ALWAYS*,
		 * I don't think there's any use for
		 * reaching the menu that way.
		 */
		if (wParam == SC_KEYMENU && HIWORD(lParam) <= 0) {
			return 0; /*disable ALT key for menu*/
		}

	default:
		return(DefWindowProc(hwnd, message, wParam, lParam));
    }

    return(FALSE);
}


/* Dummy window procedure, used by D3D when in full-screen mode. */
static LRESULT CALLBACK
SubWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return(DefWindowProc(hwnd, message, wParam, lParam));
}


/* Catch WM_INPUT messages for 'current focus' window. */
static WIN_RESULT CALLBACK
input_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
	case WM_INPUT:
		keyboard_handle(lParam, infocus);
		break;

	case WM_SETFOCUS:
		infocus = 1;
		if (! hook_enabled) {
			hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL,
							 LowLevelKeyboardProc,
							 GetModuleHandle(NULL),
							 0);
			hook_enabled = 1;
		}
		break;

	case WM_KILLFOCUS:
		infocus = 0;
		ui_mouse_capture(0);
		if (hook_enabled) {
			UnhookWindowsHookEx(hKeyboardHook);
			hook_enabled = 0;
		}
		break;

	case WM_LBUTTONUP:
		if (! vid_fullscreen)
			ui_mouse_capture(1);
		break;

	case WM_MBUTTONUP:
		if (mouse_get_buttons() < 3)
			ui_mouse_capture(0);
		break;

	default:
		return(CallWindowProc((WNDPROC)input_orig_proc,
				      hwnd, message, wParam, lParam));
    }

    return(0);
}


/* Set up a handler for the 'currently active' window. */
void
plat_set_input(HWND h)
{
    /* If needed, rest the old one first. */
    if (input_orig_hwnd != NULL) {
	SetWindowLongPtr(input_orig_hwnd, GWLP_WNDPROC,
			 (LONG_PTR)input_orig_proc);
    }

    /* Redirect the window procedure so we can catch WM_INPUT. */
    input_orig_proc = GetWindowLongPtr(h, GWLP_WNDPROC);
    input_orig_hwnd = h;
    SetWindowLongPtr(h, GWLP_WNDPROC, (LONG_PTR)input_proc);
}


/* UI: reset the lowlevel (platform) UI. */
void
ui_plat_reset(void)
{
    /* Load the main menu from the appropriate DLL. */
    menuMain = LoadMenu(plat_lang_dll(), MENU_MAIN_NAME);

    SetMenu(hwndMain, menuMain);

    /* Load the statusbar menu. */
    menuSBAR = LoadMenu(hInstance, MENU_SB_NAME);
}


/* UI: initialize the Win32 User Interface module. */
int
ui_init(int nCmdShow)
{
    WCHAR title[200];
    WNDCLASSEX wincl;			/* buffer for main window's class */
    INITCOMMONCONTROLSEX icex;		/* common controls, new style */
    RAWINPUTDEVICE ridev;		/* RawInput device */
    MSG messages;			/* received-messages buffer */
    HACCEL haccel;			/* handle to accelerator table */
    DWORD flags;
    int ret;

    /* Register the new version of the Common Controls. */
    memset(&icex, 0x00, sizeof(INITCOMMONCONTROLSEX));
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    if (settings_only) {
	if (! pc_init()) {
		/* Dang, no ROMs found at all! */
		ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NOROMS);
		return(6);
	}

	(void)dlg_settings(0);

	return(0);
    }

    /* Create our main window's class and register it. */
    wincl.hInstance = hInstance;
    wincl.lpszClassName = CLASS_NAME;
    wincl.lpfnWndProc = MainWindowProcedure;
    wincl.style = CS_DBLCLKS;		/* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = LoadIcon(hInstance, (LPCTSTR)ICON_MAIN);
    wincl.hIconSm = LoadIcon(hInstance, (LPCTSTR)ICON_MAIN);
    wincl.hCursor = NULL;
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = CreateSolidBrush(RGB(0,0,0));
    if (! RegisterClassEx(&wincl))
			return(2);

    /* Register a class for the Fullscreen renderer window. */
    wincl.lpszClassName = FS_CLASS_NAME;
    wincl.lpfnWndProc = SubWindowProcedure;
    if (! RegisterClassEx(&wincl))
			return(2);

    /* Set up main window for resizing if configured. */
    flags = WS_OVERLAPPEDWINDOW;
    if (! vid_resize)
	flags &= ~(WS_SIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX);

    /*
     * Create our main window.
     *
     * We want our (initial) render panel to be a certain
     * size (default 640x480), and we have no idea what
     * our window decorations are, size-wise.
     *
     * Rather than depending on the GetSystemMetrics API
     * calls (which return incorrect data as of Vista+),
     * the simplest trick is to just set the desired window
     * size, and create the window.  The first WM_SIZE
     * message will indicate the client size left after
     * creating all the decorations, and the difference
     * is the decoration overhead ('cruft') we need to
     * always keep in mind when changing the window size.
     */
    swprintf(title, sizeof_w(title), L"%s %s", EMU_NAME, emu_version);
    hwndMain = CreateWindowEx(
#if 0
		WS_EX_LAYOUTRTL | WS_EX_RIGHT,
#else
		0,
#endif
		CLASS_NAME,			/* class name */
		title,				/* Title Text */
		flags,				/* style flags */
		CW_USEDEFAULT, CW_USEDEFAULT,	/* no preset position */
		scrnsz_x, scrnsz_y,		/* window size in pixels */
		HWND_DESKTOP,			/* child of desktop */
		NULL,				/* menu */
		hInstance,			/* Program Instance handler */
		NULL);				/* no Window Creation data */

    /* Create the status bar window. */
    StatusBarCreate(IDC_STATBAR);

    ui_window_title(title);

    /* Reset all menus to their defaults. */
    ui_reset();

    /* Move to the last-saved position if needed. */
    if (window_remember)
	MoveWindow(hwndMain, window_x, window_y, window_w, window_h, FALSE);

    /* Load the accelerator table */
    haccel = LoadAccelerators(hInstance, ACCEL_NAME);
    if (haccel == NULL) {
	ui_msgbox(MBX_CONFIG, (wchar_t *)IDS_ERR_ACCEL);
	return(3);
    }

    /* Make the window visible on the screen. */
    ShowWindow(hwndMain, nCmdShow);

    /* Initialize the RawInput (keyboard) module. */
    memset(&ridev, 0x00, sizeof(ridev));
    ridev.usUsagePage = 0x01;
    ridev.usUsage = 0x06;
    ridev.dwFlags = RIDEV_NOHOTKEYS;
    ridev.hwndTarget = NULL;	/* current focus window */
    if (! RegisterRawInputDevices(&ridev, 1, sizeof(ridev))) {
	ui_msgbox(MBX_CONFIG, (wchar_t *)IDS_ERR_INPUT);
	return(4);
    }
    keyboard_getkeymap();

    /* Set up the main window for RawInput. */
    plat_set_input(hwndMain);

    /* Create the Machine Rendering window. */
    hwndRender = CreateWindow(L"STATIC",
			      NULL,
			      WS_CHILD|SS_BITMAP,
			      0, 0,
			      scrnsz_x, scrnsz_y,
			      hwndMain,
			      NULL,
			      hInstance,
			      NULL);

    /* That looks good, now continue setting up the machine. */
    switch (pc_init()) {
	case -1:	/* General failure during init, give up. */
		return(6);

	case 0:		/* Configuration error, user wants to exit. */
		return(0);

	case 1:		/* All good. */
		break;

	case 2:		/* Configuration error, user wants to re-config. */
		if (dlg_settings(0)) {
			/* Save the new configuration to file. */
			config_save();

			/* Remind them to restart. */
			ui_msgbox(MBX_INFO, (wchar_t *)IDS_MSG_RESTART);
		}
		return(0);
    }

    /* Activate the render window, this will also set the screen size. */
    if (hwndRender != NULL)
	MoveWindow(hwndRender, 0, 0, scrnsz_x, scrnsz_y, TRUE);

    /* Initialize the configured Video API. */
again:
    if (! vidapi_set(vid_api)) {
	/*
	 * Selected renderer is not available.
	 *
	 * This can happen if one of the optional renderers
	 * was selected previously, but is currently not
	 * available for whatever reason.
	 *
	 * Inform the user, and ask if they want to reset
	 * to the system default one instead.
	 */
	swprintf(title, sizeof_w(title),
		 get_string(IDS_ERR_NORENDR),
		 vidapi_get_internal_name(vid_api));
	if (ui_msgbox(MBX_CONFIG, title) != 0) {
		/* Nope, they don't, so just exit. */
		return(5);
	}

	/* OK, reset to the default one and retry. */
	vid_api = vidapi_from_internal_name("default");
	goto again;
    }

    /* Initialize the rendering window, or fullscreen. */
    if (start_in_fullscreen)
	ui_fullscreen(1);

    /* Initialize the mouse module. */
    win_mouse_init();

    /* Fire up the machine. */
    pc_reset_hard();

    /* Set the PAUSE mode depending on the renderer. */
    pc_pause(0);

#ifdef USE_MANAGER
    /*
     * If so requested via the command line, inform the
     * application that started us of our HWND, using the
     * the hWnd and unique ID the application has given
     * us.
     */
    if (source_hwnd)
	PostMessage((HWND) (uintptr_t) source_hwnd,
		    WM_SEND_HWND, (WPARAM)unique_id, (LPARAM)hwndMain);
#endif

    /*
     * Everything has been configured, and all seems to work,
     * so now it is time to start the main thread to do some
     * real work, and we will hang in here, dealing with the
     * UI until we're done.
     */
    plat_start();

    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (! quited) {
	ret = GetMessage(&messages, NULL, 0, 0);
	if ((ret == 0) || quited) break;

	if (ret == -1) {
		fatal("ret is -1\n");
	}

	if (messages.message == WM_QUIT) {
		quited = 1;
		break;
	}

	if (! TranslateAccelerator(hwndMain, haccel, &messages)) {
                TranslateMessage(&messages);
                DispatchMessage(&messages);
	}

	if (mouse_capture && keyboard_ismsexit()) {
		/* Release the in-app mouse. */
		ui_mouse_capture(0);
        }

	if (vid_fullscreen && keyboard_isfsexit()) {
		/* Signal "exit fullscreen mode". */
		/* INFO("leave full screen though key combination\n"); */
		ui_fullscreen(0);
	}
    }

    timeEndPeriod(1);

    if (mouse_capture)
	ui_mouse_capture(0);

    /* Close down the emulator. */
    plat_stop();

    UnregisterClass(CLASS_NAME, hInstance);
    UnregisterClass(FS_CLASS_NAME, hInstance);

    win_mouse_close();

    return((int)messages.wParam);
}


/* UI support: tell the UI about a new screen resolution. */
void
ui_resize(int x, int y)
{
    RECT r;

    /* First, see if we should resize the UI window. */
    if (vid_resize) return;

    video_blit_wait();

    /* Re-position and re-size the main window. */
    GetWindowRect(hwndMain, &r);
    MoveWindow(hwndMain, r.left, r.top,
	       x+cruft_x, y+cruft_y+cruft_sb, TRUE);
}


/* UI support: update the application's title bar. */
wchar_t *
ui_window_title(const wchar_t *s)
{
    if (! vid_fullscreen) {
	if (s != NULL)
		wcscpy(wTitle, s);
	  else
		s = wTitle;

       	SetWindowText(hwndMain, s);
    } else {
	if (s == NULL)
		s = wTitle;
    }

    return((wchar_t *)s);
}


/* UI support: set cursor visible or not. */
void
ui_show_cursor(int val)
{
    static int vis = -1;

    if (val == vis)
	return;

    if (val == 0) {
    	while (1) {
		if (ShowCursor(FALSE) < 0) break;
	}
    } else {
	ShowCursor(TRUE);
    }

    vis = val;
}


/* UI support: show or hide the render window. */
void
ui_show_render(int on)
{
#ifndef USE_WX
    if (on)
	ShowWindow(hwndRender, SW_SHOW);
      else
	ShowWindow(hwndRender, SW_HIDE);
#endif
}


/* Set the desired fullscreen/windowed mode. */
void
plat_fullscreen(int on)
{
    win_mouse_close();

#ifdef USE_WX
    wx_set_fullscreen(on);
#endif

    win_mouse_init();
}


/* Platform support for the PAUSE action. */
void
plat_pause(int paused)
{
#ifdef USE_MANAGER
    /* Send the WM to a manager if needed. */
    if (source_hwnd)
	PostMessage((HWND) (uintptr_t) source_hwnd,
		    WM_SENDSTATUS, (WPARAM)paused, (LPARAM)hwndMain);
#endif
}


/* Grab the current keyboard state. */
int
plat_kbd_state(void)
{
    BYTE kbdata[256];
    int ret = 0x00;

    /* Grab the system keyboard state. */
    memset(kbdata, 0x00, sizeof(kbdata));
    GetKeyboardState(kbdata);

    /* Pick out the keys we are interested in. */
    if (kbdata[VK_NUMLOCK]) ret |= KBD_FLAG_NUM;
    if (kbdata[VK_CAPITAL]) ret |= KBD_FLAG_CAPS;
    if (kbdata[VK_SCROLL]) ret |= KBD_FLAG_SCROLL;
    if (kbdata[VK_PAUSE]) ret |= KBD_FLAG_PAUSE;

    return(ret);
}


/* UI support: enable or disable mouse clipping. */
void
plat_mouse_capture(int on)
{
    RECT rect;

    if ((on == -1) || !on) {
	/* Disable the in-app mouse. */
	if (on == -1)
		GetClipCursor(&oldclip);
	  else
		ClipCursor(&oldclip);
    } else if (on && !mouse_capture) {
	/* Enable the in-app mouse. */
	GetClipCursor(&oldclip);
	GetWindowRect(hwndRender, &rect);
	ClipCursor(&rect);
    }
}


/* UI support: add an item to a menu. */
void
menu_add_item(int idm, int type, int new_id, const wchar_t *str)
{
    MENUITEMINFO info;
    HMENU menu;

    /* Get the handle for the intended (sub)menu. */
    memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_SUBMENU;
    if (! GetMenuItemInfo(menuMain, idm, FALSE, &info)) {
	ERRLOG("UI: cannot find submenu %d\n", idm);
	return;
    }
    menu = info.hSubMenu;

    switch(type) {
	case ITEM_SEPARATOR:
		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		break;

	default:
		AppendMenu(menu, MF_STRING, new_id, str);
    }

    /* We changed the menu bar, so redraw it. */
    DrawMenuBar(hwndMain);
}


/* UI support: enable or disable a menu item. */
void
menu_enable_item(int idm, int val)
{
    EnableMenuItem(menuMain, idm, (val) ? MF_ENABLED : MF_DISABLED);
}


/* UI support: set/check a menu item. */
void
menu_set_item(int idm, int val)
{
    CheckMenuItem(menuMain, idm, val ? MF_CHECKED : MF_UNCHECKED);
}


/* UI support: set a radio group menu item. */
void
menu_set_radio_item(int idm, int num, int val)
{
    int i;

    if (val < 0) return;

    for (i = 0; i < num; i++)
	menu_set_item(idm + i, 0);

    menu_set_item(idm + val, 1);
}


/* UI support: delete all menus from the status bar. */
static void
menu_destroy(void)
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


/* UI support: reset the status bar. */
void
sb_setup(int nparts, const sbpart_t *data)
{
    RECT r;

    /* Set the info. */
    sb_nparts = nparts;
    sb_parts = data;

    if (sb_menu != NULL) {
	menu_destroy();
    }

    /* First, get width of the status bar window. */
    if (nparts > 0) {
	/* Calculate and set up the parts. */
	GetWindowRect(hwndSBAR, &r);
	StatusBarResize(r.right - r.left);

	/* Allocate and clear a new menu. */
	sb_menu = (HMENU *)mem_alloc(nparts * sizeof(HMENU));
	memset(sb_menu, 0x00, nparts * sizeof(HMENU));
    }
}


/* UI support: set the icon ID for a status bar field. */
void
sb_set_icon(int part, int icon)
{
    HANDLE ptr;

    if (icon == 255) ptr = NULL;
      else ptr = hIcon[(intptr_t)icon];

    SendMessage(hwndSBAR, SB_SETICON, part, (LPARAM)ptr);
}


/* UI support: set a text label for a status bar field. */
void
sb_set_text(int part, const wchar_t *str)
{
    int flags;

//    flags = (part  < (sb_nparts - 2)) ? SBT_NOBORDERS : SBT_POPOUT;
    flags = (part  < (sb_nparts - 2)) ? SBT_NOBORDERS : 0;

    SendMessage(hwndSBAR, SB_SETTEXT, part | flags, (LPARAM)str);
}


/* UI support: set a tooltip for a status bar field. */
void
sb_set_tooltip(int part, const wchar_t *str)
{
    SendMessage(hwndSBAR, SB_SETTIPTEXT, part, (LPARAM)str);
}


/* UI support: create a menu for a status bar part. */
void
sb_menu_create(int part)
{
    HMENU h;

    h = CreatePopupMenu();

    sb_menu[part] = h;
}


/* UI support: add an item to a (status bar) menu. */
void
sb_menu_add_item(int part, int idm, const wchar_t *str)
{
    if (idm >= 0)
	AppendMenu(sb_menu[part], MF_STRING, idm, str);
      else
	AppendMenu(sb_menu[part], MF_SEPARATOR, 0, NULL);
}


/* UI support: enable or disable a status bar menu item. */
void
sb_menu_enable_item(int part, int idm, int val)
{
    EnableMenuItem(sb_menu[part], idm,
	val ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
}


/* UI support: set or reset a status bar menu item. */
void
sb_menu_set_item(int part, int idm, int val)
{
    CheckMenuItem(sb_menu[part], idm, val ? MF_CHECKED : MF_UNCHECKED);
}
