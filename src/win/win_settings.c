/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Settings dialog.
 *
 * Version:	@(#)win_settings.c	1.0.42	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../machines/machine.h"
#include "../nvr.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/ports/game_dev.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/parallel_dev.h"
#include "../devices/ports/serial.h"
#include "../devices/misc/isamem.h"
#include "../devices/misc/isartc.h"
#include "../devices/input/mouse.h"
#include "../devices/input/game/joystick.h"
#include "../devices/floppy/fdd.h"
#include "../devices/disk/hdd.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/scsi/scsi.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/cdrom/cdrom.h"
#include "../devices/disk/zip.h"
#include "../devices/network/network.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/midi.h"
#include "../devices/sound/snd_mpu401.h"
#include "../devices/video/video.h"
#include "win.h"
#include "resource.h"

/* Defined in the Video module. */
extern const device_t voodoo_device;


/* Floppy drives category. */
static int	temp_fdd_types[FDD_NUM],
		temp_fdd_turbo[FDD_NUM],
		temp_fdd_check_bpb[FDD_NUM];

static hard_disk_t temp_hdd[HDD_NUM];
static cdrom_t temp_cdrom_drives[CDROM_NUM];
static zip_drive_t temp_zip_drives[ZIP_NUM];


static HWND	hwndParentDialog,
		hwndChildDialog;
static int	displayed_category = 0;
static int	ask_sure = 0;
static uint8_t	temp_deviceconfig;
static config_t	temp_cfg;


/* Show a MessageBox dialog.  This is nasty, I know.  --FvK */
static int
settings_msgbox(int type, void *arg)
{
    HWND h;
    int i;

    h = hwndMain;
    hwndMain = hwndParentDialog;

    i = ui_msgbox(type, arg);

    hwndMain = h;

    return(i);
}


/* Load the per-page dialogs. */
#include "win_settings_machine.h"		/* Machine dialog */
#include "win_settings_video.h"			/* Video dialog */
#include "win_settings_input.h"			/* Input dialog */
#include "win_settings_sound.h"			/* Sound dialog */
#include "win_settings_ports.h"			/* Ports dialog */
#include "win_settings_periph.h"		/* Other Peripherals dialog */
#include "win_settings_network.h"		/* Network dialog */
#include "win_settings_floppy.h"		/* Floppy dialog */
#include "win_settings_disk.h"			/* (Hard) Disk dialog */
#include "win_settings_remov.h"			/* Removable Devices dialog */


/* This does the initial read of global variables into the temporary ones. */
static void
settings_init(void)
{
    int i;

    /* Copy the current configuration to a temporary one. */
    memcpy(&temp_cfg, &config, sizeof(config_t));

    /* Floppy drives category */
    for (i = 0; i < FDD_NUM; i++) {
	temp_fdd_types[i] = fdd_get_type(i);
	temp_fdd_turbo[i] = fdd_get_turbo(i);
	temp_fdd_check_bpb[i] = fdd_get_check_bpb(i);
    }

    /* Hard disks category */
    memcpy(temp_hdd, hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Other removable devices category */
    memcpy(temp_cdrom_drives, cdrom, CDROM_NUM * sizeof(cdrom_t));
    memcpy(temp_zip_drives, zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    temp_deviceconfig = 0;
}


/* This returns 1 if any variable has changed, 0 if not. */
static int
settings_changed(void)
{
    int i = 0, j;

    /* If the main block has changed, done. */
    if (config_compare(&config, &temp_cfg)) return(1);

    /* Hard disks category */
    i = i || memcmp(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (j = 0; j < FDD_NUM; j++) {
	i = i || (temp_fdd_types[j] != fdd_get_type(j));
	i = i || (temp_fdd_turbo[j] != fdd_get_turbo(j));
	i = i || (temp_fdd_check_bpb[j] != fdd_get_check_bpb(j));
    }

    /* Other removable devices category */
    i = i || memcmp(cdrom, temp_cdrom_drives, CDROM_NUM * sizeof(cdrom_t));
    i = i || memcmp(zip_drives, temp_zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    i = i || !!temp_deviceconfig;

    return(i);
}


static int
msgbox_reset(void)
{
    int changed = 0;
    int i = 0;

    changed = settings_changed();

    if (changed) {
	i = settings_msgbox(MBX_QUESTION, (wchar_t *)IDS_MSG_SAVE);

	if (i == 1) return(1);	/* no */

	if (i < 0) return(0);	/* cancel */

	return(2);		/* yes */
    } else {
	return(1);
    }
}


/* This saves the settings back to the global variables. */
static void
settings_save(void)
{
    int i;

    /* Shut down the active machine. */
    pc_reset_hard_close();

    /* Save the temporary configuration back to the active one. */
    memcpy(&config, &temp_cfg, sizeof(config_t));

    /* Hard disks category */
    memcpy(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (i = 0; i < FDD_NUM; i++) {
	fdd_set_type(i, temp_fdd_types[i]);
	fdd_set_turbo(i, temp_fdd_turbo[i]);
	fdd_set_check_bpb(i, temp_fdd_check_bpb[i]);
    }

    /* Removable devices category */
    memcpy(cdrom, temp_cdrom_drives, CDROM_NUM * sizeof(cdrom_t));
    memcpy(zip_drives, temp_zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    /* Mark configuration as changed. */
    config_changed = 1;
}


/************************************************************************
 *									*
 *			    Main Settings Dialog			*
 *									*
 ************************************************************************/

#define PAGE_MACHINE			0
#define PAGE_VIDEO			1
#define PAGE_INPUT			2
#define PAGE_SOUND			3
#define PAGE_NETWORK			4
#define PAGE_PORTS			5
#define PAGE_PERIPHERALS		6
#define PAGE_HARD_DISKS			7
#define PAGE_FLOPPY_DRIVES		8
#define PAGE_OTHER_REMOVABLE_DEVICES	9
#define PAGE_MAX			10

/* Icon, Bus, File, C, H, S, Size. */
#define C_COLUMNS_HARD_DISKS		6


static void
show_child(HWND hwndParent, DWORD child_id)
{
    if (child_id == displayed_category) return;

    displayed_category = child_id;

    SendMessage(hwndChildDialog, WM_SAVE_CFG, 0, 0);

    DestroyWindow(hwndChildDialog);

    switch(child_id) {
	case PAGE_MACHINE:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_MACHINE,
					       hwndParent, machine_proc);
		break;

	case PAGE_VIDEO:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_VIDEO,
					       hwndParent, video_proc);
		break;

	case PAGE_INPUT:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_INPUT,
					       hwndParent, input_proc);
		break;

	case PAGE_SOUND:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_SOUND,
					       hwndParent, sound_proc);
		break;

	case PAGE_NETWORK:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_NETWORK,
					       hwndParent, network_proc);
		break;

	case PAGE_PORTS:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_PORTS,
					       hwndParent, ports_proc);
		break;

	case PAGE_PERIPHERALS:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_PERIPHERALS,
					       hwndParent, peripherals_proc);
		break;

	case PAGE_HARD_DISKS:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_DISK,
					       hwndParent, disk_proc);
		break;

	case PAGE_FLOPPY_DRIVES:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_FLOPPY,
					       hwndParent, floppy_proc);
		break;

	case PAGE_OTHER_REMOVABLE_DEVICES:
		hwndChildDialog = CreateDialog(plat_lang_dll(),
					       (LPCWSTR)DLG_CFG_RMV_DEVICES,
					       hwndParent, rmv_devices_proc);
		break;

	default:
		fatal("Invalid child dialog ID\n");
		return;
    }

    ShowWindow(hwndChildDialog, SW_SHOWNORMAL);
}


