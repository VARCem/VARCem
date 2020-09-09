/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the CPU's dynamic recompiler.
 *
 * Version:	@(#)386_dynarec.c	1.0.12	2020/09/09
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
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
#include "../io.h"
#include "cpu.h"
#include "../mem.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "386_common.h"
#ifdef USE_DYNAREC
# include "codegen.h"
#endif


#define CPU_BLOCK_END() cpu_block_end = 1

/* Also in 386.c: */
cpu_state_t	cpu_state;
int		inscounts[256];
uint32_t	oxpc;
extern int		trap;
int		inttype;
int		optype;
extern int		cgate32;
uint16_t	rds;
uint16_t	ea_rseg;
uint32_t	*eal_r, *eal_w;
extern uint16_t	*mod1add[2][8];
extern uint32_t	*mod1seg[8];


uint32_t	cpu_cur_status = 0;
int		cpu_reps, cpu_reps_latched;
int		cpu_notreps, cpu_notreps_latched;
int		cpu_recomp_blocks, cpu_recomp_full_ins, cpu_new_blocks;
int		cpu_recomp_blocks_latched, cpu_recomp_ins_latched,
		cpu_recomp_full_ins_latched, cpu_new_blocks_latched;

int		inrecomp = 0;
int		cpu_block_end = 0;

int cpl_override=0;
int fpucount=0;
int oddeven=0;


uint32_t rmdat32;


static INLINE uint32_t
get_phys(uint32_t addr)
{
    if (! ((addr ^ get_phys_virt) & ~0xfff))
	return get_phys_phys | (addr & 0xfff);

    get_phys_virt = addr;

    if (! (cr0 >> 31)) {
	get_phys_phys = (addr & rammask) & ~0xfff;

	return addr & rammask;
    }

    if (readlookup2[addr >> 12] != -1)
	get_phys_phys = ((uintptr_t)readlookup2[addr >> 12] + (addr & ~0xfff)) - (uintptr_t)ram;
    else {
	get_phys_phys = (mmutranslatereal(addr, 0) & rammask) & ~0xfff;

	if (!cpu_state.abrt && mem_addr_is_ram(get_phys_phys))
		addreadlookup(get_phys_virt, get_phys_phys);
    }

    return get_phys_phys | (addr & 0xfff);
}

static INLINE void
fetch_ea_32_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        ea_rseg = cpu_state.ea_seg->seg;
        if (cpu_rm == 4)
        {
                uint8_t sib = rmdat >> 8;
                
                switch (cpu_mod)
                {
                        case 0: 
                        cpu_state.eaaddr = cpu_state.regs[sib & 7].l; 
                        cpu_state.pc++; 
                        break;
                        case 1: 
                        cpu_state.pc++;
                        cpu_state.eaaddr = ((uint32_t)(int8_t)getbyte()) + cpu_state.regs[sib & 7].l; 
                        break;
                        case 2: 
                        cpu_state.eaaddr = (fastreadl(cs + cpu_state.pc + 1)) + cpu_state.regs[sib & 7].l; 
                        cpu_state.pc += 5; 
                        break;
                }
                /*SIB byte present*/
                if ((sib & 7) == 5 && !cpu_mod) 
                        cpu_state.eaaddr = getlong();
                else if ((sib & 6) == 4 && !cpu_state.ssegs)
                {
                        easeg = ss;
                        ea_rseg = SS;
                        cpu_state.ea_seg = &_ss;
                }
                if (((sib >> 3) & 7) != 4) 
                        cpu_state.eaaddr += cpu_state.regs[(sib >> 3) & 7].l << (sib >> 6);
        }
        else
        {
                cpu_state.eaaddr = cpu_state.regs[cpu_rm].l;
                if (cpu_mod) 
                {
                        if (cpu_rm == 5 && !cpu_state.ssegs)
                        {
                                easeg = ss;
                                ea_rseg = SS;
                                cpu_state.ea_seg = &_ss;
                        }
                        if (cpu_mod == 1) 
                        { 
                                cpu_state.eaaddr += ((uint32_t)(int8_t)(rmdat >> 8)); 
                                cpu_state.pc++; 
                        }
                        else          
                        {
                                cpu_state.eaaddr += getlong(); 
                        }
                }
                else if (cpu_rm == 5) 
                {
                        cpu_state.eaaddr = getlong();
                }
        }
        if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
        {
                uint32_t addr = easeg + cpu_state.eaaddr;
                if (readlookup2[addr >> 12] != (uintptr_t)-1)
                   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != (uintptr_t)-1)
                   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
        }
	cpu_state.last_ea = cpu_state.eaaddr;
}

