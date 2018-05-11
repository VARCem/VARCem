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
 * Version:	@(#)win_ui.c	1.0.23	2018/05/10
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
#include "../version.h"
#include "../config.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/video/video.h"
#include "win.h"


#ifndef GWL_WNDPROC
# define GWL_WNDPROC	GWLP_WNDPROC
#endif


#define TIMER_1SEC	1		/* ID of the one-second timer */


/* Platform Public data, specific. */
HWND		hwndMain = NULL,	/* application main window */
		hwndRender = NULL;	/* machine render window */
HICON		hIcon[512];		/* icon data loaded from resources */
RECT		oldclip;		/* mouse rect */
int		infocus = 1;


/* Local data. */
static wchar_t	wTitle[512];
static HHOOK	hKeyboardHook;
static LONG_PTR	OriginalProcedure;
static HWND	hwndSBAR = NULL;	/* application status bar */
static HMENU	menuMain = NULL,	/* application menu bar */
		menuSBAR = NULL,	/* status bar menu bar */
		*sb_menu = NULL;
static int	sb_nparts;
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
#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
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
		return(CallWindowProc((WNDPROC)OriginalProcedure,
				      hwnd, message, wParam, lParam));
    }

    return(0);
}


/* Create and set up the Status Bar window. */
void
StatusBarCreate(uintptr_t id)
{
    int borders[3];
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

    GetWindowRect(hwndMain, &r);
    dw = r.right - r.left;
    dh = r.bottom - r.top;

    /* Load the Common Controls DLL if needed. */
    InitCommonControls();

    /* Create the window, and make sure it's using the STATUS class. */
    hwndSBAR = CreateWindow(STATUSCLASSNAME, 
			    NULL,
			    SBARS_SIZEGRIP|WS_CHILD|WS_VISIBLE|SBT_TOOLTIPS,
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
    OriginalProcedure = GetWindowLongPtr(hwndSBAR, GWLP_WNDPROC);
    SetWindowLongPtr(hwndSBAR, GWL_WNDPROC, (LONG_PTR)sb_dlg_proc);

    SendMessage(hwndSBAR, SB_SETMINHEIGHT, (WPARAM)SB_HEIGHT, (LPARAM)0);

    /* Load the dummy menu for this window. */
    menuSBAR = LoadMenu(hInstance, SB_MENU_NAME);

    /* Clear the menus, just in case.. */
    sb_nparts = 0;
    sb_menu = NULL;
}


static LRESULT CALLBACK
LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    BOOL bControlKeyDown;
    KBDLLHOOKSTRUCT *p;

    if (nCode < 0 || nCode != HC_ACTION)
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
                        	MoveWindow(hwnd, rect.left, rect.top,
					unscaled_size_x + cruft_x,
					unscaled_size_y + cruft_y + cruft_sb,
					TRUE);

#if 0
				/* Render window. */
				MoveWindow(hwndRender, 0, 0,
					   unscaled_size_x,
					   unscaled_size_y,
					   TRUE);

				/* Status bar. */
				GetWindowRect(hwndRender, &rect);
				MoveWindow(hwndSBAR,

				if (mouse_capture) {
					GetWindowRect(hwndRender, &rect);

					ClipCursor(&rect);
				}
#endif
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

	case WM_LBUTTONUP:
		if (! vid_fullscreen)
			plat_mouse_capture(1);
		break;

	case WM_MBUTTONUP:
		if (mouse_get_buttons() < 3)
			plat_mouse_capture(0);
		break;

	case WM_ENTERMENULOOP:
		break;

	case WM_SIZE:
		/* Note: this is the *client area* size!! */
		x = (lParam & 0xffff);
		y = (lParam >> 16);
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
		}

		config_save();
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

	case WM_RESETD3D:
		plat_startblit();
		vidapi_reset();
		plat_endblit();
		break;

	case WM_LEAVEFULLSCREEN:
		plat_setfullscreen(0);
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

	case WM_SHOWSETTINGS:
		plat_pause(1);
		if (dlg_settings(1) == 2)
			pc_reset_hard_init();
		plat_pause(0);
		break;

	case WM_PAUSE:
		plat_pause(dopause ^ 1);
		menu_set_item(IDM_PAUSE, dopause);
		break;

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


/* Initialize the Win32 User Interface module. */
int
ui_init(int nCmdShow)
{
    WCHAR title[200];
    WNDCLASSEX wincl;			/* buffer for main window's class */
    RAWINPUTDEVICE ridev;		/* RawInput device */
    MSG messages;			/* received-messages buffer */
    HACCEL haccel;			/* handle to accelerator table */
    DWORD flags;
    int ret;

#if 0
    /* We should have an application-wide at_exit catcher. */
    atexit(plat_mouse_capture);
#endif

    if (settings_only) {
	if (! pc_init()) {
		/* Dang, no ROMs found at all! */
		ui_msgbox(MBX_ERROR, (wchar_t *)IDS_2056);
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
    wincl.hIcon = LoadIcon(hInstance, (LPCTSTR)100);
    wincl.hIconSm = LoadIcon(hInstance, (LPCTSTR)100);
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

    /* Load the Window Menu(s) from the resources. */
    menuMain = LoadMenu(hInstance, MENU_NAME);

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
     * Rather than depending on the GetSYstemMetrics API
     * calls (which return incorrect data of Vista+), the
     * simplest trick is to just set the desired window
     * size, and create the window.  The first WM_SIZE
     * message will indicate the client size left after
     * creating all the decorations, and the difference
     * is the decoration overhead ('cruft') we need to
     * always keep in mind when changing the window size.
     */
    wsprintf(title, L"%S", emu_version, sizeof_w(title));
    hwndMain = CreateWindow(
		CLASS_NAME,			/* class name */
		title,				/* Title Text */
		flags,				/* style flags */
		CW_USEDEFAULT, CW_USEDEFAULT,	/* no preset position */
		scrnsz_x, scrnsz_y,		/* window size in pixels */
		HWND_DESKTOP,			/* child of desktop */
		menuMain,			/* menu */
		hInstance,			/* Program Instance handler */
		NULL);				/* no Window Creation data */

    ui_window_title(title);

    /* Move to the last-saved position if needed. */
    if (window_remember)
	MoveWindow(hwndMain, window_x, window_y, window_w, window_h, FALSE);

    /* Reset all menus to their defaults. */
    ui_menu_reset_all();

    /* Load the accelerator table */
    haccel = LoadAccelerators(hInstance, ACCEL_NAME);
    if (haccel == NULL) {
	ui_msgbox(MBX_CONFIG, (wchar_t *)IDS_2153);
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
	ui_msgbox(MBX_CONFIG, (wchar_t *)IDS_2154);
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
			ui_msgbox(MBX_INFO, (wchar_t *)IDS_2062);
		}
		return(0);
    }

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
	_swprintf(title, get_string(IDS_2095), vidapi_internal_name(vid_api));
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
	plat_setfullscreen(1);

    /* Activate the render window, this will also set the screen size. */
    if (hwndRender != NULL)
	MoveWindow(hwndRender, 0, 0, scrnsz_x, scrnsz_y, TRUE);

    /* Create the status bar window. */
    StatusBarCreate(IDC_STATBAR);

    /* Initialize the mouse module. */
    win_mouse_init();

    /* Fire up the machine. */
    pc_reset_hard();

    /* Set the PAUSE mode depending on the renderer. */
    plat_pause(0);

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
		plat_mouse_capture(0);
        }

	if (vid_fullscreen && keyboard_isfsexit()) {
		/* Signal "exit fullscreen mode". */
		/* pclog("leave full screen though key combination\n"); */
		plat_setfullscreen(0);
	}
    }

    timeEndPeriod(1);

    if (mouse_capture)
	plat_mouse_capture(0);

    /* Close down the emulator. */
    plat_stop();

    UnregisterClass(CLASS_NAME, hInstance);
    UnregisterClass(FS_CLASS_NAME, hInstance);

    win_mouse_close();

    return(messages.wParam);
}