static BOOL
image_list_init(HWND hwndList)
{
    int icons[PAGE_MAX] = {
	ICON_MACHINE,
	ICON_DISPLAY,
	ICON_INPDEV,
	ICON_SOUND,
	ICON_NETWORK,
	ICON_PORTS,
	ICON_PERIPH,
	ICON_DISK,
	ICON_FLOPPY,
	ICON_REMOV
    };
    HICON hiconItem;
    HIMAGELIST hSmall;
    int i;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);


    for (i = 0; i < PAGE_MAX; i++) {
#if (defined(_MSC_VER) && defined(_M_X64)) || \
    (defined(__GNUC__) && defined(__amd64__)) || defined(__aarch64__)
	hiconItem = LoadIcon(hInstance, (LPCWSTR)((uint64_t)icons[i]));
#else
	hiconItem = LoadIcon(hInstance, (LPCWSTR)((uint32_t)icons[i]));
#endif
	ImageList_AddIcon(hSmall, hiconItem);
	DestroyIcon(hiconItem);
    }

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);

    return(TRUE);
}


static BOOL
insert_categories(HWND hwndList)
{
    LVITEM lvI;
    int i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i = 0; i < PAGE_MAX; i++) {
	lvI.pszText = (LPTSTR)get_string(IDS_3310+i);
	lvI.iItem = i;
	lvI.iImage = i;

	if (ListView_InsertItem(hwndList, &lvI) == -1)
		return(FALSE);
    }

    return(TRUE);
}


