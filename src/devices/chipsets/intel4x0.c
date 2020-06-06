/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Intel 430/440 PCISet chipsets.
 *
 * Version:	@(#)intel4x0.c	1.0.8	2020/01/29
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include "../system/pci.h"
#include "intel4x0.h"


enum {
    INTEL_430LX,
    INTEL_430NX,
    INTEL_430FX,
    INTEL_430FX_PB640,
    INTEL_430HX,
    INTEL_430VX,
    INTEL_440FX
};


typedef struct {
    int		chip;

    uint8_t	regs[256];
} i4x0_t;


static void
i4x0_map(uint32_t addr, uint32_t size, int state)
{
    switch (state & 3) {
	case 0:
		mem_set_mem_state(addr, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 1:
		mem_set_mem_state(addr, size,
				  MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 2:
		mem_set_mem_state(addr, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
		break;

	case 3:
		mem_set_mem_state(addr, size,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		break;
    }

    flushmmucache_nopc();
}


static void
i4x0_write(int func, int addr, uint8_t val, priv_t priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    if (func)
	return;

    if ((addr >= 0x10) && (addr < 0x4f))
	return;

    switch (addr) {
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x0c: case 0x0e:
		return;

	case 0x04: /*Command register*/
		if (dev->chip >= INTEL_430FX) {
			if (dev->chip == INTEL_430FX_PB640)
				val &= 0x06;
			else
				val &= 0x02;
		} else
			val &= 0x42;
		val |= 0x04;
		break;

	case 0x05:
		if (dev->chip >= INTEL_430FX)
			val = 0;
		else
			val &= 0x01;
		break;

	case 0x06: /*Status*/
		val = 0;
		break;

	case 0x07:
		if (dev->chip >= INTEL_430HX) {
			val &= 0x80;
			val |= 0x02;
		} else {
			val = 0x02;
			if (dev->chip == INTEL_430FX_PB640)
				val |= 0x20;
		}
		break;
		
	case 0x52: /*Cache Control*/
		if (dev->chip < INTEL_440FX) {
			cpu_cache_ext_enabled = (val & 0x01);
			cpu_update_waitstates();
		}
		break;

	case 0x59: /*PAM0*/
		if ((dev->regs[0x59] ^ val) & 0xf0) {
			i4x0_map(0xf0000, 0x10000, val >> 4);
			shadowbios = (val & 0x10);
		}
		break;

	case 0x5a: /*PAM1*/
		if ((dev->regs[0x5a] ^ val) & 0x0f)
			i4x0_map(0xc0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5a] ^ val) & 0xf0)
			i4x0_map(0xc4000, 0x04000, val >> 4);
		break;

	case 0x5b: /*PAM2*/
		if ((dev->regs[0x5b] ^ val) & 0x0f)
			i4x0_map(0xc8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5b] ^ val) & 0xf0)
			i4x0_map(0xcc000, 0x04000, val >> 4);
		break;

	case 0x5c: /*PAM3*/
		if ((dev->regs[0x5c] ^ val) & 0x0f)
			i4x0_map(0xd0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5c] ^ val) & 0xf0)
			i4x0_map(0xd4000, 0x04000, val >> 4);
		break;

	case 0x5d: /*PAM4*/
		if ((dev->regs[0x5d] ^ val) & 0x0f)
			i4x0_map(0xd8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5d] ^ val) & 0xf0)
			i4x0_map(0xdc000, 0x04000, val >> 4);
		break;

	case 0x5e: /*PAM5*/
		if ((dev->regs[0x5e] ^ val) & 0x0f)
			i4x0_map(0xe0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5e] ^ val) & 0xf0)
			i4x0_map(0xe4000, 0x04000, val >> 4);
		break;

	case 0x5f: /*PAM6*/
		if ((dev->regs[0x5f] ^ val) & 0x0f)
			i4x0_map(0xe8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5f] ^ val) & 0xf0)
			i4x0_map(0xec000, 0x04000, val >> 4);
		break;

	case 0x72: /*SMRAM*/
		if ((dev->chip >= INTEL_430FX) &&
		    ((dev->regs[0x72] ^ val) & 0x48))
			i4x0_map(0xa0000, 0x20000, ((val & 0x48) == 0x48) ? 3 : 0);
		break;
    }

    dev->regs[addr] = val;
}


static uint8_t
i4x0_read(int func, int addr, priv_t priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    if (func)
	return 0xff;

    return dev->regs[addr];
}


static void
i4x0_reset(priv_t priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    i4x0_write(0, 0x59, 0x00, priv);
    if (dev->chip >= INTEL_430FX)
	i4x0_write(0, 0x72, 0x02, priv);
}


static void
i4x0_close(priv_t priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    free(dev);
}


