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
 * Version:	@(#)win_ui.c	1.0.15	2018/04/28
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
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include "../emu.h"
#include "../version.h"
#include "../config.h"
#include "../device.h"
#include "../input/keyboard.h"
#include "../input/mouse.h"
#include "../video/video.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"
#include "win_d3d.h"


#define TIMER_1SEC	1		/* ID of the one-second timer */


/* Platform Public data, specific. */
HWND		hwndMain,		/* application main window */
		hwndRender;		/* machine render window */
HMENU		menuMain;		/* application main menu */
HICON		hIcon[512];		/* icon data loaded from resources */
RECT		oldclip;		/* mouse rect */
int		infocus = 1;
int		rctrl_is_lalt = 0;

char		openfilestring[260];
WCHAR		wopenfilestring[260];


/* Local data. */
static wchar_t	wTitle[512];
static RAWINPUTDEVICE	device;
static HHOOK	hKeyboardHook;
static int	hook_enabled = 0;
static int	save_window_pos = 0;


static int vis = -1;

/* Set host cursor visible or not. */
void
show_cursor(int val)
{
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


HICON
LoadIconEx(PCTSTR pszIconName)
{
    return((HICON)LoadImage(hinstance, pszIconName, IMAGE_ICON,
						16, 16, LR_SHARED));
}


#if 0
static void
menu_update(void)
{
    menuMain = LoadMenu(hinstance, L"MainMenu"));

    menuSBAR = LoadMenu(hinstance, L"StatusBarMenu");

    initmenu();

    SetMenu(memnuMain, menu);

    win_title_update = 1;
}
#endif


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
//    HMENU hmenu;
    RECT rect;
    int sb_borders[3];
    int temp_x, temp_y;
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
// We may need this later.
//		hmenu = GetMenu(hwnd);
		idm = LOWORD(wParam);

		/* Let the general UI handle it first, and then we do. */
		if (! ui_menu_command(idm)) switch(idm) {
			case IDM_EXIT:
				PostQuitMessage(0);
				break;

			case IDM_RESIZE:
				GetWindowRect(hwnd, &rect);
				if (vid_resize)
					SetWindowLongPtr(hwnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW) | WS_VISIBLE);
				  else
					SetWindowLongPtr(hwnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE);

				SendMessage(hwndSBAR, SB_GETBORDERS, 0, (LPARAM) sb_borders);

				/* Main Window. */
                        	MoveWindow(hwnd, rect.left, rect.top,
					unscaled_size_x + (GetSystemMetrics(vid_resize ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME) * 2),
					unscaled_size_y + (GetSystemMetrics(SM_CYEDGE) * 2) + (GetSystemMetrics(vid_resize ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 17 + sb_borders[1] + 1,
					TRUE);

				/* Render window. */
				MoveWindow(hwndRender, 0, 0, unscaled_size_x, unscaled_size_y, TRUE);
				GetWindowRect(hwndRender, &rect);

				/* Status bar. */
				MoveWindow(hwndSBAR,
					   0, rect.bottom + GetSystemMetrics(SM_CYEDGE),
					   unscaled_size_x, 17, TRUE);

				if (mouse_capture) {
					GetWindowRect(hwndRender, &rect);

					ClipCursor(&rect);
				}
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
		plat_mouse_capture(0);
		if (hook_enabled) {
			UnhookWindowsHookEx(hKeyboardHook);
			hook_enabled = 0;
		}
		break;

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
		SendMessage(hwndSBAR, SB_GETBORDERS, 0, (LPARAM) sb_borders);

		temp_x = (lParam & 0xFFFF);
		temp_y = (lParam >> 16) - (21 + sb_borders[1]);
		if (temp_x < 1)
			temp_x = 1;
		if (temp_y < 1)
			temp_y = 1;

		if ((temp_x != scrnsz_x) || (temp_y != scrnsz_y))
			doresize = 1;

 		scrnsz_x = temp_x;
		scrnsz_y = temp_y;

		MoveWindow(hwndRender, 0, 0, scrnsz_x, scrnsz_y, TRUE);

		GetWindowRect(hwndRender, &rect);

		/* Status bar. */
		MoveWindow(hwndSBAR,
			   0, rect.bottom + GetSystemMetrics(SM_CYEDGE),
			   scrnsz_x, 17, TRUE);

		plat_vidsize(scrnsz_x, scrnsz_y);

		MoveWindow(hwndSBAR, 0, scrnsz_y + 6, scrnsz_x, 17, TRUE);

		if (mouse_capture) {
			GetWindowRect(hwndRender, &rect);

			ClipCursor(&rect);
		}

		if (window_remember) {
			GetWindowRect(hwnd, &rect);
			window_x = rect.left;
			window_y = rect.top;
			window_w = rect.right - rect.left;
			window_h = rect.bottom - rect.top;
			save_window_pos = 1;
		}

		config_save();
		break;

	case WM_MOVE:
		/* If window is not resizable, then tell the main thread to
		   resize it, as sometimes, moves can mess up the window size. */
		if (!vid_resize)
			doresize = 1;

		if (window_remember) {
			GetWindowRect(hwnd, &rect);
			window_x = rect.left;
			window_y = rect.top;
			window_w = rect.right - rect.left;
			window_h = rect.bottom - rect.top;
			save_window_pos = 1;
		}
		break;
                
	case WM_TIMER:
		if (wParam == TIMER_1SEC) {
			pc_onesec();
		}
		break;

	case WM_RESETD3D:
		startblit();
		if (vid_fullscreen)
			d3d_reset_fs();
		  else
			d3d_reset();
		endblit();
		break;

	case WM_LEAVEFULLSCREEN:
		/* pclog("leave full screen on window message\n"); */
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

    return(0);
}


static LRESULT CALLBACK
SubWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return(DefWindowProc(hwnd, message, wParam, lParam));
}


int
ui_init(int nCmdShow)
{
    WCHAR title[200];
    WNDCLASSEX wincl;			/* buffer for main window's class */
    MSG messages;			/* received-messages buffer */
    HWND hwnd = 0;			/* handle for our window */
    HACCEL haccel;			/* handle to accelerator table */
    int ret;

#if 0
    /* We should have an application-wide at_exit catcher. */
    atexit(plat_mouse_capture);
#endif

    if (settings_only) {
	if (! pc_init_modules()) {
		/* Dang, no ROMs found at all! */
		MessageBox(hwnd,
			   plat_get_string(IDS_2056),
			   plat_get_string(IDS_2050),
			   MB_OK | MB_ICONERROR);
		return(6);
	}

	(void)dlg_settings(0);

	return(0);
    }

    /* Create our main window's class and register it. */
    wincl.hInstance = hinstance;
    wincl.lpszClassName = CLASS_NAME;
    wincl.lpfnWndProc = MainWindowProcedure;
    wincl.style = CS_DBLCLKS;		/* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = LoadIcon(hinstance, (LPCTSTR)100);
    wincl.hIconSm = LoadIcon(hinstance, (LPCTSTR)100);
    wincl.hCursor = NULL;
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = CreateSolidBrush(RGB(0,0,0));
    if (! RegisterClassEx(&wincl))
			return(2);
    wincl.lpszClassName = SUB_CLASS_NAME;
    wincl.lpfnWndProc = SubWindowProcedure;
    if (! RegisterClassEx(&wincl))
			return(2);

    /* Load the Window Menu(s) from the resources. */
    menuMain = LoadMenu(hinstance, MENU_NAME);

    /* Now create our main window. */
    wsprintf(title, L"%S", emu_version, sizeof_w(title));
    hwnd = CreateWindowEx (
		0,			/* no extended possibilites */
		CLASS_NAME,		/* class name */
		title,			/* Title Text */
		(WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX) | DS_3DLOOK,
		CW_USEDEFAULT,		/* Windows decides the position */
		CW_USEDEFAULT,		/* where window ends up on the screen */
		scrnsz_x+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),	/* width */
		scrnsz_y+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+1,	/* and height in pixels */
		HWND_DESKTOP,		/* window is a child to desktop */
		menuMain,		/* menu */
		hinstance,		/* Program Instance handler */
		NULL);			/* no Window Creation data */
    hwndMain = hwnd;

    ui_window_title(title);

    /* Set up main window for resizing if configured. */
    if (vid_resize)
	SetWindowLongPtr(hwnd, GWL_STYLE,
			(WS_OVERLAPPEDWINDOW));
      else
	SetWindowLongPtr(hwnd, GWL_STYLE,
			(WS_OVERLAPPEDWINDOW&~WS_SIZEBOX&~WS_THICKFRAME&~WS_MAXIMIZEBOX));

    /* Move to the last-saved position if needed. */
    if (window_remember)
	MoveWindow(hwnd, window_x, window_y, window_w, window_h, TRUE);

    /* Reset all menus to their defaults. */
    ui_menu_reset_all();

    /* Make the window visible on the screen. */
    ShowWindow(hwnd, nCmdShow);

    /* Load the accelerator table */
    haccel = LoadAccelerators(hinstance, ACCEL_NAME);
    if (haccel == NULL) {
	MessageBox(hwndMain,
		   plat_get_string(IDS_2153),
		   plat_get_string(IDS_2050),
		   MB_OK | MB_ICONERROR);
	return(3);
    }

    /* Initialize the input (keyboard, mouse, game) module. */
    device.usUsagePage = 0x01;
    device.usUsage = 0x06;
    device.dwFlags = RIDEV_NOHOTKEYS;
    device.hwndTarget = hwnd;
    if (! RegisterRawInputDevices(&device, 1, sizeof(device))) {
	MessageBox(hwndMain,
		   plat_get_string(IDS_2154),
		   plat_get_string(IDS_2050),
		   MB_OK | MB_ICONERROR);
	return(4);
    }
    keyboard_getkeymap();

    /* Initialize the mouse module. */
    win_mouse_init();

    /* Create the status bar window. */
    StatusBarCreate(hwndMain, IDC_STATUS, hinstance);

    /*
     * Before we can create the Render window, we first have
     * to prepare some other things that it depends on.
     */
    ghMutex = CreateMutex(NULL, FALSE, L"VARCem.BlitMutex");

    /* Create the Machine Rendering window. */
    hwndRender = CreateWindow(L"STATIC", NULL, WS_CHILD|SS_BITMAP,
			      0, 0, 1, 1, hwnd, NULL, hinstance, NULL);

    /* That looks good, now continue setting up the machine. */
    switch (pc_init_modules()) {
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
    if (! plat_setvid(vid_api)) {
	MessageBox(hwnd,
		   plat_get_string(IDS_2095),
		   plat_get_string(IDS_2050),
		   MB_OK | MB_ICONERROR);
	return(5);
    }

    /* Initialize the rendering window, or fullscreen. */
    if (start_in_fullscreen)
	plat_setfullscreen(1);

    /* Activate the render window, this will also set the screen size. */
    MoveWindow(hwndRender, 0, 0, scrnsz_x, scrnsz_y, TRUE);

#if 0
    /* Set up the current window size. */
    plat_resize(scrnsz_x, scrnsz_y);
#endif

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
    do_start();

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

	if (! TranslateAccelerator(hwnd, haccel, &messages)) {
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
    do_stop();

    UnregisterClass(SUB_CLASS_NAME, hinstance);
    UnregisterClass(CLASS_NAME, hinstance);

    win_mouse_close();

    return(messages.wParam);
}


wchar_t *
ui_window_title(wchar_t *s)
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

    return(s);
}


/* We should have the language ID as a parameter. */
void
plat_pause(int p)
{
    static wchar_t oldtitle[512];
    wchar_t title[512];

    /* If un-pausing, as the renderer if that's OK. */
    if (p == 0)
	p = get_vidpause();

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

    /* Update the actual menu. */
    menu_set_item(IDM_PAUSE, dopause);
}


/* Tell the UI about a new screen resolution. */
void
plat_resize(int x, int y)
{
    int sb_borders[3];
    RECT r;

#if 0
pclog("PLAT: VID[%d,%d] resizing to %dx%d\n", vid_fullscreen, vid_api, x, y);
#endif
    /* First, see if we should resize the UI window. */
    if (!vid_resize) {
	video_wait_for_blit();
	SendMessage(hwndSBAR, SB_GETBORDERS, 0, (LPARAM) sb_borders);
	GetWindowRect(hwndMain, &r);
	MoveWindow(hwndRender, 0, 0, x, y, TRUE);

	GetWindowRect(hwndMain, &r);
	MoveWindow(hwndMain, r.left, r.top,
		   x + (GetSystemMetrics(vid_resize ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME) * 2),
		   y + (GetSystemMetrics(SM_CYEDGE) * 2) + (GetSystemMetrics(vid_resize ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 17 + sb_borders[1] + 1,
		   TRUE);
	GetWindowRect(hwndMain, &r);

	MoveWindow(hwndRender, 0, 0, x, y, TRUE);

	if (mouse_capture) {
		GetWindowRect(hwndRender, &r);
		ClipCursor(&r);
	}
    }
}


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
	/* pclog("mouse capture off, hide cursor\n"); */
	show_cursor(0);
	mouse_capture = 1;
    } else if (!on && mouse_capture) {
	/* Disable the in-app mouse. */
	ClipCursor(&oldclip);
	/* pclog("mouse capture on, show cursor\n"); */
	show_cursor(-1);

	mouse_capture = 0;
    }
}