#ifdef USE_MANAGER
static void
communicate_closure(void)
{
    if (source_hwnd)
	PostMessage((HWND)(uintptr_t)source_hwnd,
		    WM_SEND_SSTATUS, (WPARAM)0, (LPARAM)hwndMain);
}
#endif


static WIN_RESULT CALLBACK
settings_confirm(HWND hdlg, int button)
{
    int i;

    SendMessage(hwndChildDialog, WM_SAVE_CFG, 0, 0);

    if (ask_sure) {
	i = msgbox_reset();
	if (i == 0) {
		/* CANCEL, just kidding! */
		return(FALSE);
	}
    } else
	i = 2;

    if (i == 2) {
	/* YES, reset system. */
	settings_save();
    }

    DestroyWindow(hwndChildDialog);
    EndDialog(hdlg, i);

#ifdef USE_MANAGER
    communicate_closure();
#endif

    return button ? FALSE : TRUE;
}


static WIN_RESULT CALLBACK
dlg_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h = NULL;
    int category;
    int i, j;

    hwndParentDialog = hdlg;

    switch (message) {
	case WM_INITDIALOG:
		dialog_center(hdlg);
		settings_init();

		disk_track_init();
		cdrom_track_init();
		zip_track_init();

		displayed_category = -1;
		h = GetDlgItem(hdlg, IDC_SETTINGSCATLIST);
		image_list_init(h);
		insert_categories(h);
		ListView_SetItemState(h, 0, LVIS_FOCUSED|LVIS_SELECTED, 0x000F);
		return(TRUE);

	case WM_NOTIFY:
		if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_SETTINGSCATLIST)) {
			category = -1;
			for (i = 0; i < PAGE_MAX; i++) {
				h = GetDlgItem(hdlg, IDC_SETTINGSCATLIST);
				j = ListView_GetItemState(h, i, LVIS_SELECTED);
				if (j) category = i;
			}
			if (category != -1)
				show_child(hdlg, category);
		}
		break;

	case WM_CLOSE:
		return settings_confirm(hdlg, 0);

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
				return settings_confirm(hdlg, 1);

			case IDCANCEL:
				/* CANCEL, just kidding! */
				DestroyWindow(hwndChildDialog);
				EndDialog(hdlg, 0);
#ifdef USE_MANAGER
				communicate_closure();
#endif
				return(TRUE);
		}
		break;

	default:
		return(FALSE);
    }

    return(FALSE);
}


/*
 * Find all available machine ROMs.
 *
 * While we scan for the roms, we also build
 * the arrays needed by the Machines dialog,
 * saving us from having to do it twice..
 */
static int
find_roms(void)
{
    const char *str;
    int c, d, i;

    c = d = 0;
    for (;;) {
	/* Get ANSI name of machine. */
	str = machine_get_name_ex(c);
	if (str == NULL)
		break;

	/* If entry already used, clear it. */
	if (mach_names[d] != NULL)
		free(mach_names[d]);

	/* Is this machine available? */
	if (machine_available(c)) {
		/* Allocate space and copy name to Unicode. */
		i = (int)strlen(str) + 1;
		mach_names[d] = (wchar_t *)mem_alloc(i * sizeof(wchar_t));
		mbstowcs(mach_names[d], str, i);

		/* Add entry to the conversion lists. */
		list_to_mach[d] = c;
		mach_to_list[c] = d++;
	}

	/* Next machine from table. */
	c++;
    }

    return(d);
}


int
dlg_settings(int ask)
{
    int i;

#ifdef USE_MANAGER
    if (source_hwnd) {
	pc_pause(1);

	PostMessage((HWND) (uintptr_t) source_hwnd,
		    WM_SEND_SSTATUS, (WPARAM)1, (LPARAM)hwndMain;
    }
#endif

    /*
     * This looks weird here, but we have to do it
     * before we open up the Settings dialog, else
     * we cannot close it after the msgbox exits..
     */
    INFO("Scanning for ROM images:\n");
    if ((i = find_roms()) <= 0) {
	/* No usable ROMs found, aborting. */
	ERRLOG("No usable machine has been found!\n");

	/* Tell the user about it. */
	ui_msgbox(MBX_ERROR|MBX_FATAL, (wchar_t *)IDS_ERR_NOROMS);

	return(0);
    }
    INFO("A total of %i machines are available.\n", i);

    ask_sure = ask;
    i = (int)DialogBox(plat_lang_dll(),
		       (LPCWSTR)DLG_CONFIG, hwndMain, dlg_proc);

    return(i);
}
