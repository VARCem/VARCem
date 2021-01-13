/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of SCSI (and ATAPI) CD-ROM drives.
 *
 * Version:	@(#)scsi_cdrom.c	1.0.14	2021/01/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog scsi_cdrom_log
#include "../../emu.h"
#include "../../version.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "scsi_device.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../cdrom/cdrom.h"
#include "../cdrom/cdrom_image.h"
#include "scsi_cdrom.h"

/* Bits of 'status' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT		0x10
#define SERVICE_STAT		0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */


#pragma pack(push,1)
typedef struct {
    uint8_t	opcode;
    uint8_t	polled;
    uint8_t	reserved2[2];
    uint8_t	class;
    uint8_t	reserved3[2];
    uint16_t	len;
    uint8_t	control;
} gesn_cdb_t;

typedef struct {
    uint16_t	len;
    uint8_t	notification_class;
    uint8_t	supported_events;
} gesn_event_header_t;
#pragma pack(pop)


#ifdef ENABLE_SCSI_CDROM_LOG
int scsi_cdrom_do_log = ENABLE_SCSI_CDROM_LOG;
#endif


/*
 * Table of all SCSI commands and their flags,
 * needed for the new disc change / not ready handler.
 */
static const uint8_t command_flags[0x100] = {
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x00 */
    IMPLEMENTED | ALLOW_UA | NONDATA | SCSI_ONLY,		/* 0x01 */
    0,								/* 0x02 */
    IMPLEMENTED | ALLOW_UA,					/* 0x03 */
    0, 0, 0, 0,							/* 0x04-0x07 */
    IMPLEMENTED | CHECK_READY,					/* 0x08 */
    0, 0,							/* 0x09-0x0A */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x0B */
    0, 0, 0, 0, 0, 0,						/* 0x0C-0x11 */
    IMPLEMENTED | ALLOW_UA,					/* 0x12 */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0x13 */
    0,								/* 0x14 */
    IMPLEMENTED,						/* 0x15 */
    0, 0, 0, 0,							/* 0x16-0x19 */
    IMPLEMENTED,						/* 0x1A */
    IMPLEMENTED | CHECK_READY,					/* 0x1B */
    0, 0,							/* 0x1C-0x1D */
    IMPLEMENTED | CHECK_READY,					/* 0x1E */
    0, 0, 0, 0, 0, 0,						/* 0x1F-0x24 */
    IMPLEMENTED | CHECK_READY,					/* 0x25 */
    0, 0,							/* 0x26-0x27 */
    IMPLEMENTED | CHECK_READY,					/* 0x28 */
    0, 0,							/* 0x29-0x2A */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x2B */
    0, 0, 0,							/* 0x2C-0x2E */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0x2F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0x30-0x3F */
    0, 0,							/* 0x40-0x41 */
    IMPLEMENTED | CHECK_READY,					/* 0x42 */
    IMPLEMENTED | CHECK_READY,	/* 0x43 - Read TOC - can get through UNIT_ATTENTION, per VIDE-CDD.SYS
				   NOTE: The ATAPI reference says otherwise, but I think this is a question of
				   interpreting things right - the UNIT ATTENTION condition we have here
				   is a tradition from not ready to ready, by definition the drive
				   eventually becomes ready, make the condition go away. */
    IMPLEMENTED | CHECK_READY,					/* 0x44 */
    IMPLEMENTED | CHECK_READY,					/* 0x45 */
    IMPLEMENTED | ALLOW_UA,					/* 0x46 */
    IMPLEMENTED | CHECK_READY,					/* 0x47 */
    IMPLEMENTED | CHECK_READY,					/* 0x48 */
    0,								/* 0x49 */
    IMPLEMENTED | ALLOW_UA,					/* 0x4A */
    IMPLEMENTED | CHECK_READY,					/* 0x4B */
    0, 0,							/* 0x4C-0x4D */
    IMPLEMENTED | CHECK_READY,					/* 0x4E */
    0, 0,							/* 0x4F-0x50 */
    IMPLEMENTED | CHECK_READY,					/* 0x51 */
    IMPLEMENTED | CHECK_READY,					/* 0x52 */
    0, 0,							/* 0x53-0x54 */
    IMPLEMENTED,						/* 0x55 */
    0, 0, 0, 0,							/* 0x56-0x59 */
    IMPLEMENTED,						/* 0x5A */
    0, 0, 0, 0, 0,						/* 0x5B-0x5F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0x60-0x6F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0x70-0x7F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0x80-0x8F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0x90-0x9F */
    0, 0, 0, 0, 0,						/* 0xA0-0xA4 */
    IMPLEMENTED | CHECK_READY,					/* 0xA5 */
    0, 0,							/* 0xA6-0xA7 */
    IMPLEMENTED | CHECK_READY,					/* 0xA8 */
    0, 0, 0, 0,							/* 0xA9-0xAC */
    IMPLEMENTED | CHECK_READY,					/* 0xAD */
    0,								/* 0xAE */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0xAF */
    0, 0, 0, 0,							/* 0xB0-0xB3 */
    IMPLEMENTED | CHECK_READY | ATAPI_ONLY,			/* 0xB4 */
    0, 0, 0,							/* 0xB5-0xB7 */
    IMPLEMENTED | CHECK_READY | ATAPI_ONLY,			/* 0xB8 */
    IMPLEMENTED | CHECK_READY,					/* 0xB9 */
    IMPLEMENTED | CHECK_READY,					/* 0xBA */
    IMPLEMENTED,						/* 0xBB */
    IMPLEMENTED | CHECK_READY,					/* 0xBC */
    IMPLEMENTED,						/* 0xBD */
    IMPLEMENTED | CHECK_READY,					/* 0xBE */
    IMPLEMENTED | CHECK_READY,					/* 0xBF */
    0, 0,							/* 0xC0-0xC1 */
    IMPLEMENTED | CHECK_READY | SCSI_ONLY,			/* 0xC2 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,				/* 0xC3-0xCC */
    IMPLEMENTED | CHECK_READY | SCSI_ONLY,			/* 0xCD */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,				/* 0xCE-0xD9 */
    IMPLEMENTED | SCSI_ONLY,					/* 0xDA */
    0, 0, 0, 0, 0,						/* 0xDB-0xDF */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 0xE0-0xEF */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0		/* 0xF0-0xFF */
};

static const uint64_t mode_sense_page_flags =
		(GPMODEP_R_W_ERROR_PAGE |
		 GPMODEP_CDROM_PAGE |
		 GPMODEP_CDROM_AUDIO_PAGE |
		 GPMODEP_CAPABILITIES_PAGE |
		 GPMODEP_ALL_PAGES);

static const mode_sense_pages_t mode_sense_pages_default = {
    {
    {                        0,    0 },
    {    GPMODE_R_W_ERROR_PAGE,    6, 0, 5, 0,  0, 0,  0 },
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
    {        GPMODE_CDROM_PAGE,    6, 0, 1, 0, 60, 0, 75 },
    {                     0x8E,  0xE, 4, 0, 0,  0, 0, 75, 1,  255, 2, 255, 0, 0, 0,    0 },
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
    { GPMODE_CAPABILITIES_PAGE, 0x12, 0, 0, 1,  0, 0,  0, 2, 0xC2, 1,   0, 0, 0, 2, 0xC2, 0, 0, 0, 0 }
}   };

static const mode_sense_pages_t mode_sense_pages_default_scsi = {
    {
    {                        0,    0 },
    {    GPMODE_R_W_ERROR_PAGE,    6, 0, 5, 0,  0, 0,  0 },
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
    {        GPMODE_CDROM_PAGE,    6, 0, 1, 0, 60, 0, 75 },
    {                     0x8E,  0xE, 5, 4, 0,128, 0, 75, 1,  255, 2, 255, 0, 0, 0,    0 },
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
    { GPMODE_CAPABILITIES_PAGE, 0x12, 0, 0, 1,  0, 0,  0, 2, 0xC2, 1,   0, 0, 0, 2, 0xC2, 0, 0, 0, 0 }
}   };

static const mode_sense_pages_t mode_sense_pages_changeable = {
    {
    {                        0,    0 },
    {    GPMODE_R_W_ERROR_PAGE,    6, 0xFF, 0xFF, 0, 0, 0, 0 },
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
    {        GPMODE_CDROM_PAGE,    6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    {                     0x8E,  0xE, 0xFF, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
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
    { GPMODE_CAPABILITIES_PAGE, 0x12, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
}   };


static void	do_callback(void *p);


#ifdef _LOGGING
static void
scsi_cdrom_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SCSI_CDROM_LOG
    va_list ap;

    if (scsi_cdrom_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


static void
set_callback(scsi_cdrom_t *dev)
{
    if (dev && dev->drv && (dev->drv->bus_type != CDROM_BUS_SCSI))
	ide_set_callback(dev->drv->bus_id.ide_channel >> 1, dev->callback);
}


static void
set_signature(void *priv)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev == NULL)
	return;

    dev->phase = 1;
    dev->request_length = 0xEB14;
}


/* Returns: 0 for none, 1 for PIO, 2 for DMA. */
static int
current_mode(scsi_cdrom_t *dev)
{
    if (dev->drv->bus_type == CDROM_BUS_SCSI)
	return 2;

    if (dev->drv->bus_type == CDROM_BUS_ATAPI) {
	DEBUG("CD-ROM %i: ATAPI drive, setting to %s\n", dev->id,
		  (dev->features & 1) ? "DMA" : "PIO", dev->id);
	return (dev->features & 1) ? 2 : 1;
    }

    return 0;
}


#if 1	/* will be moved to IDE layer */
/* Translates ATAPI status (ERR_STAT flag) to SCSI status. */
static int
err_stat_to_scsi(void *priv)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev->status & ERR_STAT)
	return SCSI_STATUS_CHECK_CONDITION;

    return SCSI_STATUS_OK;
}
#endif


/* Translates ATAPI phase (DRQ, I/O, C/D) to SCSI phase (MSG, C/D, I/O). */
static int
atapi_phase_to_scsi(scsi_cdrom_t *dev)
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


static uint32_t
get_channel(void *priv, int channel)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev == NULL)
	return channel + 1;

    return dev->ms_pages_saved.pages[GPMODE_CDROM_AUDIO_PAGE][channel ? 10 : 8];
}


static uint32_t
get_volume(void *priv, int channel)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev == NULL)
	return 255;

    return dev->ms_pages_saved.pages[GPMODE_CDROM_AUDIO_PAGE][channel ? 11 : 9];
}


static void
mode_sense_load(scsi_cdrom_t *dev)
{
    wchar_t temp[512];
    const mode_sense_pages_t *ptr;
    FILE *fp;

    memset(temp, 0, sizeof(temp));
    if (dev->drv->bus_type == CDROM_BUS_SCSI) {
	swprintf(temp, sizeof_w(temp),
		 L"scsi_cdrom_%02i_mode_sense_bin", dev->id);
	ptr = &mode_sense_pages_default_scsi;
    } else {
	swprintf(temp, sizeof_w(temp),
		 L"cdrom_%02i_mode_sense_bin", dev->id);
	ptr = &mode_sense_pages_default;
    }

    memcpy(&dev->ms_pages_saved, ptr, sizeof(mode_sense_pages_t));

    fp = plat_fopen(nvr_path(temp), L"rb");
    if (fp != NULL) {
	(void)fread(&dev->ms_pages_saved.pages[GPMODE_CDROM_AUDIO_PAGE],
		    1, 0x10, fp);
	(void)fclose(fp);
    }
}


