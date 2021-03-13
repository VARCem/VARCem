/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		AMD SYSCALL and SYSRET CPU Instructions.
 *
 * Version:	@(#)x86_ops_amd.h	1.0.4	2020/12/04
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
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


#ifndef HAVE_INTERNAL_ILLEGAL
# define HAVE_INTERNAL_ILLEGAL
static int internal_illegal(char *s)
{
	cpu_state.pc = cpu_state.oldpc;
	x86gpf(s, 0);
	return cpu_state.abrt;
}
#endif

/*	0 = Limit 0-15
	1 = Base 0-15
	2 = Base 16-23 (bits 0-7), Access rights
		8-11	Type
		12	S
		13, 14	DPL
		15	P
	3 = Limit 16-19 (bits 0-3), Base 24-31 (bits 8-15), granularity, etc.
		4	A
		6	DB
		7	G	*/

#define AMD_SYSCALL_EIP	(star & 0xFFFFFFFF)
#define AMD_SYSCALL_SB	((star >> 32) & 0xFFFF)
#define AMD_SYSRET_SB	((star >> 48) & 0xFFFF)

/* 0F 05 */
static int opSYSCALL(uint32_t fetchdat)
{
	uint16_t syscall_cs_seg_data[4] = {0, 0, 0, 0};
	uint16_t syscall_ss_seg_data[4] = {0, 0, 0, 0};

	if (!(cr0 & 1))  return internal_illegal("SYSCALL: CPU not in protected mode");
	if (!AMD_SYSCALL_SB)  return internal_illegal("SYSCALL: AMD SYSCALL SB MSR is zero");

	/* Set VM, IF, RF to 0. */
	/* cpu_state.eflags &= ~0x00030200;
	cpu_state.flags &= ~0x0200; */

	/* Let's do this by the AMD spec. */
	ECX = cpu_state.pc;

	cpu_state.eflags &= ~0x0002;
	cpu_state.flags &= ~0x0200;

	/* CS */
	cpu_state.seg_cs.seg = AMD_SYSCALL_SB & ~7;
	if (AMD_SYSCALL_SB & 4)
	{
		if (cpu_state.seg_cs.seg >= ldt.limit)
		{
			ERRLOG("CPU: bigger than LDT limit %04X %04X CS\n",AMD_SYSCALL_SB,ldt.limit);
			x86gpf(NULL, AMD_SYSCALL_SB & ~3);
			return 1;
		}
		cpu_state.seg_cs.seg +=ldt.base;
	}
	else
	{
		if (cpu_state.seg_cs.seg >= gdt.limit)
		{
			ERRLOG("CPU: bigger than GDT limit %04X %04X CS\n",AMD_SYSCALL_SB,gdt.limit);
			x86gpf(NULL, AMD_SYSCALL_SB & ~3);
			return 1;
		}
		cpu_state.seg_cs.seg += gdt.base;
	}
	cpl_override = 1;

	syscall_cs_seg_data[0] = 0xFFFF;
	syscall_cs_seg_data[1] = 0;
	syscall_cs_seg_data[2] = 0x9B00;
	syscall_cs_seg_data[3] = 0xC0;

	cpl_override = 0;

	use32 = 0x300;
	CS = (AMD_SYSCALL_SB & ~3) | 0;

	do_seg_load(&cpu_state.seg_cs, syscall_cs_seg_data);
	use32 = 0x300;

	CS = (CS & 0xFFFC) | 0;

	cpu_state.seg_cs.limit = 0xFFFFFFFF;
	cpu_state.seg_cs.limit_high = 0xFFFFFFFF;

	/* SS */
	syscall_ss_seg_data[0] = 0xFFFF;
	syscall_ss_seg_data[1] = 0;
	syscall_ss_seg_data[2] = 0x9300;
	syscall_ss_seg_data[3] = 0xC0;
	do_seg_load(&cpu_state.seg_ss, syscall_ss_seg_data);
	cpu_state.seg_ss.seg = (AMD_SYSCALL_SB + 8) & 0xFFFC;
	stack32 = 1;

	cpu_state.seg_ss.limit = 0xFFFFFFFF;
	cpu_state.seg_ss.limit_high = 0xFFFFFFFF;

	cpu_state.seg_ss.checked = 0;

	cpu_state.pc = AMD_SYSCALL_EIP;

	CLOCK_CYCLES(20);

	CPU_BLOCK_END();

	return 0;
}

/* 0F 07 */
static int opSYSRET(uint32_t fetchdat)
{
	uint16_t sysret_cs_seg_data[4] = {0, 0, 0, 0};
	uint16_t sysret_ss_seg_data[4] = {0, 0, 0, 0};

	if (!AMD_SYSRET_SB)  return internal_illegal("SYSRET: CS MSR is zero");
	if (!(cr0 & 1))  return internal_illegal("SYSRET: CPU not in protected mode");

	cpu_state.pc = ECX;

	cpu_state.eflags |= (1 << 1);

	/* CS */
	cpu_state.seg_cs.seg = AMD_SYSRET_SB & ~7;
	if (AMD_SYSRET_SB & 4)
	{
		if (cpu_state.seg_cs.seg >= ldt.limit)
		{
			ERRLOG("CPU: bigger than LDT limit %04X %04X CS\n",AMD_SYSRET_SB,ldt.limit);
			x86gpf(NULL, AMD_SYSRET_SB & ~3);
			return 1;
		}
		cpu_state.seg_cs.seg +=ldt.base;
	}
	else
	{
		if (cpu_state.seg_cs.seg >= gdt.limit)
		{
			ERRLOG("CPU: bigger than GDT limit %04X %04X CS\n",AMD_SYSRET_SB,gdt.limit);
			x86gpf(NULL, AMD_SYSRET_SB & ~3);
			return 1;
		}
		cpu_state.seg_cs.seg += gdt.base;
	}
	cpl_override = 1;

	sysret_cs_seg_data[0] = 0xFFFF;
	sysret_cs_seg_data[1] = 0;
	sysret_cs_seg_data[2] = 0xFB00;
	sysret_cs_seg_data[3] = 0xC0;

	cpl_override = 0;

	use32 = 0x300;
	CS = (AMD_SYSRET_SB & ~3) | 3;

	do_seg_load(&cpu_state.seg_cs, sysret_cs_seg_data);
	flushmmucache_cr3();
	use32 = 0x300;

	CS = (CS & 0xFFFC) | 3;

	cpu_state.seg_cs.limit = 0xFFFFFFFF;
	cpu_state.seg_cs.limit_high = 0xFFFFFFFF;

	/* SS */
	sysret_ss_seg_data[0] = 0xFFFF;
	sysret_ss_seg_data[1] = 0;
	sysret_ss_seg_data[2] = 0xF300;
	sysret_ss_seg_data[3] = 0xC0;
	do_seg_load(&cpu_state.seg_ss, sysret_ss_seg_data);
	cpu_state.seg_ss.seg = ((AMD_SYSRET_SB + 8) & 0xFFFC) | 3;
	stack32 = 1;

	cpu_state.seg_ss.limit = 0xFFFFFFFF;
	cpu_state.seg_ss.limit_high = 0xFFFFFFFF;

	cpu_state.seg_ss.checked = 0;

	CLOCK_CYCLES(20);

	CPU_BLOCK_END();

	return 0;
}
