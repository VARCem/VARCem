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
 * Version:	@(#)x86_ops_mmx_mov.h	1.0.1	2018/02/14
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

static int opMOVD_l_mm_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].l[0] = cpu_state.regs[cpu_rm].l;
                cpu_state.MM[cpu_reg].l[1] = 0;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst;
        
                dst = readmeml(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 1;
                cpu_state.MM[cpu_reg].l[0] = dst;
                cpu_state.MM[cpu_reg].l[1] = 0;

                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVD_l_mm_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].l[0] = cpu_state.regs[cpu_rm].l;
                cpu_state.MM[cpu_reg].l[1] = 0;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst;
        
                dst = readmeml(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 1;
                cpu_state.MM[cpu_reg].l[0] = dst;
                cpu_state.MM[cpu_reg].l[1] = 0;

                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVD_mm_l_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.regs[cpu_rm].l = cpu_state.MM[cpu_reg].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 3);
                writememl(easeg, cpu_state.eaaddr, cpu_state.MM[cpu_reg].l[0]); if (cpu_state.abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVD_mm_l_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.regs[cpu_rm].l = cpu_state.MM[cpu_reg].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 3);
                writememl(easeg, cpu_state.eaaddr, cpu_state.MM[cpu_reg].l[0]); if (cpu_state.abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVQ_q_mm_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].q = cpu_state.MM[cpu_rm].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint64_t dst;
        
                dst = readmemq(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 1;
                cpu_state.MM[cpu_reg].q = dst;
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVQ_q_mm_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].q = cpu_state.MM[cpu_rm].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint64_t dst;
        
                dst = readmemq(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 1;
                cpu_state.MM[cpu_reg].q = dst;
                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVQ_mm_q_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_rm].q = cpu_state.MM[cpu_reg].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 7);
                writememq(easeg, cpu_state.eaaddr,     cpu_state.MM[cpu_reg].q); if (cpu_state.abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVQ_mm_q_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_rm].q = cpu_state.MM[cpu_reg].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 7);
                writememq(easeg, cpu_state.eaaddr,     cpu_state.MM[cpu_reg].q); if (cpu_state.abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
