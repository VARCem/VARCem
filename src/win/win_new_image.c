/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the New Floppy/ZIP/MO Image dialog.
 *
 * Version:	@(#)win_new_image.c	1.0.24	2021/05/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../timer.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/disk/zip.h"
#include "../devices/disk/mo.h"
#include "win.h"
#include "resource.h"


static wchar_t	fd_file_name[512];
static int	is_zip, is_mo;
static int	drive_id, sb_part;
static int	file_type = 0;		/* 0 = IMG, 1 = Japanese FDI, 2 = 86F */
static int  listtosize[16];


/* Show a MessageBox dialog.  This is nasty, I know.  --FvK */
static int
msg_box(HWND hwnd, int type, void *arg)
{
    HWND h;
    int i;

    h = hwndMain;
    hwndMain = hwnd;

    i = ui_msgbox(type, arg);

    hwndMain = h;

    return(i);
}


static void
dlg_init(HWND hdlg)
{
    int i, j, zip_types;
    HWND h;
    WCHAR temp[255];

    h = GetDlgItem(hdlg, IDC_COMBO_DISK_SIZE);
    if (is_zip) {
	zip_types = zip_drives[drive_id].is_250 ? 2 : 1;
	for (i = 0; i < zip_types; i++)
                SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3282 + 12 + i));
    } else if (is_mo) {
    	i = j = 0;
    	memset(listtosize, 0x00, sizeof(listtosize));
	    while (i < KNOWN_MO_TYPES) {
			if (mo_drive_types[mo_drives[drive_id].type].supported_media[i]) {
				mbstowcs(temp, mo_types[i].name, sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
           		listtosize[j] = i;
           		j++;
			}
    		i++;
    	}
    } else {
	for (i = 0; i < 12; i++)
                SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3282 + i));
    }
    SendMessage(h, CB_SETCURSEL, 0, 0);
    EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_COMBO_RPM_MODE);
    for (i = 0; i < 4; i++)
	SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3278 + i));
    SendMessage(h, CB_SETCURSEL, 0, 0);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDT_1751);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDOK);
    EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_PBAR_IMG_CREATE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDT_1757);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);
}


static WIN_RESULT CALLBACK
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    wchar_t temp_path[512];
    HWND h;
    int wcs_len, ext_offs;
    wchar_t *ext;
    uint32_t disk_size, rpm_mode;
    int ret;
    FILE *f;
    wchar_t *twcs;

    switch (message) {
	case WM_INITDIALOG:
		pc_pause(1);

		dialog_center(hdlg);

		memset(fd_file_name, 0x00, sizeof(fd_file_name));

		dlg_init(hdlg);

		break;

	case WM_COMMAND:
                switch (LOWORD(wParam)) {
			case IDOK:
				h = GetDlgItem(hdlg, IDC_COMBO_DISK_SIZE);
				disk_size = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				if (is_zip)
					disk_size += 12;
				if (is_mo)
                    disk_size = listtosize[(int)SendMessage(h, CB_GETCURSEL, 0, 0)];

                if (file_type == 2) {
					h = GetDlgItem(hdlg, IDC_COMBO_RPM_MODE);
					rpm_mode = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
					ret = floppy_create_86f(fd_file_name, disk_size, rpm_mode);
				} else {
					if (is_zip)
						ret = zip_create_image(fd_file_name, disk_size, file_type);
					else if (is_mo)
						ret = mo_create_image(fd_file_name, disk_size);
					else
						ret = floppy_create_image(fd_file_name, disk_size, file_type);
				}
				if (ret) {
					if (is_zip)
						ui_zip_mount(drive_id, sb_part, 0, fd_file_name);
					else if (is_mo)
						ui_mo_mount(drive_id, sb_part, 0, fd_file_name);
					else
						ui_floppy_mount(drive_id, sb_part, 0, fd_file_name);
				} else {
					msg_box(hdlg, MBX_ERROR, (wchar_t *)IDS_OPEN_WRITE);
					pc_pause(0);
					return TRUE;
				}

			case IDCANCEL:
				EndDialog(hdlg, 0);
				pc_pause(0);
				return TRUE;

			case IDC_CFILE:
	                        if (dlg_file_ex(hdlg, get_string(is_zip ? IDS_3952 : is_mo ? IDS_3982 : IDS_3912), NULL, temp_path, DLG_FILE_SAVE)) {
					if (! wcschr(temp_path, L'.')) {
						if (wcslen(temp_path) && (wcslen(temp_path) <= 256)) {
							twcs = &temp_path[wcslen(temp_path)];
							twcs[0] = L'.';
							if (is_zip || is_mo) {
								twcs[1] = L'i';
								twcs[2] = L'm';
								twcs[3] = L'g';
							} else if (filterindex == 3) {
								twcs[1] = L'8';
								twcs[2] = L'6';
								twcs[3] = L'f';
							} else {
								twcs[1] = L'i';
								twcs[2] = L'm';
								twcs[3] = L'a';
							}
							twcs[4] = L'\0';
						}
					}
					h = GetDlgItem(hdlg, IDC_EDIT_FILE_NAME);
					f = _wfopen(temp_path, L"rb");
					if (f != NULL) {
						fclose(f);
						if (msg_box(hdlg, MBX_QUESTION, (wchar_t *)IDS_IMG_EXIST) != 0)	/* yes */ {
							return FALSE;
						}
					}
					SendMessage(h, WM_SETTEXT, 0, (LPARAM)temp_path);
					memset(fd_file_name, 0x00, sizeof(fd_file_name));
					wcscpy(fd_file_name, temp_path);
					h = GetDlgItem(hdlg, IDC_COMBO_DISK_SIZE);
					if (!is_zip || zip_drives[drive_id].is_250)
						EnableWindow(h, TRUE);
					wcs_len = (int)wcslen(temp_path);
					ext_offs = wcs_len - 4;
					ext = &(temp_path[ext_offs]);
					if (is_zip) {
						if (((wcs_len >= 4) && !wcsicmp(ext, L".ZDI")))
							file_type = 1;
						else
							file_type = 0;
					} else if (is_mo)
                        file_type = 0;
                    else {
                        if (((wcs_len >= 4) && !wcsicmp(ext, L".FDI")))
							file_type = 1;
						else if ((((wcs_len >= 4) && !wcsicmp(ext, L".86F")) || (filterindex == 3)))
							file_type = 2;
						else
							file_type = 0;
                    }
                    h = GetDlgItem(hdlg, IDT_1751);
					if (file_type == 2) {
						EnableWindow(h, TRUE);
						ShowWindow(h, SW_SHOW);
					} else {
						EnableWindow(h, FALSE);
						ShowWindow(h, SW_HIDE);
					}
					h = GetDlgItem(hdlg, IDC_COMBO_RPM_MODE);
					if (file_type == 2) {
						EnableWindow(h, TRUE);
						ShowWindow(h, SW_SHOW);
					} else {
						EnableWindow(h, FALSE);
						ShowWindow(h, SW_HIDE);
					}
					h = GetDlgItem(hdlg, IDOK);
					EnableWindow(h, TRUE);
break;
					return TRUE;
				} else
					return FALSE;

			default:
				break;
		}
		break;
    }

    return(FALSE);
}


void
dlg_new_image(int drive, int part, int zip, int mo)
{
    is_zip = zip;
    is_mo = mo;
    drive_id = drive;
    sb_part = part;

    DialogBox(plat_lang_dll(), (LPCWSTR)DLG_NEW_FLOPPY, hwndMain, dlg_proc);
}
