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
 * Version:	@(#)m_at_headland.c	1.0.4	2018/04/26
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
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../cpu/x86.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../input/keyboard.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "machine.h"


static int headland_index;
static uint8_t headland_regs[256];


static void
headland_write(uint16_t addr, uint8_t val, void *priv)
{
    uint8_t old_val;

    if (addr & 1) {
	old_val = headland_regs[headland_index];

	if (headland_index == 0xc1 && !is486) val = 0;
	headland_regs[headland_index] = val;
	if (headland_index == 0x82) {
		shadowbios = val & 0x10;
		shadowbios_write = !(val & 0x10);
		if (shadowbios)
			mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
		else
			mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
	} else if (headland_index == 0x87) {
		if ((val & 1) && !(old_val & 1))
			softresetx86();
	}
    } else
	headland_index = val;
}


static uint8_t
headland_read(uint16_t addr, void *priv)
{
    if (addr & 1)  {
	if ((headland_index >= 0xc0 || headland_index == 0x20) && cpu_iscyrix)
		return(0xff); /*Don't conflict with Cyrix config registers*/
	return(headland_regs[headland_index]);
    }

    return(headland_index);
}


static void
headland_init(void)
{
    io_sethandler(0x0022, 2,
		  headland_read,NULL,NULL, headland_write,NULL,NULL, NULL);
}


void
machine_at_headland_init(const machine_t *model, void *arg)
{
    machine_at_common_ide_init(model, arg);

    device_add(&keyboard_at_ami_device);
    device_add(&fdc_at_device);

    headland_init();
}
