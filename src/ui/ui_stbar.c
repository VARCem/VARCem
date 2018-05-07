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
 * Version:	@(#)ui_stbar.c	1.0.4	2018/05/06
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
#include "../devices/floppy/fdd.h"
#include "../devices/disk/hdd.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/zip.h"
#include "../devices/cdrom/cdrom.h"
#include "../devices/cdrom/cdrom_image.h"
#include "../devices/cdrom/cdrom_null.h"
#include "../devices/scsi/scsi.h"
#include "../devices/scsi/scsi_disk.h"
#include "../devices/network/network.h"
#include "../devices/sound/sound.h"
#include "../devices/video/video.h"
#include "ui.h"
#include "ui_resource.h"


static int	*sb_widths;
static int	*sb_flags;
static int	*sb_tags;
static int	*sb_icons;
static wchar_t	**sb_tips;

static int	sb_parts = 0;
static int	sb_ready = 0;


/* Find the part in which a given tag lives. */
static int
find_tag(int tag)
{
    int part;

    if (sb_tags == NULL) return(-1);

    for (part = 0; part < sb_parts; part++) {
	if (sb_tags[part] == tag)
		return(part);
    }

    return(-1);
}


/* Enable or disable a given status bar menu item. */
void
ui_sb_menu_enable_item(int tag, int idm, int val)
{
    int part;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    sb_menu_enable_item(part, idm, val);
}


/* Set (check) or clear (uncheck) a given status bar menu item. */
void
ui_sb_menu_set_item(int tag, int idm, int val)
{
    int part;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    sb_menu_set_item(part, idm, val);
}


/* Update one of the icons after activity. */
// FIXME: implement a timer on each icon!
void
ui_sb_icon_update(int tag, int active)
{
    int part;

    if (! update_icons) return;

    if (((tag & 0xf0) >= SB_TEXT) || !sb_ready || (sb_parts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    if ((sb_flags[part] & 1) != active) {
	sb_flags[part] &= ~1;
	sb_flags[part] |= active;

	sb_icons[part] &= ~257;
	sb_icons[part] |= sb_flags[part];

	sb_set_icon(part, sb_icons[part]);
    }
}


/* Set the 'active' state for an icon. */
void
ui_sb_icon_state(int tag, int state)
{
    int part;

    if (((tag & 0xf0) >= SB_HDD) || !sb_ready ||
	(sb_parts == 0) || (sb_flags == NULL) || (sb_icons == NULL)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    sb_flags[part] &= ~256;
    sb_flags[part] |= state ? 256 : 0;

    sb_icons[part] &= ~257;
    sb_icons[part] |= sb_flags[part];

    sb_set_icon(part, sb_icons[part]);
}


/* Set the 'text' field with a given (Unicode) string. */
void
ui_sb_text_set_w(const wchar_t *str)
{
    int part;

    if (!sb_ready || (sb_parts == 0)) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(SB_TEXT)) == -1) return;

    sb_set_text(part, str);
}


/* Set the 'text' field with a given (ANSI) string. */
void
ui_sb_text_set(const char *str)
{
    static wchar_t temp[512];

    mbstowcs(temp, str, sizeof_w(temp));

    ui_sb_text_set_w(temp);
}


/* Update the 'tool tip' text on one of the parts. */
void
ui_sb_tip_update(int tag)
{
    wchar_t tip[512];
    wchar_t temp[512];
    wchar_t *str;
    const char *stransi;
    int bus, drive, id;
    int type, part;

    if (sb_tags == NULL) return;

    /* Find the status bar part for this tag. */
    if ((part = find_tag(tag)) == -1) return;

    switch(tag & 0xf0) {
	case SB_FLOPPY:
		drive = sb_tags[part] & 0x0f;
		stransi = fdd_getname(fdd_get_type(drive));
		mbstowcs(temp, stransi, sizeof_w(temp));
		str = floppyfns[drive];
		if (*str == L'\0')
			str = plat_get_string(IDS_2057);	/*"empty"*/
		swprintf(tip, sizeof_w(tip),
			 plat_get_string(IDS_2158), drive+1, temp, str);
		break;

	case SB_CDROM:
		drive = sb_tags[part] & 0x0f;
		bus = cdrom_drives[drive].bus_type;
		id = IDS_4352 + (bus - 1);
		wcscpy(temp, plat_get_string(id));
		str = cdrom_image[drive].image_path;
		if (*str == L'\0')
			str = plat_get_string(IDS_2057);	/*"empty"*/
		if (cdrom_drives[drive].host_drive == 200) {
			swprintf(tip, sizeof_w(tip),
				 plat_get_string(IDS_5120),
				 drive+1, temp, str);
		} else if ((cdrom_drives[drive].host_drive >= 'A') &&
			   (cdrom_drives[drive].host_drive <= 'Z')) {
			swprintf(temp, sizeof_w(temp),
				 plat_get_string(IDS_2058),
				 cdrom_drives[drive].host_drive & ~0x20);
			swprintf(tip, sizeof_w(tip),
				 plat_get_string(IDS_5120),
				 drive+1, plat_get_string(id), temp);
		} else {
			swprintf(tip, sizeof_w(tip),
				 plat_get_string(IDS_5120),
				 drive+1, temp, str);
		}
		break;

	case SB_ZIP:
		drive = sb_tags[part] & 0x0f;
		type = zip_drives[drive].is_250 ? 250 : 100;
		str = zip_drives[drive].image_path;
		if (*str == L'\0')
			str = plat_get_string(IDS_2057);	/*"empty"*/
		swprintf(tip, sizeof_w(tip),
			 plat_get_string(IDS_2177), drive+1, type, str);
		break;

	case SB_RDISK:
		drive = sb_tags[part] & 0x1f;
		str = hdd[drive].fn;
		if (*str == L'\0')
			str = plat_get_string(IDS_2057);	/*"empty"*/
		swprintf(tip, sizeof_w(tip),
			 plat_get_string(IDS_4115), drive, str);
		break;

	case SB_HDD:
		bus = sb_tags[part] & 0x0f;
		id = IDS_4352 + (bus - 1);
		str = plat_get_string(id);
		swprintf(tip, sizeof_w(tip),
			 plat_get_string(IDS_4096), str);
		break;

	case SB_NETWORK:
		swprintf(tip, sizeof_w(tip), plat_get_string(IDS_2069));
		break;

	case SB_SOUND:
		swprintf(tip, sizeof_w(tip), plat_get_string(IDS_2068));
		break;

	default:
		break;
    }

    if (sb_tips[part] != NULL)
	free(sb_tips[part]);

    sb_tips[part] = (wchar_t *)malloc(sizeof(wchar_t) * (wcslen(tip) + 1));
    wcscpy(sb_tips[part], tip);

    sb_set_tooltip(part, sb_tips[part]);
}


void
ui_sb_tip_destroy(void)
{
    int part;

    if ((sb_parts == 0) || (sb_tips == NULL)) return;

    for (part = 0; part < sb_parts; part++) {
	if (sb_tips[part] != NULL)
		free(sb_tips[part]);
    }

    free(sb_tips);

    sb_tips = NULL;
}


/* Create the "Floppy drive" menu. */
static void
menu_floppy(int part, int drive)
{
    sb_menu_add_item(part, IDM_FLOPPY_IMAGE_NEW | drive,
		     plat_get_string(IDS_2161));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_FLOPPY_IMAGE_EXISTING | drive,
		     plat_get_string(IDS_2162));
    sb_menu_add_item(part, IDM_FLOPPY_IMAGE_EXISTING_WP | drive,
		     plat_get_string(IDS_2163));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_FLOPPY_EXPORT_TO_86F | drive,
		     plat_get_string(IDS_2172));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_FLOPPY_EJECT | drive,
		     plat_get_string(IDS_2164));

    if (floppyfns[drive][0] == L'\0') {
	sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, 0);
	sb_menu_enable_item(part, IDM_FLOPPY_EXPORT_TO_86F | drive, 0);
    }
}


