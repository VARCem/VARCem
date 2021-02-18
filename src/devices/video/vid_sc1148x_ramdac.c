/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of a Sierra SC1148X RAMDAC.
 *
 * Version:	@(#)vid_sc1148x_ramdac.c	1.0.0	2021/02/17
 *
 * Authors:	TheCollector1995, <mariogplayer90@gmail.com>
 *          Altheos
 *
 *		Copyright 2020-2021 TheCollector1995.
 *      Copyright 2021 Altheos.
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
#include "vid_sc1148x_ramdac.h"


void
sc1148x_ramdac_out(uint16_t addr, uint8_t val, sc1148x_ramdac_t *dev, svga_t *svga)
{
    switch (addr) {
	case 0x3C6:
	    if (dev->state == 4) {
		    dev->state = 0;
		    dev->ctrl = val;
		    if (dev->ctrl & 0x80) {
			    if (dev->ctrl & 0x40)
				    svga->bpp = 16;
    			else
	   				svga->bpp = 15;
	    	} else {
		    	svga->bpp = 8;
		    }
		    svga_recalctimings(svga);
		    return;
	    }
	    dev->state = 0;
	    break;
	case 0x3C7:
	case 0x3C8:
	case 0x3C9:
		dev->state = 0;
		break;
    }

    svga_out(addr, val, svga);
}


uint8_t
sc1148x_ramdac_in(uint16_t addr, sc1148x_ramdac_t *dev, svga_t *svga)
{
    uint8_t temp;

    temp = svga_in(addr, svga);

    switch (addr) {
	case 0x3C6:
		if (dev->state == 4) {
			dev->state = 0;
			temp = dev->ctrl;

			if (dev->type == 1) {
				if (dev->ctrl & 0x80)
					temp |= 1;
				else
					temp &= ~1;
			}
			break;
		}
		dev->state++;
		break;
    case 0x3C7:
	case 0x3C8:
	case 0x3C9:
		dev->state = 0;
		break;
    }

    return temp;
}


static void *
sc1148x_init(const device_t *info, UNUSED(void *parent))
{
    sc1148x_ramdac_t *dev = (sc1148x_ramdac_t *)mem_alloc(sizeof(sc1148x_ramdac_t));
    memset(dev, 0x00, sizeof(sc1148x_ramdac_t));

    dev->type = info->local;
    
    return dev;
}


static void
sc1148x_close(void *priv)
{
    sc1148x_ramdac_t *dev = (sc1148x_ramdac_t *)priv;

    if (dev != NULL)
	    free(dev);
}


const device_t sc11483_ramdac_device = {
    "Sierra SC11483 RAMDAC",
    0, 0, NULL,
    sc1148x_init, sc1148x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t sc11487_ramdac_device = {
    "Sierra SC11487 RAMDAC",
    0, 1, NULL,
    sc1148x_init, sc1148x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
