/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Platform support defintions for Win32.
 *
 * Version:	@(#)win.h	1.0.17	2018/05/28
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PLAT_WIN_H
# define PLAT_WIN_H


#include "resource.h"		/* platform resources */
#include "../ui/ui_resource.h"	/* common resources */


/* Class names and such. */
#define MUTEX_NAME		L"VARCem.BlitMutex"
#define CLASS_NAME		L"VARCem.MainWindow"
#define FS_CLASS_NAME		L"VARCem.FullScreen"
#define MENU_NAME		L"MainMenu"
#define SB_MENU_NAME		L"StatusBarMenu"
#define ACCEL_NAME		L"MainAccel"

/* Application-specific window messages. */
#define WM_PAUSE		WM_USER
#define WM_LEAVEFULLSCREEN	WM_USER+1
#define WM_RESETD3D		WM_USER+2
#define WM_SAVESETTINGS		WM_USER+3
#define WM_SHOWSETTINGS		WM_USER+4


/* Status bar definitions. */
#define SB_HEIGHT		16		/* for 16x16 icons */
#define SB_PADDING		1		/* 1px of padding */


#ifdef __cplusplus
extern "C" {
#endif

/* Define an in-memory language entry. */
typedef struct _lang_ {
    const wchar_t	*name;
    const wchar_t	*dll;

    const wchar_t	*author;
    const wchar_t	*email;
    const wchar_t	*version;

    int			id;
    struct _lang_	*next;
} lang_t;


extern HINSTANCE	hInstance;
extern HICON		hIcon[512];
extern HWND		hwndMain,
			hwndRender;
extern DWORD		filterindex;
extern int		status_is_open;

/* VidApi initializers. */
extern const vidapi_t	ddraw_vidapi;
extern const vidapi_t	d3d_vidapi;

/* Languages. */
extern lang_t		*languages;


/* Internal platform support functions. */
#ifdef USE_CRASHDUMP
extern void	InitCrashDump(void);
#endif
extern HICON	LoadIconEx(PCTSTR name);
extern void	keyboard_getkeymap(void);
extern void	keyboard_handle(LPARAM lParam, int infocus);
extern void     win_mouse_init(void);
extern void     win_mouse_close(void);

/* Internal dialog functions. */
extern void	dialog_center(HWND hdlg);
extern int	dlg_file_ex(HWND hwnd, const wchar_t *filt,
			    const wchar_t *ifn, wchar_t *fn, int save);
#ifdef EMU_DEVICE_H
extern uint8_t	dlg_devconf(HWND hwnd, device_t *device);
#endif
extern uint8_t	dlg_jsconf(HWND hwnd, int joy_nr, int type);

/* Platform UI support functions. */
extern int	ui_init(int nCmdShow);
extern void	plat_set_input(HWND h);
extern HMODULE	plat_lang_dll(void);
extern void	plat_lang_menu(void);

#ifdef __cplusplus
}
#endif


#endif	/*PLAT_WIN_H*/
