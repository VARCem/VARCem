/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation the ALI M1429 mainboard.
 *
 * Version:	@(#)m_at_ali1429.c	1.0.2	2018/03/15
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
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../keyboard.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "machine.h"


static int ali1429_index;
static uint8_t ali1429_regs[256];


static void ali1429_recalc(void)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                uint32_t base = 0xc0000 + (c << 15);
                if (ali1429_regs[0x13] & (1 << c))
                {
                        switch (ali1429_regs[0x14] & 3)
                        {
                                case 0:
                                mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                                break;
                                case 1: 
                                mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                                break;
                                case 2:
                                mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                                break;
                                case 3:
                                mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                                break;
                        }
                }
                else
                        mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }
        
        flushmmucache();
}

void ali1429_write(uint16_t port, uint8_t val, void *priv)
{
        if (!(port & 1)) 
                ali1429_index = val;
        else
        {
                ali1429_regs[ali1429_index] = val;
                switch (ali1429_index)
                {
                        case 0x13:
                        ali1429_recalc();
                        break;
                        case 0x14:
                        shadowbios = val & 1;
                        shadowbios_write = val & 2;
                        ali1429_recalc();
                        break;
                }
        }
}

uint8_t ali1429_read(uint16_t port, void *priv)
{
        if (!(port & 1)) 
                return ali1429_index;
        if ((ali1429_index >= 0xc0 || ali1429_index == 0x20) && cpu_iscyrix)
                return 0xff; /*Don't conflict with Cyrix config registers*/
        return ali1429_regs[ali1429_index];
}


static void ali1429_reset(void)
{
        memset(ali1429_regs, 0xff, 256);
}


static void ali1429_init(void)
{
        io_sethandler(0x0022, 0x0002, ali1429_read, NULL, NULL, ali1429_write, NULL, NULL, NULL);
}


void
machine_at_ali1429_init(const machine_t *model)
{
        ali1429_reset();

        machine_at_common_ide_init(model);

	device_add(&keyboard_at_ami_device);
	device_add(&fdc_at_device);

        ali1429_init();

	secondary_ide_check();
}
