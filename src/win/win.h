/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Platform support defintions for Win32.
 *
 * Version:	@(#)win.h	1.0.4	2018/03/07
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
#ifndef PLAT_WIN_H
# define PLAT_WIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "resource.h"


/* Class names and such. */
#define CLASS_NAME		L"VARCem.MainWnd"
#define MENU_NAME		L"MainMenu"
#define ACCEL_NAME		L"MainAccel"
#define SUB_CLASS_NAME		L"VARCem.SubWnd"
#define SB_CLASS_NAME		L"VARCem.StatusBar"
#define SB_MENU_NAME		L"StatusBarMenu"

/* Application-specific window messages. */
#define WM_RESETD3D		WM_USER
#define WM_LEAVEFULLSCREEN	WM_USER+1
#define WM_SAVESETTINGS		0x8888
#define WM_SHOWSETTINGS		0x8889
#define WM_PAUSE		0x8890
#define WM_SENDHWND		0x8891


extern HINSTANCE	hinstance;
extern HWND		hwndMain,
			hwndRender;
extern HANDLE		ghMutex;
extern LCID		lang_id;
extern HICON		hIcon[512];

extern int		status_is_open;

extern char		openfilestring[260];
extern WCHAR		wopenfilestring[260];

extern uint8_t		filterindex;


#ifdef USE_CRASHDUMP
extern void	InitCrashDump(void);
#endif

extern HICON	LoadIconEx(PCTSTR pszIconName);

/* Emulator start/stop support functions. */
extern void	do_start(void);
extern void	do_stop(void);

/* Internal platform support functions. */
extern void	set_language(int id);
extern int	get_vidpause(void);
extern void	show_cursor(int);

extern void	keyboard_getkeymap(void);
extern void	keyboard_handle(LPARAM lParam, int infocus);

extern void     win_mouse_init(void);
extern void     win_mouse_close(void);

extern intptr_t	fdd_type_to_icon(int type);

#ifdef EMU_DEVICE_H
extern uint8_t	deviceconfig_open(HWND hwnd, device_t *device);
#endif
extern uint8_t	joystickconfig_open(HWND hwnd, int joy_nr, int type);

extern int	getfile(HWND hwnd, char *f, char *fn);
extern int	getsfile(HWND hwnd, char *f, char *fn);

extern int	win_settings_open(HWND hwnd, int ask);

extern void	hard_disk_add_open(HWND hwnd, int is_existing);
extern int	hard_disk_was_added(void);


/* Platform UI support functions. */
extern int	ui_init(int nCmdShow);


/* Functions in win_about.c: */
extern void	AboutDialogCreate(HWND hwnd);


/* Functions in win_snd_gain.c: */
extern void	SoundGainDialogCreate(HWND hwnd);


/* Functions in win_new_floppy.c: */
extern void	NewFloppyDialogCreate(HWND hwnd, int id, int part);


/* Functions in win_status.c: */
extern HWND	hwndStatus;
extern void	StatusWindowCreate(HWND hwnd);


/* Functions in win_stbar.c: */
extern HWND	hwndSBAR;
extern void	StatusBarCreate(HWND hwndParent, uintptr_t idStatus,
				HINSTANCE hInst);


/* Functions in win_dialog.c: */
extern void	dialog_center(HWND hdlg);

extern wchar_t	*BrowseFolder(wchar_t *saved_path, wchar_t *title);

extern int	file_dlg_w(HWND hwnd, WCHAR *f, WCHAR *fn, int save);
extern int	file_dlg(HWND hwnd, WCHAR *f, char *fn, int save);
extern int	file_dlg_mb(HWND hwnd, char *f, char *fn, int save);
extern int	file_dlg_w_st(HWND hwnd, int i, WCHAR *fn, int save);
extern int	file_dlg_st(HWND hwnd, int i, char *fn, int save);


#ifdef __cplusplus
}
#endif


#endif	/*PLAT_WIN_H*/
