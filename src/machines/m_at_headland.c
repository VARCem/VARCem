/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the HEADLAND AT286 chipset.
 *
 * FIXME:	fix the mem_map_t stuff in mem_read_b() et al!
 *
 * Version:	@(#)m_at_headland.c	1.0.7	2018/10/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original by GreatPsycho for PCem.
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2017,2018 Miran Grca.
 *		Copyright 2017,2018 GreatPsycho.
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
#include "../cpu/cpu.h"
#include "../cpu/x86.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h"
#include "machine.h"


#define BIOS_AMA932J_VIDEO_PATH	L"machines/unknown/ama932j/oti067.bin"


typedef struct {
    uint8_t	port_92;

    uint8_t	indx;
    uint8_t	regs[256];

    uint8_t	cri;
    uint8_t	cr[8];

    uint8_t	ems_mar;
    uint16_t	ems_mr[64];

    rom_t	vid_bios;

    mem_map_t	low_mapping;
    mem_map_t	ems_mapping[64];
    mem_map_t	mid_mapping;
    mem_map_t	high_mapping;
    mem_map_t	upper_mapping[24];
} headland_t;


/* TODO - Headland chipset's memory address mapping emulation isn't fully implemented yet,
	  so memory configuration is hardcoded now. */
static const int mem_conf_cr0[41] = {
    0x00, 0x00, 0x20, 0x40, 0x60, 0xA0, 0x40, 0xE0,
    0xA0, 0xC0, 0xE0, 0xE0, 0xC0, 0xE0, 0xE0, 0xE0,
    0xE0, 0x20, 0x40, 0x40, 0xA0, 0xC0, 0xE0, 0xE0,
    0xC0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,
    0x20, 0x40, 0x60, 0x60, 0xC0, 0xE0, 0xE0, 0xE0,
    0xE0
};
static const int mem_conf_cr1[41] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40,
    0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x40
};
static const video_timings_t oti067_timing = {VID_ISA,6,8,16,6,8,16};


static uint32_t 
get_addr(headland_t *dev, uint32_t addr, uint16_t *mr)
{
    if (mr && (dev->cr[0] & 2) && (*mr & 0x200)) {
	addr = (addr & 0x3fff) | ((*mr & 0x1F) << 14);
	if (dev->cr[1] & 0x40) {
		if ((dev->cr[4] & 0x80) && (dev->cr[6] & 1)) {
			if (dev->cr[0] & 0x80) {
				addr |= (*mr & 0x60) << 14;
				if (*mr & 0x100)
					addr += ((*mr & 0xC00) << 13) + (((*mr & 0x80) + 0x80) << 15);
				else
					addr += (*mr & 0x80) << 14;
			} else if (*mr & 0x100)
				addr += ((*mr & 0xC00) << 13) + (((*mr & 0x80) + 0x20) << 15);
			else
				addr += (*mr & 0x80) << 12;
		} else if (dev->cr[0] & 0x80)
			addr |= (*mr & 0x100) ? ((*mr & 0x80) + 0x400) << 12 : (*mr & 0xE0) << 14;
		else
			addr |= (*mr & 0x100) ? ((*mr & 0xE0) + 0x40) << 14 : (*mr & 0x80) << 12; 
	} else {
		if ((dev->cr[4] & 0x80) && (dev->cr[6] & 1)) {
			if (dev->cr[0] & 0x80) {
				addr |= ((*mr & 0x60) << 14);
				if (*mr & 0x180)
					addr += ((*mr & 0xC00) << 13) + (((*mr & 0x180) - 0x60) << 16);
			} else
				addr |= ((*mr & 0x60) << 14) | ((*mr & 0x180) << 16) | ((*mr & 0xC00) << 13);
		} else if (dev->cr[0] & 0x80)
			addr |= (*mr & 0x1E0) << 14;
		else
			addr |= (*mr & 0x180) << 12;
	}
    } else if (mr == NULL && (dev->cr[0] & 4) == 0 && mem_size >= 1024 && addr >= 0x100000)
	addr -= 0x60000;

    return addr;
}


