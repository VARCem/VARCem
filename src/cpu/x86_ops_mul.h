/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x86 CPU Instructions.
 *
 * Version:	@(#)x86_ops_mul.h	1.0.1	2018/02/14
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

static int opIMUL_w_iw_a16(uint32_t fetchdat)
{
        int32_t templ;
        int16_t tempw, tempw2; 
        
        fetch_ea_16(fetchdat);
                        
        tempw = geteaw();               if (cpu_state.abrt) return 1;
        tempw2 = getword();             if (cpu_state.abrt) return 1;
        
        templ = ((int)tempw) * ((int)tempw2);
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].w = templ & 0xffff;

        CLOCK_CYCLES((cpu_mod == 3) ? 14 : 17);
        PREFETCH_RUN((cpu_mod == 3) ? 14 : 17, 4, rmdat, 1,0,0,0, 0);
        return 0;
}
static int opIMUL_w_iw_a32(uint32_t fetchdat)
{
        int32_t templ;
        int16_t tempw, tempw2;
        
        fetch_ea_32(fetchdat);
                        
        tempw = geteaw();               if (cpu_state.abrt) return 1;
        tempw2 = getword();             if (cpu_state.abrt) return 1;
        
        templ = ((int)tempw) * ((int)tempw2);
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].w = templ & 0xffff;

        CLOCK_CYCLES((cpu_mod == 3) ? 14 : 17);
        PREFETCH_RUN((cpu_mod == 3) ? 14 : 17, 4, rmdat, 1,0,0,0, 1);
        return 0;
}

static int opIMUL_l_il_a16(uint32_t fetchdat)
{
        int64_t temp64;
        int32_t templ, templ2;
        
        fetch_ea_16(fetchdat);
        
        templ = geteal();               if (cpu_state.abrt) return 1;
        templ2 = getlong();             if (cpu_state.abrt) return 1;
        
        temp64 = ((int64_t)templ) * ((int64_t)templ2);
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].l = temp64 & 0xffffffff;
        
        CLOCK_CYCLES(25);
        PREFETCH_RUN(25, 6, rmdat, 0,1,0,0, 0);
        return 0;
}
static int opIMUL_l_il_a32(uint32_t fetchdat)
{
        int64_t temp64;
        int32_t templ, templ2;
        
        fetch_ea_32(fetchdat);
        
        templ = geteal();               if (cpu_state.abrt) return 1;
        templ2 = getlong();             if (cpu_state.abrt) return 1;
        
        temp64 = ((int64_t)templ) * ((int64_t)templ2);
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].l = temp64 & 0xffffffff;
        
        CLOCK_CYCLES(25);
        PREFETCH_RUN(25, 6, rmdat, 0,1,0,0, 1);
        return 0;
}

static int opIMUL_w_ib_a16(uint32_t fetchdat)
{
        int32_t templ;
        int16_t tempw, tempw2;
        
        fetch_ea_16(fetchdat);
        
        tempw = geteaw();               if (cpu_state.abrt) return 1;
        tempw2 = getbyte();             if (cpu_state.abrt) return 1;
        if (tempw2 & 0x80) tempw2 |= 0xff00;
        
        templ = ((int)tempw) * ((int)tempw2);
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].w = templ & 0xffff;
        
        CLOCK_CYCLES((cpu_mod == 3) ? 14 : 17);
        PREFETCH_RUN((cpu_mod == 3) ? 14 : 17, 3, rmdat, 1,0,0,0, 0);
        return 0;
}
static int opIMUL_w_ib_a32(uint32_t fetchdat)
{
        int32_t templ;
        int16_t tempw, tempw2;
        
        fetch_ea_32(fetchdat);
        
        tempw = geteaw();               if (cpu_state.abrt) return 1;
        tempw2 = getbyte();             if (cpu_state.abrt) return 1;
        if (tempw2 & 0x80) tempw2 |= 0xff00;
        
        templ = ((int)tempw) * ((int)tempw2);
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].w = templ & 0xffff;
        
        CLOCK_CYCLES((cpu_mod == 3) ? 14 : 17);
        PREFETCH_RUN((cpu_mod == 3) ? 14 : 17, 3, rmdat, 1,0,0,0, 1);
        return 0;
}

