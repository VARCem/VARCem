/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the CD-ROM null interface for unmounted
 *		guest CD-ROM drives.
 *
 * Version:	@(#)cdrom_null.c	1.0.2	2018/02/21
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
#include "../emu.h"
#include "cdrom.h"


static CDROM null_cdrom;


static int
null_ready(uint8_t id)
{
    return(0);
}


/* Always return 0, the contents of a null CD-ROM drive never change. */
static int
null_medium_changed(uint8_t id)
{
    return(0);
}


static uint8_t
null_getcurrentsubchannel(uint8_t id, uint8_t *b, int msf)
{
    return(0x13);
}


static void
null_eject(uint8_t id)
{
}


static void
null_load(uint8_t id)
{
}

static int
null_readsector_raw(uint8_t id, uint8_t *buffer, int sector, int ismsf, int cdrom_sector_type, int cdrom_sector_flags, int *len)
{
    *len = 0;

    return(0);
}


static int
null_readtoc(uint8_t id, uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single)
{
    return(0);
}


static int
null_readtoc_session(uint8_t id, uint8_t *b, int msf, int maxlen)
{
    return(0);
}


static int
null_readtoc_raw(uint8_t id, uint8_t *b, int maxlen)
{
    return(0);
}


static uint32_t
null_size(uint8_t id)
{
    return(0);
}


static int
null_status(uint8_t id)
{
    return(CD_STATUS_EMPTY);
}


void
cdrom_null_reset(uint8_t id)
{
}


void cdrom_set_null_handler(uint8_t id);

int
cdrom_null_open(uint8_t id, char d)
{
    cdrom_set_null_handler(id);

    return(0);
}


void
null_close(uint8_t id)
{
}


static
void null_exit(uint8_t id)
{
}


static int
null_is_track_audio(uint8_t id, uint32_t pos, int ismsf)
{
    return(0);
}


static int
null_pass_through(uint8_t id, uint8_t *in_cdb, uint8_t *b, uint32_t *len)
{
    return(0);
}


static int
null_media_type_id(uint8_t id)
{
    return(0x70);
}


void
cdrom_set_null_handler(uint8_t id)
{
    cdrom_drives[id].handler = &null_cdrom;
    cdrom_drives[id].host_drive = 0;
    memset(cdrom_image[id].image_path, 0, sizeof(cdrom_image[id].image_path));
}


static CDROM null_cdrom = {
    null_ready,
    null_medium_changed,
    null_media_type_id,
    NULL,
    NULL,
    null_readtoc,
    null_readtoc_session,
    null_readtoc_raw,
    null_getcurrentsubchannel,
    null_pass_through,
    null_readsector_raw,
    NULL,
    null_load,
    null_eject,
    NULL,
    NULL,
    null_size,
    null_status,
    null_is_track_audio,
    NULL,
    null_exit
};