static priv_t
i4x0_init(const device_t *info, UNUSED(void *parent))
{
    i4x0_t *dev;

    dev = (i4x0_t *)mem_alloc(sizeof(i4x0_t));
    memset(dev, 0x00, sizeof(i4x0_t));
    dev->chip = info->local;

    dev->regs[0x00] = 0x86;	/*Intel*/
    dev->regs[0x01] = 0x80;

    switch(dev->chip) {
	case INTEL_430LX:
		dev->regs[0x02] = 0xa3; dev->regs[0x03] = 0x04; /*82434LX/NX*/
		dev->regs[0x08] = 0x03; /*A3 stepping*/
		dev->regs[0x50] = 0x80;
		dev->regs[0x52] = 0x40; /*256kb PLB cache*/
		break;

	case INTEL_430NX:
		dev->regs[0x02] = 0xa3; dev->regs[0x03] = 0x04; /*82434LX/NX*/
		dev->regs[0x08] = 0x10; /*A0 stepping*/
		dev->regs[0x50] = 0xA0;
		dev->regs[0x52] = 0x44; /*256kb PLB cache*/
		dev->regs[0x66] = dev->regs[0x67] = 0x02;
		break;

	case INTEL_430FX:
	case INTEL_430FX_PB640:
		dev->regs[0x02] = 0x2d; dev->regs[0x03] = 0x12; /*SB82437FX-66*/
		if (dev->chip == INTEL_430FX_PB640)
			dev->regs[0x08] = 0x02; /*???? stepping*/
		else
			dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x52] = 0x40; /*256kb PLB cache*/
		break;

	case INTEL_430HX:
		dev->regs[0x02] = 0x50; dev->regs[0x03] = 0x12; /*82439HX*/
		dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x51] = 0x20;
		dev->regs[0x52] = 0xB5; /*512kb cache*/
		dev->regs[0x56] = 0x52; /*DRAM control*/
		dev->regs[0x59] = 0x40;
		dev->regs[0x5A] = dev->regs[0x5B] = \
				  dev->regs[0x5C] = dev->regs[0x5D] = 0x44;
		dev->regs[0x5E] = dev->regs[0x5F] = 0x44;
		dev->regs[0x65] = dev->regs[0x66] = dev->regs[0x67] = 0x02;
		dev->regs[0x68] = 0x11;
		break;

	case INTEL_430VX:
		dev->regs[0x02] = 0x30; dev->regs[0x03] = 0x70; /*82437VX*/
		dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x52] = 0x42; /*256kb PLB cache*/
		dev->regs[0x53] = 0x14;
		dev->regs[0x56] = 0x52; /*DRAM control*/
		dev->regs[0x67] = 0x11;
		dev->regs[0x69] = 0x03;
		dev->regs[0x70] = 0x20;
		dev->regs[0x74] = 0x0e;
		dev->regs[0x78] = 0x23;
		break;

	case INTEL_440FX:
		dev->regs[0x02] = 0x37; dev->regs[0x03] = 0x12; /*82441FX*/
		dev->regs[0x08] = 0x02; /*A0 stepping*/
		dev->regs[0x2c] = 0xf4;
		dev->regs[0x2d] = 0x1a;
		dev->regs[0x2f] = 0x11;
		dev->regs[0x51] = 0x01;
		dev->regs[0x53] = 0x80;
		dev->regs[0x58] = 0x10;
		dev->regs[0x5a] = dev->regs[0x5b] = \
				  dev->regs[0x5c] = dev->regs[0x5d] = 0x11;
		dev->regs[0x5e] = 0x11;
		dev->regs[0x5f] = 0x31;
		break;
    }

    dev->regs[0x04] = 0x06; dev->regs[0x05] = 0x00;
    if (dev->chip == INTEL_440FX)
	dev->regs[0x06] = 0x80;

    if (dev->chip == INTEL_430FX)
	dev->regs[0x07] = 0x82;
    else if (dev->chip != INTEL_440FX)
	dev->regs[0x07] = 0x02;
    dev->regs[0x0b] = 0x06;
    if (dev->chip >= INTEL_430FX)
	dev->regs[0x57] = 0x01;
    else
	dev->regs[0x57] = 0x31;
    dev->regs[0x60] = dev->regs[0x61] = dev->regs[0x62] = \
					dev->regs[0x63] = 0x02;
    dev->regs[0x64] = 0x02;
    if (dev->chip >= INTEL_430FX)
	dev->regs[0x72] = 0x02;
	
    if (dev->chip == INTEL_440FX) {
	cpu_cache_ext_enabled = 1;
	cpu_update_waitstates();
    }

    pci_add_card(0, i4x0_read, i4x0_write, dev);

    return((priv_t)dev);
}


const device_t i430lx_device = {
    "Intel 82434LX",
    DEVICE_PCI,
    INTEL_430LX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430nx_device = {
    "Intel 82434NX",
    DEVICE_PCI,
    INTEL_430NX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430fx_device = {
    "Intel SB82437FX-66",
    DEVICE_PCI,
    INTEL_430FX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430fx_pb640_device = {
    "Intel SB82437FX-66 (PB640)",
    DEVICE_PCI,
    INTEL_430FX_PB640,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430hx_device = {
    "Intel 82439HX",
    DEVICE_PCI,
    INTEL_430HX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430vx_device = {
    "Intel 82437VX",
    DEVICE_PCI,
    INTEL_430VX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i440fx_device = {
    "Intel 82441FX",
    DEVICE_PCI,
    INTEL_440FX,
    NULL,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};