static void
mode_sense_save(scsi_cdrom_t *dev)
{
    wchar_t temp[512];
    FILE *fp;

    memset(temp, 0, sizeof(temp));
    if (dev->drv->bus_type == CDROM_BUS_SCSI)
	swprintf(temp, sizeof_w(temp),
		 L"scsi_cdrom_%02i_mode_sense_bin", dev->id);
    else
	swprintf(temp, sizeof_w(temp),
		 L"cdrom_%02i_mode_sense_bin", dev->id);

    fp = plat_fopen(nvr_path(temp), L"wb");
    if (fp != NULL) {
	(void)fwrite(&dev->ms_pages_saved.pages[GPMODE_CDROM_AUDIO_PAGE],
		     1, 0x10, fp);
	(void)fclose(fp);
    }
}


static int
read_capacity(void *priv, uint8_t *cdb, uint8_t *buffer, uint32_t *len)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;
    int size = 0;

    /* IMPORTANT: What's returned is the last LBA block. */
    if (dev->drv->ops && dev->drv->ops->size)
	size = dev->drv->ops->size(dev->drv) - 1;

    memset(buffer, 0, 8);
    buffer[0] = (size >> 24) & 0xff;
    buffer[1] = (size >> 16) & 0xff;
    buffer[2] = (size >> 8) & 0xff;
    buffer[3] = size & 0xff;
    buffer[6] = 8;		/* 2048 = 0x0800 */

    *len = 8;

    return 1;
}


/*SCSI Mode Sense 6/10*/
static uint8_t
mode_sense_read(scsi_cdrom_t *dev, uint8_t page_control, uint8_t page, uint8_t pos)
{
    switch (page_control) {
	case 0:
	case 3:
		return dev->ms_pages_saved.pages[page][pos];
		break;

	case 1:
		return mode_sense_pages_changeable.pages[page][pos];
		break;

	case 2:
		if (dev->drv->bus_type == CDROM_BUS_SCSI)
			return mode_sense_pages_default_scsi.pages[page][pos];
		else
			return mode_sense_pages_default.pages[page][pos];
		break;
    }

    return 0;
}


static uint32_t
mode_sense(scsi_cdrom_t *dev, uint8_t *buf, uint32_t pos, uint8_t page, uint8_t block_descriptor_len)
{
    uint8_t page_control = (page >> 6) & 3;
    int i = 0, j = 0;
    uint8_t msplen;

    page &= 0x3f;

    if (block_descriptor_len) {
	buf[pos++] = 1;		/* Density code. */
	buf[pos++] = 0;		/* Number of blocks (0 = all). */
	buf[pos++] = 0;
	buf[pos++] = 0;
	buf[pos++] = 0;		/* Reserved. */
	buf[pos++] = 0;		/* Block length (0x800 = 2048 bytes). */
	buf[pos++] = 8;
	buf[pos++] = 0;
    }

    for (i = 0; i < 0x40; i++) {
        if ((page == GPMODE_ALL_PAGES) || (page == i)) {
		if (mode_sense_page_flags & (1LL << ((uint64_t) (page & 0x3f)))) {
			buf[pos++] = mode_sense_read(dev, page_control, i, 0);
			msplen = mode_sense_read(dev, page_control, i, 1);
			buf[pos++] = msplen;
			DEBUG("CD-ROM %i: MODE SENSE: Page [%02X] length %i\n", dev->id, i, msplen);
			for (j = 0; j < msplen; j++) {
				if ((i == GPMODE_CAPABILITIES_PAGE) && (j >= 6) && (j <= 7)) {
					if (j & 1)
						buf[pos++] = ((cdrom_speeds[dev->drv->speed_idx].speed * 176) & 0xff);
					else
						buf[pos++] = ((cdrom_speeds[dev->drv->speed_idx].speed * 176) >> 8);
				} else if ((i == GPMODE_CAPABILITIES_PAGE) && (j >= 12) && (j <= 13)) {
					if (j & 1)
						buf[pos++] = ((cdrom_speeds[dev->drv->cur_speed].speed * 176) & 0xff);
					else
						buf[pos++] = ((cdrom_speeds[dev->drv->cur_speed].speed * 176) >> 8);
				} else
					buf[pos++] = mode_sense_read(dev, page_control, i, 2 + j);
			}
		}
	}
    }

    return pos;
}