static int opIMUL_l_ib_a16(uint32_t fetchdat)
{
        int64_t temp64;
        int32_t templ, templ2;

        fetch_ea_16(fetchdat);
        templ = geteal();               if (cpu_state.abrt) return 1;
        templ2 = getbyte();             if (cpu_state.abrt) return 1;
        if (templ2 & 0x80) templ2 |= 0xffffff00;
        
        temp64 = ((int64_t)templ)*((int64_t)templ2);
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].l = temp64 & 0xffffffff;
        
        CLOCK_CYCLES(20);
        PREFETCH_RUN(20, 3, rmdat, 0,1,0,0, 0);
        return 0;
}
static int opIMUL_l_ib_a32(uint32_t fetchdat)
{
        int64_t temp64;
        int32_t templ, templ2;

        fetch_ea_32(fetchdat);
        templ = geteal();               if (cpu_state.abrt) return 1;
        templ2 = getbyte();             if (cpu_state.abrt) return 1;
        if (templ2 & 0x80) templ2 |= 0xffffff00;
        
        temp64 = ((int64_t)templ)*((int64_t)templ2);
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        cpu_state.regs[cpu_reg].l = temp64 & 0xffffffff;
        
        CLOCK_CYCLES(20);
        PREFETCH_RUN(20, 3, rmdat, 0,1,0,0, 1);
        return 0;
}



static int opIMUL_w_w_a16(uint32_t fetchdat)
{
        int32_t templ;
        
        fetch_ea_16(fetchdat);
        templ = (int32_t)(int16_t)cpu_state.regs[cpu_reg].w * (int32_t)(int16_t)geteaw();
        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = templ & 0xFFFF;
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        
        CLOCK_CYCLES(18);
        PREFETCH_RUN(18, 2, rmdat, 1,0,0,0, 0);
        return 0;
}
static int opIMUL_w_w_a32(uint32_t fetchdat)
{
        int32_t templ;
        
        fetch_ea_32(fetchdat);
        templ = (int32_t)(int16_t)cpu_state.regs[cpu_reg].w * (int32_t)(int16_t)geteaw();
        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].w = templ & 0xFFFF;
        flags_rebuild();
        if ((templ >> 15) != 0 && (templ >> 15) != -1) flags |=   C_FLAG | V_FLAG;
        else                                           flags &= ~(C_FLAG | V_FLAG);
        
        CLOCK_CYCLES(18);
        PREFETCH_RUN(18, 2, rmdat, 1,0,0,0, 1);
        return 0;
}

static int opIMUL_l_l_a16(uint32_t fetchdat)
{
        int64_t temp64;

        fetch_ea_16(fetchdat);
        temp64 = (int64_t)(int32_t)cpu_state.regs[cpu_reg].l * (int64_t)(int32_t)geteal();
        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = temp64 & 0xFFFFFFFF;
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        
        CLOCK_CYCLES(30);
        PREFETCH_RUN(30, 2, rmdat, 0,1,0,0, 0);
        return 0;
}
static int opIMUL_l_l_a32(uint32_t fetchdat)
{
        int64_t temp64;

        fetch_ea_32(fetchdat);
        temp64 = (int64_t)(int32_t)cpu_state.regs[cpu_reg].l * (int64_t)(int32_t)geteal();
        if (cpu_state.abrt) return 1;
        cpu_state.regs[cpu_reg].l = temp64 & 0xFFFFFFFF;
        flags_rebuild();
        if ((temp64 >> 31) != 0 && (temp64 >> 31) != -1) flags |=   C_FLAG | V_FLAG;
        else                                             flags &= ~(C_FLAG | V_FLAG);
        
        CLOCK_CYCLES(30);
        PREFETCH_RUN(30, 2, rmdat, 0,1,0,0, 1);
        return 0;
}

