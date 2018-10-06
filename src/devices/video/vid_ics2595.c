/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ICS2595 clock chip emulation.  Used by ATI Mach64.
 *
 * Version:	@(#)vid_ics2595.c	1.0.3	2018/10/05
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
#include "../../emu.h"
#include "../../device.h"
#include "vid_ics2595.h"


enum {
    ICS2595_IDLE = 0,
    ICS2595_WRITE,
    ICS2595_READ
};


static int ics2595_div[4] = {8, 4, 2, 1};


void
ics2595_write(ics2595_t *dev, int strobe, int dat)
{
    int d, n, l;

    if (strobe) {
	if ((dat & 8) && !dev->oldfs3) {	/*Data clock*/
		switch (dev->state) {
			case ICS2595_IDLE:
				dev->state = (dat & 4) ? ICS2595_WRITE : ICS2595_IDLE;
				dev->pos = 0;
				break;

			case ICS2595_WRITE:
				dev->dat = (dev->dat >> 1);
				if (dat & 4)
					dev->dat |= (1 << 19);
				dev->pos++;
				if (dev->pos == 20) {
					l = (dev->dat >> 2) & 0xf;
					n = ((dev->dat >> 7) & 255) + 257;
					d = ics2595_div[(dev->dat >> 16) & 3];

					dev->clocks[l] = (14318181.8 * ((double)n / 46.0)) / (double)d;
					dev->state = ICS2595_IDLE;
				}
				break;                                                
		}
	}

	dev->oldfs2 = dat & 4;
	dev->oldfs3 = dat & 8;
    }

    dev->output_clock = dev->clocks[dat];
}


static void *
ics2595_init(const device_t *info)
{
    ics2595_t *dev = (ics2595_t *)mem_alloc(sizeof(ics2595_t));
    memset(dev, 0x00, sizeof(ics2595_t));

    return dev;
}


static void
ics2595_close(void *priv)
{
    ics2595_t *dev = (ics2595_t *)priv;

    if (dev != NULL)
	free(dev);
}


const device_t ics2595_device = {
    "ICS2595 clock chip",
    0,
    0,
    ics2595_init, ics2595_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