static void
update_request_length(scsi_cdrom_t *dev, int len, int block_len)
{
    int32_t bt, min_len = 0;

    dev->max_transfer_len = dev->request_length;

    /* For media access commands, make sure the requested DRQ length matches the block length. */
    switch (dev->current_cdb[0]) {
	case 0x08:
	case 0x28:
	case 0xa8:
	case 0xb9:
	case 0xbe:
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
    if (!dev->max_transfer_len)
	dev->max_transfer_len = 65534;

    if ((len <= dev->max_transfer_len) && (len >= min_len))
	dev->request_length = dev->max_transfer_len = len;
    else if (len > dev->max_transfer_len)
	dev->request_length = dev->max_transfer_len;

    return;
}


static double
bus_speed(scsi_cdrom_t *dev)
{
    if (dev->drv->bus_type != CDROM_BUS_SCSI) {
	/* TODO: Get the actual selected speed from IDE. */
	if (current_mode(dev) == 2)
		return 66666666.666666666666666;    /* 66 MB/s MDMA-2 speed */

	return 8333333.333333333333333;		    /* 8.3 MB/s PIO-2 speed */
    }

    dev->callback = -1LL;	/* Speed depends on SCSI controller */

    return 0.0;
}


static void
command_bus(scsi_cdrom_t *dev)
{
    dev->status = BUSY_STAT;
    dev->phase = 1;
    dev->pos = 0;
    dev->callback = 1LL * CDROM_TIME;

    set_callback(dev);
}


static void
command_common(scsi_cdrom_t *dev)
{
    double bytes_per_second, period;
    double dusec;

    dev->status = BUSY_STAT;
    dev->phase = 1;
    dev->pos = 0;
    dev->callback = 0LL;

    DEBUG("CD-ROM %i: Current speed: %ix\n",
	  dev->id, cdrom_speeds[dev->drv->cur_speed].speed);

    if (dev->packet_status == PHASE_COMPLETE) {
	do_callback(dev);
	dev->callback = 0LL;
    } else {
	switch(dev->current_cdb[0]) {
		case GPCMD_REZERO_UNIT:
		case 0x0b:
		case 0x2b:
			/* Seek time is in us. */
			period = cdrom_seek_time(dev->drv);
			DEBUG("CD-ROM %i: Seek period: %" PRIu64 " us\n",
				  dev->id, (tmrval_t)period);
			period = period * ((double) TIMER_USEC);
			dev->callback += ((tmrval_t)period);
			set_callback(dev);
			return;

		case 0x08:
		case 0x28:
		case 0xa8:
			/* Seek time is in us. */
			period = cdrom_seek_time(dev->drv);
			DEBUG("CD-ROM %i: Seek period: %" PRIu64 " us\n",
				  dev->id, (tmrval_t)period);
			period = period * ((double) TIMER_USEC);
			dev->callback += ((tmrval_t)period);
			/*FALLTHROUGH*/

		case 0x25:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x51:
		case 0x52:
		case 0xad:
		case 0xb8:
		case 0xb9:
		case 0xbe:
			if (dev->current_cdb[0] == 0x42)
				dev->callback += (tmrval_t)200 * CDROM_TIME;

			/* Account for seek time. */
			bytes_per_second = 176.0 * 1024.0;
			bytes_per_second *= (double)cdrom_speeds[dev->drv->cur_speed].speed;
			break;

		default:
			bytes_per_second = bus_speed(dev);
			if (bytes_per_second == 0.0) {
				/* Speed depends on SCSI controller. */
				dev->callback = -1LL;
				return;
			}
			break;
	}

	period = 1000000.0 / bytes_per_second;
	DEBUG("CD-ROM %i: Byte transfer period: %" PRIu64 " us\n", dev->id, (tmrval_t) period);
	period = period * (double) (dev->packet_len);
	DEBUG("CD-ROM %i: Sector transfer period: %" PRIu64 " us\n", dev->id, (tmrval_t) period);
	dusec = period * ((double) TIMER_USEC);
	dev->callback += ((tmrval_t) dusec);
    }

    set_callback(dev);
}


static void
command_complete(scsi_cdrom_t *dev)
{
    dev->packet_status = PHASE_COMPLETE;

    command_common(dev);
}


static void
command_read(scsi_cdrom_t *dev)
{
    dev->packet_status = PHASE_DATA_IN;

    command_common(dev);

    dev->total_read = 0;
}


static void
command_read_dma(scsi_cdrom_t *dev)
{
    dev->packet_status = PHASE_DATA_IN_DMA;

    command_common(dev);

    dev->total_read = 0;
}


static void
command_write(scsi_cdrom_t *dev)
{
    dev->packet_status = PHASE_DATA_OUT;

    command_common(dev);
}


static void
command_write_dma(scsi_cdrom_t *dev)
{
    dev->packet_status = PHASE_DATA_OUT_DMA;

    command_common(dev);
}


/* id = Current CD-ROM device ID;
   len = Total transfer length;
   block_len = Length of a single block (it matters because media access commands on ATAPI);
   alloc_len = Allocated transfer length;
   direction = Transfer direction (0 = read from host, 1 = write to host).
*/
static void
data_command_finish(scsi_cdrom_t *dev, int len, int block_len, int alloc_len, int direction)
{
    DEBUG("CD-ROM %i: Finishing command (%02X): %i, %i, %i, %i, %i\n",
	  dev->id, dev->current_cdb[0], len, block_len, alloc_len, direction, dev->request_length);
    dev->pos = 0;

    if (alloc_len >= 0) {
	if (alloc_len < len)
		len = alloc_len;
    }

    if ((len == 0) || (current_mode(dev) == 0)) {
	if (dev->drv->bus_type != CDROM_BUS_SCSI)
		dev->packet_len = 0;

	command_complete(dev);
    } else {
	if (current_mode(dev) == 2) {
		if (dev->drv->bus_type != CDROM_BUS_SCSI)
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

    DEBUG("CD-ROM %i: Status: %i, cylinder %i, packet length: %i, position: %i, phase: %i\n",
	  dev->id, dev->packet_status, dev->request_length, dev->packet_len, dev->pos, dev->phase);
}


static void
sense_clear(scsi_cdrom_t *dev, int command)
{
    dev->previous_command = command;

    scsi_cdrom_sense_key = scsi_cdrom_asc = scsi_cdrom_ascq = 0;
}


static void
set_phase(scsi_cdrom_t *dev, uint8_t phase)
{
    if (dev->drv->bus_type != CDROM_BUS_SCSI)
	return;

    scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].phase = phase;
}


static void
cmd_error(scsi_cdrom_t *dev)
{
    set_phase(dev, SCSI_PHASE_STATUS);

    dev->error = ((scsi_cdrom_sense_key & 0xf) << 4) | ABRT_ERR;
    if (dev->unit_attention)
	dev->error |= MCR_ERR;
    dev->status = READY_STAT | ERR_STAT;
    dev->phase = 3;
    dev->pos = 0;
    dev->packet_status = 0x80;
    dev->callback = 50LL * CDROM_TIME;

    set_callback(dev);

    DEBUG("CD-ROM %i: ERROR: %02X/%02X/%02X\n",
	  dev->id, scsi_cdrom_sense_key, scsi_cdrom_asc, scsi_cdrom_ascq);
}


static void
unit_attention(scsi_cdrom_t *dev)
{
    set_phase(dev, SCSI_PHASE_STATUS);

    dev->error = (SENSE_UNIT_ATTENTION << 4) | ABRT_ERR;
    if (dev->unit_attention)
	dev->error |= MCR_ERR;
    dev->status = READY_STAT | ERR_STAT;
    dev->phase = 3;
    dev->pos = 0;
    dev->packet_status = 0x80;
    dev->callback = 50LL * CDROM_TIME;

    set_callback(dev);

    DEBUG("CD-ROM %i: UNIT ATTENTION\n", dev->id);
}


static void
bus_master_error(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = scsi_cdrom_asc = scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
not_ready(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_NOT_READY;
    scsi_cdrom_asc = ASC_MEDIUM_NOT_PRESENT;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
invalid_lun(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_INV_LUN;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
illegal_opcode(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_ILLEGAL_OPCODE;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
lba_out_of_range(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_LBA_OUT_OF_RANGE;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
invalid_field(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_INV_FIELD_IN_CMD_PACKET;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
invalid_field_pl(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_INV_FIELD_IN_PARAMETER_LIST;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
illegal_mode(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_ILLEGAL_MODE_FOR_THIS_TRACK;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


static void
incompatible_format(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_INCOMPATIBLE_FORMAT;

    scsi_cdrom_ascq = 2;

    cmd_error(dev);
}


static void
data_phase_error(scsi_cdrom_t *dev)
{
    scsi_cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    scsi_cdrom_asc = ASC_DATA_PHASE_ERROR;
    scsi_cdrom_ascq = 0;

    cmd_error(dev);
}


void
scsi_cdrom_update_cdb(uint8_t *cdb, int lba_pos, int number_of_blocks)
{
    int temp;

    switch(cdb[0]) {
	case GPCMD_READ_6:
		cdb[1] = (lba_pos >> 16) & 0xff;
		cdb[2] = (lba_pos >> 8) & 0xff;
		cdb[3] = lba_pos & 0xff;
		break;

	case GPCMD_READ_10:
		cdb[2] = (lba_pos >> 24) & 0xff;
		cdb[3] = (lba_pos >> 16) & 0xff;
		cdb[4] = (lba_pos >> 8) & 0xff;
		cdb[5] = lba_pos & 0xff;
		cdb[7] = (number_of_blocks >> 8) & 0xff;
		cdb[8] = number_of_blocks & 0xff;
		break;

	case GPCMD_READ_12:
		cdb[2] = (lba_pos >> 24) & 0xff;
		cdb[3] = (lba_pos >> 16) & 0xff;
		cdb[4] = (lba_pos >> 8) & 0xff;
		cdb[5] = lba_pos & 0xff;
		cdb[6] = (number_of_blocks >> 24) & 0xff;
		cdb[7] = (number_of_blocks >> 16) & 0xff;
		cdb[8] = (number_of_blocks >> 8) & 0xff;
		cdb[9] = number_of_blocks & 0xff;
		break;

	case GPCMD_READ_CD_MSF:
		temp = cdrom_lba_to_msf_accurate(lba_pos);
		cdb[3] = (temp >> 16) & 0xff;
		cdb[4] = (temp >> 8) & 0xff;
		cdb[5] = temp & 0xff;

		temp = cdrom_lba_to_msf_accurate(lba_pos + number_of_blocks - 1);
		cdb[6] = (temp >> 16) & 0xff;
		cdb[7] = (temp >> 8) & 0xff;
		cdb[8] = temp & 0xff;
		break;			

	case GPCMD_READ_CD:
		cdb[2] = (lba_pos >> 24) & 0xff;
		cdb[3] = (lba_pos >> 16) & 0xff;
		cdb[4] = (lba_pos >> 8) & 0xff;
		cdb[5] = lba_pos & 0xff;
		cdb[6] = (number_of_blocks >> 16) & 0xff;
		cdb[7] = (number_of_blocks >> 8) & 0xff;
		cdb[8] = number_of_blocks & 0xff;
		break;
    }
}


static int
read_data(scsi_cdrom_t *dev, int msf, int type, int flags, int32_t *len)
{
    uint32_t cdsize = 0;
    int temp_len = 0;
    int i, ret = 0;

    if (dev->drv->ops && dev->drv->ops->size)
	cdsize = dev->drv->ops->size(dev->drv);
    else {
	not_ready(dev);
	return -1;
    }

    /* FIXME:
     * Temporarily disabled this because the Triones ATAPI DMA driver
     * seems to always request a 4-sector read but sets the DMA bus
     * master to transfer less data than that.
     */
#if 0
    if (dev->sector_pos >= cdsize) {
	DEBUG("CD-ROM %i: Trying to read from beyond the end of disc (%i >= %i)\n", dev->id,
		  dev->sector_pos, cdsize);
	lba_out_of_range(dev);
	return -1;
    }
#endif

    if ((dev->sector_pos + dev->sector_len - 1) >= cdsize) {
	DEBUG("CD-ROM %i: Trying to read to beyond the end of disc (%i >= %i)\n", dev->id,
		  (dev->sector_pos + dev->sector_len - 1), cdsize);
	lba_out_of_range(dev);
	return 0;
    }

    dev->old_len = 0;
    *len = 0;

    for (i = 0; i < dev->requested_blocks; i++) {
	if (dev->drv->ops && dev->drv->ops->readsector_raw)
		ret = dev->drv->ops->readsector_raw(dev->drv,
						    dev->buffer + dev->data_pos,
					   	    dev->sector_pos + i, msf,
					   	    type, flags, &temp_len);
	else {
		not_ready(dev);
		return 0;
	}

	dev->data_pos += temp_len;
	dev->old_len += temp_len;

	*len += temp_len;

	if (!ret) {
		illegal_mode(dev);
		return 0;
	}
    }

    return 1;
}


static int
read_blocks(scsi_cdrom_t *dev, int32_t *len, int first_batch)
{
    int ret = 0, msf = 0;
    int type = 0, flags = 0;

    if (dev->current_cdb[0] == GPCMD_READ_CD_MSF)
	msf = 1;

    if ((dev->current_cdb[0] == GPCMD_READ_CD_MSF) || (dev->current_cdb[0] == GPCMD_READ_CD)) {
	type = (dev->current_cdb[1] >> 2) & 7;
	flags = dev->current_cdb[9] | (((uint32_t) dev->current_cdb[10]) << 8);
    } else {
	type = 8;
	flags = 0x10;
    }

    dev->data_pos = 0;

    if (!dev->sector_len) {
	command_complete(dev);
	return -1;
    }

    DEBUG("Reading %i blocks starting from %i...\n",
	  dev->requested_blocks, dev->sector_pos);

    scsi_cdrom_update_cdb(dev->current_cdb, dev->sector_pos, dev->requested_blocks);

    ret = read_data(dev, msf, type, flags, len);

    DEBUG("Read %i bytes of blocks...\n", *len);

    if (ret == -1)
	return 0;

    if (!ret || ((dev->old_len != *len) && !first_batch)) {
	if ((dev->old_len != *len) && !first_batch)
		illegal_mode(dev);

	return 0;
    }

    dev->sector_pos += dev->requested_blocks;
    dev->sector_len -= dev->requested_blocks;

    return 1;
}


/*SCSI Read DVD Structure*/
static int
read_dvd_structure(scsi_cdrom_t *dev, int format, const uint8_t *packet, uint8_t *buf)
{
    int layer = packet[6];
    uint64_t sectors = 0;
    int ret = 0;

    switch (format) {
       	case 0x00:	/* Physical format information */
		if (dev->drv->ops && dev->drv->ops->size)
			sectors = (uint64_t) dev->drv->ops->size(dev->drv);

	        if (layer != 0) {
			invalid_field(dev);
			return 0;
		}

               	sectors >>= 2;
		if (sectors == 0) {
			/* return -ASC_MEDIUM_NOT_PRESENT; */
			not_ready(dev);
			return 0;
		}

		buf[4] = 1;	/* DVD-ROM, part version 1 */
		buf[5] = 0xf;	/* 120mm disc, minimum rate unspecified */
		buf[6] = 1;	/* one layer, read-only (per MMC-2 spec) */
		buf[7] = 0;	/* default densities */

		/* FIXME: 0x30000 per spec? */
		buf[8] = buf[9] = buf[10] = buf[11] = 0; /* start sector */
		buf[12] = (sectors >> 24) & 0xff; /* end sector */
		buf[13] = (sectors >> 16) & 0xff;
		buf[14] = (sectors >> 8) & 0xff;
		buf[15] = sectors & 0xff;

		buf[16] = (sectors >> 24) & 0xff; /* l0 end sector */
		buf[17] = (sectors >> 16) & 0xff;
		buf[18] = (sectors >> 8) & 0xff;
		buf[19] = sectors & 0xff;

		/* Size of buffer, not including 2 byte size field */				
		buf[0] = ((2048 +2 ) >> 8) & 0xff;
		buf[1] = (2048 + 2) & 0xff;

		/* 2k data + 4 byte header */
		ret = (2048 + 4);
		break;

	case 0x01:	/* DVD copyright information */
		buf[4] = 0; /* no copyright data */
		buf[5] = 0; /* no region restrictions */

		/* Size of buffer, not including 2 byte size field */
		buf[0] = ((4 + 2) >> 8) & 0xff;
		buf[1] = (4 + 2) & 0xff;			

		/* 4 byte header + 4 byte data */
		ret = (4 + 4);
		break;

	case 0x03:	/* BCA information - invalid field for no BCA info */
		invalid_field(dev);
		break;

	case 0x04:	/* DVD disc manufacturing information */
			/* Size of buffer, not including 2 byte size field */
		buf[0] = ((2048 + 2) >> 8) & 0xff;
		buf[1] = (2048 + 2) & 0xff;

		/* 2k data + 4 byte header */
		ret = (2048 + 4);
		break;

	case 0xff:
		/*
		 * This lists all the command capabilities above.  Add new ones
		 * in order and update the length and buffer return values.
		 */

		buf[4] = 0x00; /* Physical format */
		buf[5] = 0x40; /* Not writable, is readable */
		buf[6] = ((2048 + 4) >> 8) & 0xff;
		buf[7] = (2048 + 4) & 0xff;

		buf[8] = 0x01; /* Copyright info */
		buf[9] = 0x40; /* Not writable, is readable */
		buf[10] = ((4 + 4) >> 8) & 0xff;
		buf[11] = (4 + 4) & 0xff;

		buf[12] = 0x03; /* BCA info */
		buf[13] = 0x40; /* Not writable, is readable */
		buf[14] = ((188 + 4) >> 8) & 0xff;
		buf[15] = (188 + 4) & 0xff;

		buf[16] = 0x04; /* Manufacturing info */
		buf[17] = 0x40; /* Not writable, is readable */
		buf[18] = ((2048 + 4) >> 8) & 0xff;
		buf[19] = (2048 + 4) & 0xff;

		/* Size of buffer, not including 2 byte size field */
		buf[6] = ((16 + 2) >> 8) & 0xff;
		buf[7] = (16 + 2) & 0xff;

		/* data written + 4 byte header */
		ret = (16 + 4);
		break;

	default: /* TODO: formats beyond DVD-ROM requires */
		invalid_field(dev);
		break;
    }

    return ret;
}


static void
do_insert(void *p)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)p;

    if (dev == NULL)
	return;

    dev->unit_attention = 1;
    DEBUG("CD-ROM %i: Media insert\n", dev->id);
}


static int
pre_execution_check(scsi_cdrom_t *dev, uint8_t *cdb)
{
    int ready = 0, status;

    if (dev->drv->bus_type == CDROM_BUS_SCSI) {
	if ((cdb[0] != GPCMD_REQUEST_SENSE) && (cdb[1] & 0xe0)) {
		DEBUG("CD-ROM %i: Attempting to execute a unknown command targeted at SCSI LUN %i\n",
			  dev->id, ((dev->request_length >> 5) & 7));
		invalid_lun(dev);
		return 0;
	}
    }

    if (!(command_flags[cdb[0]] & IMPLEMENTED)) {
	DEBUG("CD-ROM %i: Attempting to execute unknown command %02X over %s\n", dev->id, cdb[0],
		  (dev->drv->bus_type == CDROM_BUS_SCSI) ? "SCSI" : "ATAPI");

	illegal_opcode(dev);
	return 0;
    }

    if ((dev->drv->bus_type < CDROM_BUS_SCSI) && (command_flags[cdb[0]] & SCSI_ONLY)) {
	DEBUG("CD-ROM %i: Attempting to execute SCSI-only command %02X over ATAPI\n", dev->id, cdb[0]);
	illegal_opcode(dev);
	return 0;
    }

    if ((dev->drv->bus_type == CDROM_BUS_SCSI) && (command_flags[cdb[0]] & ATAPI_ONLY)) {
	DEBUG("CD-ROM %i: Attempting to execute ATAPI-only command %02X over SCSI\n", dev->id, cdb[0]);
	illegal_opcode(dev);
	return 0;
    }

    if (dev->drv->ops && dev->drv->ops->status)
	status = dev->drv->ops->status(dev->drv);
    else
	status = CD_STATUS_EMPTY;

    if ((status == CD_STATUS_PLAYING) || (status == CD_STATUS_PAUSED)) {
	ready = 1;
	goto skip_ready_check;
    }

    if (dev->drv->ops && dev->drv->ops->ready)
	ready = dev->drv->ops->ready(dev->drv);

skip_ready_check:
    /* If the drive is not ready, there is no reason to keep the
       UNIT ATTENTION condition present, as we only use it to mark
       disc changes. */
    if (!ready && dev->unit_attention)
	dev->unit_attention = 0;

    /* If the UNIT ATTENTION condition is set and the command does not allow
       execution under it, error out and report the condition. */
    if (dev->unit_attention == 1) {
	/* Only increment the unit attention phase if the command can not pass through it. */
	if (!(command_flags[cdb[0]] & ALLOW_UA)) {
		DBGLOG(1, "CD-ROM %i: Unit attention now 2\n", dev->id);
		dev->unit_attention++;
		DEBUG("CD-ROM %i: UNIT ATTENTION: Command %02X not allowed to pass through\n",
			  dev->id, cdb[0]);
		unit_attention(dev);
		return 0;
	}
    } else if (dev->unit_attention == 2) {
	if (cdb[0] != GPCMD_REQUEST_SENSE) {
		DBGLOG(1, "CD-ROM %i: Unit attention now 0\n", dev->id);
		dev->unit_attention = 0;
	}
    }

    /* Unless the command is REQUEST SENSE, clear the sense. This will *NOT*
       the UNIT ATTENTION condition if it's set. */
    if (cdb[0] != GPCMD_REQUEST_SENSE)
	sense_clear(dev, cdb[0]);

    /* Next it's time for NOT READY. */
    if (!ready)
	dev->media_status = MEC_MEDIA_REMOVAL;
    else
	dev->media_status = (dev->unit_attention) ? MEC_NEW_MEDIA : MEC_NO_CHANGE;

    if ((command_flags[cdb[0]] & CHECK_READY) && !ready) {
	DEBUG("CD-ROM %i: Not ready (%02X)\n", dev->id, cdb[0]);
	not_ready(dev);
	return 0;
    }

    DEBUG("CD-ROM %i: Continuing with command %02X\n", dev->id, cdb[0]);

    return 1;
}


static void
do_rezero(scsi_cdrom_t *dev)
{
    dev->sector_pos = dev->sector_len = 0;

    cdrom_seek(dev->drv, 0);
}


static void
do_reset(void *p)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *) p;

    if (dev == NULL)
	return;

    do_rezero(dev);

    dev->status = 0;
    dev->callback = 0LL;

    set_callback(dev);

    set_signature(dev);

    dev->packet_status = 0xff;
    dev->unit_attention = 0xff;
}


static void
request_sense(scsi_cdrom_t *dev, uint8_t *buffer, uint8_t alloc_length)
{
    int status = dev->drv->cd_status;

    /*Will return 18 bytes of 0*/
    if (alloc_length != 0) {
	memset(buffer, 0, alloc_length);
	memcpy(buffer, dev->sense, alloc_length);
    }

    buffer[0] = 0x70;

    if ((scsi_cdrom_sense_key > 0) && ((status < CD_STATUS_PLAYING) ||
	(status == CD_STATUS_STOPPED)) && cdrom_playing_completed(dev->drv)) {
	buffer[2] = SENSE_ILLEGAL_REQUEST;
	buffer[12] = ASC_AUDIO_PLAY_OPERATION;
	buffer[13] = ASCQ_AUDIO_PLAY_OPERATION_COMPLETED;
    } else if ((scsi_cdrom_sense_key == 0) && (status >= CD_STATUS_PLAYING) &&
	       (status != CD_STATUS_STOPPED)) {
	buffer[2] = SENSE_ILLEGAL_REQUEST;
	buffer[12] = ASC_AUDIO_PLAY_OPERATION;
	buffer[13] = (status == CD_STATUS_PLAYING) ? ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS : ASCQ_AUDIO_PLAY_OPERATION_PAUSED;
    } else {
	if (dev->unit_attention && (scsi_cdrom_sense_key == 0)) {
		buffer[2] = SENSE_UNIT_ATTENTION;
		buffer[12] = ASC_MEDIUM_MAY_HAVE_CHANGED;
		buffer[13] = 0;
	}
    }

    DEBUG("CD-ROM %i: Reporting sense: %02X %02X %02X\n",
	  dev->id, buffer[2], buffer[12], buffer[13]);

    if (buffer[2] == SENSE_UNIT_ATTENTION) {
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
    scsi_cdrom_t *dev = (scsi_cdrom_t *)p;
    int ready = 0;

    if (dev->drv->ops && dev->drv->ops->ready)
	ready = dev->drv->ops->ready(dev->drv);

    if (!ready && dev->unit_attention) {
	/* If the drive is not ready, there is no reason to keep the
	   UNIT ATTENTION condition present, as we only use it to mark
	   disc changes. */
	dev->unit_attention = 0;
    }

    /* Do *NOT* advance the unit attention phase. */
    request_sense(dev, buffer, alloc_length);
}


static void
set_buf_len(scsi_cdrom_t *dev, int32_t *BufLen, int32_t *src_len)
{
    if (dev->drv->bus_type == CDROM_BUS_SCSI) {
	if (*BufLen == -1)
		*BufLen = *src_len;
	else {
		*BufLen = MIN(*src_len, *BufLen);
		*src_len = *BufLen;
	}
	DEBUG("CD-ROM %i: Actual transfer length: %i\n", dev->id, *BufLen);
    }
}


static void
buf_alloc(scsi_cdrom_t *dev, uint32_t len)
{
    DEBUG("CD-ROM %i: Allocated buffer length: %i\n", dev->id, len);

    dev->buffer = (uint8_t *)mem_alloc(len);
}


static void
buf_free(scsi_cdrom_t *dev)
{
    if (dev->buffer) {
	DEBUG("CD-ROM %i: Freeing buffer...\n", dev->id);
	free(dev->buffer);
	dev->buffer = NULL;
    }
}


static void
do_command(void *p, uint8_t *cdb)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *) p;
    int len, max_len, used_len, alloc_length, msf;
    int pos = 0, i= 0, size_idx, idx = 0;
    uint32_t feature;
    unsigned preamble_len;
    int toc_format, block_desc = 0;
    int ret, format = 0;
    int real_pos, track = 0;
    char device_identify[9] = { 'E', 'M', 'U', '_', 'C', 'D', '0', '0', 0 };
    char device_identify_ex[15] = { 'E', 'M', 'U', '_', 'C', 'D', '0', '0', ' ', 'v', '0'+EMU_VER_MAJOR, '.', '0'+EMU_VER_MINOR, '0'+EMU_VER_REV, 0 };
    int32_t blen = 0, *BufLen;
    uint8_t *b;
    uint32_t profiles[2] = { MMC_PROFILE_CD_ROM, MMC_PROFILE_DVD_ROM };
    gesn_cdb_t *gcdb;
    gesn_event_header_t *geh;

    if (dev->drv->bus_type == CDROM_BUS_SCSI) {
	BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;
	dev->status &= ~ERR_STAT;
    } else {
	BufLen = &blen;
	dev->error = 0;
    }

    dev->packet_len = 0;
    dev->request_pos = 0;

    device_identify[7] = dev->id + 0x30;

    device_identify_ex[7] = dev->id + 0x30;
    device_identify_ex[10] = EMU_VERSION[0];
    device_identify_ex[12] = EMU_VERSION[2];
    device_identify_ex[13] = EMU_VERSION[3];

    dev->data_pos = 0;

    memcpy(dev->current_cdb, cdb, 12);

    if (dev->drv->ops && dev->drv->ops->status)
	dev->drv->cd_status = dev->drv->ops->status(dev->drv);
    else
	dev->drv->cd_status = CD_STATUS_EMPTY;

    if (cdb[0] != 0) {
	DEBUG("CD-ROM %i: Command 0x%02X, Sense Key %02X, Asc %02X, Ascq %02X, Unit attention: %i\n",
		  dev->id, cdb[0], scsi_cdrom_sense_key, scsi_cdrom_asc, scsi_cdrom_ascq, dev->unit_attention);
	DEBUG("CD-ROM %i: Request length: %04X\n", dev->id, dev->request_length);

	DEBUG("CD-ROM %i: CDB: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", dev->id,
		  cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
		  cdb[8], cdb[9], cdb[10], cdb[11]);
    }

    msf = cdb[1] & 2;
    dev->sector_len = 0;

    set_phase(dev, SCSI_PHASE_STATUS);

    /* This handles the Not Ready/Unit Attention check if it has to be handled at this point. */
    if (pre_execution_check(dev, cdb) == 0)
	return;

    switch (cdb[0]) {
	case GPCMD_TEST_UNIT_READY:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_REZERO_UNIT:
		if (dev->drv->ops->stop)
			dev->drv->ops->stop(dev->drv);
		dev->sector_pos = dev->sector_len = 0;
		dev->drv->seek_diff = dev->drv->seek_pos;
		cdrom_seek(dev->drv, 0);
		set_phase(dev, SCSI_PHASE_STATUS);
		break;

	case GPCMD_REQUEST_SENSE:
		/*
		 * If there is a UNIT ATTENTION condition and there is
		 * a buffered NOT READY, a standalone REQUEST SENSE
	 	 * should forget about the NOT READY, and report the
		 * UNIT ATTENTION straight away.
		 */
		set_phase(dev, SCSI_PHASE_DATA_IN);
		max_len = cdb[4];

		if (!max_len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20LL * CDROM_TIME;
			set_callback(dev);
			break;
		}

		buf_alloc(dev, 256);
		set_buf_len(dev, BufLen, &max_len);
		request_sense(dev, dev->buffer, max_len);
		data_command_finish(dev, 18, 18, cdb[4], 0);
		break;

	case GPCMD_SET_SPEED:
	case GPCMD_SET_SPEED_ALT:
		len = (cdb[3] | (cdb[2] << 8)) / 176;
		dev->drv->cur_speed = cdrom_speed_idx(len);
		if (dev->drv->cur_speed > dev->drv->speed_idx)
			dev->drv->cur_speed = dev->drv->speed_idx;
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_MECHANISM_STATUS:
		set_phase(dev, SCSI_PHASE_DATA_IN);
		len = (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];

		buf_alloc(dev, 8);
		set_buf_len(dev, BufLen, &len);

		memset(dev->buffer, 0, 8);
		dev->buffer[5] = 1;

		data_command_finish(dev, 8, 8, len, 0);
		break;

	case GPCMD_READ_TOC_PMA_ATIP:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		max_len = cdb[7];
		max_len <<= 8;
		max_len |= cdb[8];

		buf_alloc(dev, 65536);

		toc_format = cdb[2] & 0xf;

		if (toc_format == 0)
			toc_format = (cdb[9] >> 6) & 3;

		if (! dev->drv->ops) {
			not_ready(dev);
			return;
		}

		switch (toc_format) {
			case 0: /*Normal*/
				if (! dev->drv->ops->readtoc) {
					not_ready(dev);
					return;
				}
				len = dev->drv->ops->readtoc(dev->drv, dev->buffer, cdb[6], msf, max_len, 0);
				break;

			case 1: /*Multi session*/
				if (! dev->drv->ops->readtoc_session) {
					not_ready(dev);
					return;
				}
				len = dev->drv->ops->readtoc_session(dev->drv, dev->buffer, msf, max_len);
				dev->buffer[0] = 0; dev->buffer[1] = 0xA;
				break;

			case 2: /*Raw*/
				if (! dev->drv->ops->readtoc_raw) {
					not_ready(dev);
					return;
				}
				len = dev->drv->ops->readtoc_raw(dev->drv, dev->buffer, max_len);
				break;

			default:
				invalid_field(dev);
				buf_free(dev);
				return;
		}

		if (len > max_len) {
			len = max_len;

			dev->buffer[0] = ((len - 2) >> 8) & 0xff;
			dev->buffer[1] = (len - 2) & 0xff;
		}

		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, len, 0);
		return;

	case GPCMD_READ_CD_OLD:
		/* IMPORTANT: Convert the command to new read CD
			      for pass through purposes. */
		dev->current_cdb[0] = 0xbe;
		/*FALLTHROUGH*/

	case GPCMD_READ_6:
	case GPCMD_READ_10:
	case GPCMD_READ_12:
	case GPCMD_READ_CD:
	case GPCMD_READ_CD_MSF:
		set_phase(dev, SCSI_PHASE_DATA_IN);
		alloc_length = 2048;

		switch(cdb[0]) {
			case GPCMD_READ_6:
				dev->sector_len = cdb[4];
				dev->sector_pos = ((((uint32_t) cdb[1]) & 0x1f) << 16) | (((uint32_t) cdb[2]) << 8) | ((uint32_t) cdb[3]);
				msf = 0;
				break;

			case GPCMD_READ_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				DEBUG("CD-ROM %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len,
					  dev->sector_pos);
				msf = 0;
				break;

			case GPCMD_READ_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				DEBUG("CD-ROM %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len,
					  dev->sector_pos);
				msf = 0;
				break;

			case GPCMD_READ_CD_MSF:
				alloc_length = 2856;
				dev->sector_len = MSFtoLBA(cdb[6], cdb[7], cdb[8]);
				dev->sector_pos = MSFtoLBA(cdb[3], cdb[4], cdb[5]);

				dev->sector_len -= dev->sector_pos;
				dev->sector_len++;
				msf = 1;
				break;

			case GPCMD_READ_CD_OLD:
			case GPCMD_READ_CD:
				alloc_length = 2856;
				dev->sector_len = (cdb[6] << 16) | (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];

				msf = 0;
				break;
		}

		dev->drv->seek_diff = ABS((int) (pos - dev->drv->seek_pos));
		dev->drv->seek_pos = dev->sector_pos;

		if (!dev->sector_len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DBGLOG(1, "CD-ROM %i: All done - callback set\n", dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20LL * CDROM_TIME;
			set_callback(dev);
			break;
		}

		max_len = dev->sector_len;
		dev->requested_blocks = max_len;	/* If we're reading all blocks in one go for DMA, why not also for PIO, it should NOT
							   matter anyway, this step should be identical and only the way the read dat is
							   transferred to the host should be different. */

		dev->packet_len = max_len * alloc_length;
		buf_alloc(dev, dev->packet_len);

		ret = read_blocks(dev, &alloc_length, 1);
		if (ret <= 0) {
			buf_free(dev);
			return;
		}

		dev->requested_blocks = max_len;
		dev->packet_len = alloc_length;

		set_buf_len(dev, BufLen, (int32_t *) &dev->packet_len);

		data_command_finish(dev, alloc_length,
				    alloc_length / dev->requested_blocks,
				    alloc_length, 0);

		if (dev->packet_status != PHASE_COMPLETE)
			ui_sb_icon_update(SB_CDROM | dev->id, 1);
		else
			ui_sb_icon_update(SB_CDROM | dev->id, 0);
		return;

	case GPCMD_READ_HEADER:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		alloc_length = ((cdb[7] << 8) | cdb[8]);
		buf_alloc(dev, 8);

		dev->sector_len = 1;
		dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4]<<8) | cdb[5];
		if (msf)
			real_pos = cdrom_lba_to_msf_accurate(dev->sector_pos);
		else
			real_pos = dev->sector_pos;
		dev->buffer[0] = 1; /*2048 bytes user data*/
		dev->buffer[1] = dev->buffer[2] = dev->buffer[3] = 0;
		dev->buffer[4] = (real_pos >> 24);
		dev->buffer[5] = ((real_pos >> 16) & 0xff);
		dev->buffer[6] = ((real_pos >> 8) & 0xff);
		dev->buffer[7] = real_pos & 0xff;

		len = 8;
		len = MIN(len, alloc_length);

		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, len, 0);
		return;

	case GPCMD_MODE_SENSE_6:
	case GPCMD_MODE_SENSE_10:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		if (dev->drv->bus_type == CDROM_BUS_SCSI)
			block_desc = ((cdb[1] >> 3) & 1) ? 0 : 1;
		else
			block_desc = 0;

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
			if (dev->drv->ops && dev->drv->ops->media_type_id)
				dev->buffer[1] = dev->drv->ops->media_type_id(dev->drv);
			else
				dev->buffer[1] = 0x70;
			if (block_desc)
				dev->buffer[3] = 8;
		} else {
			len = mode_sense(dev, dev->buffer, 8, cdb[2], block_desc);
			len = MIN(len, alloc_length);
			dev->buffer[0]=(len - 2) >> 8;
			dev->buffer[1]=(len - 2) & 255;
			if (dev->drv->ops && dev->drv->ops->media_type_id)
				dev->buffer[2] = dev->drv->ops->media_type_id(dev->drv);
			else
				dev->buffer[2] = 0x70;
			if (block_desc) {
				dev->buffer[6] = 0;
				dev->buffer[7] = 8;
			}
		}

		set_buf_len(dev, BufLen, &len);

		DEBUG("CD-ROM %i: Reading mode page: %02X...\n", dev->id, cdb[2]);

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

	case GPCMD_GET_CONFIGURATION:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		/* XXX: could result in alignment problems in some architectures */
		feature = (cdb[2] << 8) | cdb[3];
		max_len = (cdb[7] << 8) | cdb[8];

		/* only feature 0 is supported */
		if ((cdb[2] != 0) || (cdb[3] > 2)) {
			invalid_field(dev);
			buf_free(dev);
			return;
		}

		buf_alloc(dev, 65536);
		memset(dev->buffer, 0, max_len);

		alloc_length = 0;
		b = dev->buffer;

		/*
		 * the number of sectors from the media tells us which profile
		 * to use as current.  0 means there is no media
		 */
		if (dev->drv->ops && dev->drv->ops->ready &&
		    dev->drv->ops->ready(dev->drv)) {
			len = dev->drv->ops->size(dev->drv);

			if (len > CD_MAX_SECTORS) {
				b[6] = (MMC_PROFILE_DVD_ROM >> 8) & 0xff;
				b[7] = MMC_PROFILE_DVD_ROM & 0xff;
				ret = 1;
			} else {
				b[6] = (MMC_PROFILE_CD_ROM >> 8) & 0xff;
				b[7] = MMC_PROFILE_CD_ROM & 0xff;
				ret = 0;
			}
		} else
			ret = 2;

		alloc_length = 8;
		b += 8;

		if ((feature == 0) || ((cdb[1] & 3) < 2)) {
			b[2] = (0 << 2) | 0x02 | 0x01; /* persistent and current */
			b[3] = 8;

			alloc_length += 4;
			b += 4;

			for (i = 0; i < 2; i++) {
				b[0] = (profiles[i] >> 8) & 0xff;
				b[1] = profiles[i] & 0xff;

				if (ret == i)
					b[2] |= 1;

				alloc_length += 4;
				b += 4;
			}
		}
		if ((feature == 1) || ((cdb[1] & 3) < 2)) {
			b[1] = 1;
			b[2] = (2 << 2) | 0x02 | 0x01; /* persistent and current */
			b[3] = 8;

			if (dev->drv->bus_type == CDROM_BUS_SCSI)
				b[7] = 1;
			else
				b[7] = 2;
			b[8] = 1;

			alloc_length += 12;
			b += 12;
		}
		if ((feature == 2) || ((cdb[1] & 3) < 2)) {
			b[1] = 2;
			b[2] = (1 << 2) | 0x02 | 0x01; /* persistent and current */
			b[3] = 4;

			b[4] = 2;

			alloc_length += 8;
			b += 8;
		}

		dev->buffer[0] = ((alloc_length - 4) >> 24) & 0xff;
		dev->buffer[1] = ((alloc_length - 4) >> 16) & 0xff;
		dev->buffer[2] = ((alloc_length - 4) >> 8) & 0xff;
		dev->buffer[3] = (alloc_length - 4) & 0xff;

		alloc_length = MIN(alloc_length, max_len);

		set_buf_len(dev, BufLen, &alloc_length);

		data_command_finish(dev, alloc_length, alloc_length, alloc_length, 0);
		break;

	case GPCMD_GET_EVENT_STATUS_NOTIFICATION:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		buf_alloc(dev, 8 + sizeof(gesn_event_header_t));

		gcdb = (void *)cdb;
		geh = (void *)dev->buffer;

		/* It is fine by the MMC spec to not support async mode operations. */
		if (! (gcdb->polled & 0x01)) {
			/* asynchronous mode */
			/* Only polling is supported, asynchronous mode is not. */
			invalid_field(dev);
			buf_free(dev);
			return;
		}

		/*
		 * These are the supported events.
		 *
		 * We currently only support requests of the 'media' type.
		 * Notification class requests and supported event classes are bitmasks,
		 * but they are built from the same values as the "notification class"
		 * field.
		 */
		geh->supported_events = 1 << GESN_MEDIA;

		/*
		 * We use |= below to set the class field; other bits in this byte
		 * are reserved now but this is useful to do if we have to use the
		 * reserved fields later.
		 */
		geh->notification_class = 0;

		/*
		 * Responses to requests are to be based on request priority.  The
		 * notification_class_request_type enum above specifies the
		 * priority: upper elements are higher prio than lower ones.
		 */
		if (gcdb->class & (1 << GESN_MEDIA)) {
			geh->notification_class |= GESN_MEDIA;

			dev->buffer[4] = dev->media_status;	/* Bits 7-4 = Reserved, Bits 4-1 = Media Status */
			dev->buffer[5] = 1;			/* Power Status (1 = Active) */
			dev->buffer[6] = 0;
			dev->buffer[7] = 0;
			used_len = 8;
		} else {
			geh->notification_class = 0x80; /* No event available */
			used_len = sizeof(gesn_event_header_t);
		}
		geh->len = (uint16_t) (used_len - sizeof(gesn_event_header_t));

		memcpy(dev->buffer, geh, 4);

		set_buf_len(dev, BufLen, &used_len);

		data_command_finish(dev, used_len, used_len, used_len, 0);
		break;

	case GPCMD_READ_DISC_INFORMATION:
		set_phase(dev, SCSI_PHASE_DATA_IN);
		
		max_len = cdb[7];
		max_len <<= 8;
		max_len |= cdb[8];

		buf_alloc(dev, 65536);

		memset(dev->buffer, 0, 34);
		memset(dev->buffer, 1, 9);
		dev->buffer[0] = 0;
		dev->buffer[1] = 32;
		dev->buffer[2] = 0xe; /* last session complete, disc finalized */
		dev->buffer[7] = 0x20; /* unrestricted use */
		dev->buffer[8] = 0x00; /* CD-ROM */

		len = 34;
		len = MIN(len, max_len);

		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, len, 0);
		break;

	case GPCMD_READ_TRACK_INFORMATION:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		max_len = cdb[7];
		max_len <<= 8;
		max_len |= cdb[8];

		buf_alloc(dev, 65536);

		track = ((uint32_t) cdb[2]) << 24;
		track |= ((uint32_t) cdb[3]) << 16;
		track |= ((uint32_t) cdb[4]) << 8;
		track |= (uint32_t) cdb[5];

		if (((cdb[1] & 0x03) != 1) || (track != 1)) {
			invalid_field(dev);
			buf_free(dev);
			return;
		}

		len = 36;

		memset(dev->buffer, 0, 36);
		dev->buffer[0] = 0;
		dev->buffer[1] = 34;
		dev->buffer[2] = 1;	/* track number (LSB) */
		dev->buffer[3] = 1;	/* session number (LSB) */
		dev->buffer[5] = (0 << 5) | (0 << 4) | (4 << 0);/* not damaged, primary copy, data track */
		dev->buffer[6] = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 6) | (1 << 0);	/* not reserved track, not blank, not packet writing, not fixed packet, data mode 1 */
		dev->buffer[7] = (0 << 1) | (0 << 0);		/* last recorded address not valid, next recordable address not valid */

		if (dev->drv->ops && dev->drv->ops->size) {
			dev->buffer[24] = ((dev->drv->ops->size(dev->drv) - 1) >> 24) & 0xff; /* track size */
			dev->buffer[25] = ((dev->drv->ops->size(dev->drv) - 1) >> 16) & 0xff; /* track size */
			dev->buffer[26] = ((dev->drv->ops->size(dev->drv) - 1) >> 8) & 0xff; /* track size */
			dev->buffer[27] = (dev->drv->ops->size(dev->drv) - 1) & 0xff; /* track size */
		} else {
			not_ready(dev);
			buf_free(dev);
			return;
		}

		if (len > max_len) {
			len = max_len;
			dev->buffer[0] = ((max_len - 2) >> 8) & 0xff;
			dev->buffer[1] = (max_len - 2) & 0xff;
		}

		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, max_len, 0);
		break;

	case GPCMD_PLAY_AUDIO_10:
	case GPCMD_PLAY_AUDIO_12:
	case GPCMD_PLAY_AUDIO_MSF:
	case GPCMD_PLAY_AUDIO_TRACK_INDEX:
#if 0
	case GPCMD_PLAY_AUDIO_TRACK_RELATIVE_10:
	case GPCMD_PLAY_AUDIO_TRACK_RELATIVE_12:
#endif
		len = 0;

		set_phase(dev, SCSI_PHASE_STATUS);

		switch(cdb[0]) {
			case GPCMD_PLAY_AUDIO_10:
				msf = 0;
				pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				len = (cdb[7] << 8) | cdb[8];
				break;

			case GPCMD_PLAY_AUDIO_12:
				msf = 0;
				pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				len = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
				break;

			case GPCMD_PLAY_AUDIO_MSF:
				/* This is apparently deprecated in the ATAPI spec, and apparently
				   has been since 1995 (!). Hence I'm having to guess most of it. */
				msf = 1;
				pos = (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				len = (cdb[6] << 16) | (cdb[7] << 8) | cdb[8];
				break;

			case GPCMD_PLAY_AUDIO_TRACK_INDEX:
				msf = 2;
				pos = (cdb[4] << 8) | cdb[5];
				len = (cdb[7] << 8) | cdb[8];
				break;
		}

		if ((dev->drv->host_drive < 1) || (dev->drv->cd_status <= CD_STATUS_DATA_ONLY)) {
			illegal_mode(dev);
			break;
		}

		if (dev->drv->ops && dev->drv->ops->audio_play)
			ret = dev->drv->ops->audio_play(dev->drv, pos, len, msf);
		else
			ret = 0;

		if (ret)
			command_complete(dev);
		else
			illegal_mode(dev);
		break;

	case GPCMD_READ_SUBCHANNEL:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		max_len = cdb[7];
		max_len <<= 8;
		max_len |= cdb[8];
		msf = (cdb[1] >> 1) & 1;

		buf_alloc(dev, 32);

		DEBUG("CD-ROM %i: Getting page %i (%s)\n", dev->id, cdb[3], msf ? "MSF" : "LBA");

		if (cdb[3] > 3) {
			DBGLOG(1, "CD-ROM %i: Read subchannel check condition %02X\n", dev->id,
				     cdb[3]);
			invalid_field(dev);
			buf_free(dev);
			return;
		}

		switch(cdb[3]) {
			case 0:
				alloc_length = 4;
				break;

			case 1:
				alloc_length = 16;
				break;

			default:
				alloc_length = 24;
				break;
		}

		memset(dev->buffer, 0, 24);
		pos = 0;
		dev->buffer[pos++] = 0;
		dev->buffer[pos++] = 0; /*Audio status*/
		dev->buffer[pos++] = 0; dev->buffer[pos++] = 0; /*Subchannel length*/
		dev->buffer[pos++] = cdb[3] & 3; /*Format code*/
		if (cdb[3] == 1) {
			if (dev->drv->ops && dev->drv->ops->getcurrentsubchannel)
				dev->buffer[1] = dev->drv->ops->getcurrentsubchannel(dev->drv, &dev->buffer[5], msf);
			else {
				not_ready(dev);
				buf_free(dev);
				return;
			}

			switch(dev->drv->cd_status) {
				case CD_STATUS_PLAYING:
					dev->buffer[1] = 0x11;
					break;

				case CD_STATUS_PAUSED:
					dev->buffer[1] = 0x12;
					break;

				case CD_STATUS_DATA_ONLY:
					dev->buffer[1] = 0x15;
					break;

				default:
					dev->buffer[1] = 0x13;
					break;
			}
		}

		if (!(cdb[2] & 0x40) || (cdb[3] == 0))
			len = 4;
		else
			len = alloc_length;

		len = MIN(len, max_len);
		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, len, 0);
		break;

	case GPCMD_READ_DVD_STRUCTURE:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		alloc_length = (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);

		buf_alloc(dev, alloc_length);

		if (dev->drv->ops && dev->drv->ops->size)
			len = dev->drv->ops->size(dev->drv);
		else {
			not_ready(dev);
			buf_free(dev);
			return;
		}

		if ((cdb[7] < 0xc0) && (len <= CD_MAX_SECTORS)) {
			incompatible_format(dev);
			buf_free(dev);
			return;
		}

		memset(dev->buffer, 0, alloc_length);

		if ((cdb[7] <= 0x7f) || (cdb[7] == 0xff)) {
			if (cdb[1] == 0) {
				ret = read_dvd_structure(dev, format, cdb, dev->buffer);
				if (ret) {
					set_buf_len(dev, BufLen, &alloc_length);
					data_command_finish(dev, alloc_length, alloc_length,
								       alloc_length, 0);
				} else
					buf_free(dev);
				return;
			}
		} else {
			invalid_field(dev);
			buf_free(dev);
			return;
		}
		break;

	case GPCMD_START_STOP_UNIT:
		set_phase(dev, SCSI_PHASE_STATUS);

		switch(cdb[4] & 3) {
			case 0:		/* Stop the disc. */
				if (dev->drv->ops && dev->drv->ops->stop)
					dev->drv->ops->stop(dev->drv);
				break;

			case 1:		/* Start the disc and read the TOC. */
					/* This causes a TOC reload. */
				if (dev->drv->ops && dev->drv->ops->ready)
					(void)dev->drv->ops->ready(dev->drv);
				break;

			case 2:		/* Eject the disc if possible. */
				if (dev->drv->ops && dev->drv->ops->stop)
					dev->drv->ops->stop(dev->drv);
				ui_cdrom_eject(dev->id);
				break;

			case 3:		/* Load the disc (close tray). */
				ui_cdrom_reload(dev->id);
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

			dev->buffer[idx++] = 05;
			dev->buffer[idx++] = cdb[2];
			dev->buffer[idx++] = 0;

			idx++;

			switch (cdb[2]) {
				case 0x00:
					dev->buffer[idx++] = 0x00;
					dev->buffer[idx++] = 0x83;
					break;

				case 0x83:
					if (idx + 24 > max_len) {
						data_phase_error(dev);
						buf_free(dev);
						return;
					}

					dev->buffer[idx++] = 0x02;
					dev->buffer[idx++] = 0x00;
					dev->buffer[idx++] = 0x00;
					dev->buffer[idx++] = 20;
					ide_padstr8(dev->buffer + idx, 20, "53R141");	/* Serial */
					idx += 20;

					if (idx + 72 > cdb[4])
						goto atapi_out;
					dev->buffer[idx++] = 0x02;
					dev->buffer[idx++] = 0x01;
					dev->buffer[idx++] = 0x00;
					dev->buffer[idx++] = 68;
					ide_padstr8(dev->buffer + idx, 8, EMU_NAME); /* Vendor */
					idx += 8;
					ide_padstr8(dev->buffer + idx, 40, device_identify_ex); /* Product */
					idx += 40;
					ide_padstr8(dev->buffer + idx, 20, "53R141"); /* Product */
					idx += 20;
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
			dev->buffer[0] = 5; /*CD-ROM*/
			dev->buffer[1] = 0x80; /*Removable*/
			dev->buffer[2] = (dev->drv->bus_type == CDROM_BUS_SCSI) ? 0x02 : 0x00; /*SCSI-2 compliant*/
			dev->buffer[3] = (dev->drv->bus_type == CDROM_BUS_SCSI) ? 0x12 : 0x21;
			dev->buffer[4] = 31;
			if (dev->drv->bus_type == CDROM_BUS_SCSI) {
				dev->buffer[6] = 1;	/* 16-bit transfers supported */
				dev->buffer[7] = 0x20;	/* Wide bus supported */
			}

			ide_padstr8(dev->buffer + 8, 8, EMU_NAME); /* Vendor */
			ide_padstr8(dev->buffer + 16, 16, device_identify); /* Product */
			ide_padstr8(dev->buffer + 32, 4, EMU_VERSION); /* Revision */
			idx = 36;

			if (max_len == 96) {
				dev->buffer[4] = 91;
				idx = 96;
			}
		}

atapi_out:
		dev->buffer[size_idx] = idx - preamble_len;
		len = idx;

		len = MIN(len, max_len);
		set_buf_len(dev, BufLen, &len);

		data_command_finish(dev, len, len, max_len, 0);
		break;

	case GPCMD_PREVENT_REMOVAL:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_PAUSE_RESUME_ALT:
	case GPCMD_PAUSE_RESUME:
		set_phase(dev, SCSI_PHASE_STATUS);

		if (cdb[8] & 1) {
			if (dev->drv->ops && dev->drv->ops->audio_resume)
				dev->drv->ops->audio_resume(dev->drv);
			else {
				illegal_mode(dev);
				break;
			}
		} else {
			if (dev->drv->ops && dev->drv->ops->audio_pause)
				dev->drv->ops->audio_pause(dev->drv);
			else {
				illegal_mode(dev);
				break;
			}
		}
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
				pos = (cdb[2] << 24) | (cdb[3]<<16) | (cdb[4]<<8) | cdb[5];
				break;
		}
		dev->drv->seek_diff = ABS((int) (pos - dev->drv->seek_pos));
		cdrom_seek(dev->drv, pos);
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

	case GPCMD_STOP_PLAY_SCAN:
		set_phase(dev, SCSI_PHASE_STATUS);

		if (dev->drv->ops && dev->drv->ops->stop)
			dev->drv->ops->stop(dev->drv);
		else {
			illegal_mode(dev);
			break;
		}
		command_complete(dev);
		break;

	default:
		illegal_opcode(dev);
		break;
    }

    DBGLOG(1, "CD-ROM %i: Phase: %02X, request length: %i\n",
	   dev->phase, dev->request_length);

    if (atapi_phase_to_scsi(dev) == SCSI_PHASE_STATUS)
	buf_free(dev);
}


