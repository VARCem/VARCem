/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SSI2001 sound device.
 *
 * Version:	@(#)snd_si2001.c	1.0.2	2018/03/15
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../io.h"
#include "../device.h"
#include "sound.h"
#include "snd_resid.h"
#include "snd_ssi2001.h"


typedef struct ssi2001_t
{
        void    *psid;
        int16_t buffer[SOUNDBUFLEN * 2];
        int     pos;
} ssi2001_t;

static void ssi2001_update(ssi2001_t *ssi2001)
{
        if (ssi2001->pos >= sound_pos_global)
                return;
        
        sid_fillbuf(&ssi2001->buffer[ssi2001->pos], sound_pos_global - ssi2001->pos, ssi2001->psid);
        ssi2001->pos = sound_pos_global;
}

static void ssi2001_get_buffer(int32_t *buffer, int len, void *p)
{
        ssi2001_t *ssi2001 = (ssi2001_t *)p;
        int c;

        ssi2001_update(ssi2001);
        
        for (c = 0; c < len * 2; c++)
                buffer[c] += ssi2001->buffer[c >> 1] / 2;

        ssi2001->pos = 0;
}

static uint8_t ssi2001_read(uint16_t addr, void *p)
{
        ssi2001_t *ssi2001 = (ssi2001_t *)p;
        
        ssi2001_update(ssi2001);
        
        return sid_read(addr, p);
}

static void ssi2001_write(uint16_t addr, uint8_t val, void *p)
{
        ssi2001_t *ssi2001 = (ssi2001_t *)p;
        
        ssi2001_update(ssi2001);        
        sid_write(addr, val, p);
}

void *ssi2001_init(const device_t *info)
{
        ssi2001_t *ssi2001 = malloc(sizeof(ssi2001_t));
        memset(ssi2001, 0, sizeof(ssi2001_t));
        
        pclog("ssi2001_init\n");
        ssi2001->psid = sid_init();
        sid_reset(ssi2001->psid);
        io_sethandler(0x0280, 0x0020, ssi2001_read, NULL, NULL, ssi2001_write, NULL, NULL, ssi2001);
        sound_add_handler(ssi2001_get_buffer, ssi2001);
        return ssi2001;
}

void ssi2001_close(void *p)
{
        ssi2001_t *ssi2001 = (ssi2001_t *)p;
        
        sid_close(ssi2001->psid);

        free(ssi2001);
}

const device_t ssi2001_device =
{
        "Innovation SSI-2001",
        0, 0,
        ssi2001_init, ssi2001_close, NULL,
	NULL, NULL, NULL, NULL,
        NULL
};
