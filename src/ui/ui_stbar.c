/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Common UI support functions for the Status Bar module.
 *
 * Version:	@(#)ui_stbar.c	1.0.15	2018/10/19
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../machines/machine.h"
#include "../device.h"
#include "../plat.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/disk/hdd.h"
#include "../devices/disk/hdc.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/scsi/scsi_disk.h"
#include "../devices/disk/zip.h"
#include "../devices/cdrom/cdrom.h"
#include "../devices/cdrom/cdrom_image.h"
#include "../devices/network/network.h"
#include "../devices/sound/sound.h"
#include "../devices/video/video.h"
#include "ui.h"
#include "ui_resource.h"


#define USE_SPACER	1			/* include a spacer field */

#define SB_MESSAGE	SB_TEXT|0x00		/* general messages */
#define SB_DEVICE	SB_TEXT|0x01		/* device info (Bugger) */
#define SB_NUMLK	SB_TEXT|0x02		/* NUM LOCK field */
#define SB_CAPLK	SB_TEXT|0x03		/* CAPS LOCK field */


static sbpart_t	*sb_parts = NULL;
static int	sb_nparts = 0;
static int	sb_ready = 0;


/* Find the part in which a given tag lives. */
static int
find_tag(uint8_t tag)
{
    int i;

    if (sb_parts == NULL) return(-1);

    for (i = 0; i < sb_nparts; i++) {
	if (sb_parts[i].tag == tag)
		return(i);
    }

    return(-1);
}


/* Enable or disable a given status bar menu item. */
void
ui_sb_menu_enable_item(uint8_t tag, int idm, int val)
{
    int part;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    sb_menu_enable_item(part, idm, val);
}


/* Set (check) or clear (uncheck) a given status bar menu item. */
void
ui_sb_menu_set_item(uint8_t tag, int idm, int val)
{
    int part;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    sb_menu_set_item(part, idm, val);
}


/* Update one of the icons after activity. */
// FIXME: implement a timer on each icon!
void
ui_sb_icon_update(uint8_t tag, int active)
{
    sbpart_t *ptr;
    int part;

    if ((sb_parts == NULL) || !sb_ready || (sb_nparts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    ptr = &sb_parts[part];
    if ((ptr->flags & ICON_ACTIVE) != active) {
	ptr->flags &= ~ICON_ACTIVE;
	ptr->flags |= (uint8_t)active;

	ptr->icon &= ~(ICON_EMPTY | ICON_ACTIVE);
	ptr->icon |= ptr->flags;

	sb_set_icon(part, ptr->icon);
    }
}


/* Set the 'active' state for an icon. */
void
ui_sb_icon_state(uint8_t tag, int state)
{
    sbpart_t *ptr;
    int part;

    if ((sb_parts == NULL) || !sb_ready || (sb_nparts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    ptr = &sb_parts[part];
    ptr->flags &= ~ICON_EMPTY;
    ptr->flags |= state ? ICON_EMPTY : 0;

    ptr->icon &= ~(ICON_EMPTY | ICON_ACTIVE);
    ptr->icon |= ptr->flags;

    sb_set_icon(part, ptr->icon);
}


/* Set the 'text' field with a given (Unicode) string. */
void
ui_sb_text_set_w(uint8_t tag, const wchar_t *str)
{
    sbpart_t *ptr;
    int part;

    if ((sb_parts == NULL) || !sb_ready || (sb_nparts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    ptr = &sb_parts[part];
    if (ptr->text != NULL)
	free(ptr->text);
    ptr->text = (wchar_t *)mem_alloc(sizeof(wchar_t) * (wcslen(str) + 1));
    wcscpy(ptr->text, str);

    sb_set_text(part, ptr->text);
}


/* Set the 'text' field with a given (ANSI) string. */
void
ui_sb_text_set(uint8_t tag, const char *str)
{
    wchar_t temp[512];

    mbstowcs(temp, str, sizeof_w(temp));

    ui_sb_text_set_w(tag, temp);
}


/* Update the 'tool tip' text on one of the parts. */
void
ui_sb_tip_update(uint8_t tag)
{
    wchar_t tip[512];
    wchar_t temp[512];
    cdrom_t *cdev;
    zip_drive_t *zdev;
    const wchar_t *str;
    const char *stransi;
    sbpart_t *ptr;
    int bus, drive, id;
    int type, part;

    if ((sb_parts == NULL) || (sb_nparts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    ptr = &sb_parts[part];
    switch(tag & 0xf0) {
	case SB_FLOPPY:
		drive = ptr->tag & 0x0f;
		stransi = fdd_getname(fdd_get_type(drive));
		str = floppyfns[drive];
		if (*str == L'\0')
			str = get_string(IDS_3900);		/*"(empty)"*/
		swprintf(tip, sizeof_w(tip),
			 get_string(IDS_3910), drive+1, stransi, str);
		if (ui_writeprot[drive])
			wcscat(tip, get_string(IDS_3902));	/*"[WP]"*/
		break;

	case SB_CDROM:
		drive = ptr->tag & 0x0f;
		cdev = &cdrom[drive];
		bus = cdev->bus_type;
		id = IDS_3580 + (bus - 1);
		wcscpy(temp, get_string(id));
		str = cdev->image_path;
		if (*str == L'\0')
			str = get_string(IDS_3900);		/*"(empty)"*/
		if (cdev->host_drive == 200) {
			swprintf(tip, sizeof_w(tip), get_string(IDS_3920),
				 drive+1, temp, str);
		} else if ((cdev->host_drive >= 'A') &&
			   (cdev->host_drive <= 'Z')) {
			swprintf(temp, sizeof_w(temp), get_string(IDS_3901),
				 cdev->host_drive & ~0x20);
			swprintf(tip, sizeof_w(tip), get_string(IDS_3920),
				 drive+1, get_string(id), temp);
		} else {
			swprintf(tip, sizeof_w(tip), get_string(IDS_3920),
				 drive+1, temp, str);
		}
		if (cdev->is_locked)
			wcscat(tip, get_string(IDS_3903));	/*"[LOCKED]"*/
		break;

	case SB_ZIP:
		drive = ptr->tag & 0x0f;
		zdev = &zip_drives[drive];
		bus = zdev->bus_type;
		id = IDS_3580 + (bus - 1);
		wcscpy(temp, get_string(id));
		type = zdev->is_250 ? 250 : 100;
		str = zdev->image_path;
		if (*str == L'\0')
			str = get_string(IDS_3900);		/*"(empty)"*/
		swprintf(tip, sizeof_w(tip), get_string(IDS_3950),
			 type, drive+1, temp, str);
		if (zdev->ui_writeprot)
			wcscat(tip, get_string(IDS_3902));	/*"[WP]"*/
		break;

	case SB_DISK:
		drive = ptr->tag & 0x0f;
		id = IDS_3515 + hdd[drive].bus - 1;

		str = hdd[drive].fn;
		if (*str == L'\0')
			str = get_string(IDS_3900);		/*"(empty)"*/

		swprintf(tip, sizeof_w(tip),
			 get_string(IDS_3930), drive+1, get_string(id), str);

		if (hdd[drive].wp)
			wcscat(tip, get_string(IDS_3902));	/*"[WP]"*/
		break;

	case SB_NETWORK:
		stransi = network_card_getname(network_card);
		swprintf(tip, sizeof_w(tip), get_string(IDS_3960), stransi);
		break;

	case SB_SOUND:
		stransi = sound_card_getname(sound_card);
		swprintf(tip, sizeof_w(tip), get_string(IDS_3970), stransi);
		break;

	default:
		break;
    }

    if (ptr->tip != NULL)
	free(ptr->tip);
    ptr->tip = (wchar_t *)mem_alloc(sizeof(wchar_t) * (wcslen(tip) + 1));
    wcscpy(ptr->tip, tip);

    sb_set_tooltip(part, ptr->tip);
}


/* Create the "hard disk" menu. */
static void
menu_disk(int part, int drive)
{
    sb_menu_add_item(part, IDM_DISK_NOTIFY|drive, get_string(IDS_3908));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_DISK_IMAGE_NEW|drive, get_string(IDS_3904));
    sb_menu_add_item(part, IDM_DISK_IMAGE_EXIST|drive, get_string(IDS_3905));
    sb_menu_add_item(part, IDM_DISK_RELOAD|drive, get_string(IDS_3906));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_DISK_EJECT|drive, get_string(IDS_3907));
}


/* Create the "Floppy drive" menu. */
static void
menu_floppy(int part, int drive)
{
    sb_menu_add_item(part, IDM_FLOPPY_IMAGE_NEW|drive, get_string(IDS_3904));
    sb_menu_add_item(part, IDM_FLOPPY_IMAGE_EXIST|drive,
		     get_string(IDS_3905));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_FLOPPY_EJECT|drive, get_string(IDS_3907));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_FLOPPY_EXPORT|drive,
		     get_string(IDS_3914));

    if (floppyfns[drive][0] == L'\0') {
	sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, 0);
	sb_menu_enable_item(part, IDM_FLOPPY_EXPORT | drive, 0);
    }
}


/* Update the "CD-ROM drive" menu. */
static void
menu_cdrom_update(int part, int drive)
{
    cdrom_t *dev = &cdrom[drive];

    if (dev->can_lock) {
	if (dev->is_locked) {
		sb_menu_set_item(part, IDM_CDROM_LOCK | drive, 1);
		sb_menu_enable_item(part, IDM_CDROM_LOCK | drive, 0);
		sb_menu_set_item(part, IDM_CDROM_UNLOCK | drive, 0);
		sb_menu_enable_item(part, IDM_CDROM_UNLOCK | drive, 1);
	} else {
		sb_menu_set_item(part, IDM_CDROM_UNLOCK | drive, 1);
		sb_menu_enable_item(part, IDM_CDROM_UNLOCK | drive, 0);
		sb_menu_set_item(part, IDM_CDROM_LOCK | drive, 0);
		sb_menu_enable_item(part, IDM_CDROM_LOCK | drive, 1);
	}
    } else {
	sb_menu_set_item(part, IDM_CDROM_LOCK | drive, 0);
	sb_menu_enable_item(part, IDM_CDROM_LOCK | drive, 0);
	sb_menu_set_item(part, IDM_CDROM_UNLOCK | drive, 0);
	sb_menu_enable_item(part, IDM_CDROM_UNLOCK | drive, 0);
    }
}


/* Create the "Optical Drive" menu. */
static void
menu_cdrom(int part, int drive)
{
    cdrom_t *dev = &cdrom[drive];
#ifdef USE_HOST_CDROM
    wchar_t temp[64];
    int i;
#endif

    sb_menu_add_item(part, IDM_CDROM_MUTE | drive, get_string(IDS_3923));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_CDROM_IMAGE | drive, get_string(IDS_3905));
    sb_menu_add_item(part, IDM_CDROM_RELOAD | drive, get_string(IDS_3906));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_CDROM_EMPTY | drive, get_string(IDS_3907));

#ifdef USE_HOST_CDROM
    if (cdrom_host_drive_available_num == 0) {
#endif
	if ((dev->host_drive >= 'A') && (dev->host_drive <= 'Z'))
		dev->host_drive = 0;

#ifdef USE_HOST_CDROM
	goto check_items;
    } else {
	if ((dev->host_drive >= 'A') &&
	    (dev->host_drive <= 'Z')) {
		if (! cdrom_host_drive_available[dev->host_drive - 'A']) {
			dev->host_drive = 0;
		}
	}
    }

    sb_menu_add_item(part, -1, NULL);

    /* Add entries for all available host drives. */
    for (i = 3; i < 26; i++) {
	swprintf(temp, sizeof_w(temp), get_string(IDS_3921), i+'A');
	if (cdrom_host_drive_available[i])
		sb_menu_add_item(part, IDM_CDROM_HOST_DRIVE | (i << 3)|drive, temp);
    }
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_CDROM_LOCK | drive, get_string(IDS_LOCK));
    sb_menu_enable_item(part, IDM_CDROM_LOCK | drive, 0);
    sb_menu_add_item(part, IDM_CDROM_UNLOCK | drive, get_string(IDS_UNLOCK));
    sb_menu_enable_item(part, IDM_CDROM_UNLOCK | drive, 0);

check_items:
#endif
    if (! dev->sound_on)
	sb_menu_set_item(part, IDM_CDROM_MUTE | drive, 1);

    if (dev->host_drive == 200)
	sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 1);
      else
    if ((dev->host_drive >= 'A') &&
	(dev->host_drive <= 'Z')) {
	sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive |
		((dev->host_drive - 'A') << 3), 1);
    } else {
	dev->host_drive = 0;
	sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 1);
    }

    menu_cdrom_update(part, drive);
}