/* The command second phase function, needed for Mode Select. */
static uint8_t
phase_data_out(scsi_cdrom_t *dev)
{
    uint8_t page, page_len, hdr_len, val, old_val, ch;
    uint16_t block_desc_len, pos;
    uint8_t error = 0;
    uint16_t i;

    switch(dev->current_cdb[0]) {
	case GPCMD_MODE_SELECT_6:
	case GPCMD_MODE_SELECT_10:
		if (dev->current_cdb[0] == GPCMD_MODE_SELECT_10)
			hdr_len = 8;
		else
			hdr_len = 4;

		if (dev->drv->bus_type == CDROM_BUS_SCSI) {
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
				DEBUG("CD-ROM %i: Buffer has only block descriptor\n", dev->id);
				break;
			}

			page = dev->buffer[pos] & 0x3F;
			page_len = dev->buffer[pos + 1];

			pos += 2;

			if (!(mode_sense_page_flags & (1LL << ((uint64_t) page)))) {
				DEBUG("CD-ROM %i: Unimplemented page %02X\n", dev->id, page);
				error |= 1;
			} else {
				for (i = 0; i < page_len; i++) {
					ch = mode_sense_pages_changeable.pages[page][i + 2];
					val = dev->buffer[pos + i];
					old_val = dev->ms_pages_saved.pages[page][i + 2];
					if (val != old_val) {
						if (ch)
							dev->ms_pages_saved.pages[page][i + 2] = val;
						else {
							DEBUG("CD-ROM %i: Unchangeable value on position %02X on page %02X\n", dev->id, i + 2, page);
							error |= 1;
						}
					}
				}
			}

			pos += page_len;

			if (dev->drv->bus_type == CDROM_BUS_SCSI)
				val = mode_sense_pages_default_scsi.pages[page][0] & 0x80;
			else
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
pio_request(scsi_cdrom_t *dev, uint8_t out)
{
    int ret = 0;

    if (dev->drv->bus_type < CDROM_BUS_SCSI) {
	DEBUG("CD-ROM %i: Lowering IDE IRQ\n", dev->id);
	ide_irq_lower(ide_drives[dev->drv->bus_id.ide_channel]);
    }

    dev->status = BUSY_STAT;

    if (dev->pos >= dev->packet_len) {
	DEBUG("CD-ROM %i: %i bytes %s, command done\n", dev->id, dev->pos, out ? "written" : "read");

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
	DEBUG("CD-ROM %i: %i bytes %s, %i bytes are still left\n", dev->id, dev->pos,
		  out ? "written" : "read", dev->packet_len - dev->pos);

	/* If less than (packet length) bytes are remaining, update packet length
	   accordingly. */
	if ((dev->packet_len - dev->pos) < (dev->max_transfer_len))
		dev->max_transfer_len = dev->packet_len - dev->pos;
	DEBUG("CD-ROM %i: Packet length %i, request length %i\n", dev->id, dev->packet_len,
		  dev->max_transfer_len);

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
read_from_ide_dma(scsi_cdrom_t *dev)
{
    int ret;

    if (dev == NULL)
	return 0;

    if (ide_bus_master_write) {
	ret = ide_bus_master_write(dev->drv->bus_id.ide_channel >> 1,
				   dev->buffer, dev->packet_len,
				   ide_bus_master_priv[dev->drv->bus_id.ide_channel >> 1]);
	if (ret == 2)		/* DMA not enabled, wait for it to be enabled. */
		return 2;
	else if (ret == 1) {	/* DMA error. */		
		bus_master_error(dev);
		return 0;
	} else
		return 1;
    }

    return 0;
}


static int
read_from_scsi_dma(uint8_t scsi_id, uint8_t scsi_lun)
{
    scsi_cdrom_t *dev = scsi_devices[scsi_id][scsi_lun].p;
    int32_t *BufLen = &scsi_devices[scsi_id][scsi_lun].buffer_length;

    if (dev == NULL)
	return 0;

    DEBUG("Reading from SCSI DMA: SCSI ID %02X, init length %i\n",
	  scsi_id, *BufLen);
    memcpy(dev->buffer, scsi_devices[scsi_id][scsi_lun].cmd_buffer, *BufLen);

    return 1;
}


static void
irq_raise(scsi_cdrom_t *dev)
{
    if (dev->drv->bus_type < CDROM_BUS_SCSI)
	ide_irq_raise(ide_drives[dev->drv->bus_id.ide_channel]);
}


static int
read_from_dma(scsi_cdrom_t *dev)
{
#ifdef _LOGGING
    int32_t *BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;
#endif
    int ret = 0;

    if (dev->drv->bus_type == CDROM_BUS_SCSI)
	ret = read_from_scsi_dma(dev->drv->bus_id.scsi.id, dev->drv->bus_id.scsi.lun);
    else
	ret = read_from_ide_dma(dev);

    if (ret != 1)
	return ret;

    if (dev->drv->bus_type == CDROM_BUS_SCSI)
	DEBUG("CD-ROM %i: SCSI Input data length: %i\n", dev->id, *BufLen);
    else
	DEBUG("CD-ROM %i: ATAPI Input data length: %i\n", dev->id, dev->packet_len);

    ret = phase_data_out(dev);
    if (ret)
	return 1;

    return 0;
}


static int
write_to_ide_dma(scsi_cdrom_t *dev)
{
    int ret;

    if (dev == NULL)
	return 0;

    if (! ide_bus_master_read)
	return 0;

    ret = ide_bus_master_read(dev->drv->bus_id.ide_channel >> 1,
			      dev->buffer, dev->packet_len,
			      ide_bus_master_priv[dev->drv->bus_id.ide_channel >> 1]);
    if (ret == 2)	/* DMA not enabled, wait for it to be enabled. */
	return 2;
    else if (ret == 1) {	/* DMA error. */		
	bus_master_error(dev);
	return 0;
    }

    return 1;
}


static int
write_to_scsi_dma(uint8_t scsi_id, uint8_t scsi_lun)
{
    scsi_cdrom_t *dev = scsi_devices[scsi_id][scsi_lun].p;
    int32_t *BufLen = &scsi_devices[scsi_id][scsi_lun].buffer_length;

    if (dev == NULL)
	return 0;

    DEBUG("Writing to SCSI DMA: SCSI ID %02X, init length %i\n", scsi_id, *BufLen);
    memcpy(scsi_devices[scsi_id][scsi_lun].cmd_buffer, dev->buffer, *BufLen);
    DEBUG("CD-ROM %i: Data from CD buffer:  %02X %02X %02X %02X %02X %02X %02X %02X\n", dev->id,
	  dev->buffer[0], dev->buffer[1], dev->buffer[2], dev->buffer[3],
	  dev->buffer[4], dev->buffer[5], dev->buffer[6], dev->buffer[7]);

    return 1;
}


static int
write_to_dma(scsi_cdrom_t *dev)
{
#ifdef _LOGGING
    scsi_device_t *sd = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun];
    int32_t *BufLen = &sd->buffer_length;
#endif
    int ret = 0;

    if (dev->drv->bus_type == CDROM_BUS_SCSI) {
	DEBUG("Write to SCSI DMA: (ID %02X)\n", dev->drv->bus_id.scsi.id);
	ret = write_to_scsi_dma(dev->drv->bus_id.scsi.id, dev->drv->bus_id.scsi.lun);
    } else
	ret = write_to_ide_dma(dev);

    if (dev->drv->bus_type == CDROM_BUS_SCSI)
	DEBUG("CD-ROM %i: SCSI Output data length: %i\n", dev->id, *BufLen);
    else
	DEBUG("CD-ROM %i: ATAPI Output data length: %i\n", dev->id, dev->packet_len);

    return ret;
}


static void
do_callback(void *priv)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;
    int ret;

    switch(dev->packet_status) {
	case PHASE_IDLE:
		DEBUG("CD-ROM %i: PHASE_IDLE\n", dev->id);
		dev->pos = 0;
		dev->phase = 1;
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		return;

	case PHASE_COMMAND:
		DEBUG("CD-ROM %i: PHASE_COMMAND\n", dev->id);
		dev->status = BUSY_STAT | (dev->status & ERR_STAT);
		memcpy(dev->atapi_cdb, dev->buffer, 12);
		do_command(dev, dev->atapi_cdb);
		return;

	case PHASE_COMPLETE:
		DEBUG("CD-ROM %i: PHASE_COMPLETE\n", dev->id);
		dev->status = READY_STAT;
		dev->phase = 3;
		dev->packet_status = 0xFF;
		ui_sb_icon_update(SB_CDROM | dev->id, 0);
		irq_raise(dev);
		return;

	case PHASE_DATA_OUT:
		DEBUG("CD-ROM %i: PHASE_DATA_OUT\n", dev->id);
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		dev->phase = 0;
		irq_raise(dev);
		return;

	case PHASE_DATA_OUT_DMA:
		DEBUG("CD-ROM %i: PHASE_DATA_OUT_DMA\n", dev->id);
		ret = read_from_dma(dev);

		if ((ret == 1) || (dev->drv->bus_type == CDROM_BUS_SCSI)) {
			DEBUG("CD-ROM %i: DMA data out phase done\n");
			buf_free(dev);
			command_complete(dev);
		} else if (ret == 2) {
			DEBUG("CD-ROM %i: DMA out not enabled, wait\n");
			command_bus(dev);
		} else {
			DEBUG("CD-ROM %i: DMA data out phase failure\n");
			buf_free(dev);
		}
		return;

	case PHASE_DATA_IN:
		DEBUG("CD-ROM %i: PHASE_DATA_IN\n", dev->id);
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		dev->phase = 2;
		irq_raise(dev);
		return;

	case PHASE_DATA_IN_DMA:
		DEBUG("CD-ROM %i: PHASE_DATA_IN_DMA\n", dev->id);
		ret = write_to_dma(dev);

		if ((ret == 1) || (dev->drv->bus_type == CDROM_BUS_SCSI)) {
			DEBUG("CD-ROM %i: DMA data in phase done\n", dev->id);
			buf_free(dev);
			command_complete(dev);
		} else if (ret == 2) {
			DEBUG("CD-ROM %i: DMA in not enabled, wait\n", dev->id);
			command_bus(dev);
		} else {
			DEBUG("CD-ROM %i: DMA data in phase failure\n", dev->id);
			buf_free(dev);
		}
		return;

	case PHASE_ERROR:
		DEBUG("CD-ROM %i: PHASE_ERROR\n", dev->id);
		dev->status = READY_STAT | ERR_STAT;
		dev->phase = 3;
		irq_raise(dev);
		ui_sb_icon_update(SB_CDROM | dev->id, 0);
		return;
    }
}


static uint32_t
packet_read(void *priv, int length)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;
    uint16_t *cdbufferw;
    uint32_t *cdbufferl;
    uint32_t temp = 0;

    if (dev == NULL)
	return 0;

    cdbufferw = (uint16_t *)dev->buffer;
    cdbufferl = (uint32_t *)dev->buffer;

    if (dev->buffer == NULL)
	return 0;

    /* Make sure we return a 0 and don't attempt to read from the buffer if we're transferring bytes beyond it,
       which can happen when issuing media access commands with an allocated length below minimum request length
       (which is 1 sector = 2048 bytes). */
    switch(length) {
	case 1:
		temp = (dev->pos < dev->packet_len) ? dev->buffer[dev->pos] : 0;
		dev->pos++;
		dev->request_pos++;
		break;

	case 2:
		temp = (dev->pos < dev->packet_len) ? cdbufferw[dev->pos >> 1] : 0;
		dev->pos += 2;
		dev->request_pos += 2;
		break;

	case 4:
		temp = (dev->pos < dev->packet_len) ? cdbufferl[dev->pos >> 2] : 0;
		dev->pos += 4;
		dev->request_pos += 4;
		break;

	default:
		return 0;
    }

    if (dev->packet_status == PHASE_DATA_IN) {
	if ((dev->request_pos >= dev->max_transfer_len) ||
	    (dev->pos >= dev->packet_len)) {
		/* Time for a DRQ. */
		pio_request(dev, 0);
	}
	return temp;
    }

    return 0;
}


static void
packet_write(void *priv, uint32_t val, int length)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;
    uint16_t *cdbufferw;
    uint32_t *cdbufferl;

    if (dev == NULL)
	return;

    if ((dev->packet_status == PHASE_IDLE) && !dev->buffer)
	buf_alloc(dev, 12);

    cdbufferw = (uint16_t *)dev->buffer;
    cdbufferl = (uint32_t *)dev->buffer;

    if (dev->buffer == NULL)
	return;

    switch(length) {
	case 1:
		dev->buffer[dev->pos] = val & 0xff;
		dev->pos++;
		dev->request_pos++;
		break;

	case 2:
		cdbufferw[dev->pos >> 1] = val & 0xffff;
		dev->pos += 2;
		dev->request_pos += 2;
		break;

	case 4:
		cdbufferl[dev->pos >> 2] = val;
		dev->pos += 4;
		dev->request_pos += 4;
		break;

	default:
		return;
    }

    if (dev->packet_status == PHASE_DATA_OUT) {
	if ((dev->request_pos >= dev->max_transfer_len) ||
	    (dev->pos >= dev->packet_len)) {
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
		do_callback(dev);
		timer_update_outstanding();
	}
    }
}


