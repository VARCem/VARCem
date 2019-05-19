/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the UI part of CD-ROM/ZIP/DISK media changes.
 *
 * Version:	@(#)ui_cdrom.c	1.0.7	2019/05/17
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../timer.h"
#include "../config.h"
#include "../plat.h"
#include "../devices/floppy/fdd.h"
#include "../devices/disk/hdd.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/scsi/scsi_disk.h"
#include "../devices/disk/zip.h"
#include "../devices/cdrom/cdrom.h"
#include "ui.h"


void
ui_floppy_mount(uint8_t drive, int part, int8_t wp, const wchar_t *fn)
{
    int len;

    fdd_close(drive);

    ui_writeprot[drive] = wp;
    fdd_load(drive, fn);

    len = (int)wcslen(floppyfns[drive]);
    ui_sb_icon_state(SB_FLOPPY | drive, len ? 0 : 1);

    sb_menu_enable_item(part, IDM_FLOPPY_EJECT | drive, len ? 1 : 0);
    sb_menu_enable_item(part, IDM_FLOPPY_EXPORT | drive, len ? 1 : 0);

    ui_sb_tip_update(SB_FLOPPY | drive);

    config_save();
}


void
ui_cdrom_eject(uint8_t id)
{
    /* Actually eject the media, if any. */
    cdrom_eject(id);

    /* Now update the UI. */
    ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_IMAGE | id, 0);
    ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_EMPTY | id, 1);
    ui_sb_menu_enable_item(SB_CDROM|id, IDM_CDROM_RELOAD | id, 1);
    ui_sb_icon_state(SB_CDROM|id, 1);
    ui_sb_tip_update(SB_CDROM|id);

    config_save();
}


void
ui_cdrom_reload(uint8_t id)
{
    cdrom_t *dev = &cdrom[id];

    /* Actually reload the media, if any. */
    cdrom_reload(id);

    /* Now update the UI. */
    switch (dev->host_drive) {
	case 0:		/* no media */
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_EMPTY | id, 1);
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_IMAGE | id, 0);
		break;

	case 200:	/* image mounted */
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_EMPTY | id, 0);
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_IMAGE | id, 1);
		break;

	default:	/* host drive mounted */
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_EMPTY | id, 0);
		ui_sb_menu_set_item(SB_CDROM|id, IDM_CDROM_HOST_DRIVE | id | ((dev->host_drive - 'A') << 3), 1);
		break;
    }

    ui_sb_menu_enable_item(SB_CDROM|id, IDM_CDROM_RELOAD | id, 0);
    ui_sb_icon_state(SB_CDROM|id, 0);
    ui_sb_tip_update(SB_CDROM|id);

    config_save();
}


void
ui_zip_mount(uint8_t id, int part, int8_t wp, const wchar_t *fn)
{
    zip_t *dev = (zip_t *)zip_drives[id].priv;
    int len;

    zip_disk_close(dev);

    zip_drives[id].ui_writeprot = wp;
    zip_load(dev, fn);
    zip_insert(dev);

    len = (int)wcslen(zip_drives[id].image_path);
    ui_sb_icon_state(SB_ZIP | id, len ? 0 : 1);

    sb_menu_enable_item(part, IDM_ZIP_EJECT | id, len ? 1 : 0);
    sb_menu_enable_item(part, IDM_ZIP_RELOAD | id, len ? 0 : 1);

    ui_sb_tip_update(SB_ZIP | id);

    config_save();
}


void
ui_zip_eject(uint8_t id)
{
    zip_t *dev = (zip_t *)zip_drives[id].priv;

    zip_disk_close(dev);

    if (zip_drives[id].bus_type) {
	/* Signal disk change to the emulated machine. */
	zip_insert(dev);
    }

    ui_sb_icon_state(SB_ZIP | id, 1);
    ui_sb_menu_enable_item(SB_ZIP|id, IDM_ZIP_EJECT | id, 0);
    ui_sb_menu_enable_item(SB_ZIP|id, IDM_ZIP_RELOAD | id, 1);
    ui_sb_tip_update(SB_ZIP | id);

    config_save();
}


void
ui_zip_reload(uint8_t id)
{
    zip_t *dev = (zip_t *)zip_drives[id].priv;

    zip_disk_reload(dev);

    if (wcslen(zip_drives[id].image_path) == 0) {
	ui_sb_menu_enable_item(SB_ZIP|id, IDM_ZIP_EJECT | id, 0);
	ui_sb_icon_state(SB_ZIP|id, 1);
    } else {
	ui_sb_menu_enable_item(SB_ZIP|id, IDM_ZIP_EJECT | id, 1);
	ui_sb_icon_state(SB_ZIP|id, 0);
    }

    ui_sb_menu_enable_item(SB_ZIP|id, IDM_ZIP_RELOAD | id, 0);
    ui_sb_tip_update(SB_ZIP|id);

    config_save();
}


void
ui_disk_mount(uint8_t drive, int part, int8_t wp, const wchar_t *fn)
{
#if 0
    disk_close(drive);
#endif

    //FIXME: this should be "disk_load(drive, fn)" --FvK
    hdd[drive].wp = wp;
    memset(hdd[drive].fn, 0x00, sizeof(hdd[drive].fn));
    wcscpy(hdd[drive].fn, fn);

#if 0
    disk_insert(drive);
#endif

    if (wcslen(hdd[drive].fn) > 0) {
	ui_sb_icon_state(SB_DISK | drive, 0);
	sb_menu_enable_item(part, IDM_DISK_EJECT | drive, 1);
	sb_menu_enable_item(part, IDM_DISK_RELOAD | drive, 0);
	sb_menu_enable_item(part, IDM_DISK_NOTIFY | drive, 0);
    } else {
	ui_sb_icon_state(SB_DISK | drive, 1);
	sb_menu_enable_item(part, IDM_DISK_EJECT | drive, 0);
	sb_menu_enable_item(part, IDM_DISK_RELOAD | drive, 0);
	sb_menu_enable_item(part, IDM_DISK_NOTIFY | drive, 0);
    }

    ui_sb_tip_update(SB_DISK | drive);

    config_save();
}


#ifdef USE_REMOVABLE_DISK
void
ui_disk_unload(uint8_t id)
{
    if (wcslen(hdd[id].fn) == 0) {
	/* Switch from empty to empty. Do nothing. */
	return;
    }

    scsi_unloadhd(hdd[id].id.scsi.id, hdd[id].id.scsi.lun, id);
    scsi_disk_insert(id);
}


void
ui_disk_eject(uint8_t id)
{
    removable_disk_unload(id);
    ui_sb_icon_state(SB_RDISK|id, 1);
    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_EJECT | id, 0);
    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_RELOAD | id, 1);
    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_SEND_CHANGE | id, 0);

    ui_sb_tip_update(SB_RDISK|id);

    config_save();
}


void
ui_disk_reload(uint8_t id)
{
    if (wcslen(hdd[id].fn) != 0) {
	/* Attempting to reload while an image is already loaded. Do nothing. */
	return;
    }

    scsi_reloadhd(id);
#if 0
    scsi_disk_insert(id);
#endif

    ui_sb_icon_state(SB_RDISK|id, wcslen(hdd[id].fn) ? 0 : 1);

    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_EJECT | id, wcslen(hdd[id].fn) ? 1 : 0);
    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_RELOAD | id, 0);
    ui_sb_menu_enable_item(SB_RDISK|id, IDM_RDISK_SEND_CHANGE | id, wcslen(hdd[id].fn) ? 1 : 0);

    ui_sb_tip_update(SB_RDISK|id);

    config_save();
}
#endif