/* Create the "CD-ROM drive" menu. */
static void
menu_cdrom(int part, int drive)
{
    wchar_t temp[64];
    int i;

    sb_menu_add_item(part, IDM_CDROM_MUTE | drive,
		     plat_get_string(IDS_2165));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_CDROM_EMPTY | drive,
		     plat_get_string(IDS_2166));
    sb_menu_add_item(part, IDM_CDROM_RELOAD | drive,
		     plat_get_string(IDS_2167));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_CDROM_IMAGE | drive,
		     plat_get_string(IDS_2168));

    if (host_cdrom_drive_available_num == 0) {
	if ((cdrom_drives[drive].host_drive >= 'A') &&
	    (cdrom_drives[drive].host_drive <= 'Z')) {
		cdrom_drives[drive].host_drive = 0;
	}

	goto check_menu_items;
    } else {
	if ((cdrom_drives[drive].host_drive >= 'A') &&
	    (cdrom_drives[drive].host_drive <= 'Z')) {
		if (! host_cdrom_drive_available[cdrom_drives[drive].host_drive - 'A']) {
			cdrom_drives[drive].host_drive = 0;
		}
	}
    }

    sb_menu_add_item(part, -1, NULL);

    for (i = 3; i < 26; i++) {
	swprintf(temp, sizeof_w(temp), L"Host CD/DVD Drive (%c:)", i+'A');
	if (host_cdrom_drive_available[i])
		sb_menu_add_item(part, IDM_CDROM_HOST_DRIVE | (i << 3)|drive, temp);
    }