static void 
set_global_EMS_state(headland_t *dev, int state)
{
    uint32_t base_addr, virt_addr;
    int i;

    for (i = 0; i < 32; i++) {
	base_addr = (i + 16) << 14;
	if (i >= 24)
		base_addr += 0x20000;
	if ((state & 2) && (dev->ems_mr[((state & 1) << 5) | i] & 0x200)) {
		virt_addr = get_addr(dev, base_addr, &dev->ems_mr[((state & 1) << 5) | i]);
		if (i < 24)
			mem_map_disable(&dev->upper_mapping[i]);
		mem_map_disable(&dev->ems_mapping[(((state ^ 1) & 1) << 5) | i]);
		mem_map_enable(&dev->ems_mapping[((state & 1) << 5) | i]);
		if (virt_addr < ((uint32_t)mem_size << 10))
			mem_map_set_exec(&dev->ems_mapping[((state & 1) << 5) | i], ram + virt_addr);
		else
			mem_map_set_exec(&dev->ems_mapping[((state & 1) << 5) | i], NULL);
	} else {
		mem_map_set_exec(&dev->ems_mapping[((state & 1) << 5) | i], ram + base_addr);
		mem_map_disable(&dev->ems_mapping[(((state ^ 1) & 1) << 5) | i]);
		mem_map_disable(&dev->ems_mapping[((state & 1) << 5) | i]);
		if (i < 24)
			mem_map_enable(&dev->upper_mapping[i]);
	}
    }

    flushmmucache();
}


