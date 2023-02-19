/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Instruction parsing and generation.
 *
 * Version:	@(#)codegen_accumulate_x86.c	1.0.1	2021/10/29
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2020 Sarah Walker.
 *		Copyright 2016-2020 Miran Grca.
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
#include "../mem.h"
#include "cpu.h"
#include "codegen.h"
#include "codegen_accumulate.h"


static struct
{
    int count;
    uintptr_t dest_reg;
} acc_regs[] =
{
    [ACCREG_cycles] = {0, (uintptr_t) &(cycles)}
};

void codegen_accumulate(int acc_reg, int delta)
{
    acc_regs[acc_reg].count += delta;

    if ((acc_reg == ACCREG_cycles) && (delta != 0)) {
	if (delta == -1) {
		/* -delta = 1 */
		addbyte(0xff); /*inc dword ptr[&acycs]*/
		addbyte(0x05);
		addlong((uint32_t) (uintptr_t) &(acycs));
	} else if (delta == 1) {
		/* -delta = -1 */
		addbyte(0xff); /*dec dword ptr[&acycs]*/
		addbyte(0x0d);
		addlong((uint32_t) (uintptr_t) &(acycs));
	} else {
		addbyte(0x81); /*ADD $acc_regs[c].count,acc_regs[c].dest*/
		addbyte(0x05);
		addlong((uint32_t) (uintptr_t) &(acycs));
		addlong((uintptr_t) -delta);
	}
    }
}

void codegen_accumulate_flush(void)
{
    if (acc_regs[0].count) {
	addbyte(0x81); /*ADD $acc_regs[0].count,acc_regs[0].dest*/
	addbyte(0x05);
	addlong((uint32_t) acc_regs[0].dest_reg);
	addlong(acc_regs[0].count);
    }

    acc_regs[0].count = 0;
}

void codegen_accumulate_reset()
{
    acc_regs[0].count = 0;
}