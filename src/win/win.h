/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Platform support defintions for Win32. This file describes
 *		only things used globally within the Windows platform; the
 *		generic platform defintions are in the plat.h file.
 *
 * Version:	@(#)win.h	1.0.26	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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


/* Class names and such. */
#define MUTEX_NAME	L"VARCem.BlitMutex"
#define CLASS_NAME	L"VARCem.MainWindow"
#define FS_CLASS_NAME	L"VARCem.FullScreen"
#define MENU_MAIN_NAME	L"MainMenu"
#define MENU_SB_NAME	L"StatusBarMenu"
#define ACCEL_NAME	L"MainAccel"

/* Application-specific window messages. */
#define WM_PAUSE	WM_USER
#define WM_LEAVE_FS	WM_USER+1
#define WM_RESET_D3D	WM_USER+2
#define WM_SAVE_CFG	WM_USER+3
#define WM_SHOW_CFG	WM_USER+4
#define WM_HARD_RESET	WM_USER+5
#define WM_SHUTDOWN	WM_USER+6
#define WM_CTRLALTDEL	WM_USER+7
#define WM_SEND_HWND	WM_USER+8	// send main window handle
#define WM_SEND_STATUS	WM_USER+9	// pause/resume status in WPARAM
#define WM_SEND_SSTATUS	WM_USER+10	// settings status: WPARAM 1=open


/* Status bar definitions. */
#define SB_HEIGHT	16		// for 16x16 icons
#define SB_PADDING	1		// 1px of padding


#ifdef __cplusplus
extern "C" {
#endif

/* Cleans up the WinAPI sources a bit. */
#if (defined(_MSC_VER) && defined(_M_X64)) || \
    (defined(__GNUC__) && defined(__amd64__)) || defined(__aarch64__)
# define WIN_RESULT	LRESULT
#else
# define WIN_RESULT	BOOL
#endif


extern HINSTANCE	hInstance;
extern HWND		hwndMain,
			hwndRender;
extern DWORD		filterindex;

/* VidApi initializers. */
extern const vidapi_t	ddraw_vidapi;
extern const vidapi_t	d3d_vidapi;


/* Internal platform support functions. */
extern HICON	LoadIconEx(PCTSTR name);
extern void	keyboard_getkeymap(void);
extern void	keyboard_handle(RAWINPUT *raw);
extern void	mouse_handle(RAWINPUT *raw);
extern int	win_mouse_init(void);
extern void	win_mouse_close(void);
extern void	joystick_handle(RAWINPUT *raw);

/* Internal dialog functions. */
extern void	dialog_center(HWND hdlg);
extern int	dlg_file_ex(HWND hwnd, const wchar_t *filt,
			    const wchar_t *ifn, wchar_t *fn, int save);
#ifdef EMU_DEVICE_H
extern uint8_t	dlg_devconf(HWND hwnd, const device_t *device);
#endif
extern uint8_t	dlg_jsconf(HWND hwnd, int joy_nr, int type);

/* Platform UI support functions. */
extern int	ui_init(int nCmdShow);
extern LPARAM	win_string(int id);
extern void	plat_set_input(HWND h);
extern HMODULE	plat_lang_dll(void);

#ifdef __cplusplus
}
#endif


#endif	/*PLAT_WIN_H*/