/* Create the "ZIP drive" menu. */
static void
menu_zip(int part, int drive)
{
    sb_menu_add_item(part, IDM_ZIP_IMAGE_NEW|drive, get_string(IDS_3904));
    sb_menu_add_item(part, IDM_ZIP_IMAGE_EXIST|drive, get_string(IDS_3905));
    sb_menu_add_item(part, IDM_ZIP_RELOAD|drive, get_string(IDS_3906));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_ZIP_EJECT|drive, get_string(IDS_3907));

    if (zip_drives[drive].image_path[0] == L'\0') {
	sb_menu_enable_item(part, IDM_ZIP_EJECT | drive, 0);
	sb_menu_enable_item(part, IDM_ZIP_RELOAD | drive, 1);
    } else {
	sb_menu_enable_item(part, IDM_ZIP_EJECT | drive, 1);
	sb_menu_enable_item(part, IDM_ZIP_RELOAD | drive, 0);
    }
}


/* Initialize or update the entire status bar. */
void
ui_sb_reset(void)
{
    int drive, hdint, part;
    int do_net, do_sound;
    const char *hdc;
    sbpart_t *ptr;

    sb_ready = 0;

    /* First, remove any existing parts of the status bar. */
    if (sb_nparts > 0) {
	for (part = 0; part < sb_nparts; part++) {
		if (sb_parts[part].tip != NULL)
			free(sb_parts[part].tip);
	}

	/* Now release the entire array. */
	free(sb_parts);

	/* OK, all cleaned up. */
	sb_nparts = 0;
    }

    hdint = (machines[machine].flags & MACHINE_HDC) ? 1 : 0;
    do_net = network_available();
    do_sound = !!(sound_card != 0);

    /* Get name of current HDC. */
    hdc = hdc_get_internal_name(hdc_type);

    /* Count all the floppy drives. */
    for (drive = 0; drive < FDD_NUM; drive++) {
	if (fdd_get_type(drive) != 0)
		sb_nparts++;
    }

    /* Check all (active) hard disks. */
    for (drive = 0; drive < HDD_NUM; drive++) {
	if ((hdd[drive].bus == HDD_BUS_IDE) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		/* Disk, but no controller for it. */
		continue;
	}

	if ((hdd[drive].bus == HDD_BUS_SCSI) && (scsi_card == 0)) {
		/* Disk, but no controller for it. */
		continue;
	}

	if (hdd[drive].bus != HDD_BUS_DISABLED)
		sb_nparts++;
    }

    for (drive = 0; drive < CDROM_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((cdrom[drive].bus_type == CDROM_BUS_ATAPI) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		continue;
	}

	if ((cdrom[drive].bus_type == CDROM_BUS_SCSI) && (scsi_card == 0)) {
		continue;
	}

	if (cdrom[drive].bus_type != CDROM_BUS_DISABLED)
		sb_nparts++;
    }

    for (drive = 0; drive < ZIP_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((zip_drives[drive].bus_type == ZIP_BUS_ATAPI) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		continue;
	}

	if ((zip_drives[drive].bus_type == ZIP_BUS_SCSI) && (scsi_card == 0))
		continue;

	if (zip_drives[drive].bus_type != CDROM_BUS_DISABLED)
		sb_nparts++;
    }

    if (do_net)
	sb_nparts++;

    if (do_sound)
	sb_nparts++;

#ifdef USE_SPACER
    /* A spacer. */
    sb_nparts++;
#endif

    /* For devices like ISAbugger. */
    if (bugger_enabled)
	sb_nparts++;

    /* General text message field. */
    sb_nparts++;

    /* Keyboard fields. */
    sb_nparts += 2;

    /* Now (re-)allocate the parts. */
    sb_parts = (sbpart_t *)mem_alloc(sb_nparts * sizeof(sbpart_t));
    memset(sb_parts, 0x00, sb_nparts * sizeof(sbpart_t));

    /* Start populating the fields at the left edge of the window. */
    sb_nparts = 0;

    for (drive = 0; drive < FDD_NUM; drive++) {
	if (fdd_get_type(drive) != 0) {
		ptr = &sb_parts[sb_nparts++];
		ptr->width = SB_ICON_WIDTH;
		ptr->tag = SB_FLOPPY | drive;
	}
    }

    for (drive = 0; drive < HDD_NUM; drive++) {
	if ((hdd[drive].bus == HDD_BUS_IDE) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		/* Disk, but no controller for it. */
		continue;
	}

	if ((hdd[drive].bus == HDD_BUS_SCSI) && (scsi_card == 0)) {
		/* Disk, but no controller for it. */
		continue;
	}

	if (hdd[drive].bus != HDD_BUS_DISABLED) {
		ptr = &sb_parts[sb_nparts++];
		ptr->width = SB_ICON_WIDTH;
		ptr->tag = SB_DISK | drive;
	}
    }

    for (drive = 0; drive < CDROM_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((cdrom[drive].bus_type == CDROM_BUS_ATAPI) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	if ((cdrom[drive].bus_type == CDROM_BUS_SCSI) && (scsi_card == 0))
		continue;

	if (cdrom[drive].bus_type != CDROM_BUS_DISABLED) {
		ptr = &sb_parts[sb_nparts++];
		ptr->width = SB_ICON_WIDTH;
		ptr->tag = SB_CDROM | drive;
	}
    }

    for (drive = 0; drive < ZIP_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((zip_drives[drive].bus_type == ZIP_BUS_ATAPI) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	if ((zip_drives[drive].bus_type == ZIP_BUS_SCSI) && (scsi_card == 0))
		continue;

	if (zip_drives[drive].bus_type != 0) {
		ptr = &sb_parts[sb_nparts++];
		ptr->width = SB_ICON_WIDTH;
		ptr->tag = SB_ZIP | drive;
	}
    }

    if (do_net) {
	ptr = &sb_parts[sb_nparts++];
	ptr->width = SB_ICON_WIDTH;
	ptr->tag = SB_NETWORK;
    }

    /* Initialize the Sound field. */
    if (do_sound) {
	ptr = &sb_parts[sb_nparts++];
	ptr->width = SB_ICON_WIDTH;
	ptr->tag = SB_SOUND;
    }

#ifdef USE_SPACER
    /* A spacer. */
    ptr = &sb_parts[sb_nparts++];
    ptr->width = 10;
    ptr->tag = 0xff;
    ptr->icon = 255;
#endif

    /* Fun! */
    if (bugger_enabled) {
	/* Add a text field for the ISAbugger. */
	ptr = &sb_parts[sb_nparts++];
	ptr->width = 175;
	ptr->tag = SB_TEXT|1;
	ptr->icon = 255;
    }

    /* Add a text field and finalize the field settings. */
    ptr = &sb_parts[sb_nparts++];
    ptr->width = 0;
    ptr->tag = SB_TEXT;
    ptr->icon = 255;

    /* Add the two keyboard-state indicators. */
    ptr = &sb_parts[sb_nparts++];
    ptr->width = 40;
    ptr->tag = SB_NUMLK;
    ptr->icon = 255;
    ptr = &sb_parts[sb_nparts++];
    ptr->width = 45;
    ptr->tag = SB_CAPLK;
    ptr->icon = 255;

    /* Set up the platform status bar. */
    sb_setup(sb_nparts, sb_parts);

    /* Initialize each status bar part. */
    for (part = 0; part < sb_nparts; part++) {
	ptr = &sb_parts[part];

	switch (ptr->tag & 0xf0) {
		case SB_FLOPPY:		/* Floppy */
			drive = ptr->tag & 0x0f;
			ptr->flags = (wcslen(floppyfns[drive]) == 0) ? ICON_EMPTY : 0;
			ptr->icon = (uint8_t)ui_fdd_icon(fdd_get_type(drive)) | ptr->flags;
			sb_menu_create(part);
			menu_floppy(part, drive);
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_CDROM:		/* CD-ROM */
			drive = ptr->tag & 0x0f;
			if (cdrom[drive].host_drive == 200) {
				ptr->flags = (wcslen(cdrom[drive].image_path) == 0) ? ICON_EMPTY : 0;
			} else if ((cdrom[drive].host_drive >= 'A') && (cdrom[drive].host_drive <= 'Z')) {
				ptr->flags = 0;
			} else {
				ptr->flags = ICON_EMPTY;
			}
			//FIXME: different icons for CD,DVD,BD ?
			ptr->icon = (uint8_t)(ICON_CDROM | ptr->flags);
			sb_menu_create(part);
			menu_cdrom(part, drive);
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_ZIP:		/* Iomega ZIP */
			drive = ptr->tag & 0x0f;
			ptr->flags = (wcslen(zip_drives[drive].image_path) == 0) ? ICON_EMPTY : 0;
			ptr->icon = (uint8_t)(ICON_ZIP + ptr->flags);
			sb_menu_create(part);
			menu_zip(part, drive);
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_DISK:		/* Hard disk */
			drive = ptr->tag & 0x0f;
			ptr->flags = (wcslen(hdd[drive].fn) == 0) ? ICON_EMPTY : 0;
			ptr->icon = (uint8_t)(ICON_DISK + ptr->flags);
			sb_menu_create(part);
			if (hdd[drive].removable)
				menu_disk(part, drive);
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_NETWORK:	/* Network */
			ptr->icon = ICON_NETWORK;
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_SOUND:		/* Sound */
			ptr->icon = ICON_SOUND;
			ui_sb_tip_update(ptr->tag);
			break;

		case SB_TEXT:		/* Status text */
			switch(ptr->tag & 0x03) {
				case 0:		/* message field */
				case 1:		/* Bugger/Info field */
					sb_set_text(part, L"");
					break;

				case 2:		/* NUM keyboard flag */
				case 3:		/* CAP keyboard flag */
					drive = keyboard_get_state();
					ui_sb_kbstate(drive);
					break;
			}
			break;
	}

	/* Set up the correct icon for this part. */
	if (ptr->icon != 255)
		sb_set_icon(part, ptr->icon);
    }

    /* The status bar is now ready. */
    sb_ready = 1;

    /* Set a default text message. */
    ui_sb_text_set_w(SB_TEXT, L"Welcome to VARCem !!");
}


/* Handle a command from one of the status bar menus. */
void
ui_sb_menu_command(int idm, uint8_t tag)
{
    wchar_t temp[512];
    cdrom_t *cdev;
    wchar_t *str;
    int drive;
    int part;
    int i;

    switch (idm) {
	case IDM_FLOPPY_IMAGE_NEW:
		drive = tag & 0x03;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		dlg_new_image(drive, part, 0);
		break;

	case IDM_FLOPPY_IMAGE_EXIST:
		drive = tag & 0x03;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		str = floppyfns[drive];
		i = dlg_file(get_string(IDS_3911), str, temp, DLG_FILE_LOAD);
		if (i)  {
			ui_floppy_mount(drive, part,
					!!(i & DLG_FILE_RO), temp);
		}
		break;

	case IDM_FLOPPY_EJECT:
		drive = tag & 0x03;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		fdd_close(drive);
		ui_sb_icon_state(SB_FLOPPY | drive, 1);
		sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, 0);
		sb_menu_enable_item(part, IDM_FLOPPY_EXPORT | drive, 0);
		ui_sb_tip_update(SB_FLOPPY | drive);
		config_save();
		break;

	case IDM_FLOPPY_EXPORT:
		drive = tag & 0x03;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		str = floppyfns[drive];
		if (dlg_file(get_string(IDS_3913), str, temp, DLG_FILE_SAVE)) {
			pc_pause(1);
			if (! d86f_export(drive, temp))
				ui_msgbox(MBX_ERROR, (wchar_t *)IDS_OPEN_WRITE);
			pc_pause(0);
		}
		break;

	case IDM_CDROM_MUTE:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		cdev = &cdrom[drive];
		if (part == -1) break;

		cdev->sound_on ^= 1;
		sb_menu_set_item(part, IDM_CDROM_MUTE | drive, cdev->sound_on);
		config_save();
		sound_cd_stop();
		break;

	case IDM_CDROM_EMPTY:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		if (part == -1) break;

		ui_cdrom_eject(drive);
		menu_cdrom_update(part, drive);
		break;

	case IDM_CDROM_RELOAD:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		if (part == -1) break;

		ui_cdrom_reload(drive);
		menu_cdrom_update(part, drive);
		break;

	case IDM_CDROM_IMAGE:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		cdev = &cdrom[drive];
		if (part == -1) break;

		/* Browse for a new image to use. */
		str = cdev->image_path;
		if (! dlg_file(get_string(IDS_3922), str, temp,
			       DLG_FILE_LOAD|DLG_FILE_RO)) break;

		/* Save current drive/pathname for later re-use. */
		cdev->prev_host_drive = cdev->host_drive;
		if (! cdev->prev_image_path)
			cdev->prev_image_path = (wchar_t *)mem_alloc(1024 * sizeof(wchar_t));
		wcscpy(cdev->prev_image_path, str);

		/* Close the current drive/pathname. */
		if (cdev->ops && cdev->ops->close)
			cdev->ops->close(cdev);
		memset(cdev->image_path, 0, sizeof(cdev->image_path));

		/* Now open new image. */
		cdrom_image_open(cdev, temp);

		/* Signal media change to the emulated machine. */
		cdrom_insert(drive);

		/* Update stuff. */
		sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
		if ((cdev->host_drive >= 'A') && (cdev->host_drive <= 'Z')) {
			sb_menu_set_item(part,
					 IDM_CDROM_HOST_DRIVE | drive | ((cdev->host_drive - 'A') << 3),
					 0);
		}
		cdev->host_drive = (wcslen(cdev->image_path) == 0) ? 0 : 200;
		if (cdev->host_drive == 200) {
			sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 1);
			ui_sb_icon_state(SB_CDROM | drive, 0);
		} else {
			sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 0);
			sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
			ui_sb_icon_state(SB_CDROM | drive, 1);
		}
		sb_menu_enable_item(part, IDM_CDROM_RELOAD | drive, 0);
		menu_cdrom_update(part, drive);
		ui_sb_icon_state(SB_CDROM | drive, 0);
		ui_sb_tip_update(SB_CDROM | drive);
		ui_sb_tip_update(SB_CDROM | drive);
		config_save();
		break;

	case IDM_CDROM_LOCK:
	case IDM_CDROM_UNLOCK:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		cdev = &cdrom[drive];
		if (part == -1) break;

		if (cdev->ops && cdev->ops->medium_lock)
			cdev->ops->medium_lock(cdev, (idm == IDM_CDROM_LOCK) ? 1 : 0);
		menu_cdrom_update(part, drive);
		ui_sb_tip_update(SB_CDROM | drive);
		break;

	case IDM_CDROM_HOST_DRIVE:
		drive = tag & 0x07;
		part = find_tag(SB_CDROM | drive);
		i = ((tag >> 3) & 0x001f) + 'A';
		cdev = &cdrom[drive];
		if (part == -1) break;

		if (cdev->host_drive == i) {
			/* Switching to the same drive. Do nothing. */
			break;
		}
		cdev->prev_host_drive = cdev->host_drive;

		/* Close the current drive/pathname. */
		if (cdev->ops && cdev->ops->close)
			cdev->ops->close(cdev);
		memset(cdev->image_path, 0, sizeof(cdev->image_path));
		if ((cdev->host_drive >= 'A') && (cdev->host_drive <= 'Z'))
			sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive |
					((cdev->host_drive - 'A') << 3), 0);

		cdev->host_drive = i;
#ifdef USE_HOST_CDROM
		cdrom_host_open(cdev, i);
#endif

		/* Signal media change to the emulated machine. */
		cdrom_insert(drive);

		/* Update stuff. */
		sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
		sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 0);
		sb_menu_enable_item(part, IDM_CDROM_RELOAD | drive, 0);
		if ((cdev->host_drive >= 'A') && (cdev->host_drive <= 'Z'))
			sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive |
					((cdev->host_drive - 'A') << 3), 0);
		menu_cdrom_update(part, drive);
		ui_sb_icon_state(SB_CDROM | drive, 0);
		ui_sb_tip_update(SB_CDROM | drive);
		config_save();
		break;

	case IDM_ZIP_IMAGE_NEW:
		drive = tag & 0x03;
		part = find_tag(SB_ZIP | drive);
		if (part == -1) break;

		dlg_new_image(drive, part, 1);
		break;

	case IDM_ZIP_IMAGE_EXIST:
		drive = tag & 0x03;
		part = find_tag(SB_ZIP | drive);
		if (part == -1) break;

		str = zip_drives[drive].image_path;
		i = dlg_file(get_string(IDS_3951), str, temp, DLG_FILE_LOAD);
		if (i) {
			ui_zip_mount(drive, part,
				     !!(i & DLG_FILE_RO), temp);
		}
		break;

	case IDM_ZIP_EJECT:
		drive = tag & 0x03;
		ui_zip_eject(drive);
		break;

	case IDM_ZIP_RELOAD:
		drive = tag & 0x03;
		ui_zip_reload(drive);
		break;

	case IDM_DISK_NOTIFY:
		drive = tag & 0x0f;
