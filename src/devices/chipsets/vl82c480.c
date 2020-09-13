/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the VLSI 82C480 chipset.
 *
 * Version:	@(#)vl82c480.c	1.0.0	2020/09/09
 *
 * Authors:	
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020 Miran Grca.
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
#include "vl82c480.h"

typedef struct {
    uint8_t	reg_idx,
		    regs[256];
} vl82c480_t;


static int
vl82c480_shflags(uint8_t access)
{
    int ret = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;

    switch (access) {
	case 0x00:
	default:
		ret = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
		break;
	case 0x01:
		ret = MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL;
		break;
	case 0x02:
		ret = MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL;
		break;
	case 0x03:
		ret = MEM_READ_INTERNAL | MEM_WRITE_INTERNAL;
		break;
    }

    return ret;
}


static void 
shadow_recalc(vl82c480_t *dev)
{
    int i, j;
    uint32_t base;
    uint8_t access;

    shadowbios = 0;
    shadowbios_write = 0;

    for (i = 0; i < 8; i += 2) {
	for (j = 0; j < 6; j++) {
		base = 0x000a0000 + (i << 13) + (j << 16);
		access = dev->regs[0x0d + j] & (3 << i);
		mem_set_mem_state(base, 0x4000, vl82c480_shflags(access));
		shadowbios |= ((base >= 0xe0000) && (access & 0x02));
		shadowbios_write |= ((base >= 0xe0000) && (access & 0x01));
	}
    }

    flushmmucache();
}


static void
vl82c480_write(uint16_t addr, uint8_t val, priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;

    switch (addr) {
	case 0xec:
		dev->reg_idx = val;
		break;

	case 0xed:
		if (dev->reg_idx >= 0x01 && dev->reg_idx <= 0x24) {
            switch (dev->reg_idx) {
			    default:
				    dev->regs[dev->reg_idx] = val;
				    break;
			    case 0x05:
			    	dev->regs[dev->reg_idx] = (dev->regs[dev->reg_idx] & 0x10) | (val & 0xef);
				    break;
			    case 0x0d: case 0x0e: case 0x0f: case 0x10:
			    case 0x11: case 0x12:
				    dev->regs[dev->reg_idx] = val;
				    shadow_recalc(dev);
				    break;
			}
		}
	    break;

	case 0xee:
		if (mem_a20_alt)
			outb(0x92, inb(0x92) & ~2);
		break;
    }
}


static uint8_t 
vl82c480_read(uint16_t addr, priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;

    uint8_t ret = 0xff;

    switch (addr) {
	    case 0xec:
		    ret = dev->reg_idx;
		    break;

	    case 0xed:
		    ret = dev->regs[dev->reg_idx];
		    break;

	    case 0xee:
		    if (!mem_a20_alt)
			    outb(0x92, inb(0x92) | 2);
		    break;

	    case 0xef:
		    cpu_reset(0);
		    cpu_set_edx();
		    break;
    }

    return ret;
}


static void
vl82c480_close(priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;

    free(dev);
}


static priv_t
vl82c480_init(const device_t *info, UNUSED(void *parent))
{
    vl82c480_t *dev;
    
    dev = (vl82c480_t *)mem_alloc(sizeof(vl82c480_t));
    memset(dev, 0x00, sizeof(vl82c480_t));

    dev->regs[0x00] = 0x90;
    dev->regs[0x01] = 0xff;
    dev->regs[0x02] = 0x8a;
    dev->regs[0x03] = 0x88;
    dev->regs[0x06] = 0x1b;
    dev->regs[0x08] = 0x38;

    io_sethandler(0x00ec, 0x0004,  
        vl82c480_read, NULL, NULL, vl82c480_write, NULL, NULL,  dev);

    device_add(&port92_device);

    return((priv_t)dev);
}


const device_t vl82c480_device = {
    "VLSI VL82c480",
    0, 0, NULL,
    vl82c480_init, vl82c480_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
