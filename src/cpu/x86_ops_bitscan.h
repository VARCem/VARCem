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
 * Version:	@(#)x86_ops_bitscan.h	1.0.2	2020/12/11
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

#define BS_common(start, end, dir, dest, time)                                  \
        flags_rebuild();                                                        \
        instr_cycles = 0;                                                       \
        if (temp)                                                               \
        {                                                                       \
                int c;                                                          \
                cpu_state.flags &= ~Z_FLAG;                                               \
                for (c = start; c != end; c += dir)                             \
                {                                                               \
                        CLOCK_CYCLES(time);                                     \
                        instr_cycles += time;                                   \
                        if (temp & (1 << c))                                    \
                        {                                                       \
                                dest = c;                                       \
                                break;                                          \
                        }                                                       \
                }                                                               \
        }                                                                       \
        else                                                                    \
                cpu_state.flags |= Z_FLAG;

static int opBSF_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        int instr_cycles = 0;
        
        fetch_ea_16(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        
        BS_common(0, 16, 1, cpu_state.regs[cpu_reg].w, (is486) ? 1 : 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opBSF_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        int instr_cycles = 0;
        
        fetch_ea_32(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        
        BS_common(0, 16, 1, cpu_state.regs[cpu_reg].w, (is486) ? 1 : 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opBSF_l_a16(uint32_t fetchdat)
{
        uint32_t temp;
        int instr_cycles = 0;
        
        fetch_ea_16(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteal();                        if (cpu_state.abrt) return 1;
        
        BS_common(0, 32, 1, cpu_state.regs[cpu_reg].l, (is486) ? 1 : 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 0);
        return 0;
}
static int opBSF_l_a32(uint32_t fetchdat)
{
        uint32_t temp;
        int instr_cycles = 0;

        fetch_ea_32(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteal();                        if (cpu_state.abrt) return 1;
        
        BS_common(0, 32, 1, cpu_state.regs[cpu_reg].l, (is486) ? 1 : 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 1);
        return 0;
}

static int opBSR_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        int instr_cycles = 0;
        
        fetch_ea_16(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        
        BS_common(15, -1, -1, cpu_state.regs[cpu_reg].w, 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opBSR_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        int instr_cycles = 0;
        
        fetch_ea_32(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        
        BS_common(15, -1, -1, cpu_state.regs[cpu_reg].w, 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opBSR_l_a16(uint32_t fetchdat)
{
        uint32_t temp;
        int instr_cycles = 0;
        
        fetch_ea_16(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteal();                        if (cpu_state.abrt) return 1;
        
        BS_common(31, -1, -1, cpu_state.regs[cpu_reg].l, 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 0);
        return 0;
}
static int opBSR_l_a32(uint32_t fetchdat)
{
        uint32_t temp;
        int instr_cycles = 0;
        
        fetch_ea_32(fetchdat);
        if (cpu_mod != 3)
                SEG_CHECK_READ(cpu_state.ea_seg);
        temp = geteal();                        if (cpu_state.abrt) return 1;
        
        BS_common(31, -1, -1, cpu_state.regs[cpu_reg].l, 3);
        
        CLOCK_CYCLES((is486) ? 6 : 10);
        instr_cycles += ((is486) ? 6 : 10);
        PREFETCH_RUN(instr_cycles, 2, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 1);
        return 0;
}

