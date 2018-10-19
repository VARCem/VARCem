/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the CD-ROM null interface for unmounted
 *		guest CD-ROM drives.
 *
 * FIXME:	TO BE REMOVED
 *
 * Version:	@(#)cdrom_null.c	1.0.7	2018/10/18
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
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
#include "../../emu.h"
#include "cdrom.h"
#include "cdrom_null.h"


static int
null_ready(cdrom_t *dev)
{
    return(0);
}


static int
null_media_type_id(cdrom_t *dev)
{
    return(0x70);
}


static uint32_t
null_size(cdrom_t *dev)
{
    return(0);
}


static int
null_status(cdrom_t *dev)
{
    return(CD_STATUS_EMPTY);
}


static void
null_close(cdrom_t *dev)
{
    dev->ops = NULL;
}


static int
null_readtoc(cdrom_t *dev, uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single)
{
    return(0);
}


static int
null_readtoc_session(cdrom_t *dev, uint8_t *b, int msf, int maxlen)
{
    return(0);
}


static int
null_readtoc_raw(cdrom_t *dev, uint8_t *b, int maxlen)
{
    return(0);
}


static uint8_t
null_getcurrentsubchannel(cdrom_t *dev, uint8_t *b, int msf)
{
    return(0x13);
}


static int
null_readsector_raw(cdrom_t *dev, uint8_t *buffer, int sector, int ismsf, int cdrom_sector_type, int cdrom_sector_flags, int *len)
{
    *len = 0;

    return(0);
}


static cdrom_ops_t cdrom_null_ops = {
    null_ready,
    NULL,
    NULL,
    null_media_type_id,

    null_size,
    null_status,
    NULL,
    null_close,

    NULL,
    NULL,

    null_readtoc,
    null_readtoc_session,
    null_readtoc_raw,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    null_getcurrentsubchannel,
    null_readsector_raw
};


int
cdrom_null_open(cdrom_t *dev)
{
    cdrom_set_null_handler(dev);

    return(0);
}


void
cdrom_null_reset(cdrom_t *dev)
{
}


void
cdrom_set_null_handler(cdrom_t *dev)
{
    dev->ops = &cdrom_null_ops;

    dev->host_drive = 0;
    memset(dev->image_path, 0, sizeof(dev->image_path));
}