#if 0
		disk_insert(drive);
#endif
		break;

	case IDM_DISK_IMAGE_NEW:
		drive = tag & 0x0f;
		part = find_tag(SB_DISK | drive);
		if (part == -1) break;

		dlg_new_image(drive, part, 1);
		break;

	case IDM_DISK_IMAGE_EXIST:
		drive = tag & 0x0f;
		part = find_tag(SB_DISK | drive);
		if (part == -1) break;

		str = hdd[drive].fn;
		i = dlg_file(get_string(IDS_3536), str, temp, DLG_FILE_LOAD);
		if (! i) break;

		ui_disk_mount(drive, part, !!(i & DLG_FILE_RO), temp);
		break;

	case IDM_DISK_EJECT:
		drive = tag & 0x0f;
#if 0
		ui_disk_eject(drive);
#endif
		break;

	case IDM_DISK_RELOAD:
		drive = tag & 0x0f;
#if 0
		ui_disk_reload(drive);
#endif
		break;
    }
}


/* Handle a "double-click" on one of the icons. */
void
ui_sb_click(int part)
{
    if (part >= sb_nparts) return;

    switch(sb_parts[part].tag & 0xf0) {
	case SB_SOUND:
		dlg_sound_gain();
		break;
    }
}


/* Handle a keyboard state change. */
void
ui_sb_kbstate(int flags)
{
    int part;

    part = find_tag(SB_NUMLK);
    if (flags & KBD_FLAG_NUM)
	sb_set_text(part, L"NUM");
    else
	sb_set_text(part, L"");

    part = find_tag(SB_CAPLK);
    if (flags & KBD_FLAG_CAPS)
	sb_set_text(part, L"CAPS");
    else
	sb_set_text(part, L"");
}