check_menu_items:
    if (! cdrom_drives[drive].sound_on)
	sb_menu_set_item(part, IDM_CDROM_MUTE | drive, 1);

    if (cdrom_drives[drive].host_drive == 200)
	sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 1);
      else
    if ((cdrom_drives[drive].host_drive >= 'A') &&
	(cdrom_drives[drive].host_drive <= 'Z')) {
	sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive |
		((cdrom_drives[drive].host_drive - 'A') << 3), 1);
    } else {
	cdrom_drives[drive].host_drive = 0;
	sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 1);
    }
}


/* Create the "ZIP drive" menu. */
static void
menu_zip(int part, int drive)
{
    sb_menu_add_item(part, IDM_ZIP_IMAGE_NEW | drive,
		     plat_get_string(IDS_2161));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_ZIP_IMAGE_EXISTING | drive,
		     plat_get_string(IDS_2162));
    sb_menu_add_item(part, IDM_ZIP_IMAGE_EXISTING_WP | drive,
		     plat_get_string(IDS_2163));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_ZIP_EJECT | drive, plat_get_string(IDS_2164));
    sb_menu_add_item(part, IDM_ZIP_RELOAD | drive, plat_get_string(IDS_2167));

    if (zip_drives[drive].image_path[0] == L'\0') {
	sb_menu_enable_item(part, IDM_ZIP_EJECT | drive, 0);
	sb_menu_enable_item(part, IDM_ZIP_RELOAD | drive, 1);
    } else {
	sb_menu_enable_item(part, IDM_ZIP_EJECT | drive, 1);
	sb_menu_enable_item(part, IDM_ZIP_RELOAD | drive, 0);
    }
}


/* Create the "Removable Media" menu. */
static void
menu_remov(int part, int drive)
{
    sb_menu_add_item(part, IDM_RDISK_EJECT | drive,
		     plat_get_string(IDS_2164));
    sb_menu_add_item(part, IDM_RDISK_RELOAD | drive,
		     plat_get_string(IDS_2167));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_RDISK_SEND_CHANGE | drive,
		     plat_get_string(IDS_2142));
    sb_menu_add_item(part, -1, NULL);
    sb_menu_add_item(part, IDM_RDISK_IMAGE | drive,
		     plat_get_string(IDS_2168));
    sb_menu_add_item(part, IDM_RDISK_IMAGE_WP | drive,
		     plat_get_string(IDS_2169));
}