static void
do_close(void *priv)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev != NULL)
	free(dev);
}


static void
do_stop(void *priv)
{
    scsi_cdrom_t *dev = (scsi_cdrom_t *)priv;

    if (dev->drv && dev->drv->ops && dev->drv->ops->stop)
	dev->drv->ops->stop(dev->drv);
}


static int
get_max(int ide_has_dma, int type)
{
    int ret;

    switch(type) {
	case TYPE_PIO:
		ret = ide_has_dma ? 4 : 0;
		break;

	case TYPE_SDMA:
		ret = ide_has_dma ? -1 : 2;
		break;

	case TYPE_MDMA:
		ret = ide_has_dma ? -1 : 2;
		break;

	case TYPE_UDMA:
		ret = ide_has_dma ? -1 : 2;
		break;

	default:
		ret = -1;
		break;
    }

    return ret;
}


static int
get_timings(int ide_has_dma, int type)
{
    int ret;

    switch(type) {
	case TIMINGS_DMA:
		ret = ide_has_dma ? 120 : 0;
		break;

	case TIMINGS_PIO:
		ret = ide_has_dma ? 120 : 0;
		break;

	case TIMINGS_PIO_FC:
		ret = 0;
		break;

	default:
		ret = 0;
		break;
    }

    return ret;
}


