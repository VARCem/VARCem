/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		808x CPU emulation.
 *
 * Version:	@(#)808x.c	1.0.18	2019/05/07
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		Andrew Jenner (reenigne), <andrew@reenigne.org>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2016-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2015-2018 Andrew Jenner.
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
#include <math.h>
#include <wchar.h>
#include "../emu.h"
#include "cpu.h"
#include "x86.h"
#include "../io.h"
#include "../devices/system/pic.h"
#include "../devices/system/nmi.h"
#include "../mem.h"
#include "../rom.h"
#include "../timer.h"
#include "../plat.h"


/* Misc */
int		use32, stack32;
int		oldcpl;

/* The previous value of the CS segment register. */
uint16_t	oldcs;

/* The opcode of the instruction currently being executed. */
uint8_t		opcode;

/* The tables to speed up the setting of the Z, N, and P flags. */
uint8_t		znptable8[256];
uint16_t	znptable16[65536];

/* A 16-bit zero, needed because some speed-up arrays contain pointers to it. */
uint16_t	zero = 0;

/* MOD and R/M stuff. */
uint16_t	*mod1add[2][8];
uint32_t	*mod1seg[8];
int		rmdat;

/* XT CPU multiplier. */
int		xt_cpu_multi;

/* Is the CPU 8088 or 8086. */
int		is8086 = 0;

/* Variables for handling the non-maskable interrupts. */
int		nmi = 0, nmi_auto_clear = 0;
int		nmi_enable = 1;

/* Was the CPU ever reset? */
int		x86_was_reset = 0;

/* Variables used elsewhere in the emulator. */
int		tempc;

/* Number of instructions executed - used to calculate the % shown in the title bar. */
int		ins = 0;

/* Is the TRAP flag on? */
int		trap = 0;

/* The current effective address's segment. */
uint32_t	easeg;

/* The prefetch queue (4 bytes for 8088, 6 bytes for 8086). */
static uint8_t	pfq[6];

/* Variables to aid with the prefetch queue operation. */
static int	fetchcycles = 0,
		pfq_pos = 0;

/* The IP equivalent of the current prefetch queue position. */
static uint16_t pfq_ip;

/* Pointer tables needed for segment overrides. */
static uint32_t	*opseg[4];
static x86seg	*_opseg[4];

static int	takeint = 0, noint = 0;
static int	in_lock = 0, halt = 0;
static int	cpu_alu_op, pfq_size;

static uint16_t	cpu_src = 0, cpu_dest = 0;
static uint16_t	cpu_data = 0;

static uint32_t	*ovr_seg = NULL;


void
cpu_dumpregs(int force)
{
    static int indump = 0;
    char *seg_names[4] = { "ES", "CS", "SS", "DS" };
    int c;

    /* Only dump when needed, and only once.. */
    if (indump || (!force && !dump_on_exit))
	return;

    INFO("EIP=%08X CS=%04X DS=%04X ES=%04X SS=%04X FLAGS=%04X\n",
	      cpu_state.pc, CS, DS, ES, SS, flags);
    INFO("Old CS:EIP: %04X:%08X; %i ins\n", oldcs, cpu_state.oldpc, ins);
    for (c = 0; c < 4; c++) {
	INFO("%s : base=%06X limit=%08X access=%02X  low=%08X high=%08X\n",
		  seg_names[c], _opseg[c]->base, _opseg[c]->limit,
		  _opseg[c]->access, _opseg[c]->limit_low, _opseg[c]->limit_high);
    }
    if (is386) {
	INFO("FS : base=%06X limit=%08X access=%02X  low=%08X limit_high=%08X\n",
		  seg_fs, _fs.limit, _fs.access, _fs.limit_low, _fs.limit_high);
	INFO("GS : base=%06X limit=%08X access=%02X  low=%08X limit_high=%08X\n",
		  gs, _gs.limit, _gs.access, _gs.limit_low, _gs.limit_high);
	INFO("GDT : base=%06X limit=%04X\n", gdt.base, gdt.limit);
	INFO("LDT : base=%06X limit=%04X\n", ldt.base, ldt.limit);
	INFO("IDT : base=%06X limit=%04X\n", idt.base, idt.limit);
	INFO("TR  : base=%06X limit=%04X\n", tr.base, tr.limit);
	INFO("386 in %s mode: %i-bit data, %-i-bit stack\n",
		  (msw & 1) ? ((eflags & VM_FLAG) ? "V86" : "protected") : "real",
		  (use32) ? 32 : 16, (stack32) ? 32 : 16);
	INFO("CR0=%08X CR2=%08X CR3=%08X CR4=%08x\n", cr0, cr2, cr3, cr4);
	INFO("EAX=%08X EBX=%08X ECX=%08X EDX=%08X\nEDI=%08X ESI=%08X EBP=%08X ESP=%08X\n",
		  EAX, EBX, ECX, EDX, EDI, ESI, EBP, ESP);
    } else {
	INFO("808x/286 in %s mode\n", (msw & 1) ? "protected" : "real");
	INFO("AX=%04X BX=%04X CX=%04X DX=%04X DI=%04X SI=%04X BP=%04X SP=%04X\n",
		  AX, BX, CX, DX, DI, SI, BP, SP);
    }

#if 0
    INFO("Entries in readlookup : %i    writelookup : %i\n",
					readlnum, writelnum);
#endif

    x87_dumpregs();

    indump = 0;
}


/* Preparation of the various arrays needed to speed up the MOD and R/M work. */
static void
makemod1table(void)
{
    mod1add[0][0] = &BX;
    mod1add[0][1] = &BX;
    mod1add[0][2] = &BP;
    mod1add[0][3] = &BP;
    mod1add[0][4] = &SI;
    mod1add[0][5] = &DI;
    mod1add[0][6] = &BP;
    mod1add[0][7] = &BX;
    mod1add[1][0] = &SI;
    mod1add[1][1] = &DI;
    mod1add[1][2] = &SI;
    mod1add[1][3] = &DI;
    mod1add[1][4] = &zero;
    mod1add[1][5] = &zero;
    mod1add[1][6] = &zero;
    mod1add[1][7] = &zero;
    mod1seg[0] = &ds;
    mod1seg[1] = &ds;
    mod1seg[2] = &ss;
    mod1seg[3] = &ss;
    mod1seg[4] = &ds;
    mod1seg[5] = &ds;
    mod1seg[6] = &ss;
    mod1seg[7] = &ds;
    opseg[0] = &es;
    opseg[1] = &cs;
    opseg[2] = &ss;
    opseg[3] = &ds;
    _opseg[0] = &_es;
    _opseg[1] = &_cs;
    _opseg[2] = &_ss;
    _opseg[3] = &_ds;
}


/* Prepare the ZNP table needed to speed up the setting of the Z, N, and P flags. */
static void
makeznptable(void)
{
    int c, d;
    for (c = 0; c < 256; c++) {
	d = 0;
	if (c & 1)
		d++;
	if (c & 2)
		d++;
	if (c & 4)
		d++;
	if (c & 8)
		d++;
	if (c & 16)
		d++;
	if (c & 32)
		d++;
	if (c & 64)
		d++;
	if (c & 128)
		d++;
	if (d & 1)
		znptable8[c] = 0;
	else
		znptable8[c] = P_FLAG;
	if (c == 0xb1)
		DEBUG("znp8 b1 = %i %02X\n", d, znptable8[c]);
	if (c == 0x65b1)
		DEBUG("znp16 65b1 = %i %02X\n", d, znptable16[c]);
	if (!c)
		znptable8[c] |= Z_FLAG;
	if (c & 0x80)
		znptable8[c] |= N_FLAG;
    }

    for (c = 0; c < 65536; c++) {
	d = 0;
	if (c & 1)
		d++;
	if (c & 2)
		d++;
	if (c & 4)
		d++;
	if (c & 8)
		d++;
	if (c & 16)
		d++;
	if (c & 32)
		d++;
	if (c & 64)
		d++;
	if (c & 128)
		d++;
	if (d & 1)
		znptable16[c] = 0;
	else
		znptable16[c] = P_FLAG;
	if (c == 0xb1)
		DEBUG("znp16 b1 = %i %02X\n", d, znptable16[c]);
	if (c == 0x65b1)
		DEBUG("znp16 65b1 = %i %02X\n", d, znptable16[c]);
	if (!c)
		znptable16[c] |= Z_FLAG;
	if (c & 0x8000)
		znptable16[c] |= N_FLAG;
    }
}


/* Fetches the effective address from the prefetch queue according to MOD and R/M. */
static void	pfq_add(int c);
static void	set_pzs(int bits);


static int
irq_pending(void)
{
    if ((nmi && nmi_enable && nmi_mask) ||
	((flags & I_FLAG) && (pic.pend & ~pic.mask) && !noint))
	return 1;

    return 0;
}


static void
cpu_wait(int c, int bus)
{
    cycles -= c;

    if (! bus)
	pfq_add(c);
}


#undef readmemb
#undef readmemw


/* Common read function. */
static uint8_t
readmemb_common(uint32_t a)
{
    uint8_t ret;

    if (readlookup2 == NULL)
	ret = readmembl(a);
    else {
	if (readlookup2[(a) >> 12] == ((uintptr_t) -1))
		ret = readmembl(a);
	else
		ret = *(uint8_t *)(readlookup2[(a) >> 12] + (a));
    }

    return ret;
}


/* Reads a byte from the memory and accounts for memory transfer cycles to
   subtract from the cycles to use for adding to the prefetch queue. */
static uint8_t
readmemb(uint32_t a)
{
    uint8_t ret;

    cpu_wait(4, 1);
    ret = readmemb_common(a);

    return ret;
}


/* Reads a byte from the memory but does not accounts for memory transfer
   cycles to subtract from the cycles to use for adding to the prefetch
   queue. */
static uint8_t
readmembf(uint32_t a)
{
    uint8_t ret;

    a = cs + (a & 0xffff);

    ret = readmemb_common(a);

    return ret;
}


/* Reads a word from the memory and accounts for memory transfer cycles to
   subtract from the cycles to use for adding to the prefetch queue. */
static uint16_t
readmemw_common(uint32_t s, uint16_t a)
{
    uint16_t ret;

    ret = readmemb_common(s + a);
    ret |= readmemb_common(s + ((a + 1) & 0xffff)) << 8;

    return ret;
}


static uint16_t
readmemw(uint32_t s, uint16_t a)
{
    uint16_t ret;

    if (is8086 && !(a & 1))
	cpu_wait(4, 1);
    else
	cpu_wait(8, 1);
    ret = readmemw_common(s, a);

    return ret;
}


static uint16_t
readmemwf(uint16_t a)
{
    uint16_t ret;

    ret = readmemw_common(cs, a & 0xffff);

    return ret;
}


/* Writes a byte from the memory and accounts for memory transfer cycles to
   subtract from the cycles to use for adding to the prefetch queue. */
static void
writememb_common(uint32_t a, uint8_t v)
{
    if (writelookup2 == NULL)
	writemembl(a, v);
    else {
	if (writelookup2[(a) >> 12] == ((uintptr_t) -1))
		writemembl(a, v);
	else
		*(uint8_t *)(writelookup2[a >> 12] + a) = v;
    }
}


static void
writememb(uint32_t a, uint8_t v)
{
    cpu_wait(4, 1);

    writememb_common(a, v);
}


/* Writes a word from the memory and accounts for memory transfer cycles to
   subtract from the cycles to use for adding to the prefetch queue. */
static void
writememw(uint32_t s, uint32_t a, uint16_t v)
{
    if (is8086 && !(a & 1))
	cpu_wait(4, 1);
    else
	cpu_wait(8, 1);

    writememb_common(s + a, v & 0xff);
    writememb_common(s + ((a + 1) & 0xffff), v >> 8);
}


static void
pfq_write(void)
{
    uint16_t tempw;

    /* On 8086 and even IP, fetch *TWO* bytes at once. */
    if (pfq_pos < pfq_size) {
	/* If we're filling the last byte of the prefetch queue, do *NOT*
	   read more than one byte even on the 8086. */
	if (is8086 && !(pfq_ip & 1) && !(pfq_pos & 1)) {
		tempw = readmemwf(pfq_ip);
		pfq[pfq_pos++] = (tempw & 0xff);
		pfq[pfq_pos++] = (tempw >> 8);
		pfq_ip += 2;
    	} else {
		pfq[pfq_pos] = readmembf(pfq_ip);
		pfq_ip++;
		pfq_pos++;
	}
    }
}


static uint8_t
pfq_read(void)
{
    uint8_t temp, i;

    temp = pfq[0];

    for (i = 0; i < (pfq_size - 1); i++)
	pfq[i] = pfq[i + 1];
    pfq_pos--;

    cpu_state.pc++;

    return temp;
}


/* Fetches a byte from the prefetch queue, or from memory if the queue has
   been drained. */
static uint8_t
pfq_fetchb(void)
{
    uint8_t temp;

    if (pfq_pos == 0) {
	/* Extra cycles due to having to fetch on read. */
	cpu_wait(4 - (fetchcycles & 3), 1);
	fetchcycles = 4;
	/* Reset prefetch queue internal position. */
	pfq_ip = cpu_state.pc;
	/* Fill the queue. */
	pfq_write();
    } else
	fetchcycles -= 4;

    /* Fetch. */
    temp = pfq_read();
    cpu_wait(1, 0);

    return temp;
}


/* Fetches a word from the prefetch queue, or from memory if the queue has
   been drained. */
static uint16_t
pfq_fetchw(void)
{
    uint8_t temp = pfq_fetchb();
    return temp | (pfq_fetchb() << 8);
}


/* Adds bytes to the prefetch queue based on the instruction's cycle count. */
static void
pfq_add(int c)
{
    int d;
    if (c < 0)
	return;
    if (pfq_pos >= pfq_size)
	return;
    d = c + (fetchcycles & 3);
    while ((d > 3) && (pfq_pos < pfq_size)) {
	d -= 4;
	pfq_write();
    }
    fetchcycles += c;
    if (fetchcycles > 16)
	fetchcycles = 16;
}


/* Clear the prefetch queue - called on reset and on anything that affects either CS or IP. */
static void
pfq_clear(void)
{
    pfq_ip = cpu_state.pc;
    pfq_pos = 0;
}


static void
do_mod_rm(void)
{
    rmdat = pfq_fetchb();
    cpu_reg = (rmdat >> 3) & 7;
    cpu_mod = (rmdat >> 6) & 3;
    cpu_rm = rmdat & 7;

    if (cpu_mod == 3)
	return;

    cpu_wait(3, 0);

    if (!cpu_mod && (cpu_rm == 6)) {
	cpu_wait(2, 0);
	cpu_state.eaaddr = pfq_fetchw();
	easeg = ds;
	cpu_wait(1, 0);
    } else {
	switch (cpu_rm) {
		case 0:
		case 3:
			cpu_wait(2, 0);
			break;
		case 1:
		case 2:
			cpu_wait(3, 0);
			break;
	}

	cpu_state.eaaddr = (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
	easeg = *mod1seg[cpu_rm];

	switch (cpu_mod) {
		case 1:
			cpu_wait(4, 0);
			cpu_state.eaaddr += (uint16_t) (int8_t) pfq_fetchb();
			break;
		case 2:
			cpu_wait(4, 0);
			cpu_state.eaaddr += pfq_fetchw();
			break;
	}
	cpu_wait(2, 0);
    }

    cpu_state.eaaddr &= 0xffff;
    cpu_state.last_ea = cpu_state.eaaddr;

    if (ovr_seg)
	easeg = *ovr_seg;
}


#undef getr8
#define getr8(r)   ((r & 4) ? cpu_state.regs[r & 3].b.h : cpu_state.regs[r & 3].b.l)

#undef setr8
#define setr8(r,v) if (r & 4) cpu_state.regs[r & 3].b.h = v; \
                   else       cpu_state.regs[r & 3].b.l = v;


/* Reads a byte from the effective address. */
static uint8_t
geteab(void)
{
    if (cpu_mod == 3) {
	return (getr8(cpu_rm));
    }

    return readmemb(easeg + cpu_state.eaaddr);
}


/* Reads a word from the effective address. */
static uint16_t
geteaw(void)
{
    if (cpu_mod == 3)
	return cpu_state.regs[cpu_rm].w;

    return readmemw(easeg, cpu_state.eaaddr);
}


static void
read_ea(int memory_only, int bits)
{
    if (cpu_mod != 3) {
	if (bits == 16)
		cpu_data = readmemw(easeg, cpu_state.eaaddr);
	else
		cpu_data = readmemb(easeg + cpu_state.eaaddr);
	return;
    }

    if (! memory_only) {
	if (bits == 8) {
		cpu_data = getr8(cpu_rm);
	} else
		cpu_data = cpu_state.regs[cpu_rm].w;
    }
}


static void
read_ea2(int bits)
{
    if (bits == 16)
	cpu_data = readmemw(easeg, (cpu_state.eaaddr + 2) & 0xffff);
    else
	cpu_data = readmemb(easeg + ((cpu_state.eaaddr + 2) & 0xffff));
}


/* Writes a byte to the effective address. */
static void
seteab(uint8_t val)
{
    if (cpu_mod == 3) {
	setr8(cpu_rm, val);
    } else
	writememb(easeg + cpu_state.eaaddr, val);
}


/* Writes a word to the effective address. */
static void
seteaw(uint16_t val)
{
    if (cpu_mod == 3)
	cpu_state.regs[cpu_rm].w = val;
    else
	writememw(easeg, cpu_state.eaaddr, val);
}


/* Pushes a word to the stack. */
static void
do_push_ex(uint16_t val)
{
    writememw(ss, (SP & 0xffff), val);
    cpu_state.last_ea = SP;
}


static void
do_push(uint16_t val)
{
    SP -= 2;

    do_push_ex(val);
}


/* Pops a word from the stack. */
static uint16_t
do_pop(void)
{
    uint16_t tempw;

    tempw = readmemw(ss, SP);
    SP += 2;
    cpu_state.last_ea = SP;

    return tempw;
}


static void
do_access(int num, int bits)
{
    switch (num) {
	case 0: case 61: case 63: case 64:
	case 67: case 69: case 71: case 72:
	default:
		break;

	case 1: case 6: case 8: case 9:
	case 17: case 20: case 21: case 24:
	case 28: case 55: case 56:
		cpu_wait(1 + (cycles % 3), 0);
		break;

	case 2: case 15: case 22: case 23:
	case 25: case 26: case 46: case 53:
		cpu_wait(2 + (cycles % 3), 0);
		break;

	case 3: case 44: case 45: case 52:
	case 54:
		cpu_wait(2 + (cycles & 1), 0);
		break;

	case 4:
		cpu_wait(5 + (cycles & 1), 0);
		break;

	case 5:
		if (opcode == 0xcc)
			cpu_wait(7 + (cycles % 3), 0);
		else
			cpu_wait(4 + (cycles & 1), 0);
		break;

	case 7: case 47: case 48: case 49:
	case 50: case 51:
		cpu_wait(1 + (cycles % 4), 0);
		break;

	case 10: case 11: case 18: case 19:
	case 43:
		cpu_wait(3 + (cycles % 3), 0);
		break;

	case 12: case 13: case 14: case 29:
	case 30: case 33:
		cpu_wait(4 + (cycles % 3), 0);
		break;

	case 16:
		if (!(opcode & 1) && (cycles & 1))
			cpu_wait(1, 0);
		/*FALLTHROUGH*/

	case 42:
		cpu_wait(3 + (cycles & 1), 0);
		break;

	case 27: case 32: case 37:
		cpu_wait(3, 0);
		break;

	case 31:
		cpu_wait(6 + (cycles % 3), 0);
		break;

	case 34: case 39: case 41: case 60:
		cpu_wait(4, 0);
		break;

	case 35:
		cpu_wait(2, 0);
		break;

	case 36:
		cpu_wait(5 + (cycles & 1), 0);
		if (cpu_mod != 3)
			cpu_wait(1, 0);
		break;

	case 38:
		cpu_wait(5 + (cycles % 3), 0);
		break;

	case 40:
		cpu_wait(6, 0);
		break;

	case 57:
		if (cpu_mod != 3)
			cpu_wait(2, 0);
		cpu_wait(4 + (cycles & 1), 0);
		break;

	case 58:
		if (cpu_mod != 3)
			cpu_wait(1, 0);
		cpu_wait(4 + (cycles & 1), 0);
		break;

	case 59:
		if (cpu_mod != 3)
			cpu_wait(1, 0);
		cpu_wait(5 + (cycles & 1), 0);
		break;

	case 62:
		cpu_wait(1, 0);
		break;

	case 65:
		cpu_wait(3 + (cycles & 1), 0);
		if (cpu_mod != 3)
			cpu_wait(1, 0);
		break;

	case 70:
		cpu_wait(5, 0);
		break;
    }
}


/* Calls an interrupt. */
static void
do_intr(uint16_t addr, int cli)
{
    uint16_t old_cs, old_ip;
    uint16_t new_cs, new_ip;

    addr <<= 2;
    old_cs = CS;
    do_access(5, 16);
    new_ip = readmemw(0, addr);
    cpu_wait(1, 0);
    do_access(6, 16);
    new_cs = readmemw(0, (addr + 2) & 0xffff);
    do_access(39, 16);
    do_push(flags & 0x0fd7);
    if (cli)
	flags &= ~I_FLAG;
    flags &= ~T_FLAG;
    do_access(40, 16);
    do_push(old_cs);
    old_ip = cpu_state.pc;
    loadcs(new_cs);
    do_access(68, 16);
    cpu_state.pc = new_ip;
    do_access(41, 16);
    do_push(old_ip);
    pfq_clear();
}


static int
rep_action(int *completed, int *repeating, int in_rep, int bits)
{
    uint16_t t;

    if (in_rep == 0)
	return 0;

    cpu_wait(2, 0);
    t = CX;
    if (irq_pending()) {
	do_access(71, bits);
	pfq_clear();
	cpu_state.pc = cpu_state.pc - 2;
	t = 0;
    }

    if (t == 0) {
	cpu_wait(1, 0);
	*completed = 1;
	*repeating = 0;
	return 1;
    }

    --CX;
    *completed = 0;

    cpu_wait(2, 0);
    if (! *repeating)
	cpu_wait(2, 0);

    return 0;
}


static uint16_t
do_jump(uint16_t delta)
{
    uint16_t old_ip;

    do_access(67, 8);
    cpu_wait(5, 0);
    old_ip = cpu_state.pc;
    cpu_state.pc = (cpu_state.pc + delta) & 0xffff;
    pfq_clear();

    return old_ip;
}


static uint16_t
sign_extend(uint8_t data)
{
    return data + (data < 0x80 ? 0 : 0xff00);
}


static void
do_jump_short(void)
{
    do_jump(sign_extend((uint8_t) cpu_data));
}


static uint16_t
do_jump_near(void)
{
    return do_jump(pfq_fetchw());
}


/* Performs a conditional do_jump. */
static void
do_jcc(uint8_t opcode, int cond)
{
    /* int8_t offset; */

    cpu_wait(1, 0);
    cpu_data = pfq_fetchb();
    cpu_wait(1, 0);

    if ((! cond) == (opcode & 0x01))
	do_jump_short();
}


static void
set_cf(int cond)
{
    flags = (flags & ~C_FLAG) | (cond ? C_FLAG : 0);
}


static void
set_if(int cond)
{
    flags = (flags & ~I_FLAG) | (cond ? I_FLAG : 0);
}


static void
set_df(int cond)
{
    flags = (flags & ~D_FLAG) | (cond ? D_FLAG : 0);
}


static void
bitwise(int bits, uint16_t data)
{
    cpu_data = data;
    flags &= ~(C_FLAG | A_FLAG | V_FLAG);
    set_pzs(bits);
}


static void
do_test(int bits, uint16_t dest, uint16_t src)
{
    cpu_dest = dest;
    cpu_src = src;
    bitwise(bits, (cpu_dest & cpu_src));
}


static void
set_of(int of)
{
    flags = (flags & ~0x800) | (of ? 0x800 : 0);
}


static int
top_bit(uint16_t w, int bits)
{
    if (bits == 16)
	return ((w & 0x8000) != 0);
    else
	return ((w & 0x80) != 0);
}


static void
set_of_add(int bits)
{
    set_of(top_bit((cpu_data ^ cpu_src) & (cpu_data ^ cpu_dest), bits));
}


static void
set_of_sub(int bits)
{
    set_of(top_bit((cpu_dest ^ cpu_src) & (cpu_data ^ cpu_dest), bits));
}


static void
set_af(int af)
{
    flags = (flags & ~0x10) | (af ? 0x10 : 0);
}


static void
do_af(void)
{
    set_af(((cpu_data ^ cpu_src ^ cpu_dest) & 0x10) != 0);
}


static void
set_apzs(int bits)
{
    set_pzs(bits);
    do_af();
}


static void
do_add(int bits)
{
    int size_mask = (1 << bits) - 1;

    cpu_data = cpu_dest + cpu_src;
    set_apzs(bits);
    set_of_add(bits);

    /* Anything - FF with carry on is basically anything + 0x100: value stays
       unchanged but carry goes on. */
    if ((cpu_alu_op == 2) && !(cpu_src & size_mask) && (flags & C_FLAG))
	flags |= C_FLAG;
    else
	set_cf((cpu_src & size_mask) > (cpu_data & size_mask));
}


static void
do_sub(int bits)
{
    int size_mask = (1 << bits) - 1;

    cpu_data = cpu_dest - cpu_src;
    set_apzs(bits);
    set_of_sub(bits);

    /* Anything - FF with carry on is basically anything - 0x100: value stays
       unchanged but carry goes on. */
    if ((cpu_alu_op == 3) && !(cpu_src & size_mask) && (flags & C_FLAG))
	flags |= C_FLAG;
    else
	set_cf((cpu_src & size_mask) > (cpu_dest & size_mask));
}


static void
alu_op(int bits)
{
    switch(cpu_alu_op) {
	case 1:
		bitwise(bits, (cpu_dest | cpu_src));
		break;

	case 2:
		if (flags & C_FLAG)
			cpu_src++;
		/*FALLTHROUGH*/

	case 0:
		do_add(bits);
		break;

	case 3:
		if (flags & C_FLAG)
			cpu_src++;
		/*FALLTHROUGH*/

	case 5:
	case 7:
		do_sub(bits);
		break;

	case 4:
		do_test(bits, cpu_dest, cpu_src);
		break;

	case 6:
		bitwise(bits, (cpu_dest ^ cpu_src));
		break;
    }
}


static void
set_sf(int bits)
{
    flags = (flags & ~0x80) | (top_bit(cpu_data, bits) ? 0x80 : 0);
}


static void
set_pf(void)
{
    static const uint8_t table[0x100] = {
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
	4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4};

    flags = (flags & ~4) | table[cpu_data & 0xff];
}


static void
do_mul(uint16_t a, uint16_t b)
{
    int negate = 0;
    int bit_count = 8;
    int carry, i;
    uint16_t high_bit = 0x80;
    uint16_t size_mask;
    uint16_t c, r;

    size_mask = (1 << bit_count) - 1;

    if (opcode != 0xd5) {
	if (opcode & 1) {
		bit_count = 16;
		high_bit = 0x8000;
	} else
		cpu_wait(8, 0);

	size_mask = (1 << bit_count) - 1;

	if ((rmdat & 0x38) == 0x28) {
		if (! top_bit(a, bit_count)) {
			if (top_bit(b, bit_count)) {
				cpu_wait(1, 0);
				if ((b & size_mask) != ((opcode & 1) ? 0x8000 : 0x80))
					cpu_wait(1, 0);
				b = ~b + 1;
				negate = 1;
			}
		} else {
			cpu_wait(1, 0);
			a = ~a + 1;
			negate = 1;
			if (top_bit(b, bit_count)) {
				b = ~b + 1;
				negate = 0;
			} else
				cpu_wait(4, 0);
		}
		cpu_wait(10, 0);
	}
	cpu_wait(3, 0);
    }

    c = 0;
    a &= size_mask;
    carry = (a & 1) != 0;
    a >>= 1;
    for (i = 0; i < bit_count; ++i) {
	cpu_wait(7, 0);
	if (carry) {
		cpu_src = c;
		cpu_dest = b;
		do_add(bit_count);
		c = cpu_data & size_mask;
		cpu_wait(1, 0);
		carry = !!(flags & C_FLAG);
	}
	r = (c >> 1) + (carry ? high_bit : 0);
	carry = (c & 1) != 0;
	c = r;
	r = (a >> 1) + (carry ?  high_bit : 0);
	carry = (a & 1) != 0;
	a = r;
    }

    if (negate) {
	c = ~c;
	a = (~a + 1) & size_mask;
	if (a == 0)
		++c;
	cpu_wait(9, 0);
    }
    cpu_data = a;
    cpu_dest = c;

    set_sf(bit_count);
    set_pf();
}


static void
set_of_rotate(int bits)
{
    set_of(top_bit(cpu_data ^ cpu_dest, bits));
}


static void
set_zf(int bits)
{
    int size_mask = (1 << bits) - 1;

    flags = (flags & ~0x40) | (((cpu_data & size_mask) == 0) ? 0x40 : 0);
}


static void
set_pzs(int bits)
{
    set_pf();
    set_zf(bits);
    set_sf(bits);
}


static void
set_co_do_mul(int carry)
{
    set_cf(carry);
    set_of(carry);
    if (!carry)
	cpu_wait(1, 0);
}


static int
do_div(uint16_t l, uint16_t h)
{
    int b, bit_count = 8;
    int negative = 0;
    int do_dividend_negative = 0;
    int size_mask, carry;
    uint16_t r;

    if (opcode & 1) {
	l = AX;
	h = DX;
	bit_count = 16;
    }

    size_mask = (1 << bit_count) - 1;

    if (opcode != 0xd4) {
	if ((rmdat & 0x38) == 0x38) {
		if (top_bit(h, bit_count)) {
			h = ~h;
			l = (~l + 1) & size_mask;
			if (l == 0)
				++h;
			h &= size_mask;
			negative = 1;
			do_dividend_negative = 1;
			cpu_wait(4, 0);
		}
		if (top_bit(cpu_src, bit_count)) {
			cpu_src = ~cpu_src + 1;
			negative = !negative;
		} else
			cpu_wait(1, 0);
		cpu_wait(9, 0);
	}
	cpu_wait(3, 0);
    }
    cpu_wait(8, 0);
    cpu_src &= size_mask;
    if (h >= cpu_src) {
	if (opcode != 0xd4)
		cpu_wait(1, 0);
	do_intr(0, 1);
	return 0;
    }
    if (opcode != 0xd4)
	cpu_wait(1, 0);
    cpu_wait(2, 0);
    carry = 1;
    for (b = 0; b < bit_count; ++b) {
	r = (l << 1) + (carry ? 1 : 0);
	carry = top_bit(l, bit_count);
	l = r;
	r = (h << 1) + (carry ? 1 : 0);
	carry = top_bit(h, bit_count);
	h = r;
	cpu_wait(8, 0);
	if (carry) {
		carry = 0;
		h -= cpu_src;
		if (b == bit_count - 1)
			cpu_wait(2, 0);
	} else {
		carry = cpu_src > h;
		if (!carry) {
			h -= cpu_src;
			cpu_wait(1, 0);
			if (b == bit_count - 1)
				cpu_wait(2, 0);
		}
	}
    }
    l = ~((l << 1) + (carry ? 1 : 0));
    if (opcode != 0xd4 && (rmdat & 0x38) == 0x38) {
	cpu_wait(4, 0);
	if (top_bit(l, bit_count)) {
		if (cpu_mod == 3)
			cpu_wait(1, 0);
		do_intr(0, 1);
		return 0;
	}
	cpu_wait(7, 0);
	if (negative)
		l = ~l + 1;
	if (do_dividend_negative)
		h = ~h + 1;
    }
    if (opcode == 0xd4) {
	AL = h & 0xff;
	AH = l & 0xff;
    } else {
	AH = h & 0xff;
	AL = l & 0xff;
	if (opcode & 1) {
		DX = h;
		AX = l;
	}
    }
    return 1;
}


static void
do_lods(int bits)
{
    if (bits == 16)
	cpu_data = readmemw((ovr_seg ? *ovr_seg : ds), SI);
    else
	cpu_data = readmemb((ovr_seg ? *ovr_seg : ds) + SI);
    if (flags & D_FLAG)
	SI -= (bits >> 3);
    else
	SI += (bits >> 3);
}


static void
do_stos(int bits)
{
    if (bits == 16)
	writememw(es, DI, cpu_data);
    else
	writememb(es + DI, (uint8_t)(cpu_data & 0xff));
    if (flags & D_FLAG)
	DI -= (bits >> 3);
    else
	DI += (bits >> 3);
}


static void
do_da(void)
{
    set_pzs(8);

    cpu_wait(2, 0);
}


static void
do_aa(void)
{
    set_of(0);
    AL &= 0x0f;

    cpu_wait(6, 0);
}


static void
set_ca(void)
{
    set_cf(1);
    set_af(1);
}


static void
clear_ca(void)
{
    set_cf(0);
    set_af(0);
}


/* Executes instructions up to the specified number of cycles. */
void
execx86(int cycs)
{
    uint8_t temp = 0, temp2;
    uint16_t addr, tempw;
    uint16_t new_cs, new_ip;
    int bits, completed;
    int in_rep, repeating;
    int oldc;

    cycles += cycs;

    while (cycles > 0) {
	timer_start_period(cycles * xt_cpu_multi);
	cpu_state.oldpc = cpu_state.pc;
	in_rep = repeating = 0;
	completed = 0;

opcodestart:
	if (halt) {
		cpu_wait(2, 0);
		goto on_halt;
	}

	if (! repeating) {
		opcode = pfq_fetchb();
		oldc = flags & C_FLAG;
		trap = flags & T_FLAG;
		cpu_wait(1, 0);

#if 0
		if (!in_rep && !ovr_seg && (CS < 0xf000))
			DEBUG("%04X:%04X %02X\n",
			      CS, (cpu_state.pc - 1) & 0xffff, opcode);
#endif
	}

	switch (opcode) {
		case 0x06:	/* PUSH seg */
		case 0x0e:
		case 0x16:
		case 0x1e:
			do_access(29, 16);
			do_push(_opseg[(opcode >> 3) & 0x03]->seg);
			break;

		case 0x07:	/* POP seg */
		case 0x0f:
		case 0x17:
		case 0x1f:
			do_access(22, 16);
			if (opcode == 0x0f) {
				loadcs(do_pop());
				pfq_clear();
			} else
				loadseg(do_pop(), _opseg[(opcode >> 3) & 0x03]);
			cpu_wait(1, 0);
			noint = 1;
			break;

		case 0x26:	/* ES: */
		case 0x2e:	/* CS: */
		case 0x36:	/* SS: */
		case 0x3e:	/* DS: */
			cpu_wait(1, 0);
			ovr_seg = opseg[(opcode >> 3) & 0x03];
			goto opcodestart;

		case 0x00: case 0x01: case 0x02: case 0x03: /* alu rm, r / r, rm */
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x18: case 0x19: case 0x1a: case 0x1b:
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x28: case 0x29: case 0x2a: case 0x2b:
		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x38: case 0x39: case 0x3a: case 0x3b:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(46, bits);
			if (opcode & 1)
				tempw = geteaw();
			else
				tempw = geteab();
			cpu_alu_op = (opcode >> 3) & 7;
			if ((opcode & 2) == 0) {
				cpu_dest = tempw;
				cpu_src = (opcode & 1) ? cpu_state.regs[cpu_reg].w : getr8(cpu_reg);
			} else {
				cpu_dest = (opcode & 1) ? cpu_state.regs[cpu_reg].w : getr8(cpu_reg);
				cpu_src = tempw;
			}
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			cpu_wait(1, 0);
			alu_op(bits);
			if (cpu_alu_op != 7) {
				if (opcode & 2) {
					if (opcode & 1)
						cpu_state.regs[cpu_reg].w = cpu_data;
					else
						setr8(cpu_reg, (uint8_t)(cpu_data & 0xff));
					cpu_wait(1, 0);
				} else {
					do_access(10, bits);
					if (opcode & 1)
						seteaw(cpu_data);
					else
						seteab((uint8_t) (cpu_data & 0xff));
					if (cpu_mod == 3)
						cpu_wait(1, 0);
				}
			} else
				cpu_wait(1, 0);
			break;

		case 0x04: case 0x05: case 0x0c: case 0x0d: /* alu A, imm */
		case 0x14: case 0x15: case 0x1c: case 0x1d:
		case 0x24: case 0x25: case 0x2c: case 0x2d:
		case 0x34: case 0x35: case 0x3c: case 0x3d:
			bits = 8 << (opcode & 1);
			cpu_wait(1, 0);
			if (opcode & 1) {
				cpu_data = pfq_fetchw();
				cpu_dest = AX;
			} else {
				cpu_data = pfq_fetchb();
				cpu_dest = AL;
			}
			cpu_src = cpu_data;
			cpu_alu_op = (opcode >> 3) & 7;
			alu_op(bits);
			if (cpu_alu_op != 7) {
				if (opcode & 1)
					AX = cpu_data;
				else
					AL = cpu_data & 0xff;
			}
			cpu_wait(1, 0);
			break;

		case 0x27:	/* DAA */
			cpu_wait(1, 0);
			if ((flags & A_FLAG) || (AL & 0x0f) > 9) {
				cpu_data = AL + 6;
				AL = (uint8_t)cpu_data;
				set_af(1);
				if ((cpu_data & 0x0100) != 0)
					set_cf(1);
			}
			if ((flags & C_FLAG) || AL > 0x9f) {
				AL += 0x60;
				set_cf(1);
			}
			do_da();
			break;

		case 0x2f:	/* DAS */
			cpu_wait(1, 0);
			temp = AL;
			if ((flags & A_FLAG) || ((AL & 0x0f) > 9)) {
				cpu_data = AL - 6;
				AL = (uint8_t)cpu_data;
				set_af(1);
				if ((cpu_data & 0x0100) != 0)
					set_cf(1);
			}
			if ((flags & C_FLAG) || temp > 0x9f) {
				AL -= 0x60;
				set_cf(1);
			}
			do_da();
			break;

		case 0x37:	/* AAA */
			cpu_wait(1, 0);
			if ((flags & A_FLAG) || ((AL & 0x0f) > 9)) {
				AL += 6;
				AH++;
				set_ca();
			} else {
				clear_ca();
				cpu_wait(1, 0);
			}
			do_aa();
			break;

		case 0x3f:	/* AAS */
			cpu_wait(1, 0);
			if ((flags & A_FLAG) || ((AL & 0x0f) > 9)) {
				AL -= 6;
				AH--;
				set_ca();
			} else {
				clear_ca();
				cpu_wait(1, 0);
			}
			do_aa();
			break;

		case 0x40: case 0x41: case 0x42: case 0x43: /* INCDEC rw */
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			cpu_wait(1, 0);
			cpu_dest = cpu_state.regs[opcode & 7].w;
			cpu_src = 1;
			bits = 16;
			if ((opcode & 8) == 0) {
				cpu_data = cpu_dest + cpu_src;
				set_of_add(bits);
			} else {
				cpu_data = cpu_dest - cpu_src;
				set_of_sub(bits);
			}
			do_af();
			set_pzs(16);
			cpu_state.regs[opcode & 7].w = cpu_data;
			break;

		case 0x50: case 0x51: case 0x52: case 0x53:	/* PUSH r16 */
		case 0x54: case 0x55: case 0x56: case 0x57:
			do_access(30, 16);
			if (opcode == 0x54) {
				SP -= 2;
				do_push_ex(cpu_state.regs[opcode & 0x07].w);
			} else
				do_push(cpu_state.regs[opcode & 0x07].w);
			break;

		case 0x58: case 0x59: case 0x5a: case 0x5b:	/* POP r16 */
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			do_access(23, 16);
			cpu_state.regs[opcode & 0x07].w = do_pop();
			cpu_wait(1, 0);
			break;

		case 0x60:	/* JO alias */
		case 0x70:	/* JO */
		case 0x61:	/* JNO alias */
		case 0x71:	/* JNO */
			do_jcc(opcode, flags & V_FLAG);
			break;

		case 0x62:	/* JB alias */
		case 0x72:	/* JB */
		case 0x63:	/* JNB alias */
		case 0x73:	/* JNB */
			do_jcc(opcode, flags & C_FLAG);
			break;

		case 0x64:	/* JE alias */
		case 0x74:	/* JE */
		case 0x65:	/* JNE alias */
		case 0x75:	/* JNE */
			do_jcc(opcode, flags & Z_FLAG);
			break;

		case 0x66:	/* JBE alias */
		case 0x76:	/* JBE */
		case 0x67:	/* JNBE alias */
		case 0x77:	/* JNBE */
			do_jcc(opcode, flags & (C_FLAG | Z_FLAG));
			break;

		case 0x68:	/* JS alias */
		case 0x78:	/* JS */
		case 0x69:	/* JNS alias */
		case 0x79:	/* JNS */
			do_jcc(opcode, flags & N_FLAG);
			break;

		case 0x6a:	/* JP alias */
		case 0x7a:	/* JP */
		case 0x6b:	/* JNP alias */
		case 0x7b:	/* JNP */
			do_jcc(opcode, flags & P_FLAG);
			break;

		case 0x6c:	/* JL alias */
		case 0x7c:	/* JL */
		case 0x6d:	/* JNL alias */
		case 0x7d:	/* JNL */
			temp = (flags & N_FLAG) ? 1 : 0;
			temp2 = (flags & V_FLAG) ? 1 : 0;
			do_jcc(opcode, temp ^ temp2);
			break;

		case 0x6e:	/* JLE alias */
		case 0x7e:	/* JLE */
		case 0x6f:	/* JNLE alias */
		case 0x7f:	/* JNLE */
			temp = (flags & N_FLAG) ? 1 : 0;
			temp2 = (flags & V_FLAG) ? 1 : 0;
			do_jcc(opcode, (flags & Z_FLAG) || (temp != temp2));
			break;

		case 0x80: case 0x81: case 0x82: case 0x83: /* alu rm, imm */
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(47, bits);
			if (opcode & 1)
				cpu_data = geteaw();
			else
				cpu_data = geteab();
			cpu_dest = cpu_data;
			if (cpu_mod != 3)
				cpu_wait(3, 0);
			if (opcode == 0x81) {
				if (cpu_mod == 3)
					cpu_wait(1, 0);
				cpu_src = pfq_fetchw();
			} else {
				if (cpu_mod == 3)
					cpu_wait(1, 0);
				if (opcode == 0x83)
					cpu_src = sign_extend(pfq_fetchb());
				else
					cpu_src = pfq_fetchb() | 0xff00;
			}
			cpu_wait(1, 0);
			cpu_alu_op = (rmdat & 0x38) >> 3;
			alu_op(bits);
			if (cpu_alu_op != 7) {
				do_access(11, bits);
				if (opcode & 1)
					seteaw(cpu_data);
				else
					seteab((uint8_t)(cpu_data & 0xff));
			} else {
				if (cpu_mod != 3)
					cpu_wait(1, 0);
			}
			break;

		case 0x84:	/* TEST rm, reg */
		case 0x85:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(48, bits);
			if (opcode & 1) {
				cpu_data = geteaw();
				do_test(bits, cpu_data, cpu_state.regs[cpu_reg].w);
			} else {
				cpu_data = geteab();
				do_test(bits, cpu_data, getr8(cpu_reg));
			}
			if (cpu_mod == 3)
				cpu_wait(2, 0);
			cpu_wait(2, 0);
			break;

		case 0x86:	/* XCHG rm, reg */
		case 0x87:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(49, bits);
			if (opcode & 1) {
				cpu_data = geteaw();
				cpu_src = cpu_state.regs[cpu_reg].w;
				cpu_state.regs[cpu_reg].w = cpu_data;
			} else {
				cpu_data = geteab();
				cpu_src = getr8(cpu_reg);
				setr8(cpu_reg, (uint8_t)(cpu_data & 0xff));
			}
			cpu_wait(3, 0);
			do_access(12, bits);
			if (opcode & 1)
				seteaw(cpu_src);
			else
				seteab((uint8_t)(cpu_src & 0xff));
			break;

		case 0x88:	/* MOV rm, reg */
		case 0x89:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			cpu_wait(1, 0);
			do_access(13, bits);
			if (opcode & 1)
				seteaw(cpu_state.regs[cpu_reg].w);
			else
				seteab(getr8(cpu_reg));
			break;

		case 0x8a:	/* MOV reg, rm */
		case 0x8b:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(50, bits);
			if (opcode & 1)
				cpu_state.regs[cpu_reg].w = geteaw();
			else
				setr8(cpu_reg, geteab());
			cpu_wait(1, 0);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			break;

		case 0x8c:	/* MOV w,sreg */
			do_mod_rm();
			if (cpu_mod == 3)
				cpu_wait(1, 0);
			do_access(14, 16);
			switch (rmdat & 0x38) {
				case 0x00:	/* ES */
					seteaw(ES);
					break;

				case 0x08:	/* CS */
					seteaw(CS);
					break;

				case 0x18:	/* DS */
					seteaw(DS);
					break;

				case 0x10:	/* SS */
					seteaw(SS);
					break;
			}
			break;

		case 0x8d:	/* LEA */
			do_mod_rm();
			cpu_state.regs[cpu_reg].w = (cpu_mod == 3) ? cpu_state.last_ea : cpu_state.eaaddr;
			cpu_wait(1, 0);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			break;

		case 0x8e:	/* MOV sreg,w */
			do_mod_rm();
			do_access(51, 16);
			tempw = geteaw();
			switch (rmdat & 0x38) {
				case 0x00:	/* ES */
					loadseg(tempw, &_es);
					break;

				case 0x08:	/* CS - 8088/8086 only */
					loadcs(tempw);
					pfq_clear();
					break;

				case 0x18:	/* DS */
					loadseg(tempw, &_ds);
					break;

				case 0x10:	/* SS */
					loadseg(tempw, &_ss);
					break;
			}
			cpu_wait(1, 0);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			noint = 1;
			break;

		case 0x8f:	/* POPW */
			do_mod_rm();
			cpu_wait(1, 0);
			cpu_src = cpu_state.eaaddr;
			do_access(24, 16);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			cpu_data = do_pop();
			cpu_state.eaaddr = cpu_src;
			cpu_wait(2, 0);
			do_access(15, 16);
			seteaw(cpu_data);
			break;

		case 0x90: case 0x91: case 0x92: case 0x93: /* XCHG AX, rw */
		case 0x94: case 0x95: case 0x96: case 0x97:
			cpu_wait(1, 0);
			cpu_data = cpu_state.regs[opcode & 7].w;
			cpu_state.regs[opcode & 7].w = AX;
			AX = cpu_data;
			cpu_wait(1, 0);
			break;

		case 0x98:	/* CBW */
			cpu_wait(1, 0);
			AX = sign_extend(AL);
			break;

		case 0x99:	/* CWD */
			cpu_wait(4, 0);
			if (top_bit(AX, 16)) {
				cpu_wait(1, 0);
				DX = 0xffff;
			} else
				DX = 0;
			break;

		case 0x9a:	/* CALL FAR */
			cpu_wait(1, 0);
			new_ip = pfq_fetchw();
			cpu_wait(1, 0);
			new_cs = pfq_fetchw();
			do_access(31, 16);
			do_push(CS);
			do_access(60, 16);
			cpu_state.oldpc = cpu_state.pc;
			loadcs(new_cs);
			cpu_state.pc = new_ip;
			do_access(32, 16);
			do_push(cpu_state.oldpc);
			pfq_clear();
			break;

		case 0x9b:	/* WAIT */
			cpu_wait(4, 0);
			break;

		case 0x9c:	/* PUSHF */
			do_access(33, 16);
			do_push((flags & 0x0fd7) | 0xf000);
			break;

		case 0x9d:	/* POPF */
			do_access(25, 16);
			flags = do_pop() | 2;
			cpu_wait(1, 0);
			break;

		case 0x9e:	/* SAHF */
			cpu_wait(1, 0);
			flags = (flags & 0xff02) | AH;
			cpu_wait(2, 0);
			break;

		case 0x9f:	/* LAHF */
			cpu_wait(1, 0);
			AH = flags & 0xd7;
			break;

		case 0xa0:	/* MOV A, [iw] */
		case 0xa1:
			bits = 8 << (opcode & 1);
			cpu_wait(1, 0);
			addr = pfq_fetchw();
			do_access(1, bits);
			if (opcode & 1)
				AX = readmemw((ovr_seg ? *ovr_seg : ds), addr);
			else
				AL = readmemb((ovr_seg ? *ovr_seg : ds) + addr);
			cpu_wait(1, 0);
			break;

		case 0xa2:	/* MOV [iw], A */
		case 0xa3:
			bits = 8 << (opcode & 1);
			cpu_wait(1, 0);
			addr = pfq_fetchw();
			do_access(7, bits);
			if (opcode & 1)
				writememw((ovr_seg ? *ovr_seg : ds), addr, AX);
			else
				writememb((ovr_seg ? *ovr_seg : ds) + addr, AL);
			break;

		case 0xa4:	/* MOVS */
		case 0xa5:
		case 0xac:	/* LODS */
		case 0xad:
			bits = 8 << (opcode & 1);
			if (! repeating) {
				cpu_wait(1 /*2*/, 0);
				if ((opcode & 8) == 0 && in_rep != 0)
					cpu_wait(1, 0);
			}
			if (rep_action(&completed, &repeating, in_rep, bits)) {
				cpu_wait(1, 0);
				if ((opcode & 8) != 0)
					cpu_wait(1, 0);
				break;
			}
			if (in_rep != 0 && (opcode & 8) != 0)
				cpu_wait(1, 0);
			do_access(20, bits);
			do_lods(bits);
			if ((opcode & 8) == 0) {
				do_access(27, bits);
				do_stos(bits);
			} else {
				if (opcode & 1)
					AX = cpu_data;
				else
					AL = (uint8_t)(cpu_data & 0xff);
				if (in_rep != 0)
					cpu_wait(2, 0);
			}
			if (in_rep == 0) {
				cpu_wait(3, 0);
				if ((opcode & 8) != 0)
					cpu_wait(1, 0);
				break;
			}
			repeating = 1;
			timer_end_period(cycles * xt_cpu_multi);
			goto opcodestart;

		case 0xa6:	/* CMPS */
		case 0xa7:
		case 0xae:	/* SCAS */
		case 0xaf:
			bits = 8 << (opcode & 1);
			if (! repeating)
				cpu_wait(1, 0);
			if (rep_action(&completed, &repeating, in_rep, bits)) {
				cpu_wait(2, 0);
				break;
			}
			if (in_rep != 0)
				cpu_wait(1, 0);
			if (opcode & 1)
				cpu_dest = AX;
			else
				cpu_dest = AL;
			if ((opcode & 8) == 0) {
				do_access(21, bits);
				do_lods(bits);
				cpu_wait(1, 0);
				cpu_dest = cpu_data;
			}
			do_access(2, bits);
			if (opcode & 1)
				cpu_data = readmemw(es, DI);
			else
				cpu_data = readmemb(es + DI);
			if (flags & D_FLAG)
				DI -= (bits >> 3);
			else
				DI += (bits >> 3);
			cpu_src = cpu_data;
			do_sub(bits);
			cpu_wait(2, 0);
			if (in_rep == 0) {
				cpu_wait(3, 0);
				break;
			}
			if ((!!(flags & Z_FLAG)) == (in_rep == 1)) {
				cpu_wait(4, 0);
				break;
			}
			repeating = 1;
			timer_end_period(cycles * xt_cpu_multi);
			goto opcodestart;

		case 0xa8:	/* TEST A, imm */
		case 0xa9:
			bits = 8 << (opcode & 1);
			cpu_wait(1, 0);
			if (opcode & 1) {
				cpu_data = pfq_fetchw();
				do_test(bits, AX, cpu_data);
			} else {
				cpu_data = pfq_fetchb();
				do_test(bits, AL, cpu_data);
			}
			cpu_wait(1, 0);
			break;

		case 0xaa:	/* STOS */
		case 0xab:
			bits = 8 << (opcode & 1);
			if (! repeating) {
				if (opcode & 1)
					cpu_wait(1, 0);
				if (in_rep != 0)
					cpu_wait(1, 0);
			}
			if (rep_action(&completed, &repeating, in_rep, bits)) {
				cpu_wait(1, 0);
				break;
			}
			cpu_data = AX;
			do_access(28, bits);
			do_stos(bits);
			if (in_rep == 0) {
				cpu_wait(3, 0);
				break;
			}
			repeating = 1;
			timer_end_period(cycles * xt_cpu_multi);
			goto opcodestart;

		case 0xb0:	/* MOV cpu_reg,#8 */
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb6:
		case 0xb7:
			cpu_wait(1, 0);
			if (opcode & 0x04)
				cpu_state.regs[opcode & 0x03].b.h = pfq_fetchb();
			else
				cpu_state.regs[opcode & 0x03].b.l = pfq_fetchb();
			cpu_wait(1, 0);
			break;

		case 0xb8:	/* MOV cpu_reg,#16 */
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbe:
		case 0xbf:
			cpu_wait(1, 0);
			cpu_state.regs[opcode & 0x07].w = pfq_fetchw();
			cpu_wait(1, 0);
			break;

		case 0xc0:	/* RET */
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc8:
		case 0xc9:
		case 0xca:
		case 0xcb:
			bits = 8 + (opcode & 0x08);
			if ((opcode & 9) != 1)
				cpu_wait(1, 0);
			if (! (opcode & 1)) {
				cpu_src = pfq_fetchw();
				cpu_wait(1, 0);
			}
			if ((opcode & 9) == 9)
				cpu_wait(1, 0);
			do_access(26, bits);
			new_ip = do_pop();
			cpu_wait(2, 0);
			if (opcode & 8) {
				do_access(42, bits);
				new_cs = do_pop();
				if (opcode & 1)
					cpu_wait(1, 0);
			} else
				new_cs = CS;
			if (! (opcode & 1)) {
				SP += cpu_src;
				cpu_wait(1, 0);
			}
			loadcs(new_cs);
			do_access(72, bits);
			cpu_state.pc = new_ip;
			pfq_clear();
			break;

		case 0xc4:	/* LSS rw, rmd */
		case 0xc5:
			do_mod_rm();
			bits = 16;
			do_access(52, bits);
			cpu_state.regs[cpu_reg].w = readmemw(easeg, cpu_state.eaaddr);
			tempw = readmemw(easeg, (cpu_state.eaaddr + 2) & 0xFFFF);
			loadseg(tempw, (opcode & 0x01) ? &_ds : &_es);
			cpu_wait(1, 0);
			noint = 1;
			break;

		case 0xc6:	/* MOV rm, imm */
		case 0xc7:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			cpu_wait(1, 0);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			if (opcode & 1)
				cpu_data = pfq_fetchw();
			else
				cpu_data = pfq_fetchb();
			if (cpu_mod == 3)
				cpu_wait(1, 0);
			do_access(16, bits);
			if (opcode & 1)
				seteaw(cpu_data);
			else
				seteab((uint8_t)(cpu_data & 0xff));
			break;

		case 0xcc:	/* INT 3 */
			do_intr(3, 1);
			break;

		case 0xcd:	/* INT */
			cpu_wait(1, 0);
			do_intr(pfq_fetchb(), 1);
			break;

		case 0xce:	/* INTO */
			cpu_wait(3, 0);
			if (flags & V_FLAG) {
				cpu_wait(2, 0);
				do_intr(4, 1);
			}
			break;

		case 0xcf:	/* IRET */
			do_access(43, 8);
			new_ip = do_pop();
			cpu_wait(3, 0);
			do_access(44, 8);
			new_cs = do_pop();
			loadcs(new_cs);
			do_access(62, 8);
			cpu_state.pc = new_ip;
			do_access(45, 8);
			flags = do_pop() | 2;
			cpu_wait(5, 0);
			noint = 1;
			nmi_enable = 1;
			pfq_clear();
			break;

		case 0xd0:	/* rot rm */
		case 0xd1:
		case 0xd2:
		case 0xd3:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			if (cpu_mod == 3)
				cpu_wait(1, 0);
			do_access(53, bits);
			if (opcode & 1)
				cpu_data = geteaw();
			else
				cpu_data = geteab();
			if (opcode & 2) {
				cpu_src = CL;
				cpu_wait((cpu_mod != 3) ? 9 : 6, 0);
			} else {
				cpu_src = 1;
				cpu_wait((cpu_mod != 3) ? 4 : 0, 0);
			}
			while (cpu_src != 0) {
				cpu_dest = cpu_data;
				oldc = flags & C_FLAG;
				switch (rmdat & 0x38) {
					case 0x00:	/* ROL */
						set_cf(top_bit(cpu_data, bits));
						cpu_data <<= 1;
						cpu_data |= ((flags & C_FLAG) ? 1 : 0);
						set_of_rotate(bits);
						break;

					case 0x08:	/* ROR */
						set_cf((cpu_data & 1) != 0);
						cpu_data >>= 1;
						if (flags & C_FLAG)
							cpu_data |= (!(opcode & 1) ? 0x80 : 0x8000);
						set_of_rotate(bits);
						break;

					case 0x10:	/* RCL */
						set_cf(top_bit(cpu_data, bits));
						cpu_data = (cpu_data << 1) | (oldc ? 1 : 0);
						set_of_rotate(bits);
						break;

					case 0x18: 	/* RCR */
						set_cf((cpu_data & 1) != 0);
						cpu_data >>= 1;
						if (oldc)
							cpu_data |= (!(opcode & 0x01) ? 0x80 : 0x8000);
						set_cf((cpu_dest & 1) != 0);
						set_of_rotate(bits);
						break;

					case 0x20:	/* SHL */
						set_cf(top_bit(cpu_data, bits));
						cpu_data <<= 1;
						set_of_rotate(bits);
						set_pzs(bits);
						break;

					case 0x28:	/* SHR */
						set_cf((cpu_data & 1) != 0);
						cpu_data >>= 1;
						set_of_rotate(bits);
						set_af(1);
						set_pzs(bits);
						break;

					case 0x30:	/* SETMO - undocumented? */
						bitwise(bits, 0xffff);
						set_cf(0);
						set_of_rotate(bits);
						set_af(0);
						set_pzs(bits);
						break;

					case 0x38:	/* SAR */
						set_cf((cpu_data & 1) != 0);
						cpu_data >>= 1;
						if (!(opcode & 1))
							cpu_data |= (cpu_dest & 0x80);
						else
							cpu_data |= (cpu_dest & 0x8000);
						set_of_rotate(bits);
						set_af(1);
						set_pzs(bits);
						break;
				}
				if ((opcode & 2) != 0)
					cpu_wait(4, 0);
				cpu_src--;
			}
			do_access(17, bits);
			if (opcode & 1)
				seteaw(cpu_data);
			else
				seteab((uint8_t)(cpu_data & 0xff));
			break;

		case 0xd4:	/* AAM */
			cpu_wait(1, 0);
			cpu_src = pfq_fetchb();
			if (do_div(AL, 0))
				set_pzs(16);
			break;

		case 0xd5:	/* AAD */
			cpu_wait(1, 0);
			do_mul(pfq_fetchb(), AH);
			AL += cpu_data;
			AH = 0x00;
			set_pzs(16);
			break;

		case 0xd6:	/* SALC */
			cpu_wait(1, 0);
			AL = (flags & C_FLAG) ? 0xff : 0x00;
			cpu_wait(1, 0);
			break;

		case 0xd7:	/* XLATB */
			addr = BX + AL;
			cpu_state.last_ea = addr;
			do_access(4, 8);
			AL = readmemb((ovr_seg ? *ovr_seg : ds) + addr);
			cpu_wait(1, 0);
			break;

		case 0xd8:	/* esc i, r, rm */
		case 0xd9:
		case 0xda:
		case 0xdb:
		case 0xdd:
		case 0xdc:
		case 0xde:
		case 0xdf:
			do_mod_rm();
			do_access(54, 16);
			geteaw();
			cpu_wait(1, 0);
			if (cpu_mod != 3)
				cpu_wait(2, 0);
			break;

		case 0xe0:	/* LOOP */
		case 0xe1:
		case 0xe2:
		case 0xe3:
			cpu_wait(3, 0);
			cpu_data = pfq_fetchb();
			if (opcode != 0xe2)
				cpu_wait(1, 0);
			if (opcode != 0xe3) {
				CX--;
				oldc = (CX != 0);
				switch (opcode) {
					case 0xe0:
						if (flags & Z_FLAG)
							oldc = 0;
						break;

					case 0xe1:
						if (!(flags & Z_FLAG))
							oldc = 0;
						break;
				}
			} else
				oldc = (CX == 0);
			if (oldc)
				do_jump_short();
			break;

		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
		case 0xec:
		case 0xed:
		case 0xee:
		case 0xef:
			bits = 8 << (opcode & 1);
			if ((opcode & 0x0e) != 0x0c)
				cpu_wait(1, 0);
			if (opcode & 8)
				cpu_data = DX;
			else
				cpu_data = pfq_fetchb();
			if (opcode & 2) {
				if (opcode & 8)
					do_access(9, bits);
				else
					do_access(8, bits);
				if ((opcode & 1) && is8086 && !(cpu_data & 1)) {
					outw(cpu_data, AX);
					cpu_wait(4, 1);
				} else {
					outb(cpu_data, AL);
					if (opcode & 1)
						outb(cpu_data + 1, AH);
					cpu_wait(bits >> 1, 1);	/* I/O do_access. */
				}
			} else {
				do_access(3, bits);
				if ((opcode & 1) && is8086 && !(cpu_data & 1)) {
					AX = inw(cpu_data);
					cpu_wait(4, 1);		/* I/O do_access and cpu_wait state. */
				} else {
					AL = inb(cpu_data);
					if (opcode & 1)
						AH = inb(cpu_data + 1);
					cpu_wait(bits >> 1, 1);	/* I/O do_access. */
				}
				cpu_wait(1, 0);
			}
			break;

		case 0xe8:	/* CALL rel 16 */
			cpu_wait(1, 0);
			cpu_state.oldpc = do_jump_near();
			do_access(34, 8);
			do_push(cpu_state.oldpc);
			pfq_clear();
			break;

		case 0xe9:	/* JMP rel 16 */
			cpu_wait(1, 0);
			do_jump_near();
			break;

		case 0xea:	/* JMP far */
			cpu_wait(1, 0);
			addr = pfq_fetchw();
			cpu_wait(1, 0);
			tempw = pfq_fetchw();
			loadcs(tempw);
			do_access(70, 8);
			cpu_state.pc = addr;
			pfq_clear();
			break;

		case 0xeb:	/* JMP rel */
			cpu_wait(1, 0);
			cpu_data = (int8_t) pfq_fetchb();
			do_jump_short();
			cpu_wait(1, 0);
			pfq_clear();
			break;

		case 0xf0:	/* LOCK - F1 is alias */
		case 0xf1:
			in_lock = 1;
			cpu_wait(1, 0);
			goto opcodestart;

		case 0xf2:	/* REPNE */
		case 0xf3:	/* REPE */
			cpu_wait(1, 0);
			in_rep = (opcode == 0xf2 ? 1 : 2);
			repeating = 0;
			completed = 0;
			goto opcodestart;

		case 0xf4:	/* HLT */
			halt = 1;
			pfq_clear();
			cpu_wait(2, 0);
			break;

		case 0xf5:	/* CMC */
			cpu_wait(1, 0);
			flags ^= C_FLAG;
			break;

		case 0xf6:
		case 0xf7:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(55, bits);
			if (opcode & 1)
				cpu_data = geteaw();
			else
				cpu_data = geteab();
			switch (rmdat & 0x38) {
				case 0x00:	/* TEST */
				case 0x08:
					cpu_wait(2, 0);
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					if (opcode & 1)
						cpu_src = pfq_fetchw();
					else
						cpu_src = pfq_fetchb();
					cpu_wait(1, 0);
					do_test(bits, cpu_data, cpu_src);
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					break;

				case 0x10:	/* NOT */
				case 0x18:	/* NEG */
					cpu_wait(2, 0);
					if ((rmdat & 0x38) == 0x10)
						cpu_data = ~cpu_data;
					else {
						cpu_src = cpu_data;
						cpu_dest = 0;
						do_sub(bits);
					}
					do_access(18, bits);
					if (opcode & 1)
						seteaw(cpu_data);
					else
						seteab((uint8_t)(cpu_data & 0xff));
					break;

				case 0x20:	/* MUL */
				case 0x28:	/* IMUL */
					cpu_wait(1, 0);
					if (opcode & 1) {
						do_mul(AX, cpu_data);
						AX = cpu_data;
						DX = cpu_dest;
						cpu_data |= DX;
						set_co_do_mul((DX != ((AX & 0x8000) == 0) || ((rmdat & 0x38) == 0x20) ? 0 : 0xffff));
					} else {
						do_mul(AL, cpu_data);
						AL = (uint8_t)cpu_data;
						AH = (uint8_t)cpu_dest;
						if (! is_nec)
							cpu_data |= AH;
						set_co_do_mul(AH != (((AL & 0x80) == 0) || ((rmdat & 0x38) == 0x20) ? 0 : 0xff));
					}
					set_zf(bits);
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					break;

				case 0x30:	/* DIV */
				case 0x38:	/* IDIV */
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					cpu_src = cpu_data;
					if (do_div(AL, AH))
						cpu_wait(1, 0);
					break;
			}
			break;

		case 0xf8:	/* CLCSTC */
		case 0xf9:
			cpu_wait(1, 0);
			set_cf(opcode & 1);
			break;

		case 0xfa:	/* CLISTI */
		case 0xfb:
			cpu_wait(1, 0);
			set_if(opcode & 1);
			break;

		case 0xfc:	/* CLDSTD */
		case 0xfd:
			cpu_wait(1, 0);
			set_df(opcode & 1);
			break;

		case 0xfe:	/* misc */
		case 0xff:
			bits = 8 << (opcode & 1);
			do_mod_rm();
			do_access(56, bits);
			read_ea(((rmdat & 0x38) == 0x18) || ((rmdat & 0x38) == 0x28), bits);
			switch (rmdat & 0x38) {
				case 0x00:	/* INC rm */
				case 0x08:	/* DEC rm */
					cpu_dest = cpu_data;
					cpu_src = 1;
					if ((rmdat & 0x38) == 0x00) {
						cpu_data = cpu_dest + cpu_src;
						set_of_add(bits);
					} else {
						cpu_data = cpu_dest - cpu_src;
						set_of_sub(bits);
					}
					do_af();
					set_pzs(bits);
					cpu_wait(2, 0);
					do_access(19, bits);
					if (opcode & 1)
						seteaw(cpu_data);
					else
						seteab((uint8_t)(cpu_data & 0xff));
					break;

				case 0x10:	/* CALL rm */
					if (! (opcode & 1)) {
						if (cpu_mod != 3)
							cpu_data |= 0xff00;
						else
							cpu_data = cpu_state.regs[cpu_rm].w;
					}
					do_access(63, bits);
					cpu_wait(5, 0);
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					cpu_wait(1, 0);
					cpu_state.oldpc = cpu_state.pc;
					cpu_state.pc = cpu_data;
					cpu_wait(2, 0);
					do_access(35, bits);
					do_push(cpu_state.oldpc);
					pfq_clear();
					break;

				case 0x18:	/* CALL rmd */
					new_ip = cpu_data;
					do_access(58, bits);
					read_ea2(bits);
					if (! (opcode & 1))
						cpu_data |= 0xff00;
					new_cs = cpu_data;
					do_access(36, bits);
					do_push(CS);
					do_access(64, bits);
					cpu_wait(4, 0);
					cpu_state.oldpc = cpu_state.pc;
					loadcs(new_cs);
					cpu_state.pc = new_ip;
					do_access(37, bits);
					do_push(cpu_state.oldpc);
					pfq_clear();
					break;

				case 0x20:	/* JMP rm */
					if (! (opcode & 1)) {
						if (cpu_mod != 3)
							cpu_data |= 0xff00;
						else
							cpu_data = cpu_state.regs[cpu_rm].w;
					}
					do_access(65, bits);
					cpu_state.pc = cpu_data;
					pfq_clear();
					break;

				case 0x28:	/* JMP rmd */
					new_ip = cpu_data;
					do_access(59, bits);
					read_ea2(bits);
					if (! (opcode & 1))
						cpu_data |= 0xff00;
					new_cs = cpu_data;
					loadcs(new_cs);
					do_access(66, bits);
					cpu_state.pc = new_ip;
					pfq_clear();
					break;

				case 0x30:	/* PUSH rm */
				case 0x38:
					if (cpu_mod != 3)
						cpu_wait(1, 0);
					do_access(38, bits);
					if ((cpu_mod == 3) && (cpu_rm == 4))
						do_push(cpu_data - 2);
					else
						do_push(cpu_data);
					break;
			}
			break;

		default:
			ERRLOG("CPU: illegal opcode: %02x\n", opcode);
			pfq_fetchb();
			cpu_wait(8, 0);
			break;
	}

	cpu_state.pc &= 0xffff;

on_halt:
	if (ovr_seg)
		ovr_seg = NULL;

	if (in_lock)
		in_lock = 0;

	timer_end_period(cycles * xt_cpu_multi);

	if (trap && (flags & T_FLAG) && !noint) {
		halt = 0;
		do_intr(1, 1);
	} else if (nmi && nmi_enable && nmi_mask) {
		halt = 0;
		do_intr(2, 1);
		nmi_enable = 0;
	} else if (takeint && !noint) {
		temp = pic_interrupt();
		if (temp != 0xFF) {
			halt = 0;
			do_intr(temp, 1);
		}
	}
	takeint = (flags & I_FLAG) && (pic.pend &~ pic.mask);

	if (noint)
		noint = 0;

	ins++;
    }
}


/* Memory refresh read - called by reads and writes on DMA channel 0. */
void
refreshread(void)
{
    if (cpu_get_speed() > 4772728)
	cpu_wait(8, 1);	/* insert extra wait states */

    /*
     * Do the actual refresh stuff.
     *
     * If there is no extra cycles left to consume, return.
     */
    if (! (fetchcycles & 3))
	return;

    /* If the prefetch queue is full, return. */
    if (pfq_pos >= pfq_size)
	return;

    /* Subtract from 1 to 8 cycles. */
    cpu_wait(8 - (fetchcycles % 7), 1);

    /* Write to the prefetch queue. */
    pfq_write();

    /* Add those cycles to fetchcycles. */
    fetchcycles += (4 - (fetchcycles & 3));
}


/* Reset the CPU. */
void
cpu_reset(int hard)
{
    if (hard) {
	INFO("CPU: reset\n");
	ins = 0;
    }
    use32 = 0;
    cpu_cur_status = 0;
    stack32 = 0;
    msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
    msw = 0;
    if (is486)
	cr0 = 1 << 30;
    else
	cr0 = 0;
    cpu_cache_int_enabled = 0;
    cpu_update_waitstates();
    cr4 = 0;
    eflags = 0;
    cgate32 = 0;

    if (AT) {
	loadcs(0xF000);
	cpu_state.pc = 0xFFF0;
	rammask = cpu_16bitbus ? 0xFFFFFF : 0xFFFFFFFF;
    } else {
	loadcs(0xFFFF);
	cpu_state.pc=0;
	rammask = 0xfffff;
    }

    idt.base = 0;
    idt.limit = is386 ? 0x03FF : 0xFFFF;
    flags = 2;
    trap = 0;
    ovr_seg = NULL;
    in_lock = halt = 0;

    if (hard) {
	makeznptable();
	resetreadlookup();
	makemod1table();
	resetmcr();
	pfq_clear();
	cpu_set_edx();
	EAX = 0;
	ESP = 0;
	mmu_perm = 4;
	pfq_size = (is8086) ? 6 : 4;
    }

    x86seg_reset();
#ifdef USE_DYNAREC
    if (hard)
	codegen_reset();
#endif
    x86_was_reset = 1;
    port_92_clear_reset();
}
