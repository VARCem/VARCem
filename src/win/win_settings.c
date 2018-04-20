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
 * Version:	@(#)win_settings.c	1.0.22	2018/04/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../machine/machine.h"
#include "../game/gameport.h"
#include "../mouse.h"
#include "../parallel.h"
#include "../parallel_dev.h"
#include "../serial.h"
#include "../disk/hdd.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../disk/zip.h"
#include "../cdrom/cdrom.h"
#include "../floppy/fdd.h"
#include "../scsi/scsi.h"
#include "../network/network.h"
#include "../sound/sound.h"
#include "../sound/midi.h"
#include "../sound/snd_mpu401.h"
#include "../video/video.h"
#include "../plat.h"
#include "../ui.h"
#include "win.h"


/* Defined in the Video module. */
extern const device_t voodoo_device;


/* Machine category. */
static int	temp_machine,
		temp_cpu_m, temp_cpu, temp_wait_states,
		temp_mem_size, temp_fpu, temp_sync;
#ifdef USE_DYNAREC
static int	temp_dynarec;
#endif

/* Video category. */
static int	temp_video_card, temp_video_speed, temp_voodoo;

/* Input devices category. */
static int	temp_mouse, temp_joystick;

/* Sound category. */
static int	temp_sound_card, temp_midi_device, temp_mpu401,
		temp_opl3_type, temp_float;

/* Network category. */
static int	temp_net_type, temp_net_card;
static char	temp_host_dev[128];

/* Ports category. */
static int	temp_serial[SERIAL_MAX],
		temp_parallel[PARALLEL_MAX],
		temp_parallel_device[PARALLEL_MAX];

/* Other peripherals category. */
static int	temp_hdc_type,
		temp_scsi_card,
		temp_ide_ter, temp_ide_ter_irq,
		temp_ide_qua, temp_ide_qua_irq;
static int	temp_bugger;

/* Floppy drives category. */
static int	temp_fdd_types[FDD_NUM],
		temp_fdd_turbo[FDD_NUM],
		temp_fdd_check_bpb[FDD_NUM];

/* Hard disks category. */
static hard_disk_t temp_hdd[HDD_NUM];

/* Other removable devices category. */
static cdrom_drive_t temp_cdrom_drives[CDROM_NUM];
static zip_drive_t temp_zip_drives[ZIP_NUM];


static HWND	hwndParentDialog,
		hwndChildDialog;
static int	displayed_category = 0;
static int	ask_sure = 0;
static uint8_t	temp_deviceconfig;


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
    int i = 0;

    /* Machine category */
    temp_machine = machine;
    temp_cpu_m = cpu_manufacturer;
    temp_wait_states = cpu_waitstates;
    temp_cpu = cpu;
    temp_mem_size = mem_size;
#ifdef USE_DYNAREC
    temp_dynarec = cpu_use_dynarec;
#endif
    temp_fpu = enable_external_fpu;
    temp_sync = enable_sync;

    /* Video category */
    temp_video_card = video_card;
    temp_video_speed = video_speed;
    temp_voodoo = voodoo_enabled;

    /* Input devices category */
    temp_mouse = mouse_type;
    temp_joystick = joystick_type;

    /* Sound category */
    temp_sound_card = sound_card;
    temp_midi_device = midi_device;
    temp_mpu401 = mpu401_standalone_enable;
    temp_opl3_type = opl3_type;
    temp_float = sound_is_float;

    /* Network category */
    temp_net_type = network_type;
    memset(temp_host_dev, 0, sizeof(temp_host_dev));
    strcpy(temp_host_dev, network_host);
    temp_net_card = network_card;

    /* Ports category */
    for (i = 0; i < SERIAL_MAX; i++)
	temp_serial[i] = serial_enabled[i];
    for (i = 0; i < PARALLEL_MAX; i++) {
	temp_parallel[i] = parallel_enabled[i];
	temp_parallel_device[i] = parallel_device[i];
    }

    /* Other peripherals category */
    temp_scsi_card = scsi_card;
    temp_hdc_type = hdc_type;
    temp_ide_ter = ide_enable[2];
    temp_ide_ter_irq = ide_irq[2];
    temp_ide_qua = ide_enable[3];
    temp_ide_qua_irq = ide_irq[3];
    temp_bugger = bugger_enabled;

    /* Floppy drives category */
    for (i=0; i<FDD_NUM; i++) {
	temp_fdd_types[i] = fdd_get_type(i);
	temp_fdd_turbo[i] = fdd_get_turbo(i);
	temp_fdd_check_bpb[i] = fdd_get_check_bpb(i);
    }

    /* Hard disks category */
    memcpy(temp_hdd, hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Other removable devices category */
    memcpy(temp_cdrom_drives, cdrom_drives, CDROM_NUM * sizeof(cdrom_drive_t));
    memcpy(temp_zip_drives, zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    temp_deviceconfig = 0;
}


/* This returns 1 if any variable has changed, 0 if not. */
static int
settings_changed(void)
{
    int i = 0, j;

    /* Machine category */
    i = i || (machine != temp_machine);
    i = i || (cpu_manufacturer != temp_cpu_m);
    i = i || (cpu_waitstates != temp_wait_states);
    i = i || (cpu != temp_cpu);
    i = i || (mem_size != temp_mem_size);
#ifdef USE_DYNAREC
    i = i || (temp_dynarec != cpu_use_dynarec);
#endif
    i = i || (temp_fpu != enable_external_fpu);
    i = i || (temp_sync != enable_sync);

    /* Video category */
    i = i || (video_card != temp_video_card);
    i = i || (video_speed != temp_video_speed);
    i = i || (voodoo_enabled != temp_voodoo);

    /* Input devices category */
    i = i || (mouse_type != temp_mouse);
    i = i || (joystick_type != temp_joystick);

    /* Sound category */
    i = i || (sound_card != temp_sound_card);
    i = i || (midi_device != temp_midi_device);
    i = i || (mpu401_standalone_enable != temp_mpu401);
    i = i || (opl3_type != temp_opl3_type);
    i = i || (sound_is_float != temp_float);

    /* Network category */
    i = i || (network_type != temp_net_type);
    i = i || strcmp(temp_host_dev, network_host);
    i = i || (network_card != temp_net_card);

    /* Ports category */
    for (j = 0; j < SERIAL_MAX; j++)
	i = i || (temp_serial[j] != serial_enabled[j]);
    for (j = 0; j < PARALLEL_MAX; j++) {
	i = i || (temp_parallel[j] != parallel_enabled[j]);
	i = i || (temp_parallel_device[j] != parallel_device[j]);
    }

    /* Peripherals category */
    i = i || (temp_scsi_card != scsi_card);
    i = i || (temp_hdc_type != hdc_type);
    i = i || (temp_ide_ter != ide_enable[2]);
    i = i || (temp_ide_ter_irq != ide_irq[2]);
    i = i || (temp_ide_qua != ide_enable[3]);
    i = i || (temp_ide_qua_irq != ide_irq[3]);
    i = i || (temp_bugger != bugger_enabled);

    /* Hard disks category */
    i = i || memcmp(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (j=0; j<FDD_NUM; j++) {
	i = i || (temp_fdd_types[j] != fdd_get_type(j));
	i = i || (temp_fdd_turbo[j] != fdd_get_turbo(j));
	i = i || (temp_fdd_check_bpb[j] != fdd_get_check_bpb(j));
    }

    /* Other removable devices category */
    i = i || memcmp(cdrom_drives, temp_cdrom_drives, CDROM_NUM * sizeof(cdrom_drive_t));
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
	i = settings_msgbox(MBX_QUESTION, (wchar_t *)IDS_2052);

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

    pc_reset_hard_close();

    /* Machine category */
    machine = temp_machine;
    romset = machine_getromset();
    cpu_manufacturer = temp_cpu_m;
    cpu_waitstates = temp_wait_states;
    cpu = temp_cpu;
    mem_size = temp_mem_size;
#ifdef USE_DYNAREC
    cpu_use_dynarec = temp_dynarec;
#endif
    enable_external_fpu = temp_fpu;
    enable_sync = temp_sync;

    /* Video category */
    video_card = temp_video_card;
    video_speed = temp_video_speed;
    voodoo_enabled = temp_voodoo;

    /* Input devices category */
    mouse_type = temp_mouse;
    joystick_type = temp_joystick;

    /* Sound category */
    sound_card = temp_sound_card;
    midi_device = temp_midi_device;
    mpu401_standalone_enable = temp_mpu401;
    opl3_type = temp_opl3_type;
    sound_is_float = temp_float;

    /* Network category */
    network_type = temp_net_type;
    memset(network_host, '\0', sizeof(network_host));
    strcpy(network_host, temp_host_dev);
    network_card = temp_net_card;

    /* Ports category */
    for (i = 0; i < SERIAL_MAX; i++)
	serial_enabled[i] = temp_serial[i];
    for (i = 0; i < PARALLEL_MAX; i++) {
	parallel_enabled[i] = temp_parallel[i];
	parallel_device[i] = temp_parallel_device[i];
    }

    /* Peripherals category */
    scsi_card = temp_scsi_card;
    hdc_type = temp_hdc_type;
    ide_enable[2] = temp_ide_ter;
    ide_irq[2] = temp_ide_ter_irq;
    ide_enable[3] = temp_ide_qua;
    ide_irq[3] = temp_ide_qua_irq;
    bugger_enabled = temp_bugger;

    /* Hard disks category */
    memcpy(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (i=0; i<FDD_NUM; i++) {
	fdd_set_type(i, temp_fdd_types[i]);
	fdd_set_turbo(i, temp_fdd_turbo[i]);
	fdd_set_check_bpb(i, temp_fdd_check_bpb[i]);
    }

    /* Removable devices category */
    memcpy(cdrom_drives, temp_cdrom_drives, CDROM_NUM * sizeof(cdrom_drive_t));
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


static void
show_child(HWND hwndParent, DWORD child_id)
{
    if (child_id == displayed_category) return;

    displayed_category = child_id;

    SendMessage(hwndChildDialog, WM_SAVESETTINGS, 0, 0);

    DestroyWindow(hwndChildDialog);

    switch(child_id) {
	case PAGE_MACHINE:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_MACHINE,
					       hwndParent, machine_proc);
		break;

	case PAGE_VIDEO:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_VIDEO,
					       hwndParent, video_proc);
		break;

	case PAGE_INPUT:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_INPUT,
					       hwndParent, input_proc);
		break;

	case PAGE_SOUND:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_SOUND,
					       hwndParent, sound_proc);
		break;

	case PAGE_NETWORK:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_NETWORK,
					       hwndParent, network_proc);
		break;

	case PAGE_PORTS:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_PORTS,
					       hwndParent, ports_proc);
		break;

	case PAGE_PERIPHERALS:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_PERIPHERALS,
					       hwndParent, peripherals_proc);
		break;

	case PAGE_HARD_DISKS:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_DISK,
					       hwndParent, disk_proc);
		break;

	case PAGE_FLOPPY_DRIVES:
		hwndChildDialog = CreateDialog(hinstance,
					       (LPCWSTR)DLG_CFG_FLOPPY,
					       hwndParent, floppy_proc);
		break;

	case PAGE_OTHER_REMOVABLE_DEVICES:
		hwndChildDialog = CreateDialog(hinstance,
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
    HICON hiconItem;
    HIMAGELIST hSmall;
    int i = 0;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);

    for (i=0; i<10; i++) {
	hiconItem = LoadIcon(hinstance, (LPCWSTR) (256 + (uintptr_t) i));
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
    int i = 0;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i=0; i<10; i++) {
	lvI.pszText = plat_get_string(IDS_2065+i);
	lvI.iItem = i;
	lvI.iImage = i;

	if (ListView_InsertItem(hwndList, &lvI) == -1)
		return(FALSE);
    }

    return(TRUE);
}


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
settings_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h;
    int category;
    int i = 0;
    int j = 0;

    hwndParentDialog = hdlg;

    switch (message) {
	case WM_INITDIALOG:
		settings_init();

		disk_track_init();
		cdrom_track_init();
		zip_track_init();

		displayed_category = -1;
		h = GetDlgItem(hdlg, IDC_SETTINGSCATLIST);
		image_list_init(h);
		insert_categories(h);
		ListView_SetItemState(h, 0, LVIS_FOCUSED|LVIS_SELECTED, 0x000F);
#if 0
		/*Leave this commented out until we do localization. */
		h = GetDlgItem(hdlg, IDC_COMBO_LANG);	/* This is currently disabled, I am going to add localization options in the future. */
		EnableWindow(h, FALSE);
		ShowWindow(h, SW_HIDE);
		h = GetDlgItem(hdlg, IDS_LANG_ENUS);	/*was:2047 !*/
		EnableWindow(h, FALSE);
		ShowWindow(h, SW_HIDE);
#endif
		return(TRUE);

	case WM_NOTIFY:
		if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_SETTINGSCATLIST)) {
			category = -1;
			for (i=0; i<10; i++) {
				h = GetDlgItem(hdlg, IDC_SETTINGSCATLIST);
				j = ListView_GetItemState(h, i, LVIS_SELECTED);
				if (j) category = i;
			}
			if (category != -1)
				show_child(hdlg, category);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
				SendMessage(hwndChildDialog, WM_SAVESETTINGS, 0, 0);
				if (ask_sure) {
					i = msgbox_reset();
					if (i == 0) {
						/* CANCEL, just kidding! */
						return(FALSE);
					}
				} else {
					i = 2;
				}

				if (i == 2) {
					/* YES, reset system. */
					settings_save();
				}

				DestroyWindow(hwndChildDialog);
				EndDialog(hdlg, i);
				return(TRUE);

			case IDCANCEL:
				DestroyWindow(hwndChildDialog);
				EndDialog(hdlg, 0);
				return(TRUE);
		}
		break;

	default:
		return(FALSE);
    }

    return(FALSE);
}


int
win_settings_open(HWND hwnd, int ask)
{
    int i, m, v;

    /* Enumerate the available machines. */
    m = machine_detect();

    /* Enumerate the available video cards. */
    v = video_detect();

    if (m == 0 || v == 0) {
	ui_msgbox(MBX_ERROR|MBX_FATAL, (wchar_t *)IDS_2056);

	return(0);
    }

    ask_sure = ask;
    i = DialogBox(hinstance, (LPCWSTR)DLG_CONFIG, hwnd, settings_proc);

    return(i);
}
