/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ACC 2168 chipset.
 *
 * Version:	@(#)acc2168.c	1.0.3	2019/05/15
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2019 Fred N. van Kempen.
 *		Copyright 2019 Miran Grca.
 *		Copyright 2019 Sarah Walker.
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
#include "../system/port92.h"
#include "acc2168.h"


typedef struct {
    int		reg_idx;
    uint8_t	regs[256];
} acc2168_t;


static void 
shadow_recalc(acc2168_t *dev)
{
    if (dev->regs[0x02] & 8) {
	switch (dev->regs[0x02] & 0x30) {
		case 0x00:
			mem_set_mem_state(0xf0000, 0x10000,
					  MEM_READ_EXTERNAL|MEM_WRITE_INTERNAL);
			break;

		case 0x10:
			mem_set_mem_state(0xf0000, 0x10000,
					  MEM_READ_INTERNAL|MEM_WRITE_INTERNAL);
			break;

		case 0x20:
			mem_set_mem_state(0xf0000, 0x10000,
					  MEM_READ_EXTERNAL|MEM_WRITE_EXTERNAL);
			break;

		case 0x30:
			mem_set_mem_state(0xf0000, 0x10000,
					  MEM_READ_INTERNAL|MEM_WRITE_EXTERNAL);
			break;
	}
    } else
	mem_set_mem_state(0xf0000, 0x10000,
			  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

    if (dev->regs[0x02] & 4) switch (dev->regs[0x02] & 0x30) {
	case 0x00:
		mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
		break;

	case 0x10:
		mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		break;

	case 0x20:
		mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 0x30:
		mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
		break;
    } else
	mem_set_mem_state(0xe0000, 0x10000,
			  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}


static void 
acc2168_write(uint16_t addr, uint8_t val, priv_t priv)
{
    acc2168_t *dev = (acc2168_t *)priv;

    if (addr & 1) {
	dev->regs[dev->reg_idx] = val;

	switch (dev->reg_idx) {
		case 0x02:
			shadow_recalc(dev);
			break;
	}
    } else
	dev->reg_idx = val;
}


static uint8_t 
acc2168_read(uint16_t addr, priv_t priv)
{
    acc2168_t *dev = (acc2168_t *)priv;

    if (addr & 1)
	return(dev->regs[dev->reg_idx]);

    return(dev->reg_idx);
}


static void
acc2168_close(priv_t priv)
{
    acc2168_t *dev = (acc2168_t *)priv;

    free(dev);
}


static priv_t
acc2168_init(const device_t *info, UNUSED(void *parent))
{
    acc2168_t *dev;

    dev = (acc2168_t *)mem_alloc(sizeof(acc2168_t));
    memset(dev, 0x00, sizeof(acc2168_t));
	
    io_sethandler(0x00f2, 2,
		  acc2168_read,NULL,NULL, acc2168_write,NULL,NULL, dev);	

    device_add_parent(&port92_inverted_device, (priv_t)dev);

    return((priv_t)dev);
}


const device_t acc2168_device = {
    "ACC 2168",
    0, 0, NULL,
    acc2168_init, acc2168_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
