/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of a generic Magneto-Optical Disk drive
 *		commands, for both ATAPI and SCSI usage.
 *
 * Version:	@(#)mo.h	1.0.0	2020/03/27
 *
 * Authors:	Natalia Portillo <claunia@claunia.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *		    Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020 Miran Grca.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog mo_log
#include "../../emu.h"
#include "../../config.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../system/intel_piix.h"
#include "../scsi/scsi_device.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "mo.h"

#include <corecrt_io.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Bits of 'status' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT                0x10
#define SERVICE_STAT            0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */


#ifdef ENABLE_MO_LOG
int		mo_do_log = ENABLE_MO_LOG;
#endif
mo_drive_t	mo_drives[MO_NUM];


/* Table of all SCSI commands and their flags. */
static const uint8_t command_flags[0x100] = {
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x00: TEST UNIT READY */
    IMPLEMENTED | ALLOW_UA | NONDATA,				/* 0x01: REZERO UNIT */
    0,
    IMPLEMENTED | ALLOW_UA,							/* 0x03: REQUEST SENSE */
    IMPLEMENTED | CHECK_READY | ALLOW_UA | NONDATA,	/* 0x04: FORMAT UNIT */
    0, 0, 0,
    IMPLEMENTED | CHECK_READY,						/* 0x08: READ(6) */
    0,
    IMPLEMENTED | CHECK_READY,						/* 0x0A: WRITE(6) */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x0B: SEEK(6) */
    0, 0, 0, 0, 0, 0,
    IMPLEMENTED | ALLOW_UA,							/* 0x12: INQUIRY */
	IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x13: VERIFY(6) */
    0,
    IMPLEMENTED,									/* 0x15: MODE SELECT(6) */
    IMPLEMENTED,									/* 0x16: RESERVE */
    IMPLEMENTED,									/* 0x17: RELEASE */
    0, 0,
    IMPLEMENTED,									/* 0x1A: MODE SENSE(6) */
    IMPLEMENTED | CHECK_READY,						/* 0x1B: START STOP UNIT */
    0,
    IMPLEMENTED,									/* 0x1D: SEND DIAGNOSTIC */
    IMPLEMENTED | CHECK_READY,						/* 0x1E: PREVENT ALLOW MEDIUM REMOVAL */
    0, 0, 0, 0, 0, 0,
    IMPLEMENTED | CHECK_READY,						/* 0x25: READ CAPACITY */
    0, 0,
    IMPLEMENTED | CHECK_READY,						/* 0x28: READ(10) */
    IMPLEMENTED | CHECK_READY,          	        /* 0x29: READ GENERATION */
    IMPLEMENTED | CHECK_READY,						/* 0x2A: WRITE(10) */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x2B: SEEK(10) */
	IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x2C: ERASE(10) */
	IMPLEMENTED | CHECK_READY,						/* 0x2D: READ UPDATED BLOCK */
    IMPLEMENTED | CHECK_READY,						/* 0x2E: WRITE AND VERIFY(10) */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x2F: VERIFY(10) */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, // TODO: Implement -> IMPLEMENTED | CHECK_READY,	/* 0x38: MEDIUM SCAN */
    0, 0, 0, 0,
    IMPLEMENTED | CHECK_READY,						/* 0x3D: UPDATE BLOCK */
    IMPLEMENTED | CHECK_READY,						/* 0x3E: READ LONG(10) */
	IMPLEMENTED | CHECK_READY,						/* 0x3F: WRITE LONG(10) */
    IMPLEMENTED,									/* 0x40: CHANGE DEFINITION */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
    IMPLEMENTED,									/* 0x55: MODE SELECT(10) */
    0, 0, 0, 0,
    IMPLEMENTED,									/* 0x5A: MODE SENSE(10) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IMPLEMENTED | CHECK_READY,						/* 0xA8: READ(12) */
    0,
    IMPLEMENTED | CHECK_READY,						/* 0xAA: WRITE(12) */
    0,
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0xAC: ERASE(12) */
    0,
    IMPLEMENTED | CHECK_READY,						/* 0xAE: WRITE AND VERIFY(12) */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0xAF: VERIFY(12) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint64_t mode_sense_page_flags = (GPMODEP_ALL_PAGES);


static const mode_sense_pages_t mode_sense_pages_default = {
    {
    {                        0,    0 },
	{                        0,    0 },
	{                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 }
}   };

static const mode_sense_pages_t mode_sense_pages_changeable = {
    {
    {                        0,    0 },
	{                        0,    0 },
	{                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 },
    {                        0,    0 }
}   };

static void	do_callback(void *p);


#ifdef _LOGGING
static void
mo_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_MO_LOG
    va_list ap;

    if (mo_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


static void
set_callback(mo_t *dev)
{
    if (dev->drv->bus_type != MO_BUS_SCSI)
	ide_set_callback(dev->drv->bus_id.ide_channel >> 1, dev->callback);
}


static void
set_signature(void *p)
{
    mo_t *dev = (mo_t *)p;

    if (dev->id >= MO_NUM)
	return;

    dev->phase = 1;
    dev->request_length = 0xEB14;
}


static int
supports_pio(mo_t *dev)
{
    return (dev->drv->bus_mode & 1);
}


static int
supports_dma(mo_t *dev)
{
    return (dev->drv->bus_mode & 2);
}


/* Returns: 0 for none, 1 for PIO, 2 for DMA. */
static int
current_mode(mo_t *dev)
{
    if (!supports_pio(dev) && !supports_dma(dev))
	return 0;

    if (supports_pio(dev) && !supports_dma(dev)) {
	DEBUG("MO %i: Drive does not support DMA, setting to PIO\n", dev->id);
	return 1;
    }

    if (!supports_pio(dev) && supports_dma(dev))
	return 2;

    if (supports_pio(dev) && supports_dma(dev)) {
	DEBUG("MO %i: Drive supports both, setting to %s\n",
	      dev->id, (dev->features & 1) ? "DMA" : "PIO");
	return (dev->features & 1) ? 2 : 1;
    }

    return 0;
}


/* Translates ATAPI status (ERR_STAT flag) to SCSI status. */
static int
err_stat_to_scsi(void *p)
{
    mo_t *dev = (mo_t *)p;

    if (dev->status & ERR_STAT)
	return SCSI_STATUS_CHECK_CONDITION;

    return SCSI_STATUS_OK;
}


/* Translates ATAPI phase (DRQ, I/O, C/D) to SCSI phase (MSG, C/D, I/O). */
static int
atapi_phase_to_scsi(mo_t *dev)
{
    if (dev->status & 8) {
	switch (dev->phase & 3) {
		case 0:
			return 0;
		case 1:
			return 2;
		case 2:
			return 1;
		case 3:
			return 7;
	}
    } else {
	if ((dev->phase & 3) == 3)
		return 3;
	else
		return 4;
    }

    return 0;
}


static void
mode_sense_load(mo_t *dev)
{
    wchar_t temp[512];
    const mode_sense_pages_t *ptr;
    FILE *fp;

    ptr = &mode_sense_pages_default;
    memcpy(&dev->ms_pages_saved, ptr, sizeof(mode_sense_pages_t));

    memset(temp, 0, sizeof(temp));
    if (dev->drv->bus_type == MO_BUS_SCSI)
	swprintf(temp, sizeof_w(temp),
		 L"scsi_mo_%02i_mode_sense_bin", dev->id);
    else
	swprintf(temp, sizeof_w(temp),
		 L"mo_%02i_mode_sense_bin", dev->id);

    fp = plat_fopen(nvr_path(temp), L"rb");
    if (fp != NULL) {
	/* Nothing to read, not used by MO. */

	(void)fclose(fp);
    }
}


static void
mode_sense_save(mo_t *dev)
{
    wchar_t temp[512];
    FILE *fp;

    memset(temp, 0, sizeof(temp));
    if (dev->drv->bus_type == MO_BUS_SCSI)
	swprintf(temp, sizeof_w(temp),
		 L"scsi_mo_%02i_mode_sense_bin", dev->id);
    else
	swprintf(temp, sizeof_w(temp),
		 L"mo_%02i_mode_sense_bin", dev->id);

    fp = plat_fopen(nvr_path(temp), L"wb");
    if (fp != NULL) {
	/* Nothing to write, not used by MO. */

	(void)fclose(fp);
    }
}


static int
read_capacity(void *p, uint8_t *cdb, uint8_t *buffer, uint32_t *len)
{
    mo_t *dev = (mo_t *)p;
    int size;

    /* IMPORTANT: we return is the last LBA block. */
    size = dev->drv->medium_size - 1;

    memset(buffer, 0, 8);
    buffer[0] = (size >> 24) & 0xff;
    buffer[1] = (size >> 16) & 0xff;
    buffer[2] = (size >> 8) & 0xff;
    buffer[3] = size & 0xff;
	buffer[6] = (dev->drv->sector_size >> 8) & 0xff;
	buffer[7] = dev->drv->sector_size & 0xff;

    *len = 8;

    return 1;
}


/*SCSI Mode Sense 6/10*/
static uint8_t
mode_sense_read(mo_t *dev, uint8_t page_control, uint8_t page, uint8_t pos)
{
    switch (page_control) {
	case 0:
	case 3:
		return dev->ms_pages_saved.pages[page][pos];

	case 1:
		return mode_sense_pages_changeable.pages[page][pos];

	case 2:
		return mode_sense_pages_default.pages[page][pos];

	default:
		break;
    }

    return 0;
}


static uint32_t
mode_sense(mo_t *dev, uint8_t *buf, uint32_t pos, uint8_t page, uint8_t block_descriptor_len)
{
    uint8_t page_control = (page >> 6) & 3;
    uint8_t msplen;
    uint64_t pf;
    int i, j;

    page &= 0x3f;

    pf = mode_sense_page_flags;

    if (block_descriptor_len) {
	buf[pos++] = ((dev->drv->medium_size >> 24) & 0xff);
	buf[pos++] = ((dev->drv->medium_size >> 16) & 0xff);
	buf[pos++] = ((dev->drv->medium_size >>  8) & 0xff);
	buf[pos++] = ( dev->drv->medium_size        & 0xff);
	buf[pos++] = 0;		/* Reserved. */
	buf[pos++] = 0;
	buf[pos++] = ((dev->drv->sector_size >>  8) & 0xff);
	buf[pos++] = ( dev->drv->sector_size        & 0xff);
    }

    for (i = 0; i < 0x40; i++) {
        if ((page == GPMODE_ALL_PAGES) || (page == i)) {
		if (pf & (1LL << ((uint64_t) page))) {
			buf[pos++] = mode_sense_read(dev, page_control, i, 0);
			msplen = mode_sense_read(dev, page_control, i, 1);
			buf[pos++] = msplen;
			DEBUG("MO %i: MODE SENSE: Page [%02X] length %i\n", dev->id, i, msplen);
			for (j = 0; j < msplen; j++)
				buf[pos++] = mode_sense_read(dev, page_control, i, 2 + j);
		}
	}
    }

    return pos;
}


static void
update_request_length(mo_t *dev, int len, int block_len)
{
    int bt, min_len = 0;

    dev->max_transfer_len = dev->request_length;

    /*
     * For media access commands, make sure the requested
     * DRQ length matches the block length.
     */
    switch (dev->current_cdb[0]) {
	case 0x08:
	case 0x28:
	case 0xa8:
		/* Make sure total length is not bigger than sum of the lengths of
		   all the requested blocks. */
		bt = (dev->requested_blocks * block_len);
		if (len > bt)
			len = bt;

		min_len = block_len;

		if (len <= block_len) {
			/* Total length is less or equal to block length. */
			if (dev->max_transfer_len < block_len) {
				/* Transfer a minimum of (block size) bytes. */
				dev->max_transfer_len = block_len;
				dev->packet_len = block_len;
				break;
			}
		}
		/*FALLTHROUGH*/

	default:
		dev->packet_len = len;
		break;
    }

    /* If the DRQ length is odd, and the total remaining length is bigger, make sure it's even. */
    if ((dev->max_transfer_len & 1) && (dev->max_transfer_len < len))
	dev->max_transfer_len &= 0xfffe;

    /* If the DRQ length is smaller or equal in size to the total remaining length, set it to that. */
    if (! dev->max_transfer_len)
	dev->max_transfer_len = 65534;

    if ((len <= dev->max_transfer_len) && (len >= min_len))
	dev->request_length = dev->max_transfer_len = len;
    else if (len > dev->max_transfer_len)
	dev->request_length = dev->max_transfer_len;

    return;
}


static void
command_bus(mo_t *dev)
{
    dev->status = BUSY_STAT;
    dev->phase = 1;
    dev->pos = 0;
    dev->callback = 1LL * MO_TIME;
    set_callback(dev);
}


static void
command_common(mo_t *dev)
{
    double bytes_per_second, period;
    double dusec;

    dev->status = BUSY_STAT;
    dev->phase = 1;
    dev->pos = 0;
    if (dev->packet_status == PHASE_COMPLETE) {
	do_callback((void *) dev);
	dev->callback = 0LL;
    } else {
	if (dev->drv->bus_type == MO_BUS_SCSI) {
		dev->callback = -1LL;	/* Speed depends on SCSI controller */
		return;
	} else {
		if (current_mode(dev) == 2)
			bytes_per_second = 66666666.666666666666666;	/* 66 MB/s MDMA-2 speed */
		else
			bytes_per_second =  8333333.333333333333333;	/* 8.3 MB/s PIO-2 speed */
	}

	period = 1000000.0 / bytes_per_second;
	dusec = (double) TIMER_USEC;
	dusec = dusec * period * ((double)dev->packet_len);
	dev->callback = (tmrval_t)dusec;
    }

    set_callback(dev);
}


static void
command_complete(mo_t *dev)
{
    dev->packet_status = PHASE_COMPLETE;

    command_common(dev);
}


static void
command_read(mo_t *dev)
{
    dev->packet_status = PHASE_DATA_IN;

    command_common(dev);
}


static void
command_read_dma(mo_t *dev)
{
    dev->packet_status = PHASE_DATA_IN_DMA;

    command_common(dev);
}


static void
command_write(mo_t *dev)
{
    dev->packet_status = PHASE_DATA_OUT;

    command_common(dev);
}


static void
command_write_dma(mo_t *dev)
{
    dev->packet_status = PHASE_DATA_OUT_DMA;

    command_common(dev);
}


/* id = Current MO device ID;
   len = Total transfer length;
   block_len = Length of a single block (why does it matter?!);
   alloc_len = Allocated transfer length;
   direction = Transfer direction (0 = read from host, 1 = write to host). */
static void
data_command_finish(mo_t *dev, int len, int block_len, int alloc_len, int direction)
{
    DEBUG("MO %i: Finishing command (%02X): %i, %i, %i, %i, %i\n",
	    dev->id, dev->current_cdb[0], len, block_len, alloc_len, direction, dev->request_length);
    dev->pos = 0;
    if (alloc_len >= 0) {
	if (alloc_len < len)
		len = alloc_len;
    }

    if ((len == 0) || (current_mode(dev) == 0)) {
	if (dev->drv->bus_type != MO_BUS_SCSI)
		dev->packet_len = 0;

	command_complete(dev);
    } else {
	if (current_mode(dev) == 2) {
		if (dev->drv->bus_type != MO_BUS_SCSI)
			dev->packet_len = alloc_len;

		if (direction == 0)
			command_read_dma(dev);
		else
			command_write_dma(dev);
	} else {
		update_request_length(dev, len, block_len);
		if (direction == 0)
			command_read(dev);
		else
			command_write(dev);
	}
    }

    DEBUG("MO %i: Status: %i, cylinder %i, packet length: %i, position: %i, phase: %i\n",
	    dev->id, dev->packet_status, dev->request_length, dev->packet_len, dev->pos, dev->phase);
}


static void
sense_clear(mo_t *dev, int command)
{
    dev->sense[2] = dev->sense[12] = dev->sense[13] = 0;
}


static void
set_phase(mo_t *dev, uint8_t phase)
{
    if (dev->drv->bus_type != MO_BUS_SCSI)
	return;

    scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].phase = phase;
}


static void
cmd_error(mo_t *dev)
{
    set_phase(dev, SCSI_PHASE_STATUS);

    dev->error = ((dev->sense[2] & 0xf) << 4) | ABRT_ERR;
    if (dev->unit_attention)
	dev->error |= MCR_ERR;
    dev->status = READY_STAT | ERR_STAT;
    dev->phase = 3;
    dev->pos = 0;
    dev->packet_status = 0x80;
    dev->callback = 50LL * MO_TIME;

    set_callback(dev);
    DEBUG("MO %i: [%02X] ERROR: %02X/%02X/%02X\n",
	  dev->id, dev->current_cdb[0], dev->sense[2], dev->sense[12], dev->sense[13]);
}


static void
unit_attention(mo_t *dev)
{
    set_phase(dev, SCSI_PHASE_STATUS);

    dev->error = (SENSE_UNIT_ATTENTION << 4) | ABRT_ERR;
    if (dev->unit_attention)
	dev->error |= MCR_ERR;
    dev->status = READY_STAT | ERR_STAT;
    dev->phase = 3;
    dev->pos = 0;
    dev->packet_status = 0x80;
    dev->callback = 50LL * MO_TIME;

    set_callback(dev);
    DEBUG("MO %i: UNIT ATTENTION\n", dev->id);
}


static void
bus_master_error(mo_t *dev)
{
    dev->sense[2] = dev->sense[12] = dev->sense[13] = 0;

    cmd_error(dev);
}


static void
not_ready(mo_t *dev)
{
    dev->sense[2] = SENSE_NOT_READY;
    dev->sense[12] = ASC_MEDIUM_NOT_PRESENT;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
write_protected(mo_t *dev)
{
    dev->sense[2] = SENSE_UNIT_ATTENTION;
    dev->sense[12] = ASC_WRITE_PROTECTED;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
invalid_lun(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_LUN;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
illegal_opcode(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_ILLEGAL_OPCODE;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
lba_out_of_range(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_LBA_OUT_OF_RANGE;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
invalid_field(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_FIELD_IN_CMD_PACKET;
    dev->sense[13] = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
invalid_field_pl(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_FIELD_IN_PARAMETER_LIST;
    dev->sense[13] = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
data_phase_error(mo_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_DATA_PHASE_ERROR;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static int
mo_blocks(mo_t *dev, int32_t *len, int first_batch, int out)
{
    int i;

    *len = 0;

    if (! dev->sector_len) {
	command_complete(dev);
	return -1;
    }

    DEBUG("%sing %i blocks starting from %i...\n", out ? "Writ" : "Read", dev->requested_blocks, dev->sector_pos);

    if (dev->sector_pos >= dev->drv->medium_size) {
	DEBUG("MO %i: Trying to %s beyond the end of disk\n", dev->id, out ? "write" : "read");
	lba_out_of_range(dev);
	return 0;
    }

    *len = dev->requested_blocks * dev->drv->sector_size;

    fseek(dev->drv->f, dev->drv->base + (dev->sector_pos * dev->drv->sector_size), SEEK_SET);

    for (i = 0; i < dev->requested_blocks; i++) {
	if (feof(dev->drv->f))
		break;

	if (out)
		fwrite(dev->buffer + (i * dev->drv->sector_size), 1, dev->drv->sector_size, dev->drv->f);
	else
		fread(dev->buffer + (i * dev->drv->sector_size), 1, dev->drv->sector_size, dev->drv->f);
    }

    DEBUG("%s %i bytes of blocks...\n", out ? "Written" : "Read", *len);

    dev->sector_pos += dev->requested_blocks;
    dev->sector_len -= dev->requested_blocks;

    return 1;
}


static int
pre_execution_check(mo_t *dev, uint8_t *cdb)
{
    int ready = 0;

    if (dev->drv->bus_type == MO_BUS_SCSI) {
	if ((cdb[0] != GPCMD_REQUEST_SENSE) && (cdb[1] & 0xe0)) {
		DEBUG("MO %i: Attempting to execute a unknown command targeted at SCSI LUN %i\n", dev->id, ((dev->request_length >> 5) & 7));
		invalid_lun(dev);
		return 0;
	}
    }

    if (! (command_flags[cdb[0]] & IMPLEMENTED)) {
	DEBUG("MO %i: Attempting to execute unknown command %02X over %s\n", dev->id, cdb[0],
		(dev->drv->bus_type == MO_BUS_SCSI) ? "SCSI" : "ATAPI");

	illegal_opcode(dev);
	return 0;
    }

    if ((dev->drv->bus_type < MO_BUS_SCSI) &&
	(command_flags[cdb[0]] & SCSI_ONLY)) {
	DEBUG("MO %i: Attempting to execute SCSI-only command %02X over ATAPI\n", dev->id, cdb[0]);
	illegal_opcode(dev);
	return 0;
    }

    if ((dev->drv->bus_type == MO_BUS_SCSI) &&
	(command_flags[cdb[0]] & ATAPI_ONLY)) {
	DEBUG("MO %i: Attempting to execute ATAPI-only command %02X over SCSI\n", dev->id, cdb[0]);
	illegal_opcode(dev);
	return 0;
    }

    ready = (dev->drv->f != NULL);

    /* If the drive is not ready, there is no reason to keep the
       UNIT ATTENTION condition present, as we only use it to mark
       disc changes. */
    if (!ready && dev->unit_attention)
	dev->unit_attention = 0;

    /* If the UNIT ATTENTION condition is set and the command does not allow
       execution under it, error out and report the condition. */
    if (dev->unit_attention == 1) {
	/* Only increment the unit attention phase if the command can not pass through it. */
	if (! (command_flags[cdb[0]] & ALLOW_UA)) {
		DBGLOG(1, "MO %i: Unit attention now 2\n", dev->id);
		dev->unit_attention = 2;
		DEBUG("MO %i: UNIT ATTENTION: Command %02X not allowed to pass through\n", dev->id, cdb[0]);
		unit_attention(dev);
		return 0;
	}
    } else if (dev->unit_attention == 2) {
	if (cdb[0] != GPCMD_REQUEST_SENSE) {
		DBGLOG(1, "MO %i: Unit attention now 0\n", dev->id);
		dev->unit_attention = 0;
	}
    }

    /* Unless the command is REQUEST SENSE, clear the sense. This will *NOT*
       the UNIT ATTENTION condition if it's set. */
    if (cdb[0] != GPCMD_REQUEST_SENSE)
	sense_clear(dev, cdb[0]);

    /* Next it's time for NOT READY. */
    if ((command_flags[cdb[0]] & CHECK_READY) && !ready) {
	DEBUG("MO %i: Not ready (%02X)\n", dev->id, cdb[0]);
	not_ready(dev);
	return 0;
    }

    DEBUG("MO %i: Continuing with command %02X\n", dev->id, cdb[0]);

    return 1;
}


static void
do_seek(mo_t *dev, uint32_t pos)
{
    DBGLOG(1, "MO %i: Seek %08X\n", dev->id, pos);
    dev->sector_pos   = pos;
}


static void
do_rezero(mo_t *dev)
{
    dev->sector_pos = dev->sector_len = 0;
    do_seek(dev, 0);
}


static void
do_reset(void *p)
{
    mo_t *dev = (mo_t *)p;

    do_rezero(dev);

    dev->status = 0;
    dev->callback = 0LL;

    set_callback(dev);

    set_signature(dev);

    dev->packet_status = 0xff;
    dev->unit_attention = 0;
}


static void
request_sense(mo_t *dev, uint8_t *buffer, uint8_t alloc_length, int desc)
{				
    /*Will return 18 bytes of 0*/
    if (alloc_length != 0) {
	memset(buffer, 0, alloc_length);
	if (! desc)
		memcpy(buffer, dev->sense, alloc_length);
	else {
		buffer[1] = dev->sense[2];
		buffer[2] = dev->sense[12];
		buffer[3] = dev->sense[13];
	}
    }

    buffer[0] = desc ? 0x72 : 0x70;

    if (dev->unit_attention && (dev->sense[2] == 0)) {
	buffer[desc ? 1 : 2] = SENSE_UNIT_ATTENTION;
	buffer[desc ? 2 : 12] = ASC_MEDIUM_MAY_HAVE_CHANGED;
	buffer[desc ? 3 : 13] = 0;
    }

    DEBUG("MO %i: Reporting sense: %02X %02X %02X\n",
	  dev->id, buffer[2], buffer[12], buffer[13]);

    if (buffer[desc ? 1 : 2] == SENSE_UNIT_ATTENTION) {
	/* If the last remaining sense is unit attention, clear
	   that condition. */
	dev->unit_attention = 0;
    }

    /* Clear the sense stuff as per the spec. */
    sense_clear(dev, GPCMD_REQUEST_SENSE);
}


static void
request_sense_scsi(void *p, uint8_t *buffer, uint8_t alloc_length)
{
    mo_t *dev = (mo_t *) p;
    int ready = 0;

    ready = (dev->drv->f != NULL);

    if (!ready && dev->unit_attention) {
	/* If the drive is not ready, there is no reason to keep the
	   UNIT ATTENTION condition present, as we only use it to mark
	   disc changes. */
	dev->unit_attention = 0;
    }

    /* Do *NOT* advance the unit attention phase. */

    request_sense(dev, buffer, alloc_length, 0);
}


static void
set_buf_len(mo_t *dev, int32_t *BufLen, int32_t *src_len)
{
    if (dev->drv->bus_type != MO_BUS_SCSI) return;

    if (*BufLen == -1)
	*BufLen = *src_len;
    else {
	*BufLen = MIN(*src_len, *BufLen);
	*src_len = *BufLen;
    }

    DEBUG("MO %i: Actual transfer length: %i\n", dev->id, *BufLen);
}


static void
buf_alloc(mo_t *dev, uint32_t len)
{
    DEBUG("MO %i: Allocated buffer length: %i\n", dev->id, len);

    dev->buffer = (uint8_t *)mem_alloc(len);
}


static void
buf_free(mo_t *dev)
{
    if (dev->buffer) {
	DEBUG("MO %i: Freeing buffer...\n", dev->id);
	free(dev->buffer);
	dev->buffer = NULL;
    }
}


static void
do_command(void *p, uint8_t *cdb)
{
    mo_t *dev = (mo_t *)p;
    int pos = 0, block_desc = 0;
    int ret;
    int32_t len, max_len;
    int32_t alloc_length;
    uint32_t i = 0;
    int size_idx, idx = 0;
    unsigned preamble_len;
    int32_t blen = 0;
    int32_t *BufLen;
	uint32_t previous_pos = 0;

    if (dev->drv->bus_type == MO_BUS_SCSI) {
	BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;
	dev->status &= ~ERR_STAT;
    } else {
	BufLen = &blen;
	dev->error = 0;
    }

    dev->packet_len = 0;
    dev->request_pos = 0;

    memcpy(dev->current_cdb, cdb, 12);

    if (cdb[0] != 0) {
	DEBUG("MO %i: Command 0x%02X, Sense Key %02X, Asc %02X, Ascq %02X, Unit attention: %i\n",
		dev->id, cdb[0], dev->sense[2], dev->sense[12], dev->sense[13], dev->unit_attention);
	DEBUG("MO %i: Request length: %04X\n", dev->id, dev->request_length);

	DEBUG("MO %i: CDB: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", dev->id,
		cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
		cdb[8], cdb[9], cdb[10], cdb[11]);
    }

    dev->sector_len = 0;

    set_phase(dev, SCSI_PHASE_STATUS);

    /* This handles the Not Ready/Unit Attention check if it has to be handled at this point. */
    if (pre_execution_check(dev, cdb) == 0)
	return;

    switch (cdb[0]) {
	case GPCMD_SEND_DIAGNOSTIC:
		if (! (cdb[1] & (1 << 2))) {
			invalid_field(dev);
			return;
		}

	case GPCMD_SCSI_RESERVE:
	case GPCMD_SCSI_RELEASE:
	case GPCMD_TEST_UNIT_READY:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_FORMAT_UNIT:
		if (dev->drv->read_only) {
			write_protected(dev);
			return;
		}

		mo_format(dev);
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_REZERO_UNIT:
		dev->sector_pos = dev->sector_len = 0;
		do_seek(dev, 0);
		set_phase(dev, SCSI_PHASE_STATUS);
		break;

	case GPCMD_REQUEST_SENSE:
		/* If there's a unit attention condition and there's a buffered not ready, a standalone REQUEST SENSE
		   should forget about the not ready, and report unit attention straight away. */
		set_phase(dev, SCSI_PHASE_DATA_IN);
		max_len = cdb[4];

		if (! max_len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20LL * MO_TIME;

			set_callback(dev);
			break;
		}

		buf_alloc(dev, 256);
		set_buf_len(dev, BufLen, &max_len);
		len = (cdb[1] & 1) ? 8 : 18;
		request_sense(dev, dev->buffer, max_len, cdb[1] & 1);
		data_command_finish(dev, len, len, cdb[4], 0);
		break;

	case GPCMD_READ_6:
	case GPCMD_READ_10:
	case GPCMD_READ_12:
		set_phase(dev, SCSI_PHASE_DATA_IN);
		alloc_length = dev->drv->sector_size;

		switch(cdb[0]) {
			case GPCMD_READ_6:
				dev->sector_len = cdb[4];
				dev->sector_pos = ((((uint32_t) cdb[1]) & 0x1f) << 16) | (((uint32_t) cdb[2]) << 8) | ((uint32_t) cdb[3]);

				if(dev->sector_len == 0)
					dev->sector_len = 256;

				DEBUG("MO %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);
				break;

			case GPCMD_READ_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				DEBUG("MO %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);
				break;

			case GPCMD_READ_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				DEBUG("MO %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);
				break;
		}

		if (! dev->sector_len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DBGLOG(1, "MO %i: All done - callback set\n", dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20LL * MO_TIME;

			set_callback(dev);
			break;
		}

		/*
		 * If we are reading all blocks in one go for DMA, why
		 * not also for PIO, it should NOT matter anyway, this
		 * step should be identical and only the way the read
		 * data is transferred to the host should be different.
		 */
		max_len = dev->sector_len;
		dev->requested_blocks = max_len;

		dev->packet_len = max_len * alloc_length;
		buf_alloc(dev, dev->packet_len);

		ret = mo_blocks(dev, &alloc_length, 1, 0);
		if (ret <= 0) {
			buf_free(dev);
			return;
		}

		dev->requested_blocks = max_len;
		dev->packet_len = alloc_length;

		set_buf_len(dev, BufLen, (int32_t *) &dev->packet_len);

		data_command_finish(dev, alloc_length, dev->drv->sector_size, alloc_length, 0);

		if (dev->packet_status != PHASE_COMPLETE)
			ui_sb_icon_update(SB_MO | dev->id, 1);
		else
			ui_sb_icon_update(SB_MO | dev->id, 0);
		return;

	case GPCMD_VERIFY_6:
	case GPCMD_VERIFY_10:
	case GPCMD_VERIFY_12:
		// Data and blank verification cannot be set at the same time
		if((cdb[1] & 2) && (cdb[1] & 4))
		{
			invalid_field(dev);
			return;
		}

		if (! (cdb[1] & 2) || (cdb[1] & 4)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			command_complete(dev);
			break;
		}

		// TODO: Implement
		invalid_field(dev);
		return;

	case GPCMD_WRITE_6:
	case GPCMD_WRITE_10:
	case GPCMD_WRITE_AND_VERIFY_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_12:
		set_phase(dev, SCSI_PHASE_DATA_OUT);
		alloc_length = dev->drv->sector_size;

		if (dev->drv->read_only) {
			write_protected(dev);
			return;
		}

		switch(cdb[0]) {
			case GPCMD_WRITE_6:
				dev->sector_len = cdb[4];
				dev->sector_pos = ((((uint32_t) cdb[1]) & 0x1f) << 16) | (((uint32_t) cdb[2]) << 8) | ((uint32_t) cdb[3]);

				if(dev->sector_len)
					dev->sector_len = 256;
				break;

			case GPCMD_WRITE_10:
			case GPCMD_WRITE_AND_VERIFY_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				break;

			case GPCMD_WRITE_12:
			case GPCMD_WRITE_AND_VERIFY_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				break;
		}

		DEBUG("MO %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);

		if ((dev->sector_pos >= dev->drv->medium_size)) {
			lba_out_of_range(dev);
			return;
		}

		if (! dev->sector_len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DBGLOG(1, "MO %i: All done - callback set\n", dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20LL * MO_TIME;

			set_callback(dev);
			break;
		}

		/*
		 * If we are writing all blocks in one go for DMA, why
		 * not also for PIO, it should NOT matter anyway, this
		 * step should be identical and only the way the data
		 * written is transferred to the host should be different.
		 */
		max_len = dev->sector_len;
		dev->requested_blocks = max_len;

		dev->packet_len = max_len * alloc_length;
		buf_alloc(dev, dev->packet_len);

		dev->requested_blocks = max_len;
		dev->packet_len = max_len << 9;

		set_buf_len(dev, BufLen, (int32_t *) &dev->packet_len);

		data_command_finish(dev, dev->packet_len, dev->drv->sector_size, dev->packet_len, 1);

		if (dev->packet_status != PHASE_COMPLETE)
			ui_sb_icon_update(SB_MO | dev->id, 1);
		else
			ui_sb_icon_update(SB_MO | dev->id, 0);
		return;

	case GPCMD_MODE_SENSE_6:
	case GPCMD_MODE_SENSE_10:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		block_desc = ((cdb[1] >> 3) & 1) ? 0 : 1;

		if (cdb[0] == GPCMD_MODE_SENSE_6) {
			len = cdb[4];
			buf_alloc(dev, 256);
		} else {
			len = (cdb[8] | (cdb[7] << 8));
			buf_alloc(dev, 65536);
		}

		if (!(mode_sense_page_flags & (1LL << (uint64_t) (cdb[2] & 0x3f)))) {
			invalid_field(dev);
			buf_free(dev);
			return;
		}

		memset(dev->buffer, 0, len);
		alloc_length = len;

		if (cdb[0] == GPCMD_MODE_SENSE_6) {
			len = mode_sense(dev, dev->buffer, 4, cdb[2], block_desc);
			len = MIN(len, alloc_length);
			dev->buffer[0] = len - 1;
			dev->buffer[1] = 3; // Medium type
			dev->buffer[2] = 0; // Device specific parameters
			if (block_desc)
				dev->buffer[3] = 8;
		} else {
			len = mode_sense(dev, dev->buffer, 8, cdb[2], block_desc);
			len = MIN(len, alloc_length);
			dev->buffer[0]=(len - 2) >> 8;
			dev->buffer[1]=(len - 2) & 255;
			dev->buffer[2] = 3; // Medium type
			dev->buffer[3] = 0; // Device specific parameters
			if (block_desc) {
				dev->buffer[6] = 0;
				dev->buffer[7] = 8;
			}
		}

		set_buf_len(dev, BufLen, &len);

		DEBUG("MO %i: Reading mode page: %02X...\n", dev->id, cdb[2]);

		data_command_finish(dev, len, len, alloc_length, 0);
		return;

	case GPCMD_MODE_SELECT_6:
	case GPCMD_MODE_SELECT_10:
		set_phase(dev, SCSI_PHASE_DATA_OUT);

		if (cdb[0] == GPCMD_MODE_SELECT_6) {
			len = cdb[4];
			buf_alloc(dev, 256);
		} else {
			len = (cdb[7] << 8) | cdb[8];
			buf_alloc(dev, 65536);
		}

		set_buf_len(dev, BufLen, &len);

		dev->total_length = len;
		dev->do_page_save = cdb[1] & 1;

		data_command_finish(dev, len, len, len, 1);
		return;

	case GPCMD_START_STOP_UNIT:
		set_phase(dev, SCSI_PHASE_STATUS);

		switch(cdb[4] & 3) {
			case 0:
				/* Stop the disk */
				break;
			case 1:
				/* Start unit */
				break;
			case 2:
				/* Eject the disk */
				ui_mo_eject(dev->id);
				break;
			case 3:
				/* Load the disk */
				ui_mo_reload(dev->id);
				break;
		}

		command_complete(dev);
		break;

	case GPCMD_INQUIRY:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		max_len = cdb[3];
		max_len <<= 8;
		max_len |= cdb[4];

		buf_alloc(dev, 65536);

		if (cdb[1] & 1) {
			preamble_len = 4;
			size_idx = 3;

			dev->buffer[idx++] = 7; // Optical disk
			dev->buffer[idx++] = cdb[2];
			dev->buffer[idx++] = 0;

			idx++;

			switch (cdb[2]) {
				case 0x00:
					dev->buffer[idx++] = 0x00;
					dev->buffer[idx++] = 0x80;
					break;

				// Unit serial number page
				case 0x80:
					dev->buffer[idx++] = strlen("VCM!10") + 1;
					ide_padstr8(dev->buffer + idx, 20, "VCM!10");	/* Serial */
					idx += strlen("VCM!10");
					break;

				default:
					DEBUG("INQUIRY: Invalid page: %02X\n", cdb[2]);
					invalid_field(dev);
					buf_free(dev);
					return;
			}
		} else {
			preamble_len = 5;
			size_idx = 4;

			memset(dev->buffer, 0, 8);
			if (cdb[1] & 0xe0)
				dev->buffer[0] = 0x60; /*No physical device on this LUN*/
			else
				dev->buffer[0] = 0x07; /*Optical disk*/
			dev->buffer[1] = 0x80; /*Removable*/
			dev->buffer[2] = (dev->drv->bus_type == MO_BUS_SCSI) ? 0x02 : 0x00; /*SCSI-2 compliant*/
			dev->buffer[3] = (dev->drv->bus_type == MO_BUS_SCSI) ? 0x02 : 0x21;
			dev->buffer[4] = 31;
			if (dev->drv->bus_type == MO_BUS_SCSI) {
				dev->buffer[6] = 1;	/* 16-bit transfers supported */
				dev->buffer[7] = 0x20;	/* Wide bus supported */
			}

			ide_padstr8(dev->buffer + 8, 8, "VARCEM  "); /* Vendor */
			ide_padstr8(dev->buffer + 16, 16, "MAGNETO OPTICAL "); /* Product */
			ide_padstr8(dev->buffer + 32, 4, "1.00"); /* Revision */
			idx = 36;

			if (max_len == 96) {
				dev->buffer[4] = 91;
				idx = 96;
			} else if (max_len == 128) {
				dev->buffer[4] = 0x75;
				idx = 128;
			}
		}

atapi_out:
		dev->buffer[size_idx] = idx - preamble_len;
		len=idx;

		len = MIN(len, max_len);
		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, max_len, 0);
		break;

	case GPCMD_PREVENT_REMOVAL:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_SEEK_6:
	case GPCMD_SEEK_10:
		set_phase(dev, SCSI_PHASE_STATUS);

		switch(cdb[0]) {
			case GPCMD_SEEK_6:
				pos = (cdb[2] << 8) | cdb[3];
				break;

			case GPCMD_SEEK_10:
				pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				break;
		}
		do_seek(dev, pos);
		command_complete(dev);
		break;

	case GPCMD_READ_CDROM_CAPACITY:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		buf_alloc(dev, 8);

		if (read_capacity(dev, dev->current_cdb, dev->buffer, (uint32_t *) &len) == 0) {
			buf_free(dev);
			return;
		}

		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, len, 0);
		break;

	case GPCMD_ERASE_10:
	case GPCMD_ERASE_12:
		// Relative address
		if((cdb[1] & 1))
			previous_pos = dev->sector_pos;

		switch(cdb[0]) {
			case GPCMD_ERASE_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				break;

			case GPCMD_ERASE_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				break;
		}

		// Erase all remaining sectors
		if((cdb[1] & 4))
		{
			// Cannot have a sector number when erase all
			if(dev->sector_len)
			{
				invalid_field(dev);
				return;
			}

			mo_format(dev);
			set_phase(dev, SCSI_PHASE_STATUS);
			command_complete(dev);
			break;
		}

		switch(cdb[0]) {
			case GPCMD_ERASE_10:
			dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				break;

			case GPCMD_ERASE_12:
			dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				break;
		}

		dev->sector_pos += previous_pos;

		mo_erase(dev);
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

		// Never seen media that supports generations but it's interesting to know if any implementation calls this commmand
		case GPCMD_READ_GENERATION:
			set_phase(dev, SCSI_PHASE_DATA_IN);

			buf_alloc(dev, 4);
			len = 4;

			dev->buffer[0] = 0;
			dev->buffer[1] = 0;
			dev->buffer[2] = 0;
			dev->buffer[3] = 0;

			set_buf_len(dev, BufLen, &len);

			data_command_finish(dev, len, len, len, 0);
			break;

		default:
		illegal_opcode(dev);
		break;
    }

    DBGLOG(1, "MO %i: Phase: %02X, request length: %i\n",
	   dev->id, dev->phase, dev->request_length);

    if (atapi_phase_to_scsi(dev) == SCSI_PHASE_STATUS)
	buf_free(dev);
}


/* The command second phase function, needed for Mode Select. */
static uint8_t
phase_data_out(mo_t *dev)
{
    uint16_t block_desc_len;
    uint16_t pos;
    uint8_t error = 0;
    uint8_t page, page_len;
    uint32_t i = 0;
    uint8_t hdr_len, val, old_val, ch;
    uint32_t last_to_write = 0;
    uint32_t c, h, s;
    int len = 0;

    switch(dev->current_cdb[0]) {
	case GPCMD_VERIFY_6:
	case GPCMD_VERIFY_10:
	case GPCMD_VERIFY_12:
		break;

	case GPCMD_WRITE_6:
	case GPCMD_WRITE_10:
	case GPCMD_WRITE_AND_VERIFY_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_12:
		if (dev->requested_blocks > 0)
			mo_blocks(dev, &len, 1, 1);
		break;
		
	case GPCMD_MODE_SELECT_6:
	case GPCMD_MODE_SELECT_10:
		if (dev->current_cdb[0] == GPCMD_MODE_SELECT_10)
			hdr_len = 8;
		else
			hdr_len = 4;

		if (dev->drv->bus_type == MO_BUS_SCSI) {
			if (dev->current_cdb[0] == GPCMD_MODE_SELECT_6) {
				block_desc_len = dev->buffer[2];
				block_desc_len <<= 8;
				block_desc_len |= dev->buffer[3];
			} else {
				block_desc_len = dev->buffer[6];
				block_desc_len <<= 8;
				block_desc_len |= dev->buffer[7];
			}
		} else
			block_desc_len = 0;

		pos = hdr_len + block_desc_len;

		while(1) {
			if (pos >= dev->current_cdb[4]) {
				DEBUG("MO %i: Buffer has only block descriptor\n", dev->id);
				break;
			}

			page = dev->buffer[pos] & 0x3F;
			page_len = dev->buffer[pos + 1];

			pos += 2;

			if (!(mode_sense_page_flags & (1LL << ((uint64_t) page))))
				error |= 1;
			else {
				for (i = 0; i < page_len; i++) {
					ch = mode_sense_pages_changeable.pages[page][i + 2];
					val = dev->buffer[pos + i];
					old_val = dev->ms_pages_saved.pages[page][i + 2];
					if (val != old_val) {
						if (ch)
							dev->ms_pages_saved.pages[page][i + 2] = val;
						else
							error |= 1;
					}
				}
			}

			pos += page_len;

			val = mode_sense_pages_default.pages[page][0] & 0x80;
			if (dev->do_page_save && val)
				mode_sense_save(dev);

			if (pos >= dev->total_length)
				break;
		}

		if (error) {
			invalid_field_pl(dev);
			return 0;
		}
		break;
    }

    return 1;
}


/* This is the general ATAPI PIO request function. */
static void
pio_request(mo_t *dev, uint8_t out)
{
    int ret = 0;

    if (dev->drv->bus_type < MO_BUS_SCSI) {
	DEBUG("MO %i: Lowering IDE IRQ\n", dev->id);
	ide_irq_lower(ide_drives[dev->drv->bus_id.ide_channel]);
    }

    dev->status = BUSY_STAT;

    if (dev->pos >= dev->packet_len) {
	DEBUG("MO %i: %i bytes %s, command done\n",
	      dev->id, dev->pos, out ? "written" : "read");

	dev->pos = dev->request_pos = 0;
	if (out) {
		ret = phase_data_out(dev);
		/* If ret = 0 (phase 1 error), then we do not do anything else other than
		   free the buffer, as the phase and callback have already been set by the
		   error function. */
		if (ret)
			command_complete(dev);
	} else
		command_complete(dev);
	buf_free(dev);
    } else {
	DEBUG("MO %i: %i bytes %s, %i bytes are still left\n", dev->id,
	      dev->pos, out ? "written" : "read", dev->packet_len - dev->pos);

	/* If less than (packet length) bytes are remaining, update packet length
	   accordingly. */
	if ((dev->packet_len - dev->pos) < (dev->max_transfer_len))
		dev->max_transfer_len = dev->packet_len - dev->pos;
	DEBUG("MO %i: Packet length %i, request length %i\n",
	      dev->id, dev->packet_len, dev->max_transfer_len);

	dev->packet_status = out ? PHASE_DATA_OUT : PHASE_DATA_IN;

	dev->status = BUSY_STAT;
	dev->phase = 1;
	do_callback(dev);

	dev->callback = 0LL;
	set_callback(dev);

	dev->request_pos = 0;
    }
}


static int
read_from_ide_dma(mo_t *dev)
{
    int ret;

    if (dev == NULL)
	return 0;

    if (ide_bus_master_write) {
	ret = ide_bus_master_write(dev->drv->bus_id.ide_channel >> 1,
				   dev->buffer, dev->packet_len,
				   ide_bus_master_priv[dev->drv->bus_id.ide_channel >> 1]);
	if (ret == 2)		/* DMA not enabled, wait for it to be.. */
		return 2;
	if (ret == 1) {		/* DMA error */
		bus_master_error(dev);
		return 0;
	}

	return 1;
    }

    return 0;
}


static int
read_from_scsi_dma(scsi_device_t *sd, UNUSED(uint8_t scsi_id))
{
    mo_t *dev = (mo_t *)sd->p;
    int32_t *BufLen = &sd->buffer_length;

    if (dev == NULL)
	return 0;

    DEBUG("Reading from SCSI DMA: SCSI ID %02X, init length %i\n",
	  scsi_id, *BufLen);

    memcpy(dev->buffer, sd->cmd_buffer, *BufLen);

    return 1;
}


static void
mo_irq_raise(mo_t *dev)
{
    if (dev->drv->bus_type < MO_BUS_SCSI)
	ide_irq_raise(ide_drives[dev->drv->bus_id.ide_channel]);
}


static int
read_from_dma(mo_t *dev)
{
    scsi_device_t *sd = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun];
#ifdef _LOGGING
    int32_t *BufLen = &sd->buffer_length;
#endif
    int ret = 0;

    if (dev->drv->bus_type == MO_BUS_SCSI)
	ret = read_from_scsi_dma(sd, dev->drv->bus_id.scsi.id);
    else
	ret = read_from_ide_dma(dev);

    if (ret != 1)
	return ret;

    if (dev->drv->bus_type == MO_BUS_SCSI)
	DEBUG("MO %i: SCSI Input data length: %i\n", dev->id, *BufLen);
    else
	DEBUG("MO %i: ATAPI Input data length: %i\n", dev->id, dev->packet_len);

    ret = phase_data_out(dev);

    if (ret)
	return 1;

    return 0;
}


static int
write_to_ide_dma(mo_t *dev)
{
    int ret;

    if (dev == NULL)
	return 0;

    if (ide_bus_master_read) {
	ret = ide_bus_master_read(dev->drv->bus_id.ide_channel >> 1,
				  dev->buffer, dev->packet_len,
				  ide_bus_master_priv[dev->drv->bus_id.ide_channel >> 1]);
	if (ret == 2)		/* DMA not enabled, wait for it to be.. */
		return 2;
	if (ret == 1) {		/* DMA error */
		bus_master_error(dev);
		return 0;
	}
	return 1;
    }

    return 0;
}


static int
write_to_scsi_dma(scsi_device_t *sd, UNUSED(uint8_t scsi_id))
{
    mo_t *dev = (mo_t *)sd->p;
    int32_t BufLen = sd->buffer_length;

    if (dev == NULL)
	return 0;

    DEBUG("Writing to SCSI DMA: SCSI ID %02X, init length %i\n", scsi_id, BufLen);
    memcpy(sd->cmd_buffer, dev->buffer, BufLen);
    DEBUG("MO %i: Data from CD buffer:  %02X %02X %02X %02X %02X %02X %02X %02X\n", dev->id,
	    dev->buffer[0], dev->buffer[1], dev->buffer[2], dev->buffer[3], dev->buffer[4], dev->buffer[5],
	    dev->buffer[6], dev->buffer[7]);
    return 1;
}


static int
write_to_dma(mo_t *dev)
{
    scsi_device_t *sd = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun];
#ifdef _LOGGING
    int32_t BufLen = sd->buffer_length;
#endif
    int ret = 0;

    if (dev->drv->bus_type == MO_BUS_SCSI) {
	DEBUG("Write to SCSI DMA: (ID %02X)\n", dev->drv->bus_id.scsi.id);
	ret = write_to_scsi_dma(sd, dev->drv->bus_id.scsi.id);
    } else
	ret = write_to_ide_dma(dev);

    if (dev->drv->bus_type == MO_BUS_SCSI)
	DEBUG("MO %i: SCSI Output data length: %i\n", dev->id, BufLen);
    else
	DEBUG("MO %i: ATAPI Output data length: %i\n", dev->id, dev->packet_len);

    return ret;
}


static void
do_callback(void *p)
{
    mo_t *dev = (mo_t *)p;
    int ret;

    switch(dev->packet_status) {
	case PHASE_IDLE:
		DEBUG("MO %i: PHASE_IDLE\n", dev->id);
		dev->pos = 0;
		dev->phase = 1;
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		return;

	case PHASE_COMMAND:
		DEBUG("MO %i: PHASE_COMMAND\n", dev->id);
		dev->status = BUSY_STAT | (dev->status & ERR_STAT);
		memcpy(dev->atapi_cdb, dev->buffer, 12);
		do_command(dev, dev->atapi_cdb);
		return;

	case PHASE_COMPLETE:
		DEBUG("MO %i: PHASE_COMPLETE\n", dev->id);
		dev->status = READY_STAT;
		dev->phase = 3;
		dev->packet_status = 0xFF;
		ui_sb_icon_update(SB_MO | dev->id, 0);
		mo_irq_raise(dev);
		return;

	case PHASE_DATA_OUT:
		DEBUG("MO %i: PHASE_DATA_OUT\n", dev->id);
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		dev->phase = 0;
		mo_irq_raise(dev);
		return;

	case PHASE_DATA_OUT_DMA:
		DEBUG("MO %i: PHASE_DATA_OUT_DMA\n", dev->id);
		ret = read_from_dma(dev);

		if ((ret == 1) || (dev->drv->bus_type == MO_BUS_SCSI)) {
			DEBUG("MO %i: DMA data out phase done\n");
			buf_free(dev);
			command_complete(dev);
		} else if (ret == 2) {
			DEBUG("MO %i: DMA out not enabled, wait\n");
			command_bus(dev);
		} else {
			DEBUG("MO %i: DMA data out phase failure\n");
			buf_free(dev);
		}
		return;
	case PHASE_DATA_IN:
		DEBUG("MO %i: PHASE_DATA_IN\n", dev->id);
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		dev->phase = 2;
		mo_irq_raise(dev);
		return;

	case PHASE_DATA_IN_DMA:
		DEBUG("MO %i: PHASE_DATA_IN_DMA\n", dev->id);
		ret = write_to_dma(dev);

		if ((ret == 1) || (dev->drv->bus_type == MO_BUS_SCSI)) {
			DEBUG("MO %i: DMA data in phase done\n", dev->id);
			buf_free(dev);
			command_complete(dev);
		} else if (ret == 2) {
			DEBUG("MO %i: DMA in not enabled, wait\n", dev->id);
			command_bus(dev);
		} else {
			DEBUG("MO %i: DMA data in phase failure\n", dev->id);
			buf_free(dev);
		}
		return;

	case PHASE_ERROR:
		DEBUG("MO %i: PHASE_ERROR\n", dev->id);
		dev->status = READY_STAT | ERR_STAT;
		dev->phase = 3;
		dev->packet_status = 0xFF;
		mo_irq_raise(dev);
		ui_sb_icon_update(SB_MO | dev->id, 0);
		return;
    }
}


static uint32_t
packet_read(void *p, int length)
{
    mo_t *dev = (mo_t *) p;
    uint16_t *mobufferw;
    uint32_t *mobufferl;
    uint32_t temp = 0;

    if ((dev == NULL) || (dev->buffer == NULL))
	return 0;

    mobufferw = (uint16_t *)dev->buffer;
    mobufferl = (uint32_t *)dev->buffer;

    /* Make sure we return a 0 and don't attempt to read from the buffer if we're transferring bytes beyond it,
       which can happen when issuing media access commands with an allocated length below minimum request length
       (which is 1 sector = 512 bytes). */
    switch(length) {
	case 1:
		temp = (dev->pos < dev->packet_len) ? dev->buffer[dev->pos] : 0;
		dev->pos++;
		dev->request_pos++;
		break;

	case 2:
		temp = (dev->pos < dev->packet_len) ? mobufferw[dev->pos >> 1] : 0;
		dev->pos += 2;
		dev->request_pos += 2;
		break;

	case 4:
		temp = (dev->pos < dev->packet_len) ? mobufferl[dev->pos >> 2] : 0;
		dev->pos += 4;
		dev->request_pos += 4;
		break;

	default:
		return 0;
    }

    if (dev->packet_status == PHASE_DATA_IN) {
	if ((dev->request_pos >= dev->max_transfer_len) || (dev->pos >= dev->packet_len)) {
		/* Time for a DRQ. */
		pio_request(dev, 0);
	}
	return temp;
    }

    return 0;
}


static void
packet_write(void *p, uint32_t val, int length)
{
    mo_t *dev = (mo_t *) p;
    uint16_t *mobufferw;
    uint32_t *mobufferl;

    if (dev == NULL)
	return;

    if (dev->packet_status == PHASE_IDLE) {
	if (dev->buffer == NULL)
		buf_alloc(dev, 12);
    }

    if (dev->buffer == NULL)
	return;

    mobufferw = (uint16_t *)dev->buffer;
    mobufferl = (uint32_t *)dev->buffer;

    switch(length) {
	case 1:
		dev->buffer[dev->pos] = val & 0xff;
		dev->pos++;
		dev->request_pos++;
		break;

	case 2:
		mobufferw[dev->pos >> 1] = val & 0xffff;
		dev->pos += 2;
		dev->request_pos += 2;
		break;

	case 4:
		mobufferl[dev->pos >> 2] = val;
		dev->pos += 4;
		dev->request_pos += 4;
		break;

	default:
		return;
    }

    if (dev->packet_status == PHASE_DATA_OUT) {
	if ((dev->request_pos >= dev->max_transfer_len) || (dev->pos >= dev->packet_len)) {
		/* Time for a DRQ. */
		pio_request(dev, 1);
	}
	return;
    }

    if (dev->packet_status == PHASE_IDLE) {
	if (dev->pos >= 12) {
		dev->pos = 0;
		dev->status = BUSY_STAT;
		dev->packet_status = PHASE_COMMAND;
		timer_process();
		do_callback((void *) dev);
		timer_update_outstanding();
	}
    }
}


static void
mo_identify(ide_t *ide, int ide_has_dma)
{
    ide_padstr((char *) (ide->buffer + 23), "1.00", 8); /* Firmware */
    ide_padstr((char *) (ide->buffer + 27), "VARCEM MAGNETO OPTICAL", 40); /* Model */

    if (ide_has_dma) {
	ide->buffer[80] = 0x30; /*Supported ATA versions : ATA/ATAPI-4 ATA/ATAPI-5*/
	ide->buffer[81] = 0x15; /*Maximum ATA revision supported : ATA/ATAPI-5 T13 1321D revision 1*/
    }
}


static int
get_max(int ide_has_dma, int type)
{
    int ret = -1;

    switch(type) {
	case TYPE_PIO:
		ret = ide_has_dma ? 3 : 0;
		break;

	case TYPE_MDMA:
		ret = ide_has_dma ? -1 : 1;
		break;

	case TYPE_UDMA:
		ret = ide_has_dma ? -1 : 2;
		break;

	case TYPE_SDMA:
	default:
		break;
    }

    return ret;
}


static int
get_timings(int ide_has_dma, int type)
{
    int ret = 0;

    switch(type) {
	case TIMINGS_DMA:
		ret = ide_has_dma ? 0x96 : 0;
		break;

	case TIMINGS_PIO:
		ret = ide_has_dma ? 0xb4 : 0;
		break;

	case TIMINGS_PIO_FC:
		ret = ide_has_dma ? 0xb4 : 0;
		break;

	default:
		break;
    }

    return ret;
}


static void
do_identify(void *p, int ide_has_dma)
{
    ide_t *ide = (ide_t *)p;

    ide->buffer[0] = 0x8000 | (0<<8) | 0x80 | (1<<5); /* ATAPI device, direct-access device, removable media, interrupt DRQ */
    ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
    ide->buffer[49] = 0x200; /* LBA supported */
    ide->buffer[126] = 0xfffe; /* Interpret zero byte count limit as maximum length */
	mo_identify(ide, ide_has_dma);
}


static void
do_init(mo_t *dev)
{
    dev->requested_blocks = 1;
    dev->sense[0] = 0xf0;
    dev->sense[7] = 10;

    dev->drv->bus_mode = 0;
    if (dev->drv->bus_type >= MO_BUS_ATAPI)
	dev->drv->bus_mode |= 2;
    if (dev->drv->bus_type < MO_BUS_SCSI)
	dev->drv->bus_mode |= 1;
    DEBUG("MO %i: Bus type %i, bus mode %i\n",
	  dev->id, dev->drv->bus_type, dev->drv->bus_mode);
    if (dev->drv->bus_type == MO_BUS_ATAPI)
	set_signature(dev);
    dev->status = READY_STAT | DSC_STAT;
    dev->pos = 0;
    dev->packet_status = 0xff;
    dev->sense[2] = dev->sense[12] = dev->sense[13] = 0;
    dev->unit_attention = 0;
}


static void
drive_reset(int c)
{
    scsi_device_t *sd;
    mo_t *dev;
    ide_t *id;

    dev = (mo_t *)mo_drives[c].priv;
    if (dev == NULL) {
	dev = (mo_t *)mem_alloc(sizeof(mo_t));
	mo_drives[c].priv = dev;
    }
    memset(dev, 0x00, sizeof(mo_t));
    dev->id = c;
    dev->drv = &mo_drives[c];

    switch (mo_drives[c].bus_type) {
	case MO_BUS_SCSI:
pclog(0,"MO: attaching to SCSI device %d:%d\n", dev->drv->bus_id.scsi.id, dev->drv->bus_id.scsi.lun);
		sd = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun];

		sd->p = dev;
		sd->command = do_command;
		sd->callback = do_callback;
		sd->err_stat_to_scsi = err_stat_to_scsi;
		sd->request_sense = request_sense_scsi;
		sd->reset = do_reset;
		sd->read_capacity = read_capacity;
		sd->type = SCSI_REMOVABLE_DISK;

		DEBUG("SCSI MO drive %i attached to SCSI ID %i\n",
	 	     c, dev->drv->bus_id.scsi.id);
		break;

	case MO_BUS_ATAPI:
pclog(0,"MO: attaching to IDE device %d\n", dev->drv->bus_id.ide_channel);
		id = ide_drives[dev->drv->bus_id.ide_channel];

		/*
		 * If the IDE channel is initialized, we attach to it,
		 * otherwise, we do nothing - it's going to be a drive
		 * that's not attached to anything.
		 */
		if (id == NULL) break;

		id->p = dev;
		id->get_max = get_max;
		id->get_timings = get_timings;
		id->identify = do_identify;
		id->set_signature = set_signature;
		id->packet_write = packet_write;
		id->packet_read = packet_read;
		id->stop = NULL;
		id->packet_callback = do_callback;
		id->device_reset = do_reset;
		id->interrupt_drq = 1;

		ide_atapi_attach(id);

		DEBUG("ATAPI MO drive %i attached to IDE channel %i\n",
		      c, dev->drv->bus_id.ide_channel);
		break;
    }
}


/* Peform a master init on the entire module. */
void
mo_global_init(void)
{
    /* Clear the global data. */
    memset(mo_drives, 0x00, sizeof(mo_drives));
}


void
mo_hard_reset(void)
{
    mo_t *dev;
    int c;

    for (c = 0; c < MO_NUM; c++) {
	if ((mo_drives[c].bus_type != MO_BUS_ATAPI) &&
	    (mo_drives[c].bus_type != MO_BUS_SCSI)) continue;

	DEBUG("MO hard_reset drive=%d\n", c);

	/* Ignore any SCSI MO drive that has an out of range ID. */
	if ((mo_drives[c].bus_type == MO_BUS_SCSI) &&
	    (mo_drives[c].bus_id.scsi.id >= SCSI_ID_MAX)) continue;

	/* Ignore any ATAPI MO drive that has an out of range IDE channel. */
	if ((mo_drives[c].bus_type == MO_BUS_ATAPI) &&
	    (mo_drives[c].bus_id.ide_channel > 7)) continue;

	drive_reset(c);

	dev = (mo_t *)mo_drives[c].priv;

	do_init(dev);

	if (wcslen(dev->drv->image_path))
		mo_load(dev, dev->drv->image_path);

	mode_sense_load(dev);
    }
}


void
mo_close(void)
{
    mo_t *dev;
    int c;

    for (c = 0; c < MO_NUM; c++) {
	dev = (mo_t *)mo_drives[c].priv;

	if (dev != NULL) {
		mo_disk_close(dev);

		free(dev);

		mo_drives[c].priv = NULL;
	}
    }
}


void
mo_reset_bus(int bus)
{
    int i;

    for (i = 0; i < MO_NUM; i++) {
	if ((mo_drives[i].bus_type == bus) && mo_drives[i].priv)
		do_reset(mo_drives[i].priv);
    }
}


int
mo_string_to_bus(const char *str)
{
    int ret = MO_BUS_DISABLED;

    if (! strcmp(str, "none")) return(ret);

    if (! strcmp(str, "ide") || !strcmp(str, "atapi")) return(MO_BUS_ATAPI);

    if (! strcmp(str, "scsi"))
	return(MO_BUS_SCSI);

    if (! strcmp(str, "usb"))
	ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NO_USB);

    return(ret);
}


const char *
MO_BUS_to_string(int bus)
{
    const char *ret = "none";

    switch (bus) {
	case MO_BUS_DISABLED:
	default:
		break;

	case MO_BUS_ATAPI:
		ret = "atapi";
		break;

	case MO_BUS_SCSI:
		ret = "scsi";
		break;
    }

    return(ret);
}


void
mo_disk_close(mo_t *dev)
{
    if (dev->drv->f) {
	fclose(dev->drv->f);
	dev->drv->f = NULL;

	dev->drv->medium_size = 0;
    }
}


int
mo_load(mo_t *dev, const wchar_t *fn)
{
    int read_only = dev->drv->ui_writeprot;
    int size = 0;

    dev->drv->f = plat_fopen(fn, dev->drv->ui_writeprot ? L"rb" : L"rb+");
    if (!dev->drv->ui_writeprot && !dev->drv->f) {
	dev->drv->f = plat_fopen(fn, L"rb");
	read_only = 1;
    }
    if (dev->drv->f) {
	fseek(dev->drv->f, 0, SEEK_END);
	size = ftell(dev->drv->f);

	switch(size)
	{
		case MO_SECTORS_ISO10090 * MO_BPS_ISO10090:
			dev->drv->medium_size = MO_SECTORS_ISO10090;
			dev->drv->sector_size = MO_BPS_ISO10090;
			break;
		case MO_SECTORS_ISO13963 * MO_BPS_ISO13963:
			dev->drv->medium_size = MO_SECTORS_ISO13963;
			dev->drv->sector_size = MO_BPS_ISO13963;
			break;
		case MO_SECTORS_ISO15498 * MO_BPS_ISO15498:
			dev->drv->medium_size = MO_SECTORS_ISO15498;
			dev->drv->sector_size = MO_BPS_ISO15498;
			break;
		case MO_SECTORS_GIGAMO * MO_BPS_GIGAMO:
			dev->drv->medium_size = MO_SECTORS_GIGAMO;
			dev->drv->sector_size = MO_BPS_GIGAMO;
			break;
		case MO_SECTORS_GIGAMO2 * MO_BPS_GIGAMO2:
			dev->drv->medium_size = MO_SECTORS_GIGAMO2;
			dev->drv->sector_size = MO_BPS_GIGAMO2;
			break;
		default:
			DEBUG("File is incorrect size for a magneto-optical image\n");
			fclose(dev->drv->f);
			dev->drv->f = NULL;
			dev->drv->medium_size = 0;
			dev->drv->sector_size = 0;
			ui_mo_eject(dev->id);	/* Make sure the host OS knows we've rejected (and ejected) the image. */
			return 0;
	}

	fseek(dev->drv->f, dev->drv->base, SEEK_SET);

	memcpy(dev->drv->image_path, fn, sizeof(dev->drv->image_path));

	dev->drv->read_only = read_only;

	return 1;
    }

    return 0;
}


void
mo_disk_reload(mo_t *dev)
{
    int ret = 0;

    if (wcslen(dev->drv->prev_image_path) == 0)
	return;
    else
	ret = mo_load(dev, dev->drv->prev_image_path);

    if (ret)
	dev->unit_attention = 1;
}


void
mo_insert(mo_t *dev)
{
    dev->unit_attention = 1;
}


void
mo_format(mo_t *dev)
{
	long size;
	int ret;
	int fd;

	DEBUG("Formatting media...\n");

	fseek(dev->drv->f, 0, SEEK_END);
	size = ftell(dev->drv->f);

#ifdef _WIN32
	HANDLE fh;
	LARGE_INTEGER liSize;

	fd = _fileno(dev->drv->f);
	fh = (HANDLE)_get_osfhandle(fd);

	liSize.QuadPart = 0;

	ret = (int)SetFilePointerEx(fh, liSize, NULL, FILE_BEGIN);

	if(!ret)
	{
		DEBUG("MO %i: Failed seek to start of image file\n", dev->id);
		return;
	}

	ret = (int)SetEndOfFile(fh);

	if(ret)
	{
		DEBUG("MO %i: Failed to truncate image file to 0\n", dev->id);
		return;
	}

	liSize.QuadPart = size;
	ret = (int)SetFilePointerEx(fh, liSize, NULL, FILE_BEGIN);

	if(!ret)
	{
		DEBUG("MO %i: Failed seek to end of image file\n", dev->id);
		return;
	}

	ret = (int)SetEndOfFile(fh);

	if(ret)
	{
		DEBUG("MO %i: Failed to truncate image file to %llu\n", dev->id, size);
		return;
	}
#else
	fd = fileno(dev->drv->f);

	ret = ftruncate(fd, 0);

	if(ret)
	{
		DEBUG("MO %i: Failed to truncate image file to 0\n", dev->id);
		return;
	}

	ret = ftruncate(fd, size);

	if(ret)
	{
		DEBUG("MO %i: Failed to truncate image file to %llu", dev->id, size);
		return;
	}
#endif
}

static int
mo_erase(mo_t *dev)
{
        int i;

	if (! dev->sector_len) {
		command_complete(dev);
		return -1;
	}

	DEBUG("Erasing %i blocks starting from %i...\n", dev->sector_len, dev->sector_pos);

	if (dev->sector_pos >= dev->drv->medium_size) {
		DEBUG("MO %i: Trying to erase beyond the end of disk\n", dev->id);
		lba_out_of_range(dev);
		return 0;
	}

	buf_alloc(dev, dev->drv->sector_size);
	memset(dev->buffer, 0, dev->drv->sector_size);

	fseek(dev->drv->f, dev->drv->base + (dev->sector_pos * dev->drv->sector_size), SEEK_SET);

	for (i = 0; i < dev->requested_blocks; i++) {
		if (feof(dev->drv->f))
			break;

		fwrite(dev->buffer, 1, dev->drv->sector_size, dev->drv->f);
	}

	DEBUG("Erased %i bytes of blocks...\n", i * dev->drv->sector_size);

	dev->sector_pos += i;
	dev->sector_len -= i;

	return 1;
}
