/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the code generator.
 *
 * Version:	@(#)codegen_ops.h	1.0.1	2018/02/14
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
#ifndef _CODEGEN_OPS_H_
#define _CODEGEN_OPS_H_

#include "codegen.h"

typedef uint32_t (*RecompOpFn)(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block);

extern RecompOpFn recomp_opcodes[512];
extern RecompOpFn recomp_opcodes_0f[512];
extern RecompOpFn recomp_opcodes_d8[512];
extern RecompOpFn recomp_opcodes_d9[512];
extern RecompOpFn recomp_opcodes_da[512];
extern RecompOpFn recomp_opcodes_db[512];
extern RecompOpFn recomp_opcodes_dc[512];
extern RecompOpFn recomp_opcodes_dd[512];
extern RecompOpFn recomp_opcodes_de[512];
extern RecompOpFn recomp_opcodes_df[512];
RecompOpFn recomp_opcodes_REPE[512];
RecompOpFn recomp_opcodes_REPNE[512];

#define REG_EAX 0
#define REG_ECX 1
#define REG_EDX 2
#define REG_EBX 3
#define REG_ESP 4
#define REG_EBP 5
#define REG_ESI 6
#define REG_EDI 7
#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7
#define REG_AL 0
#define REG_AH 4
#define REG_CL 1
#define REG_CH 5
#define REG_DL 2
#define REG_DH 6
#define REG_BL 3
#define REG_BH 7

#endif
