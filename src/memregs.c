/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the memory I/O scratch registers on ports 0xE1
 *		and 0xE2, used by just about any emulated machine.
 *
 * Version:	@(#)memregs.c	1.0.1	2018/02/14
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2016-2018 Miran Grca.
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
#include "emu.h"
#include "io.h"
#include "memregs.h"


static uint8_t mem_regs[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint8_t mem_reg_ffff = 0;


void memregs_write(uint16_t port, uint8_t val, void *priv)
{
	if (port == 0xffff)
	{
		mem_reg_ffff = 0;
	}

	mem_regs[port & 0xf] = val;
}

uint8_t memregs_read(uint16_t port, void *priv)
{
	if (port == 0xffff)
	{
		return mem_reg_ffff;
	}

	return mem_regs[port & 0xf];
}

void memregs_init(void)
{
	/* pclog("Memory Registers Init\n"); */

        io_sethandler(0x00e1, 0x0002, memregs_read, NULL, NULL, memregs_write, NULL, NULL,  NULL);
}

void powermate_memregs_init(void)
{
	/* pclog("Memory Registers Init\n"); */

        io_sethandler(0x00ed, 0x0002, memregs_read, NULL, NULL, memregs_write, NULL, NULL,  NULL);
        io_sethandler(0xffff, 0x0001, memregs_read, NULL, NULL, memregs_write, NULL, NULL,  NULL);
}
