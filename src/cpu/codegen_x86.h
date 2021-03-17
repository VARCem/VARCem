/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the 32-bit code generator.
 *
 * Version:	@(#)codegen_x86.h	1.0.3	2020/12/13
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2018 Sarah Walker.
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
#ifndef CODEGEN_X86_H
# define CODEGEN_X86_H


#define BLOCK_SIZE 0x4000
#define BLOCK_MASK 0x3fff
#define BLOCK_START 0

#define HASH_SIZE 0x20000
#define HASH_MASK 0x1ffff

#define HASH(l) ((l) & 0x1ffff)

#define BLOCK_EXIT_OFFSET 0x7f0
//#define BLOCK_GPF_OFFSET (BLOCK_EXIT_OFFSET - 20)
#define BLOCK_GPF_OFFSET (BLOCK_EXIT_OFFSET - 14)

#define BLOCK_MAX 1720

enum
{
        OP_RET = 0xc3
};

#define NR_HOST_REGS 4
extern int host_reg_mapping[NR_HOST_REGS];
#define NR_HOST_XMM_REGS 8
extern int host_reg_xmm_mapping[NR_HOST_XMM_REGS];

extern uint32_t mem_load_addr_ea_b;
extern uint32_t mem_load_addr_ea_w;
extern uint32_t mem_load_addr_ea_l;
extern uint32_t mem_load_addr_ea_q;
extern uint32_t mem_store_addr_ea_b;
extern uint32_t mem_store_addr_ea_w;
extern uint32_t mem_store_addr_ea_l;
extern uint32_t mem_store_addr_ea_q;

extern uint32_t mem_load_addr_ea_b_no_abrt;
extern uint32_t mem_store_addr_ea_b_no_abrt;
extern uint32_t mem_load_addr_ea_w_no_abrt;
extern uint32_t mem_store_addr_ea_w_no_abrt;
extern uint32_t mem_load_addr_ea_l_no_abrt;
extern uint32_t mem_store_addr_ea_l_no_abrt;
extern uint32_t mem_check_write;
extern uint32_t mem_check_write_w;
extern uint32_t mem_check_write_l;


#endif	/*CODEGEN_X86_H*/
