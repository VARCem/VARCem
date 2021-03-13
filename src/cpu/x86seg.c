/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		x86 CPU segment emulation.
 *
 * Version:	@(#)x86seg.c	1.0.11	2020/12/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <http://pcem-emulator.co.uk/>
 *
 *		Copyright 2018,2020 Fred N. van Kempen.
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
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../emu.h"
#include "../timer.h"
#include "../mem.h"
#include "../nvr.h"
#include "cpu.h"
#include "x86.h"
#include "386.h"
#include "386_common.h"


/*Controls whether the accessed bit in a descriptor is set when CS is loaded.*/
#define CS_ACCESSED

/*Controls whether the accessed bit in a descriptor is set when a data or stack
  selector is loaded.*/
#define SEL_ACCESSED
int stimes = 0;
int dtimes = 0;
int btimes = 0;

uint32_t abrt_error;
int cgate16,cgate32;

#define breaknullsegs 0

int intgatesize;

void taskswitch286(uint16_t seg, uint16_t *segdat, int is32);
void taskswitch386(uint16_t seg, uint16_t *segdat);

void pmodeint(int num, int soft);
/*NOT PRESENT is INT 0B
  GPF is INT 0D*/


void
x86abort(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pclog_ex(fmt, ap);
    va_end(ap);

    pclog(-1, NULL);

    nvr_save();
    cpu_dumpregs(1);

    exit(-1);
}

uint8_t opcode2;

static void seg_reset(x86seg *s)
{
        s->access = (0 << 5) | 2 | 0x80;
        s->limit = 0xFFFF;
        s->limit_low = 0;
        s->limit_high = 0xffff;
        if(s == &cpu_state.seg_cs)
        {
                // TODO - When the PC is reset, initialization of the CS descriptor must be like the annotated line below.
                //s->base = AT ? (cpu_16bitbus ? 0xFF0000 : 0xFFFF0000) : 0xFFFF0;
                s->base = AT ? 0xF0000 : 0xFFFF0;
                s->seg = AT ? 0xF000 : 0xFFFF;
        }
        else
        {
                s->base = 0;
                s->seg = 0;
        }

}

void x86seg_reset()
{
        seg_reset(&cpu_state.seg_cs);
        seg_reset(&cpu_state.seg_ds);
        seg_reset(&cpu_state.seg_es);
        seg_reset(&cpu_state.seg_fs);
        seg_reset(&cpu_state.seg_gs);
        seg_reset(&cpu_state.seg_ss);
}

void x86_doabrt(int x86_abrt)
{
        CS = oldcs;
        cpu_state.pc = cpu_state.oldpc;
        cpu_state.seg_cs.access = (oldcpl << 5) | 0x80;

        if (msw & 1)
                pmodeint(x86_abrt, 0);
        else
        {
                uint32_t addr = (x86_abrt << 2) + idt.base;
                if (stack32)
                {
                        writememw(ss,ESP-2,cpu_state.flags);
                        writememw(ss,ESP-4,CS);
                        writememw(ss,ESP-6,cpu_state.pc);
                        ESP-=6;
                }
                else
                {
                        writememw(ss,((SP-2)&0xFFFF),cpu_state.flags);
                        writememw(ss,((SP-4)&0xFFFF),CS);
                        writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                        SP-=6;
                }

                cpu_state.flags&=~I_FLAG;
                cpu_state.flags&=~T_FLAG;
                oxpc=cpu_state.pc;
                cpu_state.pc=readmemw(0,addr);
                loadcs(readmemw(0,addr+2));
                return;
        }
        
        if (cpu_state.abrt || x86_was_reset) return;
        
        if (intgatesize == 16)
        {
                if (stack32)
                {
                        writememw(ss, ESP-2, abrt_error);
                        ESP-=2;
                }
                else
                {
                        writememw(ss, ((SP-2)&0xFFFF), abrt_error);
                        SP-=2;
                }
        }
        else
        {
                if (stack32)
                {
                        writememl(ss, ESP-4, abrt_error);
                        ESP-=4;
                }
                else
                {
                        writememl(ss, ((SP-4)&0xFFFF), abrt_error);
                        SP-=4;
                }
        }
}
void x86gpf(char *s, uint16_t error)
{
        cpu_state.abrt = ABRT_GPF;
        abrt_error = error;
}
void x86ss(char *s, uint16_t error)
{
        cpu_state.abrt = ABRT_SS;
        abrt_error = error;
}
void x86ts(char *s, uint16_t error)
{
        cpu_state.abrt = ABRT_TS;
        abrt_error = error;
}
void x86np(char *s, uint16_t error)
{
        cpu_state.abrt = ABRT_NP;
        abrt_error = error;
}


static void set_stack32(int s)
{
        stack32 = s;
	if (stack32)
	       cpu_cur_status |= CPU_STATUS_STACK32;
	else
	       cpu_cur_status &= ~CPU_STATUS_STACK32;
}

static void set_use32(int u)
{
        if (u) 
        {
                use32 = 0x300;
                cpu_cur_status |= CPU_STATUS_USE32;
        }
        else
        {
                use32 = 0;
                cpu_cur_status &= ~CPU_STATUS_USE32;
        }
}

void do_seg_load(x86seg *s, uint16_t *segdat)
{
        s->limit = segdat[0] | ((segdat[3] & 0xF) << 16);
        if (segdat[3] & 0x80)
                s->limit = (s->limit << 12) | 0xFFF;
        s->base = segdat[1] | ((segdat[2] & 0xFF) << 16);
        if (is386)
                s->base |= ((segdat[3] >> 8) << 24);
        s->access = segdat[2] >> 8;
                        
        if ((segdat[2] & 0x1800) != 0x1000 || !(segdat[2] & (1 << 10))) /*expand-down*/
        {
                s->limit_high = s->limit;
                s->limit_low = 0;
        }
        else
        {
                s->limit_high = (segdat[3] & 0x40) ? 0xffffffff : 0xffff;
                s->limit_low = s->limit + 1;
        }

        if (s == &cpu_state.seg_ds)
        {
                if (s->base == 0 && s->limit_low == 0 && s->limit_high == 0xffffffff)
                        cpu_cur_status &= ~CPU_STATUS_NOTFLATDS;
                else
                        cpu_cur_status |= CPU_STATUS_NOTFLATDS;
        }
        if (s == &cpu_state.seg_ss)
        {
                if (s->base == 0 && s->limit_low == 0 && s->limit_high == 0xffffffff)
                        cpu_cur_status &= ~CPU_STATUS_NOTFLATSS;
                else
                        cpu_cur_status |= CPU_STATUS_NOTFLATSS;
        }
}

static void do_seg_v86_init(x86seg *s)
{
        s->access = (3 << 5) | 2 | 0x80;
        s->limit = 0xffff;
        s->limit_low = 0;
        s->limit_high = 0xffff;
}

static void check_seg_valid(x86seg *s)
{
        int dpl = (s->access >> 5) & 3;
        int valid = 1;

        if (s->seg & 4)
        {
                if ((s->seg & ~7U) >= ldt.limit)
                {
                        valid = 0;
                }
        }
        else
        {
                if ((s->seg & ~7U) >= gdt.limit)
                {
                        valid = 0;
                }
        }

        switch (s->access & 0x1f)
        {
                case 0x10: case 0x11: case 0x12: case 0x13: /*Data segments*/
                case 0x14: case 0x15: case 0x16: case 0x17:
                case 0x1A: case 0x1B: /*Readable non-conforming code*/
                if ((s->seg & 3) > dpl || (CPL) > dpl)
                {
                        valid = 0;
                        break;
                }
                break;
                
                case 0x1E: case 0x1F: /*Readable conforming code*/
                break;
                
                default:
                valid = 0;
                break;
        }
        
        if (!valid)
                loadseg(0, s);
}

void loadseg(uint16_t seg, x86seg *s)
{
        uint16_t segdat[4];
        uint32_t addr;
        int dpl;

        if (msw&1 && !(cpu_state.eflags&VM_FLAG))
        {
                if (!(seg&~3))
                {
                        if (s==&cpu_state.seg_ss)
                        {
                                x86ss(NULL,0);
                                return;
                        }
                        s->seg=0;
                        s->access = 0x80;
                        s->base=-1;
                        if (s == &cpu_state.seg_ds)
                                cpu_cur_status |= CPU_STATUS_NOTFLATDS;
                        return;
                }
                addr=seg&~7;
                if (seg&4)
                {
#if 0
                        if (addr>=ldt.limit)
#else
                        if ((addr+7)>ldt.limit)
#endif
                        {
                                x86gpf("loadseg(): Bigger than LDT limit",seg&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
#if 0
                        if (addr>=gdt.limit)
#else
                        if ((addr+7)>gdt.limit)
#endif
                        {
                                x86gpf("loadseg(): Bigger than GDT limit",seg&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat[0]=readmemw(0,addr);
                segdat[1]=readmemw(0,addr+2);
                segdat[2]=readmemw(0,addr+4);
                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                dpl=(segdat[2]>>13)&3;
                if (s==&cpu_state.seg_ss)
                {
                        if (!(seg&~3))
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        if ((seg&3)!=CPL || dpl!=CPL)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        switch ((segdat[2]>>8)&0x1F)
                        {
                                case 0x12: case 0x13: case 0x16: case 0x17: /*r/w*/
                                break;
                                default:
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        if (!(segdat[2]&0x8000))
                        {
                                x86ss(NULL,seg&~3);
                                return;
                        }
                        set_stack32((segdat[3] & 0x40) ? 1 : 0);
                }
                else if (s!=&cpu_state.seg_cs)
                {
#if 0
                        DEBUG("Seg data %04X %04X %04X %04X\n", segdat[0], segdat[1], segdat[2], segdat[3]);
                        DEBUG("Seg type %03X\n",segdat[2]&0x1F00);
#endif
                        switch ((segdat[2]>>8)&0x1F)
                        {
                                case 0x10: case 0x11: case 0x12: case 0x13: /*Data segments*/
                                case 0x14: case 0x15: case 0x16: case 0x17:
                                case 0x1A: case 0x1B: /*Readable non-conforming code*/
                                if ((seg&3)>dpl || (CPL)>dpl)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                break;
                                case 0x1E: case 0x1F: /*Readable conforming code*/
                                break;
                                default:
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                }

                if (!(segdat[2] & 0x8000))
                {
                        x86np("Load data seg not present", seg & 0xfffc);
                        return;
                }
                s->seg = seg;
                do_seg_load(s, segdat);

#ifndef CS_ACCESSED
                if (s != &cpu_state.seg_cs)
                {
#endif                   
#ifdef SEL_ACCESSED         
                        cpl_override = 1;
                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                        cpl_override = 0;
#endif
#ifndef CS_ACCESSED
                }
#endif
                s->checked = 0;
#ifdef USE_DYNAREC
                if (s == &cpu_state.seg_ds)
                        codegen_flat_ds = 0;
                if (s == &cpu_state.seg_ss)
                        codegen_flat_ss = 0;
#endif
        }
        else
        {
                s->access = (3 << 5) | 2 | 0x80;
                s->base = seg << 4;
                s->seg = seg;
                s->checked = 1;
#ifdef USE_DYNAREC
                if (s == &cpu_state.seg_ds)
                        codegen_flat_ds = 0;
                if (s == &cpu_state.seg_ss)
                        codegen_flat_ss = 0;
#endif
                if (s == &cpu_state.seg_ss && (cpu_state.eflags & VM_FLAG))
                        set_stack32(0);
        }
        
        if (s == &cpu_state.seg_ds)
        {
                if (s->base == 0 && s->limit_low == 0 && s->limit_high == 0xffffffff)
                        cpu_cur_status &= ~CPU_STATUS_NOTFLATDS;
                else
                       cpu_cur_status |= CPU_STATUS_NOTFLATDS;
        }
        if (s == &cpu_state.seg_ss)
        {
                if (s->base == 0 && s->limit_low == 0 && s->limit_high == 0xffffffff)
                        cpu_cur_status &= ~CPU_STATUS_NOTFLATSS;
                else
                        cpu_cur_status |= CPU_STATUS_NOTFLATSS;
        }
}

#define DPL ((segdat[2]>>13)&3)
#define DPL2 ((segdat2[2]>>13)&3)
#define DPL3 ((segdat3[2]>>13)&3)

void loadcs(uint16_t seg)
{
        uint16_t segdat[4];
        uint32_t addr;
#if 0
        DEBUG("Load CS %04X\n",seg);
#endif
        if (msw&1 && !(cpu_state.eflags&VM_FLAG))
        {
                if (!(seg&~3))
                {
                        x86gpf(NULL,0);
                        return;
                }
                addr=seg&~7;
                if (seg&4)
                {
                        if (addr>=ldt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat[0]=readmemw(0,addr);
                segdat[1]=readmemw(0,addr+2);
                segdat[2]=readmemw(0,addr+4);
                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                if (segdat[2]&0x1000) /*Normal code segment*/
                {
                        if (!(segdat[2]&0x400)) /*Not conforming*/
                        {
                                if ((seg&3)>CPL)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                if (CPL != DPL)
                                {
                                        x86gpf("loadcs(): CPL != DPL",seg&~3);
                                        return;
                                }
                        }
                        if (CPL < DPL)
                        {
                                x86gpf("loadcs(): CPL < DPL",seg&~3);
                                return;
                        }
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Load CS not present", seg & 0xfffc);
                                return;
                        }
                        set_use32(segdat[3] & 0x40);
                        CS=(seg&~3)|CPL;
                        do_seg_load(&cpu_state.seg_cs, segdat);
                        use32=(segdat[3]&0x40)?0x300:0;
                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                        oldcpl = CPL;

#ifdef CS_ACCESSED                        
                        cpl_override = 1;
                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                        cpl_override = 0;
#endif
                }
                else /*System segment*/
                {
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Load CS system seg not present\n", seg & 0xfffc);
                                return;
                        }
#if 1
                        x86gpf(NULL,seg&~3);
                        return;
#else
                        switch (segdat[2]&0xF00)
                        {
                                default:
                                x86gpf(NULL,seg&~3);
                                return;
                        }
#endif
                }
        }
        else
        {
                cs=seg<<4;
                cpu_state.seg_cs.limit=0xFFFF;
                cpu_state.seg_cs.limit_low = 0;
                cpu_state.seg_cs.limit_high = 0xffff;
                CS=seg & 0xFFFF;
                if (cpu_state.eflags&VM_FLAG) cpu_state.seg_cs.access=(3<<5) | 2 | 0x80;
                else                cpu_state.seg_cs.access=(0<<5) | 2 | 0x80;
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
        }
}

void loadcsjmp(uint16_t seg, uint32_t oxpc)
{
        uint16_t segdat[4];
        uint32_t addr;
        uint16_t type,seg2;
        uint32_t newpc;
        if (msw&1 && !(cpu_state.eflags&VM_FLAG))
        {
                if (!(seg&~3))
                {
                        x86gpf(NULL,0);
                        return;
                }
                addr=seg&~7;
                if (seg&4)
                {
                        if (addr>=ldt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat[0]=readmemw(0,addr);
                segdat[1]=readmemw(0,addr+2);
                segdat[2]=readmemw(0,addr+4);
                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
#if 0
                DEBUG("%04X %04X %04X %04X\n",segdat[0],segdat[1],segdat[2],segdat[3]);
#endif
                if (segdat[2]&0x1000) /*Normal code segment*/
                {
                        if (!(segdat[2]&0x400)) /*Not conforming*/
                        {
                                if ((seg&3)>CPL)
                                {
                                        x86gpf("loadcsjmp(): segment PL > CPL",seg&~3);
                                        return;
                                }
                                if (CPL != DPL)
                                {
                                        x86gpf("loadcsjmp(): CPL != DPL",seg&~3);
                                        return;
                                }
                        }
                        if (CPL < DPL)
                        {
                                x86gpf("loadcsjmp(): CPL < DPL",seg&~3);
                                return;
                        }
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Load CS JMP not present\n", seg & 0xfffc);
                                return;
                        }
                        set_use32(segdat[3]&0x40);

#ifdef CS_ACCESSED                        
                        cpl_override = 1;
                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                        cpl_override = 0;
#endif
                        
                        CS = (seg & ~3) | CPL;
                        segdat[2] = (segdat[2] & ~(3 << (5+8))) | (CPL << (5+8));

                        do_seg_load(&cpu_state.seg_cs, segdat);
                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                        oldcpl = CPL;
                        cycles -= timing_jmp_pm;
                }
                else /*System segment*/
                {
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Load CS JMP system selector not present\n", seg & 0xfffc);
                                return;
                        }
                        type=segdat[2]&0xF00;
                        newpc=segdat[0];
                        if (type&0x800) newpc|=segdat[3]<<16;
                        switch (type)
                        {
                                case 0x400: /*Call gate*/
                                case 0xC00:
                                cgate32=(type&0x800);
                                cgate16=!cgate32;
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                if ((DPL < CPL) || (DPL < (seg&3)))
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                if (DPL < CPL)
                                {
                                        x86gpf("loadcsjmp(): ex DPL < CPL",seg&~3);
                                        return;
                                }
                                if ((DPL < (seg&3)))
                                {
                                        x86gpf("loadcsjmp(): ex (DPL < (seg&3))",seg&~3);
                                        return;
                                }
                                if (!(segdat[2]&0x8000))
                                {
                                        x86np("Load CS JMP call gate not present\n", seg & 0xfffc);
                                        return;
                                }
                                seg2=segdat[1];

                                if (!(seg2&~3))
                                {
                                        x86gpf(NULL,0);
                                        return;
                                }
                                addr=seg2&~7;
                                if (seg2&4)
                                {
                                        if (addr>=ldt.limit)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        addr+=ldt.base;
                                }
                                else
                                {
                                        if (addr>=gdt.limit)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        addr+=gdt.base;
                                }
                                cpl_override=1;
                                segdat[0]=readmemw(0,addr);
                                segdat[1]=readmemw(0,addr+2);
                                segdat[2]=readmemw(0,addr+4);
                                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;

                                if (DPL > CPL)
                                {
                                        x86gpf("loadcsjmp(): ex DPL > CPL",seg2&~3);
                                        return;
                                }
                                if (!(segdat[2]&0x8000))
                                {
                                        x86np("Load CS JMP from call gate not present\n", seg2 & 0xfffc);
                                        return;
                                }


                                switch (segdat[2]&0x1F00)
                                {
                                        case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming code*/
                                        if (DPL > CPL)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                                        CS=seg2;
                                        do_seg_load(&cpu_state.seg_cs, segdat);
                                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                                        oldcpl = CPL;
                                        set_use32(segdat[3]&0x40);
                                        cpu_state.pc=newpc;

#ifdef CS_ACCESSED                                                
                                        cpl_override = 1;
                                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                                        cpl_override = 0;
#endif
                                        break;

                                        default:
                                        x86gpf(NULL,seg2&~3);
                                        return;
                                }
                                cycles -= timing_jmp_pm_gate;
                                break;

                                
                                case 0x100: /*286 Task gate*/
                                case 0x900: /*386 Task gate*/
                                cpu_state.pc=oxpc;
                                optype=JMP;
                                cpl_override=1;
                                taskswitch286(seg,segdat,segdat[2]&0x800);
                                cpu_state.flags &= ~NT_FLAG;
                                cpl_override=0;
                                return;

                                default:
                                x86gpf(NULL,0);
                                return;
                        }
                }
        }
        else
        {
                cs=seg<<4;
                cpu_state.seg_cs.limit=0xFFFF;
                cpu_state.seg_cs.limit_low = 0;
                cpu_state.seg_cs.limit_high = 0xffff;
                CS=seg;
                if (cpu_state.eflags&VM_FLAG) cpu_state.seg_cs.access=(3<<5) | 2 | 0x80;
                else                cpu_state.seg_cs.access=(0<<5) | 2 | 0x80;
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                cycles -= timing_jmp_rm;
        }
}

void PUSHW(uint16_t v)
{
        if (stack32) {
                writememw(ss,ESP-2,v);
                if (cpu_state.abrt) return;
                ESP-=2;
        }
        else {
                writememw(ss,((SP-2)&0xFFFF),v);
                if (cpu_state.abrt) return;
                SP-=2;
        }
}
void PUSHL(uint32_t v)
{
        if (stack32)
        {
                writememl(ss,ESP-4,v);
                if (cpu_state.abrt) return;
                ESP-=4;
        }
        else
        {
                writememl(ss,((SP-4)&0xFFFF),v);
                if (cpu_state.abrt) return;
                SP-=4;
        }
}
uint16_t POPW()
{
        uint16_t tempw;
        if (stack32)
        {
                tempw=readmemw(ss,ESP);
                if (cpu_state.abrt) return 0;
                ESP+=2;
        }
        else
        {
                tempw=readmemw(ss,SP);
                if (cpu_state.abrt) return 0;
                SP+=2;
        }
        return tempw;
}
uint32_t POPL()
{
        uint32_t templ;
        if (stack32)
        {
                templ=readmeml(ss,ESP);
                if (cpu_state.abrt) return 0;
                ESP+=4;
        }
        else
        {
                templ=readmeml(ss,SP);
                if (cpu_state.abrt) return 0;
                SP+=4;
        }
        return templ;
}

void loadcscall(uint16_t seg)
{
        uint16_t seg2;
        uint16_t segdat[4],segdat2[4],newss;
        uint32_t addr,oldssbase=ss, oaddr;
        uint32_t newpc;
        int count;
        uint32_t oldss,oldsp,newsp, oldsp2;
        int type;
        uint16_t tempw;
        
        if (msw&1 && !(cpu_state.eflags&VM_FLAG))
        {
#if 0
                DEBUG("Protected mode CS load! %04X\n",seg);
#endif
                if (!(seg&~3))
                {
                        x86gpf(NULL,0);
                        return;
                }
                addr=seg&~7;
                if (seg&4)
                {
                        if (addr>=ldt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat[0]=readmemw(0,addr);
                segdat[1]=readmemw(0,addr+2);
                segdat[2]=readmemw(0,addr+4);
                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                type=segdat[2]&0xF00;
                newpc=segdat[0];
                if (type&0x800) newpc|=segdat[3]<<16;

#if 0
                DEBUG("Code seg call - %04X - %04X %04X %04X\n",seg,segdat[0],segdat[1],segdat[2]);
#endif
                if (segdat[2]&0x1000)
                {
                        if (!(segdat[2]&0x400)) /*Not conforming*/
                        {
                                if ((seg&3)>CPL)
                                {
                                        x86gpf("loadcscall(): segment > CPL",seg&~3);
                                        return;
                                }
                                if (CPL != DPL)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                        }
                        if (CPL < DPL)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Load CS call not present", seg & 0xfffc);
                                return;
                        }
                        set_use32(segdat[3]&0x40);

#ifdef CS_ACCESSED                        
                        cpl_override = 1;
                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                        cpl_override = 0;
#endif
                        
                        /*Conforming segments don't change CPL, so preserve existing CPL*/
                        if (segdat[2]&0x400)
                        {
                                seg = (seg & ~3) | CPL;
                                segdat[2] = (segdat[2] & ~(3 << (5+8))) | (CPL << (5+8));
                        }
                        else /*On non-conforming segments, set RPL = CPL*/
                                seg = (seg & ~3) | CPL;
                        CS=seg;
                        do_seg_load(&cpu_state.seg_cs, segdat);
                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                        oldcpl = CPL;
#if 0
                        DEBUG("Complete\n");
#endif
                        cycles -= timing_call_pm;
                }
                else
                {
                        type=segdat[2]&0xF00;
#if 0
                        DEBUG("Type %03X\n",type);
#endif
                        switch (type)
                        {
                                case 0x400: /*Call gate*/
                                case 0xC00: /*386 Call gate*/
#if 0
                                DEBUG("Callgate %08X\n", cpu_state.pc);
#endif
                                cgate32=(type&0x800);
                                cgate16=!cgate32;
                                oldcs=CS;
                                count=segdat[2]&31;
                                if (DPL < CPL)
                                {
                                        x86gpf("loadcscall(): ex DPL < CPL",seg&~3);
                                        return;
                                }
                                if ((DPL < (seg&3)))
                                {
                                        x86gpf("loadcscall(): ex (DPL < (seg&3))",seg&~3);
                                        return;
                                }
                                if (!(segdat[2]&0x8000))
                                {
#if 0
                                        DEBUG("Call gate not present %04X\n",seg);
#endif
                                        x86np("Call gate not present\n", seg & 0xfffc);
                                        return;
                                }
                                seg2=segdat[1];

#if 0
                                DEBUG("New address : %04X:%08X\n", seg2, newpc);
#endif
                                
                                if (!(seg2&~3))
                                {
                                        x86gpf(NULL,0);
                                        return;
                                }
                                addr=seg2&~7;
                                if (seg2&4)
                                {
                                        if (addr>=ldt.limit)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        addr+=ldt.base;
                                }
                                else
                                {
                                        if (addr>=gdt.limit)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        addr+=gdt.base;
                                }
                                cpl_override=1;
                                segdat[0]=readmemw(0,addr);
                                segdat[1]=readmemw(0,addr+2);
                                segdat[2]=readmemw(0,addr+4);
                                segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                                
#if 0
                                DEBUG("Code seg2 call - %04X - %04X %04X %04X\n",seg2,segdat[0],segdat[1],segdat[2]);
#endif
                                
                                if (DPL > CPL)
                                {
                                        x86gpf("loadcscall(): ex DPL > CPL",seg2&~3);
                                        return;
                                }
                                if (!(segdat[2]&0x8000))
                                {
#if 0
                                        DEBUG("Call gate CS not present %04X\n",seg2);
#endif
                                        x86np("Call gate CS not present", seg2 & 0xfffc);
                                        return;
                                }

                                
                                switch (segdat[2]&0x1F00)
                                {
                                        case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming code*/
                                        if (DPL < CPL)
                                        {
                                                uint16_t oldcs = CS;
						oaddr = addr;
                                                /*Load new stack*/
                                                oldss=SS;
                                                oldsp=oldsp2=ESP;
                                                cpl_override=1;
                                                if (tr.access&8)
                                                {
                                                        addr = 4 + tr.base + (DPL * 8);
                                                        newss=readmemw(0,addr+4);
                                                        newsp=readmeml(0,addr);
                                                }
                                                else
                                                {
                                                        addr = 2 + tr.base + (DPL * 4);
                                                        newss=readmemw(0,addr+2);
                                                        newsp=readmemw(0,addr);
                                                }
                                                cpl_override=0;
                                                if (cpu_state.abrt) return;
#if 0
                                                DEBUG("New stack %04X:%08X\n",newss,newsp);
#endif
                                                if (!(newss&~3))
                                                {
                                                        x86ts(NULL,newss&~3);
                                                        return;
                                                }
                                                addr=newss&~7;
                                                if (newss&4)
                                                {
#if 0
                                                        if (addr>=ldt.limit)
#else
                                                        if ((addr+7)>ldt.limit)
#endif
                                                        {
                                                                x86abort("Bigger than LDT limit %04X %08X %04X CSC SS\n",newss,addr,ldt.limit);
                                                                x86ts(NULL,newss&~3);
                                                                return;
                                                        }
                                                        addr+=ldt.base;
                                                }
                                                else
                                                {
#if 0
                                                        if (addr>=gdt.limit)
#else
                                                        if ((addr+7)>gdt.limit)
#endif
                                                        {
                                                                x86abort("Bigger than GDT limit %04X %04X CSC\n",newss,gdt.limit);
                                                                x86ts(NULL,newss&~3);
                                                                return;
                                                        }
                                                        addr+=gdt.base;
                                                }
                                                cpl_override=1;
#if 0
                                                DEBUG("Read stack seg\n");
#endif
                                                segdat2[0]=readmemw(0,addr);
                                                segdat2[1]=readmemw(0,addr+2);
                                                segdat2[2]=readmemw(0,addr+4);
                                                segdat2[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
#if 0
                                                DEBUG("Read stack seg done!\n");
#endif
                                                if (((newss & 3) != DPL) || (DPL2 != DPL))
                                                {
                                                        x86ts(NULL,newss&~3);
                                                        return;
                                                }
                                                if ((segdat2[2]&0x1A00)!=0x1200)
                                                {
                                                        x86ts(NULL,newss&~3);
                                                        return;
                                                }
                                                if (!(segdat2[2]&0x8000))
                                                {
                                                        x86ss("Call gate loading SS not present\n", newss & 0xfffc);
                                                        return;
                                                }
                                                if (!stack32) oldsp &= 0xFFFF;
                                                SS=newss;
                                                set_stack32((segdat2[3] & 0x40) ? 1 : 0);
                                                if (stack32) ESP=newsp;
                                                else         SP=newsp;
                                                
                                                do_seg_load(&cpu_state.seg_ss, segdat2);

#if 0
                                                DEBUG("Set access 1\n");
#endif

#ifdef SEL_ACCESSED                                                
                                                cpl_override = 1;
                                                writememw(0, addr+4, segdat2[2] | 0x100); /*Set accessed bit*/
                                                cpl_override = 0;
#endif
                                                
                                                CS=seg2;
                                                do_seg_load(&cpu_state.seg_cs, segdat);
                                                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                                                oldcpl = CPL;
                                                set_use32(segdat[3]&0x40);
                                                cpu_state.pc=newpc;
                                                
#if 0
                                                DEBUG("Set access 2\n");
#endif
                                                
#ifdef CS_ACCESSED
                                                cpl_override = 1;
                                                writememw(0, oaddr+4, segdat[2] | 0x100); /*Set accessed bit*/
                                                cpl_override = 0;
#endif

#if 0
                                                DEBUG("Type %04X\n",type);
#endif
                                                if (type==0xC00)
                                                {
                                                        PUSHL(oldss);
                                                        PUSHL(oldsp2);
                                                        if (cpu_state.abrt)
                                                        {
                                                                SS = oldss;
                                                                ESP = oldsp2;
								CS = oldcs;
                                                                return;
                                                        }
                                                        if (count)
                                                        {
                                                                while (count)
                                                                {
                                                                        count--;
                                                                        PUSHL(readmeml(oldssbase,oldsp+(count*4)));
                                                                        if (cpu_state.abrt)
                                                                        {
                                                                                SS = oldss;
                                                                                ESP = oldsp2;
										CS = oldcs;
                                                                                return;
                                                                        }
                                                                }
                                                        }
                                                }
                                                else
                                                {
#if 0
                                                        DEBUG("Stack %04X\n",SP);
#endif
                                                        PUSHW(oldss);
#if 0
                                                        DEBUG("Write SS to %04X:%04X\n",SS,SP);
#endif
                                                        PUSHW(oldsp2);
                                                        if (cpu_state.abrt)
                                                        {
                                                                SS = oldss;
                                                                ESP = oldsp2;
								CS = oldcs;
                                                                return;
                                                        }
#if 0
                                                        DEBUG("Write SP to %04X:%04X\n",SS,SP);
#endif
                                                        if (count)
                                                        {
                                                                while (count)
                                                                {
                                                                        count--;
                                                                        tempw=readmemw(oldssbase,(oldsp&0xFFFF)+(count*2));
#if 0
                                                                        DEBUG("PUSH %04X\n",tempw);
#endif
                                                                        PUSHW(tempw);
                                                                        if (cpu_state.abrt)
                                                                        {
                                                                                SS = oldss;
                                                                                ESP = oldsp2;
										CS = oldcs;
                                                                                return;
                                                                        }
                                                                }
                                                        }
                                                }
                                                cycles -= timing_call_pm_gate_inner;
                                                break;
                                        }
                                        else if (DPL > CPL)
                                        {
                                                x86gpf(NULL,seg2&~3);
                                                return;
                                        }
                                        case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                                        CS=seg2;
                                        do_seg_load(&cpu_state.seg_cs, segdat);
                                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                                        oldcpl = CPL;
                                        set_use32(segdat[3]&0x40);
                                        cpu_state.pc=newpc;

#ifdef CS_ACCESSED                                                
                                        cpl_override = 1;
                                        writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                                        cpl_override = 0;
#endif
                                        cycles -= timing_call_pm_gate;
                                        break;
                                        
                                        default:
                                        x86gpf(NULL,seg2&~3);
                                        return;
                                }
                                break;

                                case 0x100: /*286 Task gate*/
                                case 0x900: /*386 Task gate*/
                                cpu_state.pc=oxpc;
                                cpl_override=1;
                                taskswitch286(seg,segdat,segdat[2]&0x800);
                                cpl_override=0;
                                break;

                                default:
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                }
        }
        else
        {
                cs=seg<<4;
                cpu_state.seg_cs.limit=0xFFFF;
                cpu_state.seg_cs.limit_low = 0;
                cpu_state.seg_cs.limit_high = 0xffff;
                CS=seg;
                if (cpu_state.eflags&VM_FLAG) cpu_state.seg_cs.access=(3<<5) | 2 | 0x80;
                else                cpu_state.seg_cs.access=(0<<5) | 2 | 0x80;
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
        }
}

void pmoderetf(int is32, uint16_t off)
{
        uint32_t newpc;
        uint32_t newsp;
        uint32_t addr, oaddr;
        uint16_t segdat[4],segdat2[4],seg,newss;
        uint32_t oldsp=ESP;
#if 0
        DEBUG("RETF %i %04X:%04X  %08X %04X\n",is32,CS,cpu_state.pc,cr0,cpu_state.eflags);
#endif
        if (is32)
        {
                newpc=POPL();
                seg=POPL(); if (cpu_state.abrt) return;
        }
        else
        {
#if 0
                DEBUG("PC read from %04X:%04X\n",SS,SP);
#endif
                newpc=POPW();
#if 0
                DEBUG("CS read from %04X:%04X\n",SS,SP);
#endif
                seg=POPW(); if (cpu_state.abrt) return;
        }
#if 0
        DEBUG("Return to %04X:%08X\n",seg,newpc);
#endif
        if ((seg&3)<CPL)
        {
                ESP=oldsp;
                x86gpf("pmoderetf(): seg < CPL",seg&~3);
                return;
        }
        if (!(seg&~3))
        {
                x86gpf(NULL,0);
                return;
        }
        addr=seg&~7;
        if (seg&4)
        {
                if (addr>=ldt.limit)
                {
                        x86gpf(NULL,seg&~3);
                        return;
                }
                addr+=ldt.base;
        }
        else
        {
                if (addr>=gdt.limit)
                {
                        x86gpf(NULL,seg&~3);
                        return;
                }
                addr+=gdt.base;
        }
        cpl_override=1;
        segdat[0]=readmemw(0,addr);
        segdat[1]=readmemw(0,addr+2);
        segdat[2]=readmemw(0,addr+4);
        segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) { ESP=oldsp; return; }
        oaddr = addr;
        
#if 0
        DEBUG("CPL %i RPL %i %i\n",CPL,seg&3,is32);
#endif

        if (stack32) ESP+=off;
        else         SP+=off;

        if (CPL==(seg&3))
        {
#if 0
                DEBUG("RETF CPL = RPL  %04X\n", segdat[2]);
#endif
                switch (segdat[2]&0x1F00)
                {
                        case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming*/
                        if (CPL != DPL)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        break;
                        case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                        if (CPL < DPL)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        break;
                        default:
                        x86gpf(NULL,seg&~3);
                        return;
                }
                if (!(segdat[2]&0x8000))
                {
                        ESP=oldsp;
                        x86np("RETF CS not present\n", seg & 0xfffc);
                        return;
                }
                
#ifdef CS_ACCESSED
                cpl_override = 1;
                writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                cpl_override = 0;
#endif
                                
                cpu_state.pc=newpc;
                if (segdat[2] & 0x400)
                   segdat[2] = (segdat[2] & ~(3 << (5+8))) | ((seg & 3) << (5+8));
                CS = seg;
                do_seg_load(&cpu_state.seg_cs, segdat);
                cpu_state.seg_cs.access = (cpu_state.seg_cs.access & ~(3 << 5)) | ((CS & 3) << 5);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                set_use32(segdat[3] & 0x40);

                cycles -= timing_retf_pm;
        }
        else
        {
                switch (segdat[2]&0x1F00)
                {
                        case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming*/
                        if ((seg&3) != DPL)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,seg&~3);
                                return;
                        }
#if 0
                        DEBUG("RETF non-conforming, %i %i\n",seg&3, DPL);
#endif
                        break;
                        case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                        if ((seg&3) < DPL)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,seg&~3);
                                return;
                        }
#if 0
                        DEBUG("RETF conforming, %i %i\n",seg&3, DPL);
#endif
                        break;
                        default:
                        ESP=oldsp;
                        x86gpf(NULL,seg&~3);
                        return;
                }
                if (!(segdat[2]&0x8000))
                {
                        ESP=oldsp;
                        x86np("RETF CS not present\n", seg & 0xfffc);
                        return;
                }
                if (is32)
                {
                        newsp=POPL();
                        newss=POPL(); if (cpu_state.abrt) return;
                }
                else
                {
#if 0
                        DEBUG("SP read from %04X:%04X\n",SS,SP);
#endif
                        newsp=POPW();
#if 0
                        DEBUG("SS read from %04X:%04X\n",SS,SP);
#endif
                        newss=POPW(); if (cpu_state.abrt) return;
                }
#if 0
                DEBUG("Read new stack : %04X:%04X (%08X)\n", newss, newsp, ldt.base);
#endif
                if (!(newss&~3))
                {
                        ESP=oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                addr=newss&~7;
                if (newss&4)
                {
                        if (addr>=ldt.limit)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,newss&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                ESP=oldsp;
                                x86gpf(NULL,newss&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat2[0]=readmemw(0,addr);
                segdat2[1]=readmemw(0,addr+2);
                segdat2[2]=readmemw(0,addr+4);
                segdat2[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) { ESP=oldsp; return; }
#if 0
                DEBUG("Segment data %04X %04X %04X %04X\n", segdat2[0], segdat2[1], segdat2[2], segdat2[3]);
#endif
                if ((newss & 3) != (seg & 3))
                {
                        ESP=oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                if ((segdat2[2]&0x1A00)!=0x1200)
                {
                        ESP=oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                if (!(segdat2[2]&0x8000))
                {
                        ESP=oldsp;
                        x86np("RETF loading SS not present\n", newss & 0xfffc);
                        return;
                }
                if (DPL2 != (seg & 3))
                {
                        ESP=oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                SS=newss;
                set_stack32((segdat2[3] & 0x40) ? 1 : 0);
                if (stack32) ESP=newsp;
                else         SP=newsp;
                do_seg_load(&cpu_state.seg_ss, segdat2);

#ifdef SEL_ACCESSED
                cpl_override = 1;
                writememw(0, addr+4, segdat2[2] | 0x100); /*Set accessed bit*/

#ifdef CS_ACCESSED
                writememw(0, oaddr+4, segdat[2] | 0x100); /*Set accessed bit*/
#endif
                cpl_override = 0;
#endif                
                        /*Conforming segments don't change CPL, so CPL = RPL*/
                        if (segdat[2]&0x400)
                           segdat[2] = (segdat[2] & ~(3 << (5+8))) | ((seg & 3) << (5+8));

                cpu_state.pc=newpc;
                CS=seg;
                do_seg_load(&cpu_state.seg_cs, segdat);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                set_use32(segdat[3] & 0x40);
                
                if (stack32) ESP+=off;
                else         SP+=off;
                
                check_seg_valid(&cpu_state.seg_ds);
                check_seg_valid(&cpu_state.seg_es);
                check_seg_valid(&cpu_state.seg_fs);
                check_seg_valid(&cpu_state.seg_gs);
                cycles -= timing_retf_pm_outer;
        }
}

void restore_stack()
{
        ss=oldss; cpu_state.seg_ss.limit=oldsslimit;
}

void pmodeint(int num, int soft)
{
        uint16_t segdat[4],segdat2[4],segdat3[4];
        uint32_t addr, oaddr;
        uint16_t newss;
        uint32_t oldss,oldsp;
        int type;
        uint32_t newsp;
        uint16_t seg = 0;
        int new_cpl;
        
        if (cpu_state.eflags&VM_FLAG && IOPL!=3 && soft)
        {
#if 0
                DEBUG("V86 banned int\n");
#endif
                x86gpf(NULL,0);
                return;
        }
        addr=(num<<3);
        if (addr>=idt.limit)
        {
                if (num==8)
                {
                        /*Triple fault - reset!*/
                        cpu_reset(0);
			cpu_set_edx();
                }
                else if (num==0xD)
                {
                        pmodeint(8,0);
                }
                else
                {
                        x86gpf(NULL,(num*8)+2+((soft)?0:1));
                }
#if 0
                DEBUG("addr >= IDT.limit\n");
#endif
                return;
        }
        addr+=idt.base;
        cpl_override=1;
        segdat[0]=readmemw(0,addr);
        segdat[1]=readmemw(2,addr);
        segdat[2]=readmemw(4,addr);
        segdat[3]=readmemw(6,addr); cpl_override=0; if (cpu_state.abrt) { /* ERRLOG("Abrt reading from %08X\n",addr); */ return; }
        oaddr = addr;

#if 0
        DEBUG("Addr %08X seg %04X %04X %04X %04X\n",addr,segdat[0],segdat[1],segdat[2],segdat[3]);
#endif
        if (!(segdat[2]&0x1F00))
        {
                x86gpf(NULL,(num*8)+2);
                return;
        }
        if (DPL<CPL && soft)
        {
                x86gpf(NULL,(num*8)+2);
                return;
        }
        type=segdat[2]&0x1F00;
        switch (type)
        {
                case 0x600: case 0x700: case 0xE00: case 0xF00: /*Interrupt and trap gates*/
                        intgatesize=(type>=0x800)?32:16;
                        if (!(segdat[2]&0x8000))
                        {
                                x86np("Int gate not present\n", (num << 3) | 2);
                                return;
                        }
                        seg=segdat[1];
                        new_cpl = seg & 3;
                        
                        addr=seg&~7;
                        if (seg&4)
                        {
                                if (addr>=ldt.limit)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                addr+=ldt.base;
                        }
                        else
                        {
                                if (addr>=gdt.limit)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                addr+=gdt.base;
                        }
                        cpl_override=1;
                        segdat2[0]=readmemw(0,addr);
                        segdat2[1]=readmemw(0,addr+2);
                        segdat2[2]=readmemw(0,addr+4);
                        segdat2[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                        oaddr = addr;
                        
                        if (DPL2 > CPL)
                        {
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                        switch (segdat2[2]&0x1F00)
                        {
                                case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming*/
                                if (DPL2<CPL)
                                {
                                        if (!(segdat2[2]&0x8000))
                                        {
                                                x86np("Int gate CS not present\n", segdat[1] & 0xfffc);
                                                return;
                                        }
                                        if ((cpu_state.eflags&VM_FLAG) && DPL2)
                                        {
                                                x86gpf(NULL,segdat[1]&0xFFFC);
                                                return;
                                        }
                                        /*Load new stack*/
                                        oldss=SS;
                                        oldsp=ESP;
                                        cpl_override=1;
                                        if (tr.access&8)
                                        {
                                                addr = 4 + tr.base + (DPL2 * 8);
                                                newss=readmemw(0,addr+4);
                                                newsp=readmeml(0,addr);
                                        }
                                        else
                                        {
                                                addr = 2 + tr.base + (DPL2 * 4);
                                                newss=readmemw(0,addr+2);
                                                newsp=readmemw(0,addr);
                                        }
                                        cpl_override=0;
                                        if (!(newss&~3))
                                        {
                                                x86ss(NULL,newss&~3);
                                                return;
                                        }
                                        addr=newss&~7;
                                        if (newss&4)
                                        {
                                                if (addr>=ldt.limit)
                                                {
                                                        x86ss(NULL,newss&~3);
                                                        return;
                                                }
                                                addr+=ldt.base;
                                        }
                                        else
                                        {
                                                if (addr>=gdt.limit)
                                                {
                                                        x86ss(NULL,newss&~3);
                                                        return;
                                                }
                                                addr+=gdt.base;
                                        }
                                        cpl_override=1;
                                        segdat3[0]=readmemw(0,addr);
                                        segdat3[1]=readmemw(0,addr+2);
                                        segdat3[2]=readmemw(0,addr+4);
                                        segdat3[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) return;
                                        if (((newss & 3) != DPL2) || (DPL3 != DPL2))
                                        {
                                                x86ss(NULL,newss&~3);
                                                return;
                                        }
                                        if ((segdat3[2]&0x1A00)!=0x1200)
                                        {
                                                x86ss(NULL,newss&~3);
                                                return;
                                        }
                                        if (!(segdat3[2]&0x8000))
                                        {
                                                x86np("Int gate loading SS not present\n", newss & 0xfffc);
                                                return;
                                        }
                                        SS=newss;
                                        set_stack32((segdat3[3] & 0x40) ? 1 : 0);
                                        if (stack32) ESP=newsp;
                                        else         SP=newsp;
                                        do_seg_load(&cpu_state.seg_ss, segdat3);

#ifdef CS_ACCESSED                                        
                                        cpl_override = 1;
                                        writememw(0, addr+4, segdat3[2] | 0x100); /*Set accessed bit*/
                                        cpl_override = 0;
#endif
                                        
#if 0
                                        DEBUG("New stack %04X:%08X\n",SS,ESP);
#endif
                                        cpl_override=1;
                                        if (type>=0x800)
                                        {
                                                if (cpu_state.eflags & VM_FLAG)
                                                {
                                                        PUSHL(GS);
                                                        PUSHL(FS);
                                                        PUSHL(DS);
                                                        PUSHL(ES); if (cpu_state.abrt) return;
                                                        loadseg(0,&cpu_state.seg_ds);
                                                        loadseg(0,&cpu_state.seg_es);
                                                        loadseg(0,&cpu_state.seg_fs);
                                                        loadseg(0,&cpu_state.seg_gs);
                                                }
                                                PUSHL(oldss);
                                                PUSHL(oldsp);
                                                PUSHL(cpu_state.flags|(cpu_state.eflags<<16));
                                                PUSHL(CS);
                                                PUSHL(cpu_state.pc); if (cpu_state.abrt) return;
                                        }
                                        else
                                        {
                                                PUSHW(oldss);
                                                PUSHW(oldsp);
                                                PUSHW(cpu_state.flags);
                                                PUSHW(CS);
                                                PUSHW(cpu_state.pc); if (cpu_state.abrt) return;
                                        }
                                        cpl_override=0;
                                        cpu_state.seg_cs.access=0 | 0x80;
                                        cycles -= timing_int_pm_outer - timing_int_pm;
                                        break;
                                }
                                else if (DPL2!=CPL)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                                if (!(segdat2[2]&0x8000))
                                {
                                        x86np("Int gate CS not present\n", segdat[1] & 0xfffc);
                                        return;
                                }
                                if ((cpu_state.eflags & VM_FLAG) && DPL2<CPL)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                if (type>0x800)
                                {
                                        PUSHL(cpu_state.flags|(cpu_state.eflags<<16));
                                        PUSHL(CS);
                                        PUSHL(cpu_state.pc); if (cpu_state.abrt) return;
                                }
                                else
                                {
                                        PUSHW(cpu_state.flags);
                                        PUSHW(CS);
                                        PUSHW(cpu_state.pc); if (cpu_state.abrt) return;
                                }
                                new_cpl = CS & 3;
                                break;
                                default:
                                x86gpf(NULL,seg&~3);
                                return;
                        }
                do_seg_load(&cpu_state.seg_cs, segdat2);
                CS = (seg & ~3) | new_cpl;
                cpu_state.seg_cs.access = (cpu_state.seg_cs.access & ~(3 << 5)) | (new_cpl << 5);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                if (type>0x800) cpu_state.pc=segdat[0]|(segdat[3]<<16);
                else            cpu_state.pc=segdat[0];
                set_use32(segdat2[3]&0x40);

#ifdef CS_ACCESSED
                cpl_override = 1;
                writememw(0, oaddr+4, segdat2[2] | 0x100); /*Set accessed bit*/
                cpl_override = 0;
#endif
                        
                cpu_state.eflags&=~VM_FLAG;
                cpu_cur_status &= ~CPU_STATUS_V86;
                if (!(type&0x100))
                {
                        cpu_state.flags&=~I_FLAG;
                }
                cpu_state.flags&=~(T_FLAG|NT_FLAG);
                cycles -= timing_int_pm;
                break;
                
                case 0x500: /*Task gate*/
                seg=segdat[1];
                        addr=seg&~7;
                        if (seg&4)
                        {
                                if (addr>=ldt.limit)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                addr+=ldt.base;
                        }
                        else
                        {
                                if (addr>=gdt.limit)
                                {
                                        x86gpf(NULL,seg&~3);
                                        return;
                                }
                                addr+=gdt.base;
                        }
                        cpl_override=1;
                        segdat2[0]=readmemw(0,addr);
                        segdat2[1]=readmemw(0,addr+2);
                        segdat2[2]=readmemw(0,addr+4);
                        segdat2[3]=readmemw(0,addr+6);
                        cpl_override=0; if (cpu_state.abrt) return;
                                if (!(segdat2[2]&0x8000))
                                {
                                        x86np("Int task gate not present\n", segdat[1] & 0xfffc);
                                        return;
                                }
                optype=OPTYPE_INT;
                cpl_override=1;
                taskswitch286(seg,segdat2,segdat2[2]&0x800);
                cpl_override=0;
                break;
                
                default:
                x86gpf(NULL,seg&~3);
                return;
        }
}

void pmodeiret(int is32)
{
        uint32_t newsp;
        uint16_t newss;
        uint32_t tempflags,flagmask;
        uint32_t newpc;
        uint16_t segdat[4],segdat2[4];
        uint16_t segs[4];
        uint16_t seg;
        uint32_t addr, oaddr;
        uint32_t oldsp=ESP;
        if (is386 && (cpu_state.eflags&VM_FLAG))
        {
                if (IOPL!=3)
                {
                        x86gpf(NULL,0);
                        return;
                }
                oxpc=cpu_state.pc;
                if (is32)
                {
                        newpc=POPL();
                        seg=POPL();
                        tempflags=POPL(); if (cpu_state.abrt) return;
                }
                else
                {
                        newpc=POPW();
                        seg=POPW();
                        tempflags=POPW(); if (cpu_state.abrt) return;
                }
                cpu_state.pc=newpc;
                cs=seg<<4;
                cpu_state.seg_cs.limit=0xFFFF;
                cpu_state.seg_cs.limit_low = 0;
                cpu_state.seg_cs.limit_high = 0xffff;
		cpu_state.seg_cs.access |= 0x80;
                CS=seg;
                cpu_state.flags=(cpu_state.flags&0x3000)|(tempflags&0xCFD5)|2;
                cycles -= timing_iret_rm;
                return;
        }

        if (cpu_state.flags&NT_FLAG)
        {
                seg=readmemw(tr.base,0);
                addr=seg&~7;
                if (seg&4)
                {
                        DEBUG("TS LDT %04X %04X IRET\n",seg,gdt.limit);
                        x86ts(NULL,seg&~3);
                        return;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                x86ts(NULL,seg&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat[0]=readmemw(0,addr);
                segdat[1]=readmemw(0,addr+2);
                segdat[2]=readmemw(0,addr+4);
                segdat[3]=readmemw(0,addr+6);
                taskswitch286(seg,segdat,segdat[2] & 0x800);
                cpl_override=0;
                return;
        }
        oxpc=cpu_state.pc;
        flagmask=0xFFFF;
        if (CPL) flagmask&=~0x3000;
        if (IOPL<CPL) flagmask&=~0x200;
        if (is32)
        {
                newpc=POPL();
                seg=POPL();
                tempflags=POPL(); if (cpu_state.abrt) { ESP = oldsp; return; }
                if (is386 && ((tempflags>>16)&VM_FLAG))
                {
                        newsp=POPL();
                        newss=POPL();
                        segs[0]=POPL();
                        segs[1]=POPL();
                        segs[2]=POPL();
                        segs[3]=POPL(); if (cpu_state.abrt) { ESP = oldsp; return; }
                        cpu_state.eflags=tempflags>>16;
                        cpu_cur_status |= CPU_STATUS_V86;
			loadseg(segs[0],&cpu_state.seg_es);
                        do_seg_v86_init(&cpu_state.seg_es);
                        loadseg(segs[1],&cpu_state.seg_ds);
                        do_seg_v86_init(&cpu_state.seg_ds);
			cpu_cur_status |= CPU_STATUS_NOTFLATDS;
                        loadseg(segs[2],&cpu_state.seg_fs);
                        do_seg_v86_init(&cpu_state.seg_fs);
                        loadseg(segs[3],&cpu_state.seg_gs);
                        do_seg_v86_init(&cpu_state.seg_gs);                        
                        
                        cpu_state.pc=newpc & 0xffff;
                        cs=seg<<4;
                        cpu_state.seg_cs.limit=0xFFFF;
                        cpu_state.seg_cs.limit_low = 0;
                        cpu_state.seg_cs.limit_high = 0xffff;
                        CS=seg;
                        cpu_state.seg_cs.access=(3<<5) | 2 | 0x80;
                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                        oldcpl = CPL;

                        ESP=newsp;
                        loadseg(newss,&cpu_state.seg_ss);
                        do_seg_v86_init(&cpu_state.seg_ss);
			cpu_cur_status |= CPU_STATUS_NOTFLATSS;
                        use32=0;
			cpu_cur_status &= ~CPU_STATUS_USE32;
                        cpu_state.flags=(tempflags&0xFFD5)|2;
                        cycles -= timing_iret_v86;
                        return;
                }
        }
        else
        {
                newpc=POPW();
                seg=POPW();
                tempflags=POPW(); if (cpu_state.abrt) { ESP = oldsp; return; }
        }
        if (!(seg&~3))
        {
                ESP = oldsp;
                x86gpf(NULL,0);
                return;
        }

        addr=seg&~7;
        if (seg&4)
        {
                if (addr>=ldt.limit)
                {
                        ESP = oldsp;
                        x86gpf(NULL,seg&~3);
                        return;
                }
                addr+=ldt.base;
        }
        else
        {
                if (addr>=gdt.limit)
                {
                        ESP = oldsp;
                        x86gpf(NULL,seg&~3);
                        return;
                }
                addr+=gdt.base;
        }
        if ((seg&3) < CPL)
        {
                ESP = oldsp;
                x86gpf(NULL,seg&~3);
                return;
        }
        cpl_override=1;
        segdat[0]=readmemw(0,addr);
        segdat[1]=readmemw(0,addr+2);
        segdat[2]=readmemw(0,addr+4);
        segdat[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) { ESP = oldsp; return; }
        
        switch (segdat[2]&0x1F00)
        {
                case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming code*/
                if ((seg&3) != DPL)
                {
                        ESP = oldsp;
                        x86gpf(NULL,seg&~3);
                        return;
                }
                break;
                case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming code*/
                if ((seg&3) < DPL)
                {
                        ESP = oldsp;
                        x86gpf(NULL,seg&~3);
                        return;
                }
                break;
                default:
                ESP = oldsp;
                x86gpf(NULL,seg&~3);
                return;
        }
        if (!(segdat[2]&0x8000))
        {
                ESP = oldsp;
                x86np("IRET CS not present\n", seg & 0xfffc);
                return;
        }
        if ((seg&3) == CPL)
        {
                CS=seg;
                do_seg_load(&cpu_state.seg_cs, segdat);
                cpu_state.seg_cs.access = (cpu_state.seg_cs.access & ~(3 << 5)) | ((CS & 3) << 5);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                set_use32(segdat[3]&0x40);

#ifdef CS_ACCESSED                
                cpl_override = 1;
                writememw(0, addr+4, segdat[2] | 0x100); /*Set accessed bit*/
                cpl_override = 0;
#endif
                cycles -= timing_iret_pm;
        }
        else /*Return to outer level*/
        {
                oaddr = addr;
#if 0
                DEBUG("Outer level\n");
#endif
                if (is32)
                {
                        newsp=POPL();
                        newss=POPL(); if (cpu_state.abrt) { ESP = oldsp; return; }
                }
                else
                {
                        newsp=POPW();
                        newss=POPW(); if (cpu_state.abrt) { ESP = oldsp; return; }
                }
                
#if 0
                DEBUG("IRET load stack %04X:%04X\n",newss,newsp);
#endif
                
                if (!(newss&~3))
                {
                        ESP = oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                addr=newss&~7;
                if (newss&4)
                {
                        if (addr>=ldt.limit)
                        {
                                ESP = oldsp;
                                x86gpf(NULL,newss&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                ESP = oldsp;
                                x86gpf(NULL,newss&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                cpl_override=1;
                segdat2[0]=readmemw(0,addr);
                segdat2[1]=readmemw(0,addr+2);
                segdat2[2]=readmemw(0,addr+4);
                segdat2[3]=readmemw(0,addr+6); cpl_override=0; if (cpu_state.abrt) { ESP = oldsp; return; }
                if ((newss & 3) != (seg & 3))
                {
                        ESP = oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                if ((segdat2[2]&0x1A00)!=0x1200)
                {
                        ESP = oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                if (DPL2 != (seg & 3))
                {
                        ESP = oldsp;
                        x86gpf(NULL,newss&~3);
                        return;
                }
                if (!(segdat2[2]&0x8000))
                {
                        ESP = oldsp;
                        x86np("IRET loading SS not present\n", newss & 0xfffc);
                        return;
                }
                SS=newss;
                set_stack32((segdat2[3] & 0x40) ? 1 : 0);
                if (stack32) ESP=newsp;
                else         SP=newsp;
                do_seg_load(&cpu_state.seg_ss, segdat2);

#ifdef SEL_ACCESSED
                cpl_override = 1;
                writememw(0, addr+4, segdat2[2] | 0x100); /*Set accessed bit*/

#ifdef CS_ACCESSED
                writememw(0, oaddr+4, segdat[2] | 0x100); /*Set accessed bit*/
#endif
                cpl_override = 0;
#endif                
                        /*Conforming segments don't change CPL, so CPL = RPL*/
                        if (segdat[2]&0x400)
                           segdat[2] = (segdat[2] & ~(3 << (5+8))) | ((seg & 3) << (5+8));

                CS=seg;
                do_seg_load(&cpu_state.seg_cs, segdat);
                cpu_state.seg_cs.access = (cpu_state.seg_cs.access & ~(3 << 5)) | ((CS & 3) << 5);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                set_use32(segdat[3] & 0x40);
                        
                check_seg_valid(&cpu_state.seg_ds);
                check_seg_valid(&cpu_state.seg_es);
                check_seg_valid(&cpu_state.seg_fs);
                check_seg_valid(&cpu_state.seg_gs);
                cycles -= timing_iret_pm_outer;
        }
        cpu_state.pc=newpc;
        cpu_state.flags=(cpu_state.flags&~flagmask)|(tempflags&flagmask&0xFFD5)|2;
        if (is32) cpu_state.eflags=tempflags>>16;
}

void taskswitch286(uint16_t seg, uint16_t *segdat, int is32)
{
        uint32_t base;
        uint32_t limit;
        uint32_t templ;
        uint16_t tempw;

	uint32_t new_cr3=0;
	uint16_t new_es,new_cs,new_ss,new_ds,new_fs,new_gs;
	uint16_t new_ldt;

	uint32_t new_eax,new_ebx,new_ecx,new_edx,new_esp,new_ebp,new_esi,new_edi,new_pc,new_flags;

        uint32_t addr;
        
	uint16_t segdat2[4];

        base=segdat[1]|((segdat[2]&0xFF)<<16);
        limit=segdat[0];
        if(is386)
        {
                base |= (segdat[3]>>8)<<24;
                limit |= (segdat[3]&0xF)<<16;
        }

        if (is32)
        {
                if (limit < 103)
                {
                        x86ts(NULL, seg);
                        return;
                }

                if (optype==JMP || optype==CALL || optype==OPTYPE_INT)
                {
                        if (tr.seg&4) tempw=readmemw(ldt.base,(seg&~7)+4);
                        else          tempw=readmemw(gdt.base,(seg&~7)+4);
                        if (cpu_state.abrt) return;
                        tempw|=0x200;
                        if (tr.seg&4) writememw(ldt.base,(seg&~7)+4,tempw);
                        else          writememw(gdt.base,(seg&~7)+4,tempw);
                }
                if (cpu_state.abrt) return;

                if (optype==IRET) cpu_state.flags&=~NT_FLAG;

                cpu_386_flags_rebuild();
                writememl(tr.base,0x1C,cr3);
                writememl(tr.base,0x20,cpu_state.pc);
                writememl(tr.base,0x24,cpu_state.flags|(cpu_state.eflags<<16));
                
                writememl(tr.base,0x28,EAX);
                writememl(tr.base,0x2C,ECX);
                writememl(tr.base,0x30,EDX);
                writememl(tr.base,0x34,EBX);
                writememl(tr.base,0x38,ESP);
                writememl(tr.base,0x3C,EBP);
                writememl(tr.base,0x40,ESI);
                writememl(tr.base,0x44,EDI);
                
                writememl(tr.base,0x48,ES);
                writememl(tr.base,0x4C,CS);
                writememl(tr.base,0x50,SS);
                writememl(tr.base,0x54,DS);
                writememl(tr.base,0x58,FS);
                writememl(tr.base,0x5C,GS);
                
                if (optype==JMP || optype==IRET)
                {
                        if (tr.seg&4) tempw=readmemw(ldt.base,(tr.seg&~7)+4);
                        else          tempw=readmemw(gdt.base,(tr.seg&~7)+4);
                        if (cpu_state.abrt) return;
                        tempw&=~0x200;
                        if (tr.seg&4) writememw(ldt.base,(tr.seg&~7)+4,tempw);
                        else          writememw(gdt.base,(tr.seg&~7)+4,tempw);
                }
                if (cpu_state.abrt) return;
                
                if (optype==OPTYPE_INT || optype==CALL)
                {
                        writememl(base,0,tr.seg);
                        if (cpu_state.abrt)
                                return;
                }
                

                new_cr3=readmeml(base,0x1C);
                new_pc=readmeml(base,0x20);
                new_flags=readmeml(base,0x24);
                if (optype == OPTYPE_INT || optype == CALL)
                        new_flags |= NT_FLAG;                
                        
                new_eax=readmeml(base,0x28);
                new_ecx=readmeml(base,0x2C);
                new_edx=readmeml(base,0x30);
                new_ebx=readmeml(base,0x34);
                new_esp=readmeml(base,0x38);
                new_ebp=readmeml(base,0x3C);
                new_esi=readmeml(base,0x40);
                new_edi=readmeml(base,0x44);

                new_es=readmemw(base,0x48);
                new_cs=readmemw(base,0x4C);
                new_ss=readmemw(base,0x50);
                new_ds=readmemw(base,0x54);
                new_fs=readmemw(base,0x58);
                new_gs=readmemw(base,0x5C);
                new_ldt=readmemw(base,0x60);

                cr0 |= 8;

                cr3=new_cr3;
                flushmmucache();

                cpu_state.pc=new_pc;
                cpu_state.flags=new_flags;
                cpu_state.eflags=new_flags>>16;
                cpu_386_flags_extract();

                ldt.seg=new_ldt;
                templ=(ldt.seg&~7)+gdt.base;
                ldt.limit=readmemw(0,templ);
                if (readmemb(0,templ+6)&0x80)
                {
                        ldt.limit<<=12;
                        ldt.limit|=0xFFF;
                }
                ldt.base=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16)|(readmemb(0,templ+7)<<24);

                if (cpu_state.eflags & VM_FLAG)
                {
                        loadcs(new_cs);
                        set_use32(0);
                        cpu_cur_status |= CPU_STATUS_V86;
                }
                else
                {
                        if (!(new_cs&~3))
                        {
                                x86ts(NULL,0);
                                return;
                        }
                        addr=new_cs&~7;
                        if (new_cs&4)
                        {
                                if (addr>=ldt.limit)
                                {
                                        x86ts(NULL,new_cs&~3);
                                        return;
                                }
                                addr+=ldt.base;
                        }
                        else
                        {
                                if (addr>=gdt.limit)
                                {
                                        x86ts(NULL,new_cs&~3);
                                        return;
                                }
                                addr+=gdt.base;
                        }
                        segdat2[0]=readmemw(0,addr);
                        segdat2[1]=readmemw(0,addr+2);
                        segdat2[2]=readmemw(0,addr+4);
                        segdat2[3]=readmemw(0,addr+6);
                        if (!(segdat2[2]&0x8000))
                        {
                                x86np("TS loading CS not present\n", new_cs & 0xfffc);
                                return;
                        }
                        switch (segdat2[2]&0x1F00)
                        {
                                case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming*/
                                if ((new_cs&3) != DPL2)
                                {
                                        x86ts(NULL,new_cs&~3);
                                        return;
                                }
                                break;
                                case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                                if ((new_cs&3) < DPL2)
                                {
                                        x86ts(NULL,new_cs&~3);
                                        return;
                                }
                                break;
                                default:
                                x86ts(NULL,new_cs&~3);
                                return;
                        }

                        CS=new_cs;
                        do_seg_load(&cpu_state.seg_cs, segdat2);
                        if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                        oldcpl = CPL;
                        set_use32(segdat2[3] & 0x40);
                        cpu_cur_status &= ~CPU_STATUS_V86;
                }

                EAX=new_eax;
                ECX=new_ecx;
                EDX=new_edx;
                EBX=new_ebx;
                ESP=new_esp;
                EBP=new_ebp;
                ESI=new_esi;
                EDI=new_edi;

                loadseg(new_es,&cpu_state.seg_es);
                loadseg(new_ss,&cpu_state.seg_ss);
                loadseg(new_ds,&cpu_state.seg_ds);
                loadseg(new_fs,&cpu_state.seg_fs);
                loadseg(new_gs,&cpu_state.seg_gs);
        }
        else
        {
                if (limit < 43)
                {
                        x86ts(NULL, seg);
                        return;
                }

                if (optype==JMP || optype==CALL || optype==OPTYPE_INT)
                {
                        if (tr.seg&4) tempw=readmemw(ldt.base,(seg&~7)+4);
                        else          tempw=readmemw(gdt.base,(seg&~7)+4);
                        if (cpu_state.abrt) return;
                        tempw|=0x200;
                        if (tr.seg&4) writememw(ldt.base,(seg&~7)+4,tempw);
                        else          writememw(gdt.base,(seg&~7)+4,tempw);
                }
                if (cpu_state.abrt) return;

                if (optype==IRET) cpu_state.flags&=~NT_FLAG;

                cpu_386_flags_rebuild();
                writememw(tr.base,0x0E,cpu_state.pc);
                writememw(tr.base,0x10,cpu_state.flags);
                
                writememw(tr.base,0x12,AX);
                writememw(tr.base,0x14,CX);
                writememw(tr.base,0x16,DX);
                writememw(tr.base,0x18,BX);
                writememw(tr.base,0x1A,SP);
                writememw(tr.base,0x1C,BP);
                writememw(tr.base,0x1E,SI);
                writememw(tr.base,0x20,DI);
                
                writememw(tr.base,0x22,ES);
                writememw(tr.base,0x24,CS);
                writememw(tr.base,0x26,SS);
                writememw(tr.base,0x28,DS);
                
                if (optype==JMP || optype==IRET)
                {
                        if (tr.seg&4) tempw=readmemw(ldt.base,(tr.seg&~7)+4);
                        else          tempw=readmemw(gdt.base,(tr.seg&~7)+4);
                        if (cpu_state.abrt) return;
                        tempw&=~0x200;
                        if (tr.seg&4) writememw(ldt.base,(tr.seg&~7)+4,tempw);
                        else          writememw(gdt.base,(tr.seg&~7)+4,tempw);
                }
                if (cpu_state.abrt) return;
                
                if (optype==OPTYPE_INT || optype==CALL)
                {
                        writememw(base,0,tr.seg);
                        if (cpu_state.abrt)
                                return;
                }
                
                new_pc=readmemw(base,0x0E);
                new_flags=readmemw(base,0x10);
                if (optype == OPTYPE_INT || optype == CALL)
                        new_flags |= NT_FLAG;
                                
                new_eax=readmemw(base,0x12);
                new_ecx=readmemw(base,0x14);
                new_edx=readmemw(base,0x16);
                new_ebx=readmemw(base,0x18);
                new_esp=readmemw(base,0x1A);
                new_ebp=readmemw(base,0x1C);
                new_esi=readmemw(base,0x1E);
                new_edi=readmemw(base,0x20);

                new_es=readmemw(base,0x22);
                new_cs=readmemw(base,0x24);
                new_ss=readmemw(base,0x26);
                new_ds=readmemw(base,0x28);
                new_ldt=readmemw(base,0x2A);

                msw |= 8;
               
                cpu_state.pc=new_pc;
                cpu_state.flags=new_flags;
                cpu_386_flags_extract();

                ldt.seg=new_ldt;
                templ=(ldt.seg&~7)+gdt.base;
                ldt.limit=readmemw(0,templ);
                ldt.base=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16);
                if (is386)
                {
                        if (readmemb(0,templ+6)&0x80)
                        {
                                ldt.limit<<=12;
                                ldt.limit|=0xFFF;
                        }
                        ldt.base|=(readmemb(0,templ+7)<<24);
                }

                if (!(new_cs&~3))
                {
                        x86ts(NULL,0);
                        return;
                }
                addr=new_cs&~7;
                if (new_cs&4)
                {
                        if (addr>=ldt.limit)
                        {
                                x86ts(NULL,new_cs&~3);
                                return;
                        }
                        addr+=ldt.base;
                }
                else
                {
                        if (addr>=gdt.limit)
                        {
                                x86ts(NULL,new_cs&~3);
                                return;
                        }
                        addr+=gdt.base;
                }
                segdat2[0]=readmemw(0,addr);
                segdat2[1]=readmemw(0,addr+2);
                segdat2[2]=readmemw(0,addr+4);
                segdat2[3]=readmemw(0,addr+6);
                if (!(segdat2[2]&0x8000))
                {
                        x86np("TS loading CS not present\n", new_cs & 0xfffc);
                        return;
                }
                switch (segdat2[2]&0x1F00)
                {
                        case 0x1800: case 0x1900: case 0x1A00: case 0x1B00: /*Non-conforming*/
                        if ((new_cs&3) != DPL2)
                        {
                                x86ts(NULL,new_cs&~3);
                                return;
                        }
                        break;
                        case 0x1C00: case 0x1D00: case 0x1E00: case 0x1F00: /*Conforming*/
                        if ((new_cs&3) < DPL2)
                        {
                                x86ts(NULL,new_cs&~3);
                                return;
                        }
                        break;
                        default:
                        x86ts(NULL,new_cs&~3);
                        return;
                }

                CS=new_cs;
                do_seg_load(&cpu_state.seg_cs, segdat2);
                if (CPL==3 && oldcpl!=3) flushmmucache_cr3();
                oldcpl = CPL;
                set_use32(0);

                EAX=new_eax | 0xFFFF0000;
                ECX=new_ecx | 0xFFFF0000;
                EDX=new_edx | 0xFFFF0000;
                EBX=new_ebx | 0xFFFF0000;
                ESP=new_esp | 0xFFFF0000;
                EBP=new_ebp | 0xFFFF0000;
                ESI=new_esi | 0xFFFF0000;
                EDI=new_edi | 0xFFFF0000;

                loadseg(new_es,&cpu_state.seg_es);
                loadseg(new_ss,&cpu_state.seg_ss);
                loadseg(new_ds,&cpu_state.seg_ds);
                if (is386)
                {
                        loadseg(0,&cpu_state.seg_fs);
                        loadseg(0,&cpu_state.seg_gs);
                }
        }

        tr.seg=seg;
        tr.base=base;
        tr.limit=limit;
        tr.access=segdat[2]>>8;
}