/* Initialize or update the entire status bar. */
void
ui_sb_update(void)
{
    int c_st506, c_esdi, c_scsi;
    int c_ide_pio, c_ide_dma;
    int do_net, edge = 0;
    int drive, part;
    int hdint;
    const char *hdc;

    sb_ready = 0;

    /* First, remove any existing parts of the status bar. */
    if (sb_parts > 0) {
	/* Clear all the current icons from the status bar. */
	for (part = 0; part < sb_parts; part++)
		sb_set_icon(part, -1);

	/* Reset the status bar to 0 parts. */
	sb_setup(0, NULL);

	if (sb_widths != NULL)
		free(sb_widths);
	if (sb_tags != NULL)
		free(sb_tags);
	if (sb_icons != NULL)
		free(sb_icons);
	if (sb_flags != NULL)
		free(sb_flags);

	/* Destroy all existing tool tips. */
	ui_sb_tip_destroy();

	/* Destroy all current menus. */
	sb_menu_destroy();
    }

    /* OK, all cleaned up. */
    sb_parts = 0;

    /* Calculate the number of part classes. */
    hdint = (machines[machine].flags & MACHINE_HDC) ? 1 : 0;
    c_st506 = hdd_count(HDD_BUS_ST506);
    c_esdi = hdd_count(HDD_BUS_ESDI);
    c_ide_pio = hdd_count(HDD_BUS_IDE_PIO_ONLY);
    c_ide_dma = hdd_count(HDD_BUS_IDE_PIO_AND_DMA);
    c_scsi = hdd_count(HDD_BUS_SCSI);
    do_net = network_available();

    for (drive = 0; drive < FDD_NUM; drive++) {
	if (fdd_get_type(drive) != 0)
		sb_parts++;
    }

    /* Get name of current HDC. */
    hdc = hdc_get_internal_name(hdc_type);

    for (drive = 0; drive < CDROM_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if (((cdrom_drives[drive].bus_type==CDROM_BUS_ATAPI_PIO_ONLY) &&
	     (cdrom_drives[drive].bus_type==CDROM_BUS_ATAPI_PIO_AND_DMA)) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		continue;
	}

	if ((cdrom_drives[drive].bus_type == CDROM_BUS_SCSI) && (scsi_card == 0)) {
		continue;
	}

	if (cdrom_drives[drive].bus_type != 0) {
		sb_parts++;
	}
    }

    for (drive = 0; drive < ZIP_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if (((zip_drives[drive].bus_type==ZIP_BUS_ATAPI_PIO_ONLY) &&
	     (zip_drives[drive].bus_type==ZIP_BUS_ATAPI_PIO_AND_DMA)) &&
	    !(hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
		continue;
	}

	if ((zip_drives[drive].bus_type == ZIP_BUS_SCSI) &&
	    (scsi_card == 0)) {
		continue;
	}

	if (zip_drives[drive].bus_type != 0) {
		sb_parts++;
	}
    }

    for (drive = 0; drive < HDD_NUM; drive++) {
	if ((hdd[drive].bus==HDD_BUS_SCSI_REMOVABLE) && (scsi_card != 0)) {
		sb_parts++;
	}
    }

    if (c_st506 && (hdint || !strncmp(hdc, "st506", 5))) {
	/* ST506 MFM/RLL drives, and MFM or Internal controller. */
	sb_parts++;
    }
    if (c_esdi && (hdint || !strncmp(hdc, "esdi", 4))) {
	/* ESDI drives, and ESDI or Internal controller. */
	sb_parts++;
    }
    if (c_ide_pio && (hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
	/* IDE_PIO drives, and IDE or Internal controller. */
	sb_parts++;
    }
    if (c_ide_dma && (hdint || !strncmp(hdc, "ide", 3))) {
	/* IDE_DMA drives, and IDE or Internal controller. */
	sb_parts++;
    }
    if (c_scsi && (scsi_card != 0)) {
	sb_parts++;
    }
    if (do_net) {
	sb_parts++;
    }
    sb_parts += 2;

    /* Now (re-)allocate the fields. */
    sb_widths = (int *)malloc(sb_parts * sizeof(int));
     memset(sb_widths, 0x00, sb_parts * sizeof(int));
    sb_tags = (int *)malloc(sb_parts * sizeof(int));
     memset(sb_tags, 0x00, sb_parts * sizeof(int));
    sb_icons = (int *)malloc(sb_parts * sizeof(int));
     memset(sb_icons, 0x00, sb_parts * sizeof(int));
    sb_flags = (int *)malloc(sb_parts * sizeof(int));
     memset(sb_flags, 0x00, sb_parts * sizeof(int));
    sb_tips = (wchar_t **)malloc(sb_parts * sizeof(wchar_t *));
     memset(sb_tips, 0x00, sb_parts * sizeof(wchar_t *));

    /* Start populating the fields. */
    sb_parts = 0;

    for (drive = 0; drive < FDD_NUM; drive++) {
	if (fdd_get_type(drive) != 0) {
		edge += SB_ICON_WIDTH;
		sb_widths[sb_parts] = edge;
		sb_tags[sb_parts] = SB_FLOPPY | drive;
		sb_parts++;
	}
    }

    for (drive = 0; drive < CDROM_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((cdrom_drives[drive].bus_type==CDROM_BUS_ATAPI_PIO_ONLY) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	/* Could be Internal or External IDE.. */
	if ((cdrom_drives[drive].bus_type==CDROM_BUS_ATAPI_PIO_AND_DMA) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	if ((cdrom_drives[drive].bus_type == CDROM_BUS_SCSI) && (scsi_card == 0)) {
		continue;
	}

	if (cdrom_drives[drive].bus_type != 0) {
		edge += SB_ICON_WIDTH;
		sb_widths[sb_parts] = edge;
		sb_tags[sb_parts] = SB_CDROM | drive;
		sb_parts++;
	}
    }

    for (drive = 0; drive < ZIP_NUM; drive++) {
	/* Could be Internal or External IDE.. */
	if ((zip_drives[drive].bus_type==ZIP_BUS_ATAPI_PIO_ONLY) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	/* Could be Internal or External IDE.. */
	if ((zip_drives[drive].bus_type==ZIP_BUS_ATAPI_PIO_AND_DMA) &&
	    !(hdint || !strncmp(hdc, "ide", 3))) {
		continue;
	}

	if ((zip_drives[drive].bus_type == ZIP_BUS_SCSI) && (scsi_card == 0)) {
		continue;
	}

	if (zip_drives[drive].bus_type != 0) {
		edge += SB_ICON_WIDTH;
		sb_widths[sb_parts] = edge;
		sb_tags[sb_parts] = SB_ZIP | drive;
		sb_parts++;
	}
    }

    for (drive = 0; drive < HDD_NUM; drive++) {
	if ((hdd[drive].bus==HDD_BUS_SCSI_REMOVABLE) && (scsi_card != 0)) {
		edge += SB_ICON_WIDTH;
		sb_widths[sb_parts] = edge;
		sb_tags[sb_parts] = SB_RDISK | drive;
		sb_parts++;
	}
    }

    if (c_st506 && (hdint || !strncmp(hdc, "st506", 5))) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_HDD | HDD_BUS_ST506;
	sb_parts++;
    }

    if (c_esdi && (hdint || !strncmp(hdc, "esdi", 4))) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_HDD | HDD_BUS_ESDI;
	sb_parts++;
    }

    if (c_ide_pio && (hdint || !strncmp(hdc, "ide", 3) || !strncmp(hdc, "xta", 3))) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_HDD | HDD_BUS_IDE_PIO_ONLY;
	sb_parts++;
    }

    if (c_ide_dma && (hdint || !strncmp(hdc, "ide", 3))) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_HDD | HDD_BUS_IDE_PIO_AND_DMA;
	sb_parts++;
    }

    if (c_scsi && (scsi_card != 0)) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_HDD | HDD_BUS_SCSI;
	sb_parts++;
    }

    if (do_net) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_NETWORK;
	sb_parts++;
    }

    /* Initialize the Sound field. */
    if (sound_card != 0) {
	edge += SB_ICON_WIDTH;
	sb_widths[sb_parts] = edge;
	sb_tags[sb_parts] = SB_SOUND;
	sb_parts++;
    }

    /* Add a text field and finalize the field settings. */
    if (sb_parts)
	sb_widths[sb_parts - 1] += (24 - SB_ICON_WIDTH);
    sb_widths[sb_parts] = -1;
    sb_tags[sb_parts] = SB_TEXT;
    sb_parts++;

    /* Set up the status bar parts and their widths. */
    sb_setup(sb_parts, sb_widths);

    /* Initialize each status bar part. */
    for (part = 0; part < sb_parts; part++) {
	switch (sb_tags[part] & 0xf0) {
		case SB_FLOPPY:		/* Floppy */
			drive = sb_tags[part] & 0x0f;
			sb_flags[part] = (wcslen(floppyfns[drive]) == 0) ? 256 : 0;
			sb_icons[part] = plat_fdd_icon(fdd_get_type(drive)) | sb_flags[part];
			sb_menu_create(part);
			menu_floppy(part, drive);
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_CDROM:		/* CD-ROM */
			drive = sb_tags[part] & 0x0f;
			if (cdrom_drives[drive].host_drive == 200) {
				sb_flags[part] = (wcslen(cdrom_image[drive].image_path) == 0) ? 256 : 0;
			} else if ((cdrom_drives[drive].host_drive >= 'A') && (cdrom_drives[drive].host_drive <= 'Z')) {
				sb_flags[part] = 0;
			} else {
				sb_flags[part] = 256;
			}
			//FIXME: different icons for CD,DVD,BD ?
			sb_icons[part] = 160 | sb_flags[part];
			sb_menu_create(part);
			menu_cdrom(part, drive);
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_ZIP:		/* Iomega ZIP */
			drive = sb_tags[part] & 0x0f;
			sb_flags[part] = (wcslen(zip_drives[drive].image_path) == 0) ? 256 : 0;
			sb_icons[part] = 176 + sb_flags[part];
			sb_menu_create(part);
			menu_zip(part, drive);
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_RDISK:		/* Removable hard disk */
			sb_flags[part] = (wcslen(hdd[drive].fn) == 0) ? 256 : 0;
			sb_icons[part] = 192 + sb_flags[part];
			sb_menu_create(part);
			menu_remov(part, drive);
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_HDD:		/* Hard disk */
			sb_icons[part] = 208;
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_NETWORK:	/* Network */
			sb_icons[part] = 224;
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_SOUND:		/* Sound */
			sb_icons[part] = 259;
			ui_sb_tip_update(sb_tags[part]);
			break;

		case SB_TEXT:		/* Status text */
			sb_icons[part] = -1;
			sb_set_text(part, L"");
			break;
	}

	/* Set up the correct icon for this part. */
	if (sb_icons[part] != -1) {
		sb_set_text(part, L"");
		sb_set_icon(part, sb_icons[part]);
	} else {
		sb_set_icon(part, -1);
	}
    }

    sb_ready = 1;
}


