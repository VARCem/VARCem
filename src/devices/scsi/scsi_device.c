/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		The generic SCSI device command handler.
 *
 * Version:	@(#)scsi_device.c	1.0.7	2018/09/15
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
#include <wchar.h>
#define dbglog scsi_log
#include "../../emu.h"
#include "../../device.h"
#include "../disk/hdd.h"
#include "scsi.h"
#include "scsi_device.h"
#include "scsi_disk.h"
#include "../cdrom/cdrom.h"
#include "../disk/zip.h"


static const uint8_t scsi_null_device_sense[18] = {
    0x70,0,SENSE_ILLEGAL_REQUEST,0,0,0,0,0,0,0,0,0,ASC_INV_LUN,0,0,0,0,0
};


static uint8_t
scsi_device_target_command(int lun_type, uint8_t id, uint8_t *cdb)
{
    uint8_t ret = SCSI_STATUS_CHECK_CONDITION;

    switch (lun_type) {
	case SCSI_DISK:
		scsi_disk_command(scsi_disk[id], cdb);
		ret = scsi_disk_err_stat_to_scsi(scsi_disk[id]);
		break;

	case SCSI_CDROM:
		cdrom_command(cdrom[id], cdb);
		ret = cdrom_CDROM_PHASE_to_scsi(cdrom[id]);
		break;

	case SCSI_ZIP:
		zip_command(zip[id], cdb);
		ret = zip_ZIP_PHASE_to_scsi(zip[id]);
		break;

	default:
		break;
    }

    return ret;
}


static void
scsi_device_target_phase_callback(int lun_type, uint8_t id)
{
    switch (lun_type) {
	case SCSI_DISK:
		scsi_disk_callback(scsi_disk[id]);
		break;

	case SCSI_CDROM:
		cdrom_phase_callback(cdrom[id]);
		break;

	case SCSI_ZIP:
		zip_phase_callback(zip[id]);
		break;

	default:
		break;
    }
}


static int
scsi_device_target_err_stat_to_scsi(int lun_type, uint8_t id)
{
    uint8_t ret = SCSI_STATUS_CHECK_CONDITION;

    switch (lun_type) {
	case SCSI_DISK:
		ret = scsi_disk_err_stat_to_scsi(scsi_disk[id]);
		break;

	case SCSI_CDROM:
		ret = cdrom_CDROM_PHASE_to_scsi(cdrom[id]);
		break;

	case SCSI_ZIP:
		ret = zip_ZIP_PHASE_to_scsi(zip[id]);
		break;

	default:
		break;
    }

    return ret;
}


int64_t
scsi_device_get_callback(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    int64_t ret = -1LL;
    uint8_t id;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		ret = scsi_disk[id]->callback;
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		ret = cdrom[id]->callback;
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		ret = zip[id]->callback;
		break;

	default:
		break;
    }

    return ret;
}


uint8_t *
scsi_device_sense(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t *ret = (uint8_t *)scsi_null_device_sense;
    uint8_t id;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		ret = scsi_disk[id]->sense;
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		ret = cdrom[id]->sense;
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		ret = zip[id]->sense;
		break;

	default:
		break;
    }

    return ret;
}


void
scsi_device_request_sense(uint8_t scsi_id, uint8_t scsi_lun, uint8_t *buffer, uint8_t alloc_length)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t id;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		scsi_disk_request_sense_for_scsi(scsi_disk[id], buffer, alloc_length);
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		cdrom_request_sense_for_scsi(cdrom[id], buffer, alloc_length);
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		zip_request_sense_for_scsi(zip[id], buffer, alloc_length);
		break;

	default:
		memcpy(buffer, scsi_null_device_sense, alloc_length);
		break;
    }
}


void
scsi_device_reset(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t id;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		scsi_disk_reset(scsi_disk[id]);
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		cdrom_reset(cdrom[id]);
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		zip_reset(zip[id]);
		break;

	default:
		break;
    }
}


void
scsi_device_type_data(uint8_t scsi_id, uint8_t scsi_lun, uint8_t *type, uint8_t *rmb)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;

    switch (lun_type) {
	case SCSI_DISK:
		*type = *rmb = 0x00;
		break;

	case SCSI_CDROM:
		*type = 0x05;
		*rmb = 0x80;
		break;

	case SCSI_ZIP:
		*type = 0x00;
		*rmb = 0x80;
		break;

	default:
		*type = *rmb = 0xff;
		break;
    }
}


int
scsi_device_read_capacity(uint8_t scsi_id, uint8_t scsi_lun, uint8_t *cdb, uint8_t *buffer, uint32_t *len)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    int ret = 0;
    uint8_t id;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		ret = scsi_disk_read_capacity(scsi_disk[id], cdb, buffer, len);
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		ret = cdrom_read_capacity(cdrom[id], cdb, buffer, len);
		break;
		
	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		ret = zip_read_capacity(zip[id], cdb, buffer, len);
		break;

	default:
		break;
    }

    return ret;
}


int
scsi_device_present(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    int ret = 0;

    switch (lun_type) {
	case SCSI_NONE:
		ret = 0;
		break;

	default:
		ret = 1;
		break;
    }

    return ret;
}


int
scsi_device_valid(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t id = 0;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		break;

	default:
		break;
    }

    return (id == 0xff) ? 0 : 1;
}


int
scsi_device_cdb_length(uint8_t scsi_id, uint8_t scsi_lun)
{
    (void)scsi_id;
    (void)scsi_lun;

    /* Right now, it's 12 for all devices. */
    return 12;
}


void
scsi_device_command_phase0(uint8_t scsi_id, uint8_t scsi_lun, uint8_t *cdb)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t id = 0;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		break;

	default:
		SCSIDevices[scsi_id][scsi_lun].Phase = SCSI_PHASE_STATUS;
		SCSIDevices[scsi_id][scsi_lun].Status = SCSI_STATUS_CHECK_CONDITION;
		return;
    }

    /* Finally, execute the SCSI command immediately and get the transfer length. */
    SCSIDevices[scsi_id][scsi_lun].Phase = SCSI_PHASE_COMMAND;
    SCSIDevices[scsi_id][scsi_lun].Status = scsi_device_target_command(lun_type, id, cdb);

    if (SCSIDevices[scsi_id][scsi_lun].Phase == SCSI_PHASE_STATUS) {
	/* Command completed (either OK or error) - call the phase callback to complete the command. */
	scsi_device_target_phase_callback(lun_type, id);
    }

    /* If the phase is DATA IN or DATA OUT, finish this here. */
}


void
scsi_device_command_phase1(uint8_t scsi_id, uint8_t scsi_lun)
{
    uint8_t lun_type = SCSIDevices[scsi_id][scsi_lun].LunType;
    uint8_t id = 0;

    switch (lun_type) {
	case SCSI_DISK:
		id = scsi_disks[scsi_id][scsi_lun];
		break;

	case SCSI_CDROM:
		id = scsi_cdrom_drives[scsi_id][scsi_lun];
		break;

	case SCSI_ZIP:
		id = scsi_zip_drives[scsi_id][scsi_lun];
		break;

	default:
		return;
    }

    /* Call the second phase. */
    scsi_device_target_phase_callback(lun_type, id);

    SCSIDevices[scsi_id][scsi_lun].Status = scsi_device_target_err_stat_to_scsi(lun_type, id);

    /* Command second phase complete - call the callback to complete the command. */
    scsi_device_target_phase_callback(lun_type, id);
}


int32_t *
scsi_device_get_buf_len(uint8_t scsi_id, uint8_t scsi_lun)
{
    return &SCSIDevices[scsi_id][scsi_lun].BufferLength;
}
