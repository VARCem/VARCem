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
 * Version:	@(#)x86_ops_movx.h	1.0.1	2018/02/14
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

static int opMOVZX_w_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = (uint16_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVZX_w_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = (uint16_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opMOVZX_l_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVZX_l_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opMOVZX_w_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVZX_w_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opMOVZX_l_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVZX_l_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}

static int opMOVSX_w_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = (uint16_t)temp;
        if (temp & 0x80)        
                cpu_state.regs[cpu_reg].w |= 0xff00;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVSX_w_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = (uint16_t)temp;
        if (temp & 0x80)        
                cpu_state.regs[cpu_reg].w |= 0xff00;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opMOVSX_l_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        if (temp & 0x80)        
                cpu_state.regs[cpu_reg].l |= 0xffffff00;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVSX_l_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        if (temp & 0x80)        
                cpu_state.regs[cpu_reg].l |= 0xffffff00;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
static int opMOVSX_l_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_16(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        if (temp & 0x8000)
                cpu_state.regs[cpu_reg].l |= 0xffff0000;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
        return 0;
}
static int opMOVSX_l_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_32(fetchdat);
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = (uint32_t)temp;
        if (temp & 0x8000)
                cpu_state.regs[cpu_reg].l |= 0xffff0000;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
        return 0;
}
