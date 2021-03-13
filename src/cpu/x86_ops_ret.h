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
 * Version:	@(#)x86_ops_ret.h	1.0.2	2020/12/04
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

#define RETF_a16(stack_offset)                                  \
                if ((msw&1) && !(cpu_state.eflags&VM_FLAG))               \
                {                                               \
                        pmoderetf(0, stack_offset);             \
                        return 1;                               \
                }                                               \
                oxpc = cpu_state.pc;                            \
                if (stack32)                                    \
                {                                               \
                        cpu_state.pc = readmemw(ss, ESP);       \
                        loadcs(readmemw(ss, ESP + 2));          \
                }                                               \
                else                                            \
                {                                               \
                        cpu_state.pc = readmemw(ss, SP);        \
                        loadcs(readmemw(ss, SP + 2));           \
                }                                               \
                if (cpu_state.abrt) return 1;                             \
                if (stack32) ESP += 4 + stack_offset;           \
                else         SP  += 4 + stack_offset;           \
                cycles -= timing_retf_rm;

#define RETF_a32(stack_offset)                                  \
                if ((msw&1) && !(cpu_state.eflags&VM_FLAG))               \
                {                                               \
                        pmoderetf(1, stack_offset);             \
                        return 1;                               \
                }                                               \
                oxpc = cpu_state.pc;                            \
                if (stack32)                                    \
                {                                               \
                        cpu_state.pc = readmeml(ss, ESP);       \
                        loadcs(readmeml(ss, ESP + 4) & 0xffff); \
                }                                               \
                else                                            \
                {                                               \
                        cpu_state.pc = readmeml(ss, SP);        \
                        loadcs(readmeml(ss, SP + 4) & 0xffff);  \
                }                                               \
                if (cpu_state.abrt) return 1;                             \
                if (stack32) ESP += 8 + stack_offset;           \
                else         SP  += 8 + stack_offset;           \
                cycles -= timing_retf_rm;

static int opRETF_a16(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        CPU_BLOCK_END();
        RETF_a16(0);
        
        PREFETCH_RUN(cycles_old-cycles, 1, -1, 2,0,0,0, 0);
        PREFETCH_FLUSH();
        return 0;
}
static int opRETF_a32(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        CPU_BLOCK_END();
        RETF_a32(0);

        PREFETCH_RUN(cycles_old-cycles, 1, -1, 0,2,0,0, 1);
        PREFETCH_FLUSH();
        return 0;
}

static int opRETF_a16_imm(uint32_t fetchdat)
{
        uint16_t offset = getwordf();
        int cycles_old = cycles; UN_USED(cycles_old);

        CPU_BLOCK_END();
        RETF_a16(offset);

        PREFETCH_RUN(cycles_old-cycles, 3, -1, 2,0,0,0, 0);
        PREFETCH_FLUSH();
        return 0;
}
static int opRETF_a32_imm(uint32_t fetchdat)
{
        uint16_t offset = getwordf();
        int cycles_old = cycles; UN_USED(cycles_old);

        CPU_BLOCK_END();
        RETF_a32(offset);

        PREFETCH_RUN(cycles_old-cycles, 3, -1, 0,2,0,0, 1);
        PREFETCH_FLUSH();
        return 0;
}

static int opIRET_286(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        if ((cr0 & 1) && (cpu_state.eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 1;
        }
        if (msw&1)
        {
                optype = IRET;
                pmodeiret(0);
                optype = 0;
        }
        else
        {
                uint16_t new_cs;
                oxpc = cpu_state.pc;
                if (stack32)
                {
                        cpu_state.pc = readmemw(ss, ESP);
                        new_cs = readmemw(ss, ESP + 2);
                        cpu_state.flags = (cpu_state.flags & 0x7000) | (readmemw(ss, ESP + 4) & 0xffd5) | 2;
                        ESP += 6;
                }
                else
                {
                        cpu_state.pc = readmemw(ss, SP);
                        new_cs = readmemw(ss, ((SP + 2) & 0xffff));
                        cpu_state.flags = (cpu_state.flags & 0x7000) | (readmemw(ss, ((SP + 4) & 0xffff)) & 0x0fd5) | 2;
                        SP += 6;
                }
                loadcs(new_cs);
                cycles -= timing_iret_rm;
        }
        flags_extract();
        nmi_enable = 1;
        CPU_BLOCK_END();

        PREFETCH_RUN(cycles_old-cycles, 1, -1, 2,0,0,0, 0);
        PREFETCH_FLUSH();
        return cpu_state.abrt;
}

static int opIRET(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        if ((cr0 & 1) && (cpu_state.eflags & VM_FLAG) && (IOPL != 3))
        {
                if (cr4 & CR4_VME)
                {
                        uint16_t new_pc, new_cs, new_flags;

                        new_pc = readmemw(ss, SP);
                        new_cs = readmemw(ss, ((SP + 2) & 0xffff));
                        new_flags = readmemw(ss, ((SP + 4) & 0xffff));
                        if (cpu_state.abrt)
                                return 1;

                        if ((new_flags & T_FLAG) || ((new_flags & I_FLAG) && (cpu_state.eflags & VIP_FLAG)))
                        {
                                x86gpf(NULL, 0);
                                return 1;
                        }
                        SP += 6;
                        if (new_flags & I_FLAG)
                                cpu_state.eflags |= VIF_FLAG;
                        else
                                cpu_state.eflags &= ~VIF_FLAG;
                        cpu_state.flags = (cpu_state.flags & 0x3300) | (new_flags & 0x4cd5) | 2;
                        loadcs(new_cs);
                        cpu_state.pc = new_pc;

                        cycles -= timing_iret_rm;
                }
                else
                {
                        x86gpf(NULL,0);
                        return 1;
                }
        }
        else
        {
		if (msw&1)
                {
                        optype = IRET;
                        pmodeiret(0);
                        optype = 0;
                }
                else
                {
                        uint16_t new_cs;
                        oxpc = cpu_state.pc;
                        if (stack32)
                        {
                                cpu_state.pc = readmemw(ss, ESP);
                                new_cs = readmemw(ss, ESP + 2);
                                cpu_state.flags = (readmemw(ss, ESP + 4) & 0xffd5) | 2;
                                ESP += 6;
                        }
                        else
                        {
                                cpu_state.pc = readmemw(ss, SP);
                                new_cs = readmemw(ss, ((SP + 2) & 0xffff));
                                cpu_state.flags = (readmemw(ss, ((SP + 4) & 0xffff)) & 0xffd5) | 2;
                                SP += 6;
                        }
                        loadcs(new_cs);
                        cycles -= timing_iret_rm;
                }
        }
        flags_extract();
        nmi_enable = 1;
        CPU_BLOCK_END();

        PREFETCH_RUN(cycles_old-cycles, 1, -1, 2,0,0,0, 0);
        PREFETCH_FLUSH();
        return cpu_state.abrt;
}

static int opIRETD(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        if ((cr0 & 1) && (cpu_state.eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 1;
        }
        if (msw & 1)
        {
                optype = IRET;
                pmodeiret(1);
                optype = 0;
        }
        else
        {
                uint16_t new_cs;
                oxpc = cpu_state.pc;
                if (stack32)
                {
                        cpu_state.pc = readmeml(ss, ESP);
                        new_cs = readmemw(ss, ESP + 4);
                        cpu_state.flags = (readmemw(ss, ESP + 8) & 0xffd5) | 2;
                        cpu_state.eflags = readmemw(ss, ESP + 10);
                        ESP += 12;
                }
                else
                {
                        cpu_state.pc = readmeml(ss, SP);
                        new_cs = readmemw(ss, ((SP + 4) & 0xffff));
                        cpu_state.flags = (readmemw(ss,(SP + 8) & 0xffff) & 0xffd5) | 2;
                        cpu_state.eflags = readmemw(ss, (SP + 10) & 0xffff);
                        SP += 12;
                }
                loadcs(new_cs);
                cycles -= timing_iret_rm;
        }
        flags_extract();
        nmi_enable = 1;
        CPU_BLOCK_END();

        PREFETCH_RUN(cycles_old-cycles, 1, -1, 0,2,0,0, 1);
        PREFETCH_FLUSH();
        return cpu_state.abrt;
}
 
