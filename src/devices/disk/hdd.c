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
 * Version:	@(#)hdd.c	1.0.13	2021/03/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#define dbglog hdd_log
#include "../../emu.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "hdd.h"


hard_disk_t	hdd[HDD_NUM];
#ifdef ENABLE_HDD_LOG
int		hdd_do_log = ENABLE_HDD_LOG;
#endif


#ifdef _LOGGING
void
hdd_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_HDD_LOG
    va_list ap;

    if (hdd_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


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

    for (i = 0; i < HDD_NUM; i++) {
	if (hdd[i].bus == bus)
		c++;
    }

    return(c);
}


int
hdd_string_to_bus(const char *str)
{
    int ret = HDD_BUS_DISABLED;

    if (! strcmp(str, "none")) return(ret);

    if (!strcmp(str, "st506") || !strcmp(str, "mfm") || !strcmp(str, "rll"))
	return(HDD_BUS_ST506);

    if (! strcmp(str, "esdi"))
	return(HDD_BUS_ESDI);

    if (! strcmp(str, "ide") || !strcmp(str, "atapi")
#if 1
	|| !strcmp(str, "ide_pio_only") || !strcmp(str, "ide_pio_and_dma")
	|| !strcmp(str, "atapi_pio_only") || !strcmp(str, "atapi_pio_and_dma")
#endif
	) return(HDD_BUS_IDE);

    if (! strcmp(str, "scsi"))
	return(HDD_BUS_SCSI);

    if (! strcmp(str, "usb"))
	ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_NO_USB);

    return(ret);
}


const char *
hdd_bus_to_string(int bus)
{
    const char *ret = "none";

    switch (bus) {
	case HDD_BUS_DISABLED:
	default:
		break;

	case HDD_BUS_ST506:
		ret = "st506";
		break;

	case HDD_BUS_ESDI:
		ret = "esdi";
		break;

	case HDD_BUS_IDE:
		ret = "ide";
		break;

	case HDD_BUS_SCSI:
		ret = "scsi";
		break;
    }

    return(ret);
}


int
hdd_is_valid(int c)
{
    if (hdd[c].bus == HDD_BUS_DISABLED) return(0);

    if (wcslen(hdd[c].fn) == 0) return(0);

    if ((hdd[c].tracks==0) || (hdd[c].hpc==0) || (hdd[c].spt==0)) return(0);

    return(1);
}


const wchar_t *
hdd_bus_to_ids(int bus)
{
    if (bus == 0)
        bus = IDS_DISABLED;
      else
	bus = IDS_3515 + bus - 1;

    return(get_string(bus));
}


#ifdef USE_MINIVHD
const wchar_t *
vhd_type_to_ids(int vhd_type)
{
    if (vhd_type == 0)
        vhd_type = IDS_DISABLED;
      else
	vhd_type = IDS_3520 + vhd_type - 2;

    return(get_string(vhd_type));
}


const wchar_t *
vhd_blksize_to_ids(int blk_size)
{
    blk_size = IDS_3540 + blk_size;

    return(get_string(blk_size));
}
#endif


void
hdd_active(int drive, int active)
{
    ui_sb_icon_update(SB_DISK | drive, active);
}
