/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Generic interface for CD-ROM/DVD/BD implementations.
 *
 * Version:	@(#)cdrom.c	1.0.21	2018/10/17
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog cdrom_log
#include "../../emu.h"
#include "../../version.h"
#include "../../timer.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../sound/sound.h"
#include "cdrom.h"
#include "cdrom_image.h"
#include "cdrom_null.h"


#define MIN_SEEK		2000
#define MAX_SEEK	      333333


#ifdef ENABLE_CDROM_LOG
int	cdrom_do_log = ENABLE_CDROM_LOG;
#endif


cdrom_t	cdrom[CDROM_NUM];


void
cdrom_log(int level, const char *fmt, ...)
{
#ifdef ENABLE_CDROM_LOG
    va_list ap;

    if (cdrom_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


int
cdrom_lba_to_msf_accurate(int lba)
{
    int pos;
    int m, s, f;

    pos = lba + 150;
    f = pos % 75;
    pos -= f;
    pos /= 75;
    s = pos % 60;
    pos -= s;
    pos /= 60;
    m = pos;

    return ((m << 16) | (s << 8) | f);
}


double
cdrom_seek_time(cdrom_t *dev)
{
    uint32_t diff = dev->seek_diff;
    double sd = (double) (MAX_SEEK - MIN_SEEK);

    if (diff < MIN_SEEK)
	return 0.0;
    if (diff > MAX_SEEK)
	diff = MAX_SEEK;

    diff -= MIN_SEEK;

    return cdrom_speeds[dev->cur_speed].seek1 +
	   ((cdrom_speeds[dev->cur_speed].seek2 * ((double) diff)) / sd);
}


void
cdrom_seek(cdrom_t *dev, uint32_t pos)
{
    if (! dev)
	return;

    DEBUG("CD-ROM %i: Seek %08lx\n", dev->id, pos);

    dev->seek_pos = pos;

    if (dev->ops && dev->ops->stop)
	dev->ops->stop(dev);
}


int
cdrom_playing_completed(cdrom_t *dev)
{
    dev->prev_status = dev->cd_status;

    if (dev->ops && dev->ops->status)
	dev->cd_status = dev->ops->status(dev);

    if (((dev->prev_status == CD_STATUS_PLAYING) ||
	 (dev->prev_status == CD_STATUS_PAUSED)) &&
	((dev->cd_status != CD_STATUS_PLAYING) &&
	 (dev->cd_status != CD_STATUS_PAUSED)))
	return 1;

    return 0;
}


/* Peform a master init on the entire module. */
void
cdrom_global_init(void)
{
    /* Clear the global data. */
//FIXME: allocate these on the heap..
    memset(cdrom, 0x00, sizeof(cdrom));
}


static void
cdrom_drive_reset(cdrom_t *dev)
{
    dev->p = NULL;
    dev->insert = NULL;
    dev->close = NULL;
    dev->get_volume = NULL;
    dev->get_channel = NULL;
}


void
cdrom_hard_reset(void)
{
    cdrom_t *dev;
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
	dev = &cdrom[i];
	if (dev->bus_type) {
		DEBUG("CDROM %i: hard_reset\n", i);

		dev->id = i;

		cdrom_drive_reset(dev);

		switch(dev->bus_type) {
			case CDROM_BUS_ATAPI:
			case CDROM_BUS_SCSI:
				scsi_cdrom_drive_reset(i);
				break;

			default:
				break;
		}


		if (dev->host_drive == 200) {
			cdrom_image_open(dev, dev->image_path);

			if (dev->reset)
				dev->reset(dev);
		} else
			cdrom_null_open(dev);
	}
    }

    sound_cd_stop();
}


void
cdrom_close(void)
{
    cdrom_t *dev;
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
	dev = &cdrom[i];

	if (dev->ops && dev->ops->close)
		dev->ops->close(dev);

	if (dev->close)
		dev->close(dev->p);

	cdrom_drive_reset(dev);
    }
}


/* Signal disc change to the emulated machine. */
void
cdrom_insert(uint8_t id)
{
    cdrom_t *dev = &cdrom[id];

    if (dev->bus_type) {
	if (dev->insert)
		dev->insert(dev->p);
    }
}


/* The mechanics of ejecting a CD-ROM from a drive. */
void
cdrom_eject(uint8_t id)
{
    cdrom_t *dev = &cdrom[id];

    /* This entire block should be in cdrom.c/cdrom_eject(dev*) ... */
    if (dev->host_drive == 0) {
	/* Switch from empty to empty. Do nothing. */
	return;
    }

    if ((dev->host_drive >= 'A') && (dev->host_drive <= 'Z')) {
	ui_sb_menu_set_item(SB_CDROM | id,
		IDM_CDROM_HOST_DRIVE | id | ((dev->host_drive - 'A') << 3), 0);
    }

    if (dev->prev_image_path) {
	free(dev->prev_image_path);
	dev->prev_image_path = NULL;
    }

    if (dev->host_drive == 200) {
	dev->prev_image_path = (wchar_t *)mem_alloc(1024);
	wcscpy(dev->prev_image_path, dev->image_path);
    }

    dev->prev_host_drive = dev->host_drive;
    dev->host_drive = 0;

    if (dev->ops && dev->ops->eject)
	dev->ops->eject(dev);

    if (dev->ops && dev->ops->close)
	dev->ops->close(dev);

    memset(dev->image_path, 0, sizeof(dev->image_path));

    cdrom_null_open(dev);

    cdrom_insert(id);
}


/* The mechanics of re-loading a CD-ROM drive. */
void
cdrom_reload(uint8_t id)
{
    cdrom_t *dev = &cdrom[id];
#ifdef USE_HOST_CDROM
    int new_cdrom_drive;
#endif

    if ((dev->host_drive == dev->prev_host_drive) ||
	(dev->prev_host_drive == 0) || (dev->host_drive != 0)) {
	/* Switch from empty to empty. Do nothing. */
	return;
    }

    if (dev->ops && dev->ops->close)
	dev->ops->close(dev);

    memset(dev->image_path, 0, sizeof(dev->image_path));

    if (dev->prev_host_drive == 200) {
	/* Reload a previous image. */
	wcscpy(dev->image_path, dev->prev_image_path);
	free(dev->prev_image_path);
	dev->prev_image_path = NULL;
	cdrom_image_open(dev, dev->image_path);

	cdrom_insert(id);

	if (wcslen(dev->image_path) == 0)
		dev->host_drive = 0;
	  else
		dev->host_drive = 200;
#ifdef USE_HOST_CDROM
    } else {
	/* Reload the previous host drive. */
	new_cdrom_drive = dev->prev_host_drive;
	cdrom_host_open(dev, new_cdrom_drive);
	if (dev->bus_type) {
		/* Signal disc change to the emulated machine. */
		cdrom_insert(id);
	}
#endif
    }
}


void
cdrom_reset_bus(int bus)
{
    cdrom_t *dev;
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
	dev = &cdrom[i];

        if (dev->bus_type == bus)
		dev->reset(dev);
    }
}


int
cdrom_string_to_bus(const char *str)
{
    int ret = CDROM_BUS_DISABLED;

    if (! strcmp(str, "none")) return(ret);

    if (! strcmp(str, "ide") || !strcmp(str, "atapi")
#if 1
	|| !strcmp(str, "ide_pio_only") || !strcmp(str, "ide_pio_and_dma")
	|| !strcmp(str, "atapi_pio_only") || !strcmp(str, "atapi_pio_and_dma")
#endif
	) return(CDROM_BUS_ATAPI);

    if (! strcmp(str, "scsi"))
	return(CDROM_BUS_SCSI);

    if (! strcmp(str, "usb"))
	ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NO_USB);

    return(ret);
}


const char *
cdrom_bus_to_string(int bus)
{
    const char *ret = "none";

    switch (bus) {
	case CDROM_BUS_DISABLED:
	default:
		break;

	case CDROM_BUS_ATAPI:
		ret = "atapi";
		break;

	case CDROM_BUS_SCSI:
		ret = "scsi";
		break;
    }

    return(ret);
}