/* Handle a command from one of the status bar menus. */
void
ui_sb_menu_command(int idm, int tag)
{
    wchar_t temp[512];
    int new_cdrom_drive;
    int drive;
    int part;
    int i;

    switch (idm) {
	case IDM_FLOPPY_IMAGE_NEW:
		drive = tag & 0x0003;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		dlg_new_floppy(idm, part);
		break;

	case IDM_FLOPPY_IMAGE_EXISTING:
	case IDM_FLOPPY_IMAGE_EXISTING_WP:
		drive = tag & 0x0003;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		i = (idm == IDM_FLOPPY_IMAGE_EXISTING_WP) ? 0x80 : 0;
		if (! dlg_file(plat_get_string(IDS_2159),
			       floppyfns[drive], temp, i))
			ui_sb_mount_floppy(drive, part, i ? 1 : 0, temp);
		break;

	case IDM_FLOPPY_EJECT:
		drive = tag & 0x0003;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		fdd_close(drive);
		ui_sb_icon_state(SB_FLOPPY | drive, 1);
		sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, 0);
		sb_menu_enable_item(part, IDM_FLOPPY_EXPORT_TO_86F | drive, 0);
		ui_sb_tip_update(SB_FLOPPY | drive);
		config_save();
		break;

	case IDM_FLOPPY_EXPORT_TO_86F:
		drive = tag & 0x0003;
		part = find_tag(SB_FLOPPY | drive);
		if (part == -1) break;

		if (! dlg_file(plat_get_string(IDS_2173), floppyfns[drive], temp, 1)) {
			plat_pause(1);
			if (! d86f_export(drive, temp))
				ui_msgbox(MBX_ERROR, (wchar_t *)IDS_4108);
			plat_pause(0);
		}
		break;

	case IDM_CDROM_MUTE:
		drive = tag & 0x0007;
		part = find_tag(SB_CDROM | drive);
		if (part == -1) break;

		cdrom_drives[drive].sound_on ^= 1;
		sb_menu_set_item(part, IDM_CDROM_MUTE | drive,
				 cdrom_drives[drive].sound_on);
		config_save();
		sound_cd_stop();
		break;

	case IDM_CDROM_EMPTY:
		drive = tag & 0x0007;
		cdrom_eject(drive);
		break;

	case IDM_CDROM_RELOAD:
		drive = tag & 0x0007;
		cdrom_reload(drive);
		break;

	case IDM_CDROM_IMAGE:
		drive = tag & 0x0007;
		part = find_tag(SB_CDROM | drive);
		if (part == -1) break;

		if (dlg_file(plat_get_string(IDS_2075),
			     cdrom_image[drive].image_path, temp, 0x80)) break;

		cdrom_drives[drive].prev_host_drive = cdrom_drives[drive].host_drive;
		if (! cdrom_image[drive].prev_image_path)
			cdrom_image[drive].prev_image_path = (wchar_t *)malloc(1024);
		wcscpy(cdrom_image[drive].prev_image_path, cdrom_image[drive].image_path);
		cdrom_drives[drive].handler->exit(drive);
		cdrom_close(drive);
		image_open(drive, temp);

		/* Signal media change to the emulated machine. */
		cdrom_insert(drive);
		sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
		if ((cdrom_drives[drive].host_drive >= 'A') &&
		    (cdrom_drives[drive].host_drive <= 'Z')) {
			sb_menu_set_item(part,
					 IDM_CDROM_HOST_DRIVE | drive | ((cdrom_drives[drive].host_drive - 'A') << 3),
					 0);
		}
		cdrom_drives[drive].host_drive = (wcslen(cdrom_image[drive].image_path) == 0) ? 0 : 200;
		if (cdrom_drives[drive].host_drive == 200) {
			sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 1);
			ui_sb_icon_state(SB_CDROM | drive, 0);
		} else {
			sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 0);
			sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
			ui_sb_icon_state(SB_CDROM | drive, 1);
		}
		sb_menu_enable_item(part, IDM_CDROM_RELOAD | drive, 0);
		ui_sb_tip_update(SB_CDROM | drive);
		config_save();
		break;

	case IDM_CDROM_HOST_DRIVE:
		drive = tag & 0x0007;
		i = ((tag >> 3) & 0x001f) + 'A';
		part = find_tag(SB_CDROM | drive);
		if (part == -1) break;

		new_cdrom_drive = i;
		if (cdrom_drives[drive].host_drive == new_cdrom_drive) {
			/* Switching to the same drive. Do nothing. */
			break;
		}
		cdrom_drives[drive].prev_host_drive = cdrom_drives[drive].host_drive;
		cdrom_drives[drive].handler->exit(drive);
		cdrom_close(drive);
		ioctl_open(drive, new_cdrom_drive);

		/* Signal media change to the emulated machine. */
		cdrom_insert(drive);
		sb_menu_set_item(part, IDM_CDROM_EMPTY | drive, 0);
		if ((cdrom_drives[drive].host_drive >= 'A') &&
		    (cdrom_drives[drive].host_drive <= 'Z')) {
			sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive | ((cdrom_drives[drive].host_drive - 'A') << 3), 0);
		}
		sb_menu_set_item(part, IDM_CDROM_IMAGE | drive, 0);
		cdrom_drives[drive].host_drive = new_cdrom_drive;
		sb_menu_set_item(part, IDM_CDROM_HOST_DRIVE | drive | ((cdrom_drives[drive].host_drive - 'A') << 3), 1);
		sb_menu_enable_item(part, IDM_CDROM_RELOAD | drive, 0);
		ui_sb_icon_state(SB_CDROM | drive, 0);
		ui_sb_tip_update(SB_CDROM | drive);
		config_save();
		break;

	case IDM_ZIP_IMAGE_NEW:
		drive = tag & 0x0003;
		part = find_tag(SB_ZIP | drive);
		if (part == -1) break;

		dlg_new_floppy(drive | 0x80, part);
		break;

	case IDM_ZIP_IMAGE_EXISTING:
	case IDM_ZIP_IMAGE_EXISTING_WP:
		drive = tag & 0x0003;
		part = find_tag(SB_ZIP | drive);
		if (part == -1) break;

		i = (idm == IDM_ZIP_IMAGE_EXISTING_WP) ? 0x80 : 0;
		if (dlg_file(plat_get_string(IDS_2175),
			     zip_drives[drive].image_path, temp, i)) break;
		ui_sb_mount_zip(drive, part, i ? 1 : 0, temp);
		break;

	case IDM_ZIP_EJECT:
		drive = tag & 0x0003;
		zip_eject(drive);
		break;

	case IDM_ZIP_RELOAD:
		drive = tag & 0x0003;
		zip_reload(drive);
		break;

	case IDM_RDISK_EJECT:
		drive = tag & 0x001f;
		removable_disk_eject(drive);
		break;

	case IDM_RDISK_RELOAD:
		drive = tag & 0x001f;
		removable_disk_reload(drive);
		break;

	case IDM_RDISK_SEND_CHANGE:
		drive = tag & 0x001f;
		scsi_disk_insert(drive);
		break;

	case IDM_RDISK_IMAGE:
	case IDM_RDISK_IMAGE_WP:
		drive = tag & 0x001f;
		part = find_tag(idm | drive);

		i = (idm == IDM_RDISK_IMAGE_WP) ? 0x80 : 0;
		if (dlg_file(plat_get_string(IDS_4106),
			     hdd[drive].fn, temp, i)) break;

		removable_disk_unload(drive);

		memset(hdd[drive].fn, 0x00, sizeof(hdd[drive].fn));
		wcscpy(hdd[drive].fn, temp);
		hdd[drive].wp = i ? 1 : 0;

		scsi_loadhd(hdd[drive].id.scsi.id, hdd[drive].id.scsi.lun, drive);
		scsi_disk_insert(drive);

		if (wcslen(hdd[drive].fn) > 0) {
			ui_sb_icon_state(SB_RDISK | drive, 0);
			sb_menu_enable_item(part, IDM_RDISK_EJECT | drive, 1);
			sb_menu_enable_item(part, IDM_RDISK_RELOAD | drive, 0);
			sb_menu_enable_item(part, IDM_RDISK_SEND_CHANGE | drive, 0);
		} else {
			ui_sb_icon_state(SB_RDISK | drive, 1);
			sb_menu_enable_item(part, IDM_RDISK_EJECT | drive, 0);
			sb_menu_enable_item(part, IDM_RDISK_RELOAD | drive, 0);
			sb_menu_enable_item(part, IDM_RDISK_SEND_CHANGE | drive, 0);
		}
		ui_sb_tip_update(SB_RDISK | drive);
		config_save();
		break;

	default:
		break;
    }
}