static INLINE void fetch_ea_16_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        ea_rseg = cpu_state.ea_seg->seg;
        if (!cpu_mod && cpu_rm == 6) 
        { 
                cpu_state.eaaddr = getword();
        }
        else
        {
                switch (cpu_mod)
                {
                        case 0:
                        cpu_state.eaaddr = 0;
                        break;
                        case 1:
                        cpu_state.eaaddr = (uint16_t)(int8_t)(rmdat >> 8); cpu_state.pc++;
                        break;
                        case 2:
                        cpu_state.eaaddr = getword();
                        break;
                }
                cpu_state.eaaddr += (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
                if (mod1seg[cpu_rm] == &ss && !cpu_state.ssegs)
                {
                        easeg = ss;
                        ea_rseg = SS;
                        cpu_state.ea_seg = &_ss;
                }
                cpu_state.eaaddr &= 0xFFFF;
        }
        if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
        {
                uint32_t addr = easeg + cpu_state.eaaddr;
                if (readlookup2[addr >> 12] != (uintptr_t)-1)
                   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != (uintptr_t)-1)
                   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
        }
	cpu_state.last_ea = cpu_state.eaaddr;
}

#define fetch_ea_16(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_16_long(rmdat); if (cpu_state.abrt) return 1; } 
#define fetch_ea_32(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_32_long(rmdat); } if (cpu_state.abrt) return 1

#include "x86_flags.h"

