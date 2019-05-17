/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ALi M-1429/1431 chipset.
 *
 * Version:	@(#)ali1429.c	1.0.9	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "ali1429.h"


typedef struct {
    int		type;

    int		indx;
    uint8_t	regs[256];
} ali_t;


static void
ali1429_recalc(ali_t *dev)
{
    uint32_t base;
    int c;

    for (c = 0; c < 8; c++) {
	base = 0xc0000 + (c << 15);

	if (dev->regs[0x13] & (1 << c)) {
		switch (dev->regs[0x14] & 3) {
			case 0:
				mem_set_mem_state(base, 0x8000,
				    MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
				break;

			case 1: 
				mem_set_mem_state(base, 0x8000,
				    MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
				break;

			case 2:
				mem_set_mem_state(base, 0x8000,
				    MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
				break;

			case 3:
				mem_set_mem_state(base, 0x8000,
				    MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
				break;
		}
	} else {
	 	mem_set_mem_state(base, 0x8000,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
	}
    }

    flushmmucache();
}


static void
ali1429_write(uint16_t port, uint8_t val, priv_t priv)
{
    ali_t *dev = (ali_t *)priv;

    if (port & 1) {
	dev->regs[dev->indx] = val;

	switch (dev->indx) {
		case 0x13:
			ali1429_recalc(dev);
			break;

		case 0x14:
			shadowbios = val & 1;
			shadowbios_write = val & 2;
			ali1429_recalc(dev);
			break;
	}
    } else
	dev->indx = val;
}


static uint8_t
ali1429_read(uint16_t port, priv_t priv)
{
    ali_t *dev = (ali_t *)priv;

    if (! (port & 1)) 
	return(dev->indx);

    if ((dev->indx >= 0xc0 || dev->indx == 0x20) && is_cyrix)
	return(0xff); /*Don't conflict with Cyrix config registers*/

    return(dev->regs[dev->indx]);
}


static void
ali1429_reset(ali_t *dev)
{
    memset(dev->regs, 0xff, sizeof(dev->regs));
}


static void
ali_close(priv_t priv)
{
    ali_t *dev = (ali_t *)priv;

    free(dev);
}


static priv_t
ali_init(const device_t *info, UNUSED(void *parent))
{
    ali_t *dev;

    dev = (ali_t *)mem_alloc(sizeof(ali_t));
    memset(dev, 0x00, sizeof(ali_t));
    dev->type = info->local;

    ali1429_reset(dev);

    io_sethandler(0x0022, 2,
		  ali1429_read,NULL,NULL, ali1429_write,NULL,NULL, dev);

    return((priv_t)dev);
}


const device_t ali1429_device = {
    "ALi-M1429",
    0,
    0,
    NULL,
    ali_init, ali_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