/* Handle a "double-click" on one of the icons. */
void
ui_sb_click(int part)
{
    if (part >= sb_parts) return;

    switch(sb_tags[part] & 0xf0) {
	case SB_SOUND:
		dlg_sound_gain();
		break;
    }
}


void
ui_sb_mount_floppy(uint8_t drive, int part, int8_t wp, const wchar_t *fn)
{
    int len;

    fdd_close(drive);

    ui_writeprot[drive] = wp;
    fdd_load(drive, fn);

    len = wcslen(floppyfns[drive]);
    ui_sb_icon_state(SB_FLOPPY | drive, len ? 0 : 1);

    sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, len ? 1 : 0);
    sb_menu_enable_item(part, IDM_FLOPPY_EXPORT_TO_86F | drive, len ? 1 : 0);

    ui_sb_tip_update(SB_FLOPPY | drive);

    config_save();
}


void
ui_sb_mount_zip(uint8_t drive, int part, int8_t wp, const wchar_t *fn)
{
    int len;

    zip_close(drive);

    zip_drives[drive].ui_writeprot = wp;
    zip_load(drive, fn);
    zip_insert(drive);

    len = wcslen(zip_drives[drive].image_path);
    ui_sb_icon_state(SB_ZIP | drive, len ? 0 : 1);

    sb_menu_enable_item(part, IDM_ZIP_EJECT | drive, len ? 1 : 0);
    sb_menu_enable_item(part, IDM_ZIP_RELOAD | drive, len ? 0 : 1);

    ui_sb_tip_update(SB_ZIP | drive);

    config_save();
}