/**
 * Fill in ide->buffer with the output of the "IDENTIFY PACKET DEVICE" command
 * FIXME: we ID as a NEC drive here, because the PB machine(s) need it.
 *        maybe use a new function cdrom_set_ident() for that? --FvK
 */
static void
do_identify(void *priv, int ide_has_dma)
{
    ide_t *ide = (ide_t *)priv;
#ifdef _LOGGING
    scsi_cdrom_t *dev = (scsi_cdrom_t *)ide->sc;
    char device_identify[9] = { 'E', 'M', 'U', '_', 'C', 'D', '0', '0', 0 };


    device_identify[7] = dev->id + 0x30;
#endif
    DEBUG("ATAPI Identify: %s\n", device_identify);

    /* ATAPI device, CD-ROM drive, removable media, accelerated DRQ */
    ide->buffer[0] = 0x8000 | (5<<8) | 0x80 | (2<<5);

    ide_padstr((char *) (ide->buffer + 10), "", 20);         /* Serial Number */
#if 0
    ide_padstr((char *) (ide->buffer + 23), EMU_VERSION, 8); /* Firmware */
    ide_padstr((char *) (ide->buffer + 27), device_identify, 40); /* Model */
#else
    ide_padstr((char *) (ide->buffer + 23), "4.20    ", 8);  /* Firmware */
    ide_padstr((char *) (ide->buffer + 27), "NEC                 CD-ROM DRIVE:273    ", 40); /* Model */
#endif
    ide->buffer[49] = 0x200; /* LBA supported */
    ide->buffer[126] = 0xfffe; /* Interpret zero byte count limit as maximum length */

    if (ide_has_dma) {
	ide->buffer[71] = 30;
	ide->buffer[72] = 30;
    }
}


