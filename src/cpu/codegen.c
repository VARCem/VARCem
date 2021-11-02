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
 * Version:	@(#)codegen.c	1.0.4	2021/11/02
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2018 Sarah Walker.
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
#include "../emu.h"
#include "../mem.h"
#include "cpu.h"
#include "x86.h"
#include "x86_ops.h"
#include "codegen.h"


void (*codegen_timing_start)();
void (*codegen_timing_prefix)(uint8_t prefix, uint32_t fetchdat);
void (*codegen_timing_opcode)(uint8_t opcode, uint32_t fetchdat, int op_32, uint32_t op_pc);
void (*codegen_timing_block_start)();
void (*codegen_timing_block_end)();
int (*codegen_timing_jump_cycles)();


void codegen_timing_set(codegen_timing_t *timing)
{
    codegen_timing_start = timing->start;
    codegen_timing_prefix = timing->prefix;
    codegen_timing_opcode = timing->opcode;
    codegen_timing_block_start = timing->block_start;
    codegen_timing_block_end = timing->block_end;
    codegen_timing_jump_cycles = timing->jump_cycles;
}

int codegen_in_recompile;

/* This is for compatibility with new x87 code. */
void codegen_set_rounding_mode(int mode)
{
    cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (mode << 10);
}