static void 
memmap_state_update(headland_t *dev)
{
    uint32_t addr;
    int i;

    for (i = 0; i < 24; i++) {
	addr = get_addr(dev, 0x40000 + (i << 14), NULL);
	mem_map_set_exec(&dev->upper_mapping[i], addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);
    }
    mem_set_mem_state(0xA0000, 0x40000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    if (mem_size > 640) {
	if ((dev->cr[0] & 4) == 0) {
		mem_map_set_addr(&dev->mid_mapping, 0x100000, mem_size > 1024 ? 0x60000 : (mem_size - 640) << 10);
		mem_map_set_exec(&dev->mid_mapping, ram + 0xA0000);
		if (mem_size > 1024) {
			mem_map_set_addr(&dev->high_mapping, 0x160000, (mem_size - 1024) << 10);
			mem_map_set_exec(&dev->high_mapping, ram + 0x100000);
		}
	} else {
		mem_map_set_addr(&dev->mid_mapping, 0xA0000, mem_size > 1024 ? 0x60000 : (mem_size - 640) << 10);
		mem_map_set_exec(&dev->mid_mapping, ram + 0xA0000);
		if (mem_size > 1024) {
			mem_map_set_addr(&dev->high_mapping, 0x100000, (mem_size - 1024) << 10);
			mem_map_set_exec(&dev->high_mapping, ram + 0x100000);
		}
	}
    }

    set_global_EMS_state(dev, dev->cr[0] & 3);
}


static void 
hl_write(uint16_t addr, uint8_t val, void *priv)
{
    headland_t *dev = (headland_t *)priv;
    uint32_t base_addr, virt_addr;
    uint8_t old_val, indx;

    switch(addr) {
	case 0x0022:
		dev->indx = val;
		break;

	case 0x0023:
		old_val = dev->regs[dev->indx];
		if ((dev->indx == 0xc1) && !is486)
			val = 0;
		dev->regs[dev->indx] = val;
		if (dev->indx == 0x82) {
			shadowbios = val & 0x10;
			shadowbios_write = !(val & 0x10);
			if (shadowbios)
				mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
			else
				mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
		} else if (dev->indx == 0x87) {
			if ((val & 1) && !(old_val & 1))
				softresetx86();
		}
		break;

	case 0x0092:
		if ((mem_a20_alt ^ val) & 2) {
			mem_a20_alt = val & 2;
			mem_a20_recalc();
		}
                if ((~dev->port_92 & val) & 1) {
			softresetx86();
			cpu_set_edx();
		}
		dev->port_92 = val | 0xfc;
		break;

	case 0x01ec:
		dev->ems_mr[dev->ems_mar & 0x3f] = val | 0xff00;
		indx = dev->ems_mar & 0x1f;
		base_addr = (indx + 16) << 14;
		if (indx >= 24)
			base_addr += 0x20000;
		if ((dev->cr[0] & 2) && ((dev->cr[0] & 1) == ((dev->ems_mar & 0x20) >> 5))) {
			virt_addr = get_addr(dev, base_addr, &dev->ems_mr[dev->ems_mar & 0x3F]);
			if (indx < 24)
				mem_map_disable(&dev->upper_mapping[indx]);
			if (virt_addr < ((uint32_t)mem_size << 10))
				mem_map_set_exec(&dev->ems_mapping[dev->ems_mar & 0x3f], ram + virt_addr);
			else
				mem_map_set_exec(&dev->ems_mapping[dev->ems_mar & 0x3f], NULL);
			mem_map_enable(&dev->ems_mapping[dev->ems_mar & 0x3f]);
			flushmmucache();
		}
		if (dev->ems_mar & 0x80)
			dev->ems_mar++;
		break;

	case 0x01ed:
		dev->cri = val;
		break;

	case 0x01ee:
		dev->ems_mar = val;
		break;

	case 0x01ef:
		old_val = dev->cr[dev->cri];
		switch(dev->cri) {
			case 0:
				dev->cr[0] = (val & 0x1f) | mem_conf_cr0[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
				mem_set_mem_state(0xe0000, 0x10000, (val & 8 ? MEM_READ_INTERNAL : MEM_READ_EXTERNAL) | MEM_WRITE_DISABLED);
				mem_set_mem_state(0xf0000, 0x10000, (val & 0x10 ? MEM_READ_INTERNAL: MEM_READ_EXTERNAL) | MEM_WRITE_DISABLED);
				memmap_state_update(dev);
				break;

			case 1:
				dev->cr[1] = (val & 0xbf) | mem_conf_cr1[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
				memmap_state_update(dev);
				break;

			case 2:
			case 3:
			case 5:
				dev->cr[dev->cri] = val;
				memmap_state_update(dev);
				break;

			case 4:
				dev->cr[4] = (dev->cr[4] & 0xf0) | (val & 0x0f);
				if (val & 1) {
					mem_map_disable(&bios_mapping[0]);
					mem_map_disable(&bios_mapping[1]);
					mem_map_disable(&bios_mapping[2]);
					mem_map_disable(&bios_mapping[3]);
				} else {
					mem_map_enable(&bios_mapping[0]);
					mem_map_enable(&bios_mapping[1]);
					mem_map_enable(&bios_mapping[2]);
					mem_map_enable(&bios_mapping[3]);
				}
				break;

			case 6:
				if (dev->cr[4] & 0x80) {
					dev->cr[dev->cri] = (val & 0xfe) | (mem_size > 8192 ? 1 : 0);
					memmap_state_update(dev);
				}
				break;

			default:
				break;
		}
		break;

	default:
		break;
    }
}


static void 
hl_writew(uint16_t addr, uint16_t val, void *priv)
{
    headland_t *dev = (headland_t *)priv;
    uint32_t base_addr, virt_addr;
    uint8_t indx;

    switch(addr) {
	case 0x01ec:
		dev->ems_mr[dev->ems_mar & 0x3f] = val;
		indx = dev->ems_mar & 0x1f;
		base_addr = (indx + 16) << 14;
		if (indx >= 24)
			base_addr += 0x20000;
		if ((dev->cr[0] & 2) && (dev->cr[0] & 1) == ((dev->ems_mar & 0x20) >> 5)) {
			if (val & 0x200) {
				virt_addr = get_addr(dev, base_addr, &dev->ems_mr[dev->ems_mar & 0x3f]);
				if (indx < 24)
					mem_map_disable(&dev->upper_mapping[indx]);
				if (virt_addr < ((uint32_t)mem_size << 10))
					mem_map_set_exec(&dev->ems_mapping[dev->ems_mar & 0x3f], ram + virt_addr);
                                else
					mem_map_set_exec(&dev->ems_mapping[dev->ems_mar & 0x3f], NULL);
				mem_map_enable(&dev->ems_mapping[dev->ems_mar & 0x3f]);
			} else {
				mem_map_set_exec(&dev->ems_mapping[dev->ems_mar & 0x3f], ram + base_addr);
				mem_map_disable(&dev->ems_mapping[dev->ems_mar & 0x3f]);
				if (indx < 24)
					mem_map_enable(&dev->upper_mapping[indx]);
			}
			flushmmucache();
		}
		if (dev->ems_mar & 0x80)
			dev->ems_mar++;
		break;

	default:
		break;
    }
}


static uint8_t
hl_read(uint16_t addr, void *priv)
{
    headland_t *dev = (headland_t *)priv;
    uint8_t ret = 0xff;

    switch(addr) {
	case 0x0022:
		ret = dev->indx;
		break;

	case 0x0023:
		if ((dev->indx >= 0xc0 || dev->indx == 0x20) && cpu_iscyrix)
			ret = 0xff; /*Don't conflict with Cyrix config registers*/
		else
			ret = dev->regs[dev->indx];
		break;

	case 0x0092:
		ret = dev->port_92 | 0xfc;
		break;

	case 0x01ec:
		ret = (uint8_t)dev->ems_mr[dev->ems_mar & 0x3f];
		if (dev->ems_mar & 0x80)
			dev->ems_mar++;
		break;

	case 0x01ed:
		ret = dev->cri;
		break;

	case 0x01ee:
		ret = dev->ems_mar;
		break;

	case 0x01ef:
		switch(dev->cri) {
			case 0:
				ret = (dev->cr[0] & 0x1f) | mem_conf_cr0[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
				break;

			case 1:
				ret = (dev->cr[1] & 0xbf) | mem_conf_cr1[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
				break;

			case 6:
				if (dev->cr[4] & 0x80)
					ret = (dev->cr[6] & 0xfe) | (mem_size > 8192 ? 1 : 0);
				else
					ret = 0;
				break;

			default:
				ret = dev->cr[dev->cri];
				break;
		}
		break;

	default:
		break;
    }

    return ret;
}


static uint16_t 
hl_readw(uint16_t addr, void *priv)
{
    headland_t *dev = (headland_t *)priv;
    uint16_t ret = 0xffff;

    switch(addr) {
	case 0x01ec:
		ret = dev->ems_mr[dev->ems_mar & 0x3f] | ((dev->cr[4] & 0x80) ? 0xf000 : 0xfc00);
		if (dev->ems_mar & 0x80)
			dev->ems_mar++;
		break;

	default:
		break;
    }

    return ret;
}


static uint8_t 
mem_read_b(uint32_t addr, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;
    uint8_t ret = 0xff;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	ret = ram[addr];

    return ret;
}


static uint16_t 
mem_read_w(uint32_t addr, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;
    uint16_t ret = 0xffff;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	ret = *(uint16_t *)&ram[addr];

    return ret;
}


static uint32_t 
mem_read_l(uint32_t addr, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;
    uint32_t ret = 0xffffffff;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	ret = *(uint32_t *)&ram[addr];

    return ret;
}


static void
mem_write_b(uint32_t addr, uint8_t val, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	ram[addr] = val;
}


static void 
mem_write_w(uint32_t addr, uint16_t val, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	*(uint16_t *)&ram[addr] = val;
}


static void 
mem_write_l(uint32_t addr, uint32_t val, void *priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    headland_t *dev = (headland_t *)map->dev;
    uint16_t *mr = (uint16_t *)map->p2;

    addr = get_addr(dev, addr, mr);
    if (addr < ((uint32_t)mem_size << 10))
	*(uint32_t *)&ram[addr] = val;
}


static void 
headland_init(headland_t *dev, int ht386)
{
    int i;

    dev->port_92 = 0xfc;

    for (i = 0; i < 8; i++)
	dev->cr[i] = 0x00;
    dev->cr[0] = 0x04;

   if (ht386) {
	dev->cr[4] = 0x20;
	io_sethandler(0x0092, 1,
		      hl_read,NULL,NULL, hl_write,NULL,NULL, dev);
    } else
	dev->cr[4] = 0x00;

    io_sethandler(0x01EC, 1,
		  hl_read,hl_readw,NULL, hl_write,hl_writew,NULL, dev);

    io_sethandler(0x01ED, 3, hl_read,NULL,NULL, hl_write,NULL,NULL, dev);

    for (i = 0; i < 64; i++)
	dev->ems_mr[i] = 0x00;

    /* Turn off mem.c mappings. */
    mem_map_disable(&ram_low_mapping);
    mem_map_disable(&ram_mid_mapping);
    mem_map_disable(&ram_high_mapping);

    mem_map_add(&dev->low_mapping, 0, 0x40000,
		mem_read_b,mem_read_w,mem_read_l,
		mem_write_b,mem_write_w,mem_write_l,
		ram, MEM_MAPPING_INTERNAL, &dev->low_mapping);
    mem_map_set_dev(&dev->low_mapping, dev);

    if (mem_size > 640) {
	mem_map_add(&dev->mid_mapping, 0xA0000, 0x60000,
		    mem_read_b,mem_read_w,mem_read_l,
		    mem_write_b,mem_write_w,mem_write_l,
		    ram + 0xA0000, MEM_MAPPING_INTERNAL, &dev->mid_mapping);
	mem_map_enable(&dev->mid_mapping);
	mem_map_set_dev(&dev->mid_mapping, dev);
    }

    if (mem_size > 1024) {
	mem_map_add(&dev->high_mapping, 0x100000, ((mem_size-1024)*1024),
		    mem_read_b,mem_read_w,mem_read_l,
		    mem_write_b,mem_write_w,mem_write_l,
		    ram + 0x100000, MEM_MAPPING_INTERNAL, &dev->high_mapping);
	mem_map_enable(&dev->high_mapping);
	mem_map_set_dev(&dev->high_mapping, dev);
    }

    for (i = 0; i < 24; i++) {
	mem_map_add(&dev->upper_mapping[i],
		    0x40000 + (i << 14), 0x4000,
		    mem_read_b,mem_read_w,mem_read_l,
		    mem_write_b,mem_write_w,mem_write_l,
		    mem_size > 256 + (i << 4) ? ram + 0x40000 + (i << 14) : NULL,
		    MEM_MAPPING_INTERNAL, &dev->upper_mapping[i]);
	mem_map_enable(&dev->upper_mapping[i]);
	mem_map_set_dev(&dev->upper_mapping[i], dev);
    }

    for (i = 0; i < 64; i++) {
	dev->ems_mr[i] = 0x00;
	mem_map_add(&dev->ems_mapping[i],
		    ((i & 31) + ((i & 31) >= 24 ? 24 : 16)) << 14, 0x04000,
		    mem_read_b,mem_read_w,mem_read_l,
		    mem_write_b,mem_write_w,mem_write_l,
		    ram + (((i & 31) + ((i & 31) >= 24 ? 24 : 16)) << 14),
		    0, &dev->ems_mapping[i]);
	mem_map_disable(&dev->ems_mapping[i]);
	mem_map_set_dev(&dev->ems_mapping[i], dev);

	/* HACK - we need 'mr' in the r/w routines!! */
	dev->ems_mapping[i].p2 = &dev->ems_mr[i];
    }

    memmap_state_update(dev);
}


static headland_t *
headland_common_init(int ht386)
{
    headland_t *dev;

    dev = (headland_t *)mem_alloc(sizeof(headland_t));
    memset(dev, 0x00, sizeof(headland_t));

    device_add(&keyboard_at_ami_device);

    device_add(&fdc_at_device);

    headland_init(dev, ht386);

    return(dev);
}


void
machine_at_ama932j_init(const machine_t *model, void *arg)
{
    headland_t *dev;

    machine_at_common_ide_init(model, arg);

    dev = headland_common_init(1);

    if (video_card == VID_INTERNAL) {
	rom_init(&dev->vid_bios, BIOS_AMA932J_VIDEO_PATH,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_INTERNAL);

	device_add(&oti067_onboard_device);
    }
}


void
machine_at_tg286m_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    headland_common_init(0);

    if (video_card == VID_INTERNAL) {
	device_add(&et4000k_tg286_isa_device);
	video_inform(VID_TYPE_SPEC, &oti067_timing);
    }
}
