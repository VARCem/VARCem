/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of 80286+ CPU interpreter.
 *
 * Version:	@(#)386.c	1.0.10	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#ifndef INFINITY
# define INFINITY   (__builtin_inff())
#endif
#include "../emu.h"
#include "../timer.h"
#include "../mem.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "cpu.h"
#include "x86.h"
#include "x87.h"
#include "386_common.h"


#define CPU_BLOCK_END()


extern int	codegen_flags_changed;
extern int	nmi_enable;
extern int	cpl_override;
extern int	fpucount;
extern int	oddeven;
extern int	xout;


uint16_t	flags,eflags;
uint32_t	oldds,oldss,olddslimit,oldsslimit,olddslimitw,oldsslimitw;
x86seg		gdt,ldt,idt,tr;
x86seg		_cs,_ds,_es,_ss,_fs,_gs;
x86seg		_oldds;
uint32_t	cr2, cr3, cr4;
uint32_t	dr[8];
int		timetolive = 0;

/* Also in 386_dynarec.c: */
extern cpu_state_t	cpu_state;
extern int		inscounts[256];
extern uint32_t	oxpc;
extern int		trap;
extern int		inttype;
extern int		optype;
extern int		cgate32;
extern uint16_t	rds;
extern uint16_t	ea_rseg;
extern uint32_t	*eal_r, *eal_w;
extern uint16_t	*mod1add[2][8];
extern uint32_t	*mod1seg[8];

extern uint32_t	rmdat32;
#define	rmdat	rmdat32
#define fetchdat rmdat32


#define fetch_ea_16(dat) \
		cpu_state.pc++; \
		cpu_mod = (dat >> 6) & 3; \
		cpu_reg = (dat >> 3) & 7; cpu_rm = dat & 7; \
		if (cpu_mod != 3) { \
			fetch_ea_16_long(dat); \
			if (cpu_state.abrt) return 0; \
		} 

#define fetch_ea_32(dat) \
		cpu_state.pc++; \
		cpu_mod = (dat >> 6) & 3; \
		cpu_reg = (dat >> 3) & 7; \
		cpu_rm = dat & 7; \
		if (cpu_mod != 3) { \
			fetch_ea_32_long(dat); \
		} \
		if (cpu_state.abrt) return 0

#include "x86_flags.h"

#define getbytef() ((uint8_t)(fetchdat)); cpu_state.pc++
#define getwordf() ((uint16_t)(fetchdat)); cpu_state.pc+=2
#define getbyte2f() ((uint8_t)(fetchdat>>8)); cpu_state.pc++
#define getword2f() ((uint16_t)(fetchdat>>8)); cpu_state.pc+=2

#undef NOTRM
#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG)) {\
                        x86_int(6); \
                        return 0; \
                }

#define OP_TABLE(name) ops_ ## name

#define CLOCK_CYCLES(c)		cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c)	cycles -= (c)


#include "x86_ops.h"


#undef NOTRM
#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG)) {\
                        x86_int(6); \
                        break; \
                }


void
exec386(int cycs)
{
    uint32_t addr;
    uint8_t temp;
    int cycdiff;
    int oldcyc;
    int tempi;

    cycles += cycs;
    while (cycles > 0) {
	int64_t cycle_period = (timer_count >> TIMER_SHIFT) + 1;

	x86_was_reset = 0;
	cycdiff = 0;
	oldcyc = cycles;
	timer_start_period(cycles << TIMER_SHIFT);

	while (cycdiff < cycle_period) {
		oldcs = CS;
		cpu_state.oldpc = cpu_state.pc;
		oldcpl = CPL;
		cpu_state.op32 = use32;

		x86_was_reset = 0;

		cpu_state.ea_seg = &_ds;
		cpu_state.ssegs = 0;

		fetchdat = fastreadl(cs + cpu_state.pc);

		if (! cpu_state.abrt) {               
			trap = flags & T_FLAG;
			opcode = fetchdat & 0xff;
			fetchdat >>= 8;

#if 0
			DEBUG("%04X(%06X):%04X:\n  %08X %08X %08X %08X\n  %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X\n  OPCODE=%02X FLAGS=%04X ins=%i (%08X)  ldt=%08X CPL=%i %i %02X %02X %02X   %02X %02X %f  %02X%02X %02X%02X %02X%02X  %02X\n",CS,cs,cpu_state.pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, pit.c[0], ram[0xB270+0x3F5], ram[0xB270+0x3F4], ram[0xB270+0x3F7], ram[0xB270+0x3F6], ram[0xB270+0x3F9], ram[0xB270+0x3F8], ram[0x4430+0x0D49]);
#endif

			cpu_state.pc++;

			x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
			if (x86_was_reset)
				break;
		}

		if (! use32) cpu_state.pc &= 0xffff;

		if (cpu_state.abrt) {
			flags_rebuild();
#if 0
			DEBUG("Abort\n");
			if (CS == 0x228)
				DEBUG("Abort at %04X:%04X - %i\n",
				    CS,cpu_state.pc,cpu_state.abrt);
#endif

			tempi = cpu_state.abrt;
			cpu_state.abrt = 0;
			x86_doabrt(tempi);
			if (cpu_state.abrt) {
				cpu_state.abrt = 0;
				CS = oldcs;
				cpu_state.pc = cpu_state.oldpc;
				INFO("CPU: double fault %i\n", ins);
				pmodeint(8, 0);
				if (cpu_state.abrt) {
					cpu_state.abrt = 0;
					cpu_reset(0);
					cpu_set_edx();
					INFO("CPU: triple fault - reset\n");
				}
			}
		}

		cycdiff = oldcyc - cycles;

		if (trap) {
			flags_rebuild();

			if (msw & 1) {
				pmodeint(1, 0);
			} else {
				writememw(ss, (SP-2) & 0xFFFF, flags);
				writememw(ss, (SP-4) & 0xFFFF, CS);
				writememw(ss, (SP-6) & 0xFFFF, cpu_state.pc);
				SP -= 6;
				addr = (1 << 2) + idt.base;
				flags &= ~I_FLAG;
				flags &= ~T_FLAG;
				cpu_state.pc = readmemw(0, addr);
				loadcs(readmemw(0, addr+2));
			}
		} else if (nmi && nmi_enable) {
			cpu_state.oldpc = cpu_state.pc;
			oldcs = CS;
			x86_int(2);
			nmi_enable = 0;
			if (nmi_auto_clear) {
				nmi_auto_clear = 0;
				nmi = 0;
			}
		} else if (flags & I_FLAG) {
			temp = pic_interrupt();
			if (temp != 0xFF) {
				flags_rebuild();

				if (msw & 1) {
					pmodeint(temp, 0);
				} else {
					writememw(ss, (SP-2) & 0xFFFF, flags);
					writememw(ss, (SP-4) & 0xFFFF, CS);
					writememw(ss, (SP-6) & 0xFFFF, cpu_state.pc);
					SP -= 6;
					addr = (temp << 2) + idt.base;
					flags &= ~I_FLAG;
					flags &= ~T_FLAG;
					oxpc = cpu_state.pc;
					cpu_state.pc = readmemw(0, addr);
					loadcs(readmemw(0, addr+2));
				}
			}
		}

		ins++;

		if (timetolive) {
			timetolive--;
			if (!timetolive)
				fatal("Life expired\n");
		}
	}
                
	tsc += cycdiff;
               
	timer_end_period(cycles << TIMER_SHIFT);
    }
}