static void
do_init(scsi_cdrom_t *dev)
{
    /* Do a reset (which will also rezero it). */
    do_reset(dev);

    /* Configure the drive. */
    dev->requested_blocks = 1;

    dev->drv->bus_mode = 0;
    if (dev->drv->bus_type >= CDROM_BUS_ATAPI)
	dev->drv->bus_mode |= 2;
    if (dev->drv->bus_type < CDROM_BUS_SCSI)
	dev->drv->bus_mode |= 1;
    DEBUG("CD-ROM %i: Bus type %i, bus mode %i\n", dev->id, dev->drv->bus_type, dev->drv->bus_mode);

    dev->sense[0] = 0xf0;
    dev->sense[7] = 10;
    dev->status = READY_STAT | DSC_STAT;
    dev->pos = 0;
    dev->packet_status = 0xff;
    scsi_cdrom_sense_key = scsi_cdrom_asc = scsi_cdrom_ascq = dev->unit_attention = 0;
    dev->drv->cur_speed = dev->drv->speed_idx;

    mode_sense_load(dev);
}


void
scsi_cdrom_drive_reset(int c)
{
    scsi_cdrom_t *dev;
    scsi_device_t *sd;
    cdrom_t *drv;
    ide_t *id;

    /* Get a pointer to the CD-ROM drive. */
    drv = &cdrom[c];

    /* Make sure to ignore any SCSI CD-ROM drive that has an out of range ID. */
    if ((drv->bus_type == CDROM_BUS_SCSI) && (drv->bus_id.scsi.id > SCSI_ID_MAX))
	return;

    /* Make sure to ignore any ATAPI CD-ROM drive that has an out of range IDE channel. */
    if ((drv->bus_type == CDROM_BUS_ATAPI) && (drv->bus_id.ide_channel > 7))
	return;

    /* If we do not have a local block, allocate and initialize it. */
    dev = (scsi_cdrom_t *)drv->priv;
    if (dev == NULL) {
	dev = (scsi_cdrom_t *)mem_alloc(sizeof(scsi_cdrom_t));
	memset(dev, 0, sizeof(scsi_cdrom_t));
	drv->priv = dev;
    } else
    dev->id = c;
    dev->drv = drv;

    /* Link in our local methods. */
    drv->insert = do_insert;
    drv->get_volume = get_volume;
    drv->get_channel = get_channel;
    drv->close = do_close;

    do_init(dev);

    if (drv->bus_type == CDROM_BUS_SCSI) {
	/* SCSI CD-ROM, attach to the SCSI bus. */
	sd = &scsi_devices[drv->bus_id.scsi.id][drv->bus_id.scsi.lun];

	sd->p = dev;
	sd->command = do_command;
	sd->callback = do_callback;
#if 1	/* Will be moved to IDE layer */
	sd->err_stat_to_scsi = err_stat_to_scsi;
#endif
	sd->request_sense = request_sense_scsi;
	sd->reset = do_reset;
	sd->read_capacity = read_capacity;
	sd->type = SCSI_REMOVABLE_CDROM;

	DEBUG("SCSI CD-ROM drive %i attached to SCSI ID %i LUN %i\n",
	      c, cdrom[c].bus_id.scsi.id, cdrom[c].bus_id.scsi.lun);
    } else if (drv->bus_type == CDROM_BUS_ATAPI) {
	/* ATAPI CD-ROM, attach to the IDE bus. */
	id = ide_drives[drv->bus_id.ide_channel];
	/* If the IDE channel is initialized, we attach to it,
	   otherwise, we do nothing - it's going to be a drive
	   that's not attached to anything. */
	if (id != NULL) {
		id->sc = dev;
		id->get_max = get_max;
		id->get_timings = get_timings;
		id->identify = do_identify;
		id->set_signature = set_signature;
		id->packet_write = packet_write;
		id->packet_read = packet_read;
		id->stop = do_stop;
		id->packet_callback = do_callback;
		id->device_reset = do_reset;
		id->interrupt_drq = 0;

		ide_atapi_attach(id);
	}

	DEBUG("ATAPI CD-ROM drive %i attached to IDE channel %i\n",
	      c, cdrom[c].bus_id.ide_channel);
    }
}