/* Catch WM_INPUT messages for 'current focus' window. */
static LONG_PTR	input_orig_proc;
static HWND input_orig_hwnd = NULL;
#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
input_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
	case WM_INPUT:
pclog("UI: hwnd=%08lx WM_INPUT (infocus=%d) !\n", hwnd, infocus);
		keyboard_handle(lParam, infocus);
		break;

	case WM_SETFOCUS:
pclog("UI: hwnd=%08lx WM_SETFOCUS (infocus=%d) !\n", hwnd, infocus);
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
pclog("UI: hwnd=%08lx WM_KILLFOCUS (infocus=%d) !\n", hwnd, infocus);
		infocus = 0;
		plat_mouse_capture(0);
		if (hook_enabled) {
			UnhookWindowsHookEx(hKeyboardHook);
			hook_enabled = 0;
		}
		break;

	default:
		return(CallWindowProc((WNDPROC)input_orig_proc,
				      hwnd, message, wParam, lParam));
    }

    return(0);
}


void
plat_set_input(HWND h)
{
    /* If needed, rest the old one first. */
    if (input_orig_hwnd != NULL) {
	SetWindowLongPtr(input_orig_hwnd, GWL_WNDPROC,
			 (LONG_PTR)input_orig_proc);
    }

    /* Redirect the window procedure so we can catch WM_INPUT. */
    input_orig_proc = GetWindowLongPtr(h, GWLP_WNDPROC);
    input_orig_hwnd = h;
    SetWindowLongPtr(h, GWL_WNDPROC, (LONG_PTR)input_proc);
}


/* Tell the UI about a new screen resolution. */
void
ui_resize(int x, int y)
{
    RECT r;

    /* First, see if we should resize the UI window. */
    if (vid_resize) return;

    video_wait_for_blit();

    /* Re-position and re-size the main window. */
    GetWindowRect(hwndMain, &r);
    MoveWindow(hwndMain, r.left, r.top,
	       x+cruft_x, y+cruft_y+cruft_sb, TRUE);
}


