/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Laser XT series of machines.
 *
 * Version:	@(#)m_laserxt.c	1.0.13	2019/05/13
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
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pit.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


typedef struct {
    int		type;

    int		ems_baseaddr_index;
    int		ems_page[4];
    int		ems_control[4];
    mem_map_t	ems_mapping[4];
} laser_t;


static uint32_t
ems_addr(laser_t *dev, uint32_t addr)
{
    if (dev->ems_page[(addr >> 14) & 3] & 0x80) {
	addr = (!dev->type ? 0x70000 + (((mem_size + 64) & 255) << 10)
			   : 0x30000 + (((mem_size + 320) & 511) << 10)) + ((dev->ems_page[(addr >> 14) & 3] & 0x0f) << 14) + ((dev->ems_page[(addr >> 14) & 3] & 0x40) << 12) + (addr & 0x3fff);
    }

    return addr;
}


static uint8_t
ems_read(uint32_t addr, priv_t priv)
{
    laser_t *dev = (laser_t *)priv;
    uint8_t val = 0xff;

    addr = ems_addr(dev, addr);

    if (addr < ((uint32_t)mem_size << 10))
		val = ram[addr];

    return val;
}


static void
ems_write(uint32_t addr, uint8_t val, priv_t priv)
{
    laser_t *dev = (laser_t *)priv;

    addr = ems_addr(dev, addr);

    if (addr < ((uint32_t)mem_size << 10))
	ram[addr] = val;
}


static uint8_t
do_read(uint16_t port, priv_t priv)
{
    laser_t *dev = (laser_t *)priv;

    switch (port) {
	case 0x0208:
	case 0x4208:
	case 0x8208:
	case 0xc208:
		return dev->ems_page[port >> 14];

	case 0x0209:
	case 0x4209:
	case 0x8209:
	case 0xc209:
		return dev->ems_control[port >> 14];
    }

    return 0xff;
}


static void
do_write(uint16_t port, uint8_t val, priv_t priv)
{
    laser_t *dev = (laser_t *)priv;
    uint32_t paddr, vaddr;
    int i;

    switch (port) {
	case 0x0208:
	case 0x4208:
	case 0x8208:
	case 0xc208:
		dev->ems_page[port >> 14] = val;
		paddr = 0xc0000 + (port & 0xC000) + (((dev->ems_baseaddr_index + (4 - (port >> 14))) & 0x0c) << 14);
		if (val & 0x80) {
			mem_map_enable(&dev->ems_mapping[port >> 14]);
			vaddr = ems_addr(dev, paddr);
			mem_map_set_exec(&dev->ems_mapping[port >> 14], ram + vaddr);
		} else {
			mem_map_disable(&dev->ems_mapping[port >> 14]);
		}
		flushmmucache();
		break;

	case 0x0209:
	case 0x4209:
	case 0x8209:
	case 0xc209:
		dev->ems_control[port >> 14] = val;
		dev->ems_baseaddr_index = 0;
		for (i = 0; i < 4; i++)
			dev->ems_baseaddr_index |= (dev->ems_control[i] & 0x80) >> (7 - i);

		mem_map_set_addr(&dev->ems_mapping[0], 0xc0000 + (((dev->ems_baseaddr_index + 4) & 0x0c) << 14), 0x4000);
		mem_map_set_addr(&dev->ems_mapping[1], 0xc4000 + (((dev->ems_baseaddr_index + 3) & 0x0c) << 14), 0x4000);
		mem_map_set_addr(&dev->ems_mapping[2], 0xc8000 + (((dev->ems_baseaddr_index + 2) & 0x0c) << 14), 0x4000);
		mem_map_set_addr(&dev->ems_mapping[3], 0xcc000 + (((dev->ems_baseaddr_index + 1) & 0x0c) << 14), 0x4000);
		flushmmucache();
		break;
    }
}


static void
laser_close(priv_t priv)
{
    laser_t *dev = (laser_t *)priv;

    free(dev);
}


static priv_t
laser_init(const device_t *info, void *arg)
{
    laser_t *dev;
    int i;

    dev = (laser_t *)mem_alloc(sizeof(laser_t));
    memset(dev, 0x00, sizeof(laser_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    machine_common_init();

    nmi_init();

    /* Set up our DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_xt_refresh_timer);

    switch(dev->type) {
	case 0:		/* Laser XT */
		device_add(&keyboard_xt_device);
		break;

	case 3:		/* Laser XT3 */
		device_add(&keyboard_laserxt3_device);
		break;
    }

    device_add(&fdc_xt_device);

    if (mem_size > 640) {
	io_sethandler(0x0208, 2,
		      do_read,NULL,NULL, do_write,NULL,NULL, dev);
	io_sethandler(0x4208, 2,
		      do_read,NULL,NULL, do_write,NULL,NULL, dev);
	io_sethandler(0x8208, 2,
		      do_read,NULL,NULL, do_write,NULL,NULL, dev);
	io_sethandler(0xc208, 2,
		      do_read,NULL,NULL, do_write,NULL,NULL, dev);

	mem_map_set_addr(&ram_low_mapping, 0,
			 !dev->type ? 0x70000 + (((mem_size + 64) & 255) << 10)
				    : 0x30000 + (((mem_size + 320) & 511) << 10));
    }

    for (i = 0; i < 4; i++) {
	dev->ems_page[i] = 0x7f;
	dev->ems_control[i] = (i == 3) ? 0x00 : 0x80;

	mem_map_add(&dev->ems_mapping[i], 0xe0000 + (i << 14), 0x4000,
		    ems_read,NULL,NULL, ems_write,NULL,NULL,
		    ram + 0xa0000 + (i << 14), 0, dev);

	mem_map_disable(&dev->ems_mapping[i]);
    }

    mem_set_mem_state(0x0c0000, 0x40000,
		      MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

    return((priv_t)dev);
}


static const machine_t lxt_info = {
    MACHINE_ISA,
    0,
    256, 640, 128, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_laser_xt = {
    "Vtech Laser XT",
    DEVICE_ROOT,
    0,
    L"vtech/ltxt",
    laser_init, laser_close, NULL,
    NULL, NULL, NULL,
    &lxt_info,
    NULL
};


static const machine_t lxt3_info = {
    MACHINE_ISA,
    0,
    256, 640, 256, 0, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_laser_xt3 = {
    "Vtech Laser XT/3",
    DEVICE_ROOT,
    3,
    L"vtech/lxt3",
    laser_init, laser_close, NULL,
    NULL, NULL, NULL,
    &lxt3_info,
    NULL
};
