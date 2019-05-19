/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of a AT&T 20c490/491 and 492/493 RAMDAC.
 *
 * Version:	@(#)vid_att20c49x_ramdac.c	1.0.4	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2019 Fred N. van Kempen.
 *		Copyright 2019 Miran Grca.
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
#include "../../emu.h"
#include "../../timer.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_att20c49x_ramdac.h"


enum {
    ATT_490_1 = 0,
    ATT_492_3
};


void
att49x_ramdac_out(uint16_t addr, uint8_t val, att49x_ramdac_t *ramdac, svga_t *svga)
{
    switch (addr) {
	case 0x03c6:
		if (ramdac->state == 4) {
			ramdac->state = 0;
			ramdac->ctrl = val;
			if (ramdac->type == ATT_490_1)
				svga_set_ramdac_type(svga, (val & 2) ? RAMDAC_8BIT : RAMDAC_6BIT);
			switch (val) {
				case 0:
					svga->bpp = 8;
					break;
				
				case 0x20:
					svga->bpp = 15;
					break;
				
				case 0x40:
					svga->bpp = 24;
					break;
				
				case 0x60:
					svga->bpp = 16;
					break;
				
				case 0x80:
				case 0xa0:
					svga->bpp = 15;
					break;
				
				case 0xc0:
					svga->bpp = 16;
					break;
				
				case 0xe0:
					svga->bpp = 24;
					break;
			}
			svga_recalctimings(svga);
			return;
		}
		ramdac->state = 0;
		break;

	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		ramdac->state = 0;
		break;
    }

    svga_out(addr, val, svga);
}


uint8_t
att49x_ramdac_in(uint16_t addr, att49x_ramdac_t *ramdac, svga_t *svga)
{
    uint8_t temp;

    temp = svga_in(addr, svga);

    switch (addr) {
	case 0x3c6:
		if (ramdac->state == 4) {
			ramdac->state = 0;
			temp = ramdac->ctrl;
			break;
		}
		ramdac->state++;
		break;

	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		ramdac->state = 0;
		break;
    }

    return temp;
}


static void *
att49x_ramdac_init(const device_t *info, UNUSED(void *parent))
{
    att49x_ramdac_t *dev;

    dev = (att49x_ramdac_t *)mem_alloc(sizeof(att49x_ramdac_t));
    memset(dev, 0x00, sizeof(att49x_ramdac_t));

    dev->type = info->local;
    
    return dev;
}


static void
att49x_ramdac_close(void *priv)
{
    att49x_ramdac_t *dev = (att49x_ramdac_t *)priv;

    if (dev)
	free(dev);
}


const device_t att490_ramdac_device = {
    "AT&T 20c490/20c491 RAMDAC",
    0,
    ATT_490_1,
    NULL,
    att49x_ramdac_init, att49x_ramdac_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t att492_ramdac_device = {
    "AT&T 20c492/20c493 RAMDAC",
    0,
    ATT_492_3,
    NULL,
    att49x_ramdac_init, att49x_ramdac_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
