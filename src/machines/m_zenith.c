/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Zenith SuperSport.
 *
 * Version:	@(#)m_zenith.c	1.0.1	2019/01/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original patch for PCem by 'Tux'
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2019 Fred N. van Kempen.
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
#include <time.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pit.h"
#include "../devices/ports/game.h"
#include "../devices/ports/parallel.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"


typedef struct {
    mem_map_t	scratchpad_mapping;
    uint8_t	*scratchpad_ram;
} zenith_t;


static uint8_t
scratchpad_read(uint32_t addr, void *priv)
{
    zenith_t *dev = (zenith_t *)priv;

    return dev->scratchpad_ram[addr & 0x3fff];
}


static void
scratchpad_write(uint32_t addr, uint8_t val, void *priv)
{
    zenith_t *dev = (zenith_t *)priv;

    dev->scratchpad_ram[addr & 0x3fff] = val;
}


static void *
scratchpad_init(const device_t *info)
{
    zenith_t *dev;

    dev = (zenith_t *)mem_alloc(sizeof(zenith_t));
    memset(dev, 0x00, sizeof(zenith_t));	
	
    dev->scratchpad_ram = mem_alloc(0x4000);
    
    mem_map_disable(&bios_mapping[4]);
    mem_map_disable(&bios_mapping[5]);

    mem_map_add(&dev->scratchpad_mapping, 0xf0000, 0x4000,
		scratchpad_read,NULL,NULL, scratchpad_write,NULL,NULL,
		dev->scratchpad_ram, MEM_MAPPING_EXTERNAL, dev);
			
    return dev;
}


static void 
scratchpad_close(void *priv)
{
    zenith_t *dev = (zenith_t *)priv;

    if (dev->scratchpad_ram != NULL)
	free(dev->scratchpad_ram);

    free(dev);
}


static const device_t scratchpad_device = {
    "Zenith scratchpad RAM",
    0, 0,
    scratchpad_init, scratchpad_close, NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


void
machine_zenith_supersport_init(const machine_t *model, void *arg)
{		
    machine_common_init(model, arg);

    device_add(&scratchpad_device);
    
    pit_set_out_func(&pit, 1, pit_refresh_timer_xt);

    device_add(&keyboard_xt_device);

    device_add(&fdc_xt_device);

    nmi_init();
}
