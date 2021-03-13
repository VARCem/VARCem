/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x86 CPU Instructions.
 *
 * Version:	@(#)x86_ops_flag.h	1.0.2	2020/12/04
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

static int opCMC(uint32_t fetchdat)
{
        flags_rebuild();
        cpu_state.flags ^= C_FLAG;
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}


static int opCLC(uint32_t fetchdat)
{
        flags_rebuild();
        cpu_state.flags &= ~C_FLAG;
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}
static int opCLD(uint32_t fetchdat)
{
        cpu_state.flags &= ~D_FLAG;
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}
static int opCLI(uint32_t fetchdat)
{
        if (!IOPLp)
        {
                if ((!(cpu_state.eflags & VM_FLAG) && (cr4 & CR4_PVI)) || 
                        ((cpu_state.eflags & VM_FLAG) && (cr4 & CR4_VME)))
                {
                        cpu_state.eflags &= ~VIF_FLAG;
                }
                else
                {
                        x86gpf(NULL,0);
                        return 1;
                }
        }
        else
                cpu_state.flags &= ~I_FLAG;
         
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 1, -1, 0,0,0,0, 0);
        return 0;
}

static int opSTC(uint32_t fetchdat)
{
        flags_rebuild();
        cpu_state.flags |= C_FLAG;
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}
static int opSTD(uint32_t fetchdat)
{
        cpu_state.flags |= D_FLAG;
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}
static int opSTI(uint32_t fetchdat)
{
        if (!IOPLp)
        {
                if ((!(cpu_state.eflags & VM_FLAG) && (cr4 & CR4_PVI)) || 
                        ((cpu_state.eflags & VM_FLAG) && (cr4 & CR4_VME)))
                {
                        if (cpu_state.eflags & VIP_FLAG)
                        {
                                x86gpf(NULL,0);
                                return 1;
                        }
                        else
                                cpu_state.eflags |= VIF_FLAG;
                }
                else
                {
                        x86gpf(NULL,0);
                        return 1;
                }
        }
        else
                cpu_state.flags |= I_FLAG;

        CPU_BLOCK_END();
                                
        CLOCK_CYCLES(2);
        PREFETCH_RUN(2, 1, -1, 0,0,0,0, 0);
        return 0;
}

static int opSAHF(uint32_t fetchdat)
{
        flags_rebuild();
        cpu_state.flags = (cpu_state.flags & 0xff00) | (AH & 0xd5) | 2;
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 1, -1, 0,0,0,0, 0);
        
#if 0
        codegen_flags_changed = 0;
#endif

        return 0;
}
static int opLAHF(uint32_t fetchdat)
{
        flags_rebuild();
        AH = cpu_state.flags & 0xff;
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 1, -1, 0,0,0,0, 0);
        return 0;
}

static int opPUSHF(uint32_t fetchdat)
{
        if ((cpu_state.eflags & VM_FLAG) && (IOPL < 3))
        {
                if (cr4 & CR4_VME)
                {
                        uint16_t temp;

                        flags_rebuild();
                        temp = (cpu_state.flags & ~I_FLAG) | 0x3000;
                        if (cpu_state.eflags & VIF_FLAG)
                                temp |= I_FLAG;
                        PUSH_W(temp);
                }
                else
                {
                        x86gpf(NULL,0);
                        return 1;
                }
        }
	else
	{
		flags_rebuild();
		PUSH_W(cpu_state.flags);
	}
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,1,0, 0);
        return cpu_state.abrt;
}
static int opPUSHFD(uint32_t fetchdat)
{
        uint16_t tempw;
        if ((cpu_state.eflags & VM_FLAG) && (IOPL < 3))
        {
                x86gpf(NULL, 0);
                return 1;
        }
        if (cpu_CR4_mask & CR4_VME) tempw = cpu_state.eflags & 0x3c;
        else if (CPUID)             tempw = cpu_state.eflags & 0x24;
        else                        tempw = cpu_state.eflags & 4;
        flags_rebuild();
        PUSH_L(cpu_state.flags | (tempw << 16));
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,0,1, 0);
        return cpu_state.abrt;
}