void x86_int(uint32_t num)
{
        uint32_t addr;
        flags_rebuild();
        cpu_state.pc=cpu_state.oldpc;
        if (msw&1)
        {
                pmodeint(num,0);
        }
        else
        {
                addr = (num << 2) + idt.base;

                if ((num << 2) + 3 > idt.limit)
                {
                        if (idt.limit < 35)
                        {
                                cpu_state.abrt = 0;
                                cpu_reset(0);
                                cpu_set_edx();
                                INFO("CPU: triple fault in real mode - reset\n");
                        }
                        else
                                x86_int(8);
                }
                else
                {
                        if (stack32)
                        {
                                writememw(ss,ESP-2,flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        flags&=~I_FLAG;
                        flags&=~T_FLAG;
                        oxpc=cpu_state.pc;
                        cpu_state.pc=readmemw(0,addr);
                        loadcs(readmemw(0,addr+2));
                }
        }
        cycles-=70;
        CPU_BLOCK_END();
}

void x86_int_sw(uint32_t num)
{
        uint32_t addr;
        flags_rebuild();
        cycles -= timing_int;
        if (msw&1)
        {
                pmodeint(num,1);
        }
        else
        {
                addr = (num << 2) + idt.base;

                if ((num << 2) + 3 > idt.limit)
                {
                        x86_int(13);
                }
                else
                {
                        if (stack32)
                        {
                                writememw(ss,ESP-2,flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        flags&=~I_FLAG;
                        flags&=~T_FLAG;
                        oxpc=cpu_state.pc;
                        cpu_state.pc=readmemw(0,addr);
                        loadcs(readmemw(0,addr+2));
                        cycles -= timing_int_rm;
                }
        }
        trap = 0;
        CPU_BLOCK_END();
}

int x86_int_sw_rm(int num)
{
        uint32_t addr;
        uint16_t new_pc, new_cs;
        
        flags_rebuild();
        cycles -= timing_int;

        addr = num << 2;
        new_pc = readmemw(0, addr);
        new_cs = readmemw(0, addr + 2);

        if (cpu_state.abrt) return 1;

        writememw(ss,((SP-2)&0xFFFF),flags); if (cpu_state.abrt) {ERRLOG("abrt5\n"); return 1; }
        writememw(ss,((SP-4)&0xFFFF),CS);
        writememw(ss,((SP-6)&0xFFFF),cpu_state.pc); if (cpu_state.abrt) {ERRLOG("abrt6\n"); return 1; }
        SP-=6;

        eflags &= ~VIF_FLAG;
        flags &= ~T_FLAG;
        cpu_state.pc = new_pc;
        loadcs(new_cs);
        oxpc=cpu_state.pc;

        cycles -= timing_int_rm;
        trap = 0;
        CPU_BLOCK_END();
        
        return 0;
}

void x86illegal()
{
        x86_int(6);
}

/*Prefetch emulation is a fairly simplistic model:
  - All instruction bytes must be fetched before it starts.
  - Cycles used for non-instruction memory accesses are counted and subtracted
    from the total cycles taken
  - Any remaining cycles are used to refill the prefetch queue.

  Note that this is only used for 286 / 386 systems. It is disabled when the
  internal cache on 486+ CPUs is enabled.
*/
static int prefetch_bytes = 0;
static int prefetch_prefixes = 0;

static void prefetch_run(int instr_cycles, int bytes, int modrm, int reads, int reads_l, int writes, int writes_l, int ea32)
{
        int mem_cycles = reads*cpu_cycles_read + reads_l*cpu_cycles_read_l + writes*cpu_cycles_write + writes_l*cpu_cycles_write_l;

        if (instr_cycles < mem_cycles)
                instr_cycles = mem_cycles;

        prefetch_bytes -= prefetch_prefixes;
        prefetch_bytes -= bytes;
        if (modrm != -1)
        {
                if (ea32)
                {
                        if ((modrm & 7) == 4)
                        {
                                if ((modrm & 0x700) == 0x500)
                                        prefetch_bytes -= 5;
                                else if ((modrm & 0xc0) == 0x40)
                                        prefetch_bytes -= 2;
                                else if ((modrm & 0xc0) == 0x80)
                                        prefetch_bytes -= 5;
                        }
                        else
                        {
                                if ((modrm & 0xc7) == 0x05)
                                        prefetch_bytes -= 4;
                                else if ((modrm & 0xc0) == 0x40)
                                        prefetch_bytes--;
                                else if ((modrm & 0xc0) == 0x80)
                                        prefetch_bytes -= 4;
                        }
                }
                else
                {
                        if ((modrm & 0xc7) == 0x06)
                                prefetch_bytes -= 2;
                        else if ((modrm & 0xc0) != 0xc0)
                                prefetch_bytes -= ((modrm & 0xc0) >> 6);
                }
        }
        
        /* Fill up prefetch queue */
        while (prefetch_bytes < 0)
        {
                prefetch_bytes += cpu_prefetch_width;
                cycles -= cpu_prefetch_cycles;
        }
        
        /* Subtract cycles used for memory access by instruction */
        instr_cycles -= mem_cycles;
        
        while (instr_cycles >= cpu_prefetch_cycles)
        {
                prefetch_bytes += cpu_prefetch_width;
                instr_cycles -= cpu_prefetch_cycles;
        }
        
        prefetch_prefixes = 0;
}

static void prefetch_flush()
{
        prefetch_bytes = 0;
}

#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32) \
        do { if (cpu_prefetch_cycles) prefetch_run(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32); } while (0)

#define PREFETCH_PREFIX() do { if (cpu_prefetch_cycles) prefetch_prefixes++; } while (0)
#define PREFETCH_FLUSH() prefetch_flush()


int checkio(uint32_t port)
{
        uint16_t t;
        uint8_t d;
        cpl_override = 1;
        t = readmemw(tr.base, 0x66);
        cpl_override = 0;
        if (cpu_state.abrt) return 0;
        if ((t+(port>>3))>tr.limit) return 1;
        cpl_override = 1;
        d = readmemb386l(0, tr.base + t + (port >> 3));
        cpl_override = 0;
        return d&(1<<(port&7));
}

int xout=0;


#define divexcp() { \
                x86_int(0); \
}

int divl(uint32_t val)
{
         uint64_t num, quo;
         uint32_t rem, quo32;
 
        if (val==0) 
        {
                divexcp();
                return 1;
        }

         num=(((uint64_t)EDX)<<32)|EAX;
         quo=num/val;
         rem=num%val;
         quo32=(uint32_t)(quo&0xFFFFFFFF);

        if (quo!=(uint64_t)quo32) 
        {
                divexcp();
                return 1;
        }
        EDX=rem;
        EAX=quo32;
        return 0;
}
int idivl(int32_t val)
{
         int64_t num, quo;
         int32_t rem, quo32;
 
        if (val==0) 
        {       
                divexcp();
                return 1;
        }

         num=(((uint64_t)EDX)<<32)|EAX;
         quo=num/val;
         rem=num%val;
         quo32=(int32_t)(quo&0xFFFFFFFF);

        if (quo!=(int64_t)quo32) 
        {
                divexcp();
                return 1;
        }
        EDX=rem;
        EAX=quo32;
        return 0;
}


void cpu_386_flags_extract()
{
        flags_extract();
}
void cpu_386_flags_rebuild()
{
        flags_rebuild();
}

int oldi;

uint32_t testr[9];
int dontprint=0;

#define OP_TABLE(name) ops_ ## name
#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"


#define CACHE_ON() (!(cr0 & (1 << 30)) /*&& (cr0 & 1)*/ && !(flags & T_FLAG))

#ifdef USE_DYNAREC
static int cycles_main = 0;

void exec386_dynarec(int cycs)
{
        uint8_t temp;
        uint32_t addr;
        int tempi;
        int cycdiff;
        int oldcyc;
	uint32_t start_pc = 0;

        int cyc_period = cycs / 2000; /*5us*/

        cycles_main += cycs;
        while (cycles_main > 0)
        {
                int cycles_start;

		cycles += cyc_period;
                cycles_start = cycles;

                timer_start_period(cycles << TIMER_SHIFT);
        while (cycles>0)
        {
                oldcs = CS;
                cpu_state.oldpc = cpu_state.pc;
                oldcpl = CPL;
                cpu_state.op32 = use32;


                cycdiff=0;
                oldcyc=cycles;
                if (!CACHE_ON()) /*Interpret block*/
                {
                        cpu_block_end = 0;
			x86_was_reset = 0;
                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                fetchdat = fastreadl(cs + cpu_state.pc);
                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

                                        cpu_state.pc++;
                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                if (((cs + cpu_state.pc) >> 12) != pccache)
                                        CPU_BLOCK_END();

/*                                if (ssegs)
                                {
                                        ds=oldds;
                                        ss=oldss;
                                        ssegs=0;
                                }*/
                                if (cpu_state.abrt)
                                        CPU_BLOCK_END();
                                if (trap)
                                        CPU_BLOCK_END();

                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();

                                ins++;
                                
/*                                if ((cs + pc) == 4)
                                        fatal("4\n");*/
/*                                if (ins >= 141400000)
                                        output = 3;*/
                        }
                }
                else
                {
                uint32_t phys_addr = get_phys(cs+cpu_state.pc);
                int hash = HASH(phys_addr);
                codeblock_t *block = codeblock_hash[hash];
                int valid_block = 0;
                trap = 0;

                if (block && !cpu_state.abrt)
                {
                        page_t *page = &pages[phys_addr >> 12];

                        /*Block must match current CS, PC, code segment size,
                          and physical address. The physical address check will
                          also catch any page faults at this stage*/
                        valid_block = (block->pc == cs + cpu_state.pc) && (block->_cs == cs) &&
                                      (block->phys == phys_addr) && !((block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                                      ((block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                        if (!valid_block)
                        {
                                uint64_t mask = (uint64_t)1 << ((phys_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                                
                                if (page->code_present_mask[(phys_addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] & mask)
                                {
                                        /*Walk page tree to see if we find the correct block*/
                                        codeblock_t *new_block = codeblock_tree_find(phys_addr, cs);
                                        if (new_block)
                                        {
                                                valid_block = (new_block->pc == cs + cpu_state.pc) && (new_block->_cs == cs) &&
                                                                (new_block->phys == phys_addr) && !((new_block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                                                                ((new_block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                                                if (valid_block)
                                                        block = new_block;
                                        }
                                }
                        }

                        if (valid_block && (block->page_mask & *block->dirty_mask))
                        {
                                codegen_check_flush(page, page->dirty_mask[(phys_addr >> 10) & 3], phys_addr);
                                page->dirty_mask[(phys_addr >> 10) & 3] = 0;
                                if (!block->valid)
                                        valid_block = 0;
                        }
                        if (valid_block && block->page_mask2)
                        {
                                /*We don't want the second page to cause a page
                                  fault at this stage - that would break any
                                  code crossing a page boundary where the first
                                  page is present but the second isn't. Instead
                                  allow the first page to be interpreted and for
                                  the page fault to occur when the page boundary
                                  is actually crossed.*/
                                uint32_t phys_addr_2 = get_phys_noabrt(block->endpc);
                                page_t *page_2 = &pages[phys_addr_2 >> 12];

                                if ((block->phys_2 ^ phys_addr_2) & ~0xfff)
                                        valid_block = 0;
                                else if (block->page_mask2 & *block->dirty_mask2)
                                {
                                        codegen_check_flush(page_2, page_2->dirty_mask[(phys_addr_2 >> 10) & 3], phys_addr_2);
                                        page_2->dirty_mask[(phys_addr_2 >> 10) & 3] = 0;
                                        if (!block->valid)
                                                valid_block = 0;
                                }
                        }
                        if (valid_block && block->was_recompiled && (block->flags & CODEBLOCK_STATIC_TOP) && block->TOP != cpu_state.TOP)
                        {
                                /*FPU top-of-stack does not match the value this block was compiled
                                  with, re-compile using dynamic top-of-stack*/
                                block->flags &= ~CODEBLOCK_STATIC_TOP;
                                block->was_recompiled = 0;
                        }
                }

                if (valid_block && block->was_recompiled)
                {
                        void (*code)() = (void (*)())&block->data[BLOCK_START];

                        codeblock_hash[hash] = block;

inrecomp=1;
                        code();
inrecomp=0;
                        if (!use32) cpu_state.pc &= 0xffff;
                        cpu_recomp_blocks++;
                }
                else if (valid_block && !cpu_state.abrt)
                {
                        start_pc = cpu_state.pc;
                        
                        cpu_block_end = 0;
                        x86_was_reset = 0;

                        cpu_new_blocks++;
                        
                        codegen_block_start_recompile(block);
                        codegen_in_recompile = 1;

                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                fetchdat = fastreadl(cs + cpu_state.pc);
                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

                                        cpu_state.pc++;
                                                
                                        codegen_generate_call(opcode, x86_opcodes[(opcode | cpu_state.op32) & 0x3ff], fetchdat, cpu_state.pc, cpu_state.pc-1);

                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                        if (x86_was_reset)
                                                break;
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                /*Cap source code at 4000 bytes per block; this
                                  will prevent any block from spanning more than
                                  2 pages. In practice this limit will never be
                                  hit, as host block size is only 2kB*/
                                if ((cpu_state.pc - start_pc) > 1000)
                                        CPU_BLOCK_END();
                                        
                                if (trap)
                                        CPU_BLOCK_END();

                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();


                                if (cpu_state.abrt)
                                {
                                        codegen_block_remove();
                                        CPU_BLOCK_END();
                                }

                                ins++;
                        }
                        
                        if (!cpu_state.abrt && !x86_was_reset)
                                codegen_block_end_recompile(block);
                        
                        if (x86_was_reset)
                                codegen_reset();

                        codegen_in_recompile = 0;
                }
                else if (!cpu_state.abrt)
                {
                        /*Mark block but do not recompile*/
                        start_pc = cpu_state.pc;

                        cpu_block_end = 0;
                        x86_was_reset = 0;

                        codegen_block_init(phys_addr);

                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                codegen_endpc = (cs + cpu_state.pc) + 8;
                                fetchdat = fastreadl(cs + cpu_state.pc);

                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

                                        cpu_state.pc++;
                                                
                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                        if (x86_was_reset)
                                                break;
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                /*Cap source code at 4000 bytes per block; this
                                  will prevent any block from spanning more than
                                  2 pages. In practice this limit will never be
                                  hit, as host block size is only 2kB*/
                                if ((cpu_state.pc - start_pc) > 1000)
                                        CPU_BLOCK_END();
                                        
                                if (trap)
                                        CPU_BLOCK_END();

                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();


                                if (cpu_state.abrt)
                                {
                                        codegen_block_remove();
                                        CPU_BLOCK_END();
                                }

                                ins++;
                        }
                        
                        if (!cpu_state.abrt && !x86_was_reset)
                                codegen_block_end();
                        
                        if (x86_was_reset)
                                codegen_reset();
                }
                }

                cycdiff=oldcyc-cycles;
                tsc += cycdiff;
                
                if (cpu_state.abrt)
                {
                        flags_rebuild();
                        tempi = cpu_state.abrt;
                        cpu_state.abrt = 0;
                        x86_doabrt(tempi);
                        if (cpu_state.abrt)
                        {
                                cpu_state.abrt = 0;
                                CS = oldcs;
                                cpu_state.pc = cpu_state.oldpc;
                                ERRLOG("CPU: double fault %i\n", ins);
                                pmodeint(8, 0);
                                if (cpu_state.abrt)
                                {
                                        cpu_state.abrt = 0;
                                        cpu_reset(0);
					cpu_set_edx();
                                        ERRLOG("CPU: triple fault - reset\n");
                                }
                        }
                }
                
                if (trap)
                {

                        flags_rebuild();
                        if (msw&1)
                        {
                                pmodeint(1,0);
                        }
                        else
                        {
                                writememw(ss,(SP-2)&0xFFFF,flags);
                                writememw(ss,(SP-4)&0xFFFF,CS);
                                writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                SP-=6;
                                addr = (1 << 2) + idt.base;
                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                cpu_state.pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                        }
                }
                else if (nmi && nmi_enable && nmi_mask)
                {
                        cpu_state.oldpc = cpu_state.pc;
                        oldcs = CS;
                        x86_int(2);
                        nmi_enable = 0;
                        if (nmi_auto_clear)
                        {
                                nmi_auto_clear = 0;
                                nmi = 0;
                        }
                }
                else if (flags&I_FLAG)
                {
                        temp=pic_interrupt();
                        if (temp!=0xFF)
                        {
                                CPU_BLOCK_END();
                                flags_rebuild();
                                if (msw&1)
                                {
                                        pmodeint(temp,0);
                                }
                                else
                                {
                                        writememw(ss,(SP-2)&0xFFFF,flags);
                                        writememw(ss,(SP-4)&0xFFFF,CS);
                                        writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                        SP-=6;
                                        addr=temp<<2;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=cpu_state.pc;
                                        cpu_state.pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                }
                        }
                }
        }
                timer_end_period(cycles << TIMER_SHIFT);
                cycles_main -= (cycles_start - cycles);
        }
}
#endif
