/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Intel mainboards.
 *
 * Version:	@(#)intel.c	1.0.3	2018/04/26
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
#include "cpu/cpu.h"
#include "machine/machine.h"
#include "io.h"
#include "mem.h"
#include "pit.h"
#include "timer.h"
#include "intel.h"
#include "plat.h"


static uint16_t batman_timer_latch;
static int64_t batman_timer = 0;


uint8_t batman_brdconfig(uint16_t port, UNUSED(void *p))
{
        switch (port)
        {
                case 0x73:
                return 0xff;
                case 0x75:
                return 0xdf;
        }
        return 0;
}


static void batman_timer_over(UNUSED(void *p))
{
        batman_timer = 0;
}


static void batman_timer_write(uint16_t addr, uint8_t val, UNUSED(void *p))
{
        if (addr & 1)
                batman_timer_latch = (batman_timer_latch & 0xff) | (val << 8);
        else
                batman_timer_latch = (batman_timer_latch & 0xff00) | val;
        batman_timer = batman_timer_latch * TIMER_USEC;
}


static uint8_t batman_timer_read(uint16_t addr, UNUSED(void *p))
{
        uint16_t batman_timer_latch;
        
        cycles -= (int)PITCONST;
        
        timer_clock();

        if (batman_timer < 0)
                return 0;
        
        batman_timer_latch = (uint16_t)(batman_timer / TIMER_USEC);

        if (addr & 1)
                return batman_timer_latch >> 8;
        return batman_timer_latch & 0xff;
}


void intel_batman_init(void)
{
        io_sethandler(0x0073, 0x0001, batman_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
        io_sethandler(0x0075, 0x0001, batman_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);

        io_sethandler(0x0078, 0x0002, batman_timer_read, NULL, NULL, batman_timer_write, NULL, NULL, NULL);
        timer_add(batman_timer_over, &batman_timer, &batman_timer, NULL);
}