static int opPOPF_286(uint32_t fetchdat)
{
        uint16_t tempw;
        
        if ((cpu_state.eflags & VM_FLAG) && (IOPL < 3))
        {
                x86gpf(NULL, 0);
                return 1;
        }
        
        tempw = POP_W();                if (cpu_state.abrt) return 1;

        if (!(msw & 1))           cpu_state.flags = (cpu_state.flags & 0x7000) | (tempw & 0x0fd5) | 2;
        else if (!(CPL))          cpu_state.flags = (tempw & 0x7fd5) | 2;
        else if (IOPLp)           cpu_state.flags = (cpu_state.flags & 0x3000) | (tempw & 0x4fd5) | 2;
        else                      cpu_state.flags = (cpu_state.flags & 0x3200) | (tempw & 0x4dd5) | 2;
        flags_extract();

        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 0);
        
#if 0
        codegen_flags_changed = 0;
#endif

        return 0;
}
static int opPOPF(uint32_t fetchdat)
{
        uint16_t tempw;
        
        if ((cpu_state.eflags & VM_FLAG) && (IOPL < 3))
        {
                if (cr4 & CR4_VME)
                {
                        uint32_t old_esp = ESP;

                        tempw = POP_W();
                        if (cpu_state.abrt)
                        {

                                ESP = old_esp;
                                return 1;
                        }

                        if ((tempw & T_FLAG) || ((tempw & I_FLAG) && (cpu_state.eflags & VIP_FLAG)))
                        {
                                ESP = old_esp;
                                x86gpf(NULL, 0);
                                return 1;
                        }
                        if (tempw & I_FLAG)
                                cpu_state.eflags |= VIF_FLAG;
                        else
                                cpu_state.eflags &= ~VIF_FLAG;
                        cpu_state.flags = (cpu_state.flags & 0x3200) | (tempw & 0x4dd5) | 2;
                }
                else
                {
                        x86gpf(NULL, 0);
                        return 1;
                }
        }
        else
        {        
                tempw = POP_W();
                if (cpu_state.abrt)
                        return 1;

                if (!(CPL) || !(msw & 1))
                        cpu_state.flags = (tempw & 0x7fd5) | 2;
                else if (IOPLp)
                        cpu_state.flags = (cpu_state.flags & 0x3000) | (tempw & 0x4fd5) | 2;
                else
                        cpu_state.flags = (cpu_state.flags & 0x3200) | (tempw & 0x4dd5) | 2;
        }
        flags_extract();

        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 0);
        
#if 0
        codegen_flags_changed = 0;
#endif

        return 0;
}
static int opPOPFD(uint32_t fetchdat)
{
        uint32_t templ;
        
        if ((cpu_state.eflags & VM_FLAG) && (IOPL < 3))
        {
                x86gpf(NULL, 0);
                return 1;
        }
        
        templ = POP_L();                if (cpu_state.abrt) return 1;

        if (!(CPL) || !(msw & 1)) cpu_state.flags = (templ & 0x7fd5) | 2;
        else if (IOPLp)           cpu_state.flags = (cpu_state.flags & 0x3000) | (templ & 0x4fd5) | 2;
        else                      cpu_state.flags = (cpu_state.flags & 0x3200) | (templ & 0x4dd5) | 2;
        
        templ &= is486 ? 0x3c0000 : 0;
        templ |= ((cpu_state.eflags&3) << 16);
        if (cpu_CR4_mask & CR4_VME) cpu_state.eflags = (templ >> 16) & 0x3f;
        else if (CPUID)             cpu_state.eflags = (templ >> 16) & 0x27;
        else if (is486)             cpu_state.eflags = (templ >> 16) & 7;
        else                        cpu_state.eflags = (templ >> 16) & 3;
        
        flags_extract();

        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 0,1,0,0, 0);
        
#if 0
        codegen_flags_changed = 0;
#endif

        return 0;
}
