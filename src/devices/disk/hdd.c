/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Common code to handle all sorts of hard disk images.
 *
 * Version:	@(#)hdd.c	1.0.7	2018/05/24
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../../emu.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "hdd.h"


hard_disk_t	hdd[HDD_NUM];
#ifdef ENABLE_HDD_LOG
int		hdd_do_log = ENABLE_HDD_LOG;
#endif


void
hdd_log(const char *fmt, ...)
{
#ifdef ENABLE_HDD_LOG
    va_list ap;

    if (hdd_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


int
hdd_init(void)
{
    /* Clear all global data. */
    memset(hdd, 0x00, sizeof(hdd));

    return(0);
}


int
hdd_count(int bus)
{
    int c = 0;
    int i;

    for (i=0; i<HDD_NUM; i++) {
	if (hdd[i].bus == bus)
		c++;
    }

    return(c);
}


int
hdd_string_to_bus(char *str, int cdrom)
{
    if (! strcmp(str, "none"))
	return(HDD_BUS_DISABLED);

    if (!strcmp(str, "st506") || !strcmp(str, "mfm")) {
	if (cdrom) {
no_cdrom:
		ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NOCDROM);
		return(0);
	}

	return(HDD_BUS_ST506);
    }

    if (! strcmp(str, "esdi")) {
	if (cdrom) goto no_cdrom;

	return(HDD_BUS_ESDI);
    }

    if (! strcmp(str, "ide"))
	return(HDD_BUS_IDE_PIO_ONLY);

    if (! strcmp(str, "ide_pio_only"))
	return(HDD_BUS_IDE_PIO_ONLY);

    if (! strcmp(str, "ide_pio_and_dma"))
	return(HDD_BUS_IDE_PIO_AND_DMA);

    if (! strcmp(str, "atapi"))
	return(HDD_BUS_IDE_PIO_ONLY);

    if (! strcmp(str, "atapi_pio_only"))
	return(HDD_BUS_IDE_PIO_ONLY);

    if (! strcmp(str, "atapi_pio_and_dma"))
	return(HDD_BUS_IDE_PIO_AND_DMA);

    if (! strcmp(str, "scsi"))
	return(HDD_BUS_SCSI);

    if (! strcmp(str, "removable")) {
	if (cdrom) goto no_cdrom;

	return(HDD_BUS_SCSI_REMOVABLE);
    }

    if (! strcmp(str, "scsi_removable")) {
	if (cdrom) goto no_cdrom;

	return(HDD_BUS_SCSI_REMOVABLE);
    }

    if (! strcmp(str, "removable_scsi")) {
	if (cdrom) goto no_cdrom;

	return(HDD_BUS_SCSI_REMOVABLE);
    }

    if (! strcmp(str, "usb"))
	ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NO_USB);

    return(0);
}


char *
hdd_bus_to_string(int bus, int cdrom)
{
    char *s = "none";

    switch (bus) {
	case HDD_BUS_DISABLED:
	default:
		break;

	case HDD_BUS_ST506:
		s = "st506";
		break;

	case HDD_BUS_ESDI:
		s = "esdi";
		break;

	case HDD_BUS_IDE_PIO_ONLY:
		s = cdrom ? "atapi_pio_only" : "ide_pio_only";
		break;

	case HDD_BUS_IDE_PIO_AND_DMA:
		s = cdrom ? "atapi_pio_and_dma" : "ide_pio_and_dma";
		break;

	case HDD_BUS_SCSI:
		s = "scsi";
		break;

	case HDD_BUS_SCSI_REMOVABLE:
		s = "scsi_removable";
		break;
    }

    return(s);
}


int
hdd_is_valid(int c)
{
    if (hdd[c].bus == HDD_BUS_DISABLED) return(0);

    if ((wcslen(hdd[c].fn) == 0) &&
	(hdd[c].bus != HDD_BUS_SCSI_REMOVABLE)) return(0);

    if ((hdd[c].tracks==0) || (hdd[c].hpc==0) || (hdd[c].spt==0)) return(0);

    return(1);
}