/*
 * Re-load and reset all menus.
 *
 * We should have the language ID as a parameter.
 */
void
ui_menu_update(void)
{
    menuMain = LoadMenu(hInstance, MENU_NAME);
    SetMenu(hwndMain, menuMain);

    menuSBAR = LoadMenu(hInstance, SB_MENU_NAME);
    ui_menu_reset_all();
}


/* Update the application's title bar. */
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


/* Set cursor visible or not. */
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


/* Show or hide the render window. */
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
plat_setfullscreen(int on)
{
    /* Want off and already off? */
    if (!on && !vid_fullscreen) return;

    /* Want on and already on? */
    if (on && vid_fullscreen) return;

    if (on && vid_fullscreen_first) {
	vid_fullscreen_first = 0;
	ui_msgbox(MBX_INFO, (wchar_t *)IDS_2107);
    }

    /* OK, claim the video. */
    plat_startblit();
    video_wait_for_blit();

    win_mouse_close();

    /* Close the current mode, and open the new one. */
    plat_vidapis[vid_api]->close();
    vid_fullscreen = on;
    plat_vidapis[vid_api]->init(vid_fullscreen);

#ifdef USE_WX
    wx_set_fullscreen(on);
#endif

    win_mouse_init();

    /* Release video and make it redraw the screen. */
    plat_endblit();
    device_force_redraw();

    /* Finally, handle the host's mouse cursor. */
    ui_show_cursor(vid_fullscreen ? 0 : -1);
}


/* Pause or unpause the emulator. */
void
plat_pause(int p)
{
    static wchar_t oldtitle[512];
    wchar_t title[512];

    /* If un-pausing, as the renderer if that's OK. */
    if (p == 0)
	p = vidapi_pause();

    /* If already so, done. */
    if (dopause == p) return;

    if (p) {
	wcscpy(oldtitle, ui_window_title(NULL));
	wcscpy(title, oldtitle);
	wcscat(title, L" - PAUSED -");
	ui_window_title(title);
    } else {
	ui_window_title(oldtitle);
    }

    dopause = p;

    /* Update the actual menu item. */
    menu_set_item(IDM_PAUSE, dopause);
}


/* Enable or disable mouse clipping. */
void
plat_mouse_capture(int on)
{
    RECT rect;

    /* Do not try to capture the mouse if no mouse configured. */
    if (mouse_type == MOUSE_NONE) return;

    if (on && !mouse_capture) {
	/* Enable the in-app mouse. */
	GetClipCursor(&oldclip);
	GetWindowRect(hwndRender, &rect);
	ClipCursor(&rect);

	ui_show_cursor(0);

	mouse_capture = 1;
    } else if (!on && mouse_capture) {
	/* Disable the in-app mouse. */
	ClipCursor(&oldclip);

	ui_show_cursor(-1);

	mouse_capture = 0;
    }
}


/* Enable or disable a menu item. */
void
menu_enable_item(int idm, int val)
{
    EnableMenuItem(menuMain, idm, (val) ? MF_ENABLED : MF_DISABLED);
}


/* Set (check) or clear (uncheck) a menu item. */
void
menu_set_item(int idm, int val)
{
    CheckMenuItem(menuMain, idm, val ? MF_CHECKED : MF_UNCHECKED);
}


/* Set a radio group menu item. */
void
menu_set_radio_item(int idm, int num, int val)
{
    int i;

    if (val < 0) return;

    for (i = 0; i < num; i++)
	menu_set_item(idm + i, 0);

    menu_set_item(idm + val, 1);
}


/* Initialize the status bar. */
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


/* Delete all menus from the status bar. */
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


/* Enable or disable a status bar menu item. */
void
sb_menu_enable_item(int part, int idm, int val)
{
    EnableMenuItem(sb_menu[part], idm,
	val ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
}


/* Set or reset a status bar menu item. */
void
sb_menu_set_item(int part, int idm, int val)
{
    CheckMenuItem(sb_menu[part], idm, val ? MF_CHECKED : MF_UNCHECKED);
}


/* Set the icon ID for a status bar field. */
void
sb_set_icon(int part, int icon)
{
    HANDLE ptr;

    if (icon == -1) ptr = NULL;
      else ptr = hIcon[(intptr_t)icon];

    SendMessage(hwndSBAR, SB_SETICON, part, (LPARAM)ptr);
}


/* Set a text label for a status bar field. */
void
sb_set_text(int part, const wchar_t *str)
{
    SendMessage(hwndSBAR, SB_SETTEXT, part | SBT_NOBORDERS, (LPARAM)str);
}


/* Set a tooltip for a status bar field. */
void
sb_set_tooltip(int part, const wchar_t *str)
{
    SendMessage(hwndSBAR, SB_SETTIPTEXT, part, (LPARAM)str);
}
