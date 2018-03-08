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
 * Version:	@(#)x86_ops_mmx_pack.h	1.0.1	2018/02/14
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

static int opPUNPCKLDQ_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].l[1] = cpu_state.MM[cpu_rm].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t src;
        
                src = readmeml(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 0;
                cpu_state.MM[cpu_reg].l[1] = src;

                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opPUNPCKLDQ_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (cpu_mod == 3)
        {
                cpu_state.MM[cpu_reg].l[1] = cpu_state.MM[cpu_rm].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t src;
        
                src = readmeml(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 0;
                cpu_state.MM[cpu_reg].l[1] = src;

                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opPUNPCKHDQ_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();
        
        cpu_state.MM[cpu_reg].l[0] = cpu_state.MM[cpu_reg].l[1];
        cpu_state.MM[cpu_reg].l[1] = src.l[1];

        return 0;
}
static int opPUNPCKHDQ_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].l[0] = cpu_state.MM[cpu_reg].l[1];
        cpu_state.MM[cpu_reg].l[1] = src.l[1];

        return 0;
}

static int opPUNPCKLBW_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].b[7] = src.b[3];
        cpu_state.MM[cpu_reg].b[6] = cpu_state.MM[cpu_reg].b[3];
        cpu_state.MM[cpu_reg].b[5] = src.b[2];
        cpu_state.MM[cpu_reg].b[4] = cpu_state.MM[cpu_reg].b[2];
        cpu_state.MM[cpu_reg].b[3] = src.b[1];
        cpu_state.MM[cpu_reg].b[2] = cpu_state.MM[cpu_reg].b[1];
        cpu_state.MM[cpu_reg].b[1] = src.b[0];
        cpu_state.MM[cpu_reg].b[0] = cpu_state.MM[cpu_reg].b[0];

        return 0;
}
static int opPUNPCKLBW_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].b[7] = src.b[3];
        cpu_state.MM[cpu_reg].b[6] = cpu_state.MM[cpu_reg].b[3];
        cpu_state.MM[cpu_reg].b[5] = src.b[2];
        cpu_state.MM[cpu_reg].b[4] = cpu_state.MM[cpu_reg].b[2];
        cpu_state.MM[cpu_reg].b[3] = src.b[1];
        cpu_state.MM[cpu_reg].b[2] = cpu_state.MM[cpu_reg].b[1];
        cpu_state.MM[cpu_reg].b[1] = src.b[0];
        cpu_state.MM[cpu_reg].b[0] = cpu_state.MM[cpu_reg].b[0];

        return 0;
}

static int opPUNPCKHBW_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].b[0] = cpu_state.MM[cpu_reg].b[4];
        cpu_state.MM[cpu_reg].b[1] = src.b[4];
        cpu_state.MM[cpu_reg].b[2] = cpu_state.MM[cpu_reg].b[5];
        cpu_state.MM[cpu_reg].b[3] = src.b[5];
        cpu_state.MM[cpu_reg].b[4] = cpu_state.MM[cpu_reg].b[6];
        cpu_state.MM[cpu_reg].b[5] = src.b[6];
        cpu_state.MM[cpu_reg].b[6] = cpu_state.MM[cpu_reg].b[7];
        cpu_state.MM[cpu_reg].b[7] = src.b[7];
        
        return 0;
}
static int opPUNPCKHBW_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].b[0] = cpu_state.MM[cpu_reg].b[4];
        cpu_state.MM[cpu_reg].b[1] = src.b[4];
        cpu_state.MM[cpu_reg].b[2] = cpu_state.MM[cpu_reg].b[5];
        cpu_state.MM[cpu_reg].b[3] = src.b[5];
        cpu_state.MM[cpu_reg].b[4] = cpu_state.MM[cpu_reg].b[6];
        cpu_state.MM[cpu_reg].b[5] = src.b[6];
        cpu_state.MM[cpu_reg].b[6] = cpu_state.MM[cpu_reg].b[7];
        cpu_state.MM[cpu_reg].b[7] = src.b[7];
        
        return 0;
}

static int opPUNPCKLWD_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].w[3] = src.w[1];
        cpu_state.MM[cpu_reg].w[2] = cpu_state.MM[cpu_reg].w[1];
        cpu_state.MM[cpu_reg].w[1] = src.w[0];
        cpu_state.MM[cpu_reg].w[0] = cpu_state.MM[cpu_reg].w[0];

        return 0;
}
static int opPUNPCKLWD_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].w[3] = src.w[1];
        cpu_state.MM[cpu_reg].w[2] = cpu_state.MM[cpu_reg].w[1];
        cpu_state.MM[cpu_reg].w[1] = src.w[0];
        cpu_state.MM[cpu_reg].w[0] = cpu_state.MM[cpu_reg].w[0];

        return 0;
}

static int opPUNPCKHWD_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].w[0] = cpu_state.MM[cpu_reg].w[2];
        cpu_state.MM[cpu_reg].w[1] = src.w[2];
        cpu_state.MM[cpu_reg].w[2] = cpu_state.MM[cpu_reg].w[3];
        cpu_state.MM[cpu_reg].w[3] = src.w[3];

        return 0;
}
static int opPUNPCKHWD_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].w[0] = cpu_state.MM[cpu_reg].w[2];
        cpu_state.MM[cpu_reg].w[1] = src.w[2];
        cpu_state.MM[cpu_reg].w[2] = cpu_state.MM[cpu_reg].w[3];
        cpu_state.MM[cpu_reg].w[3] = src.w[3];

        return 0;
}

static int opPACKSSWB_a16(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];

        cpu_state.MM[cpu_reg].sb[0] = SSATB(dst.sw[0]);
        cpu_state.MM[cpu_reg].sb[1] = SSATB(dst.sw[1]);
        cpu_state.MM[cpu_reg].sb[2] = SSATB(dst.sw[2]);
        cpu_state.MM[cpu_reg].sb[3] = SSATB(dst.sw[3]);
        cpu_state.MM[cpu_reg].sb[4] = SSATB(src.sw[0]);
        cpu_state.MM[cpu_reg].sb[5] = SSATB(src.sw[1]);
        cpu_state.MM[cpu_reg].sb[6] = SSATB(src.sw[2]);
        cpu_state.MM[cpu_reg].sb[7] = SSATB(src.sw[3]);
        
        return 0;
}
static int opPACKSSWB_a32(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];

        cpu_state.MM[cpu_reg].sb[0] = SSATB(dst.sw[0]);
        cpu_state.MM[cpu_reg].sb[1] = SSATB(dst.sw[1]);
        cpu_state.MM[cpu_reg].sb[2] = SSATB(dst.sw[2]);
        cpu_state.MM[cpu_reg].sb[3] = SSATB(dst.sw[3]);
        cpu_state.MM[cpu_reg].sb[4] = SSATB(src.sw[0]);
        cpu_state.MM[cpu_reg].sb[5] = SSATB(src.sw[1]);
        cpu_state.MM[cpu_reg].sb[6] = SSATB(src.sw[2]);
        cpu_state.MM[cpu_reg].sb[7] = SSATB(src.sw[3]);
        
        return 0;
}

static int opPACKUSWB_a16(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];

        cpu_state.MM[cpu_reg].b[0] = USATB(dst.sw[0]);
        cpu_state.MM[cpu_reg].b[1] = USATB(dst.sw[1]);
        cpu_state.MM[cpu_reg].b[2] = USATB(dst.sw[2]);
        cpu_state.MM[cpu_reg].b[3] = USATB(dst.sw[3]);
        cpu_state.MM[cpu_reg].b[4] = USATB(src.sw[0]);
        cpu_state.MM[cpu_reg].b[5] = USATB(src.sw[1]);
        cpu_state.MM[cpu_reg].b[6] = USATB(src.sw[2]);
        cpu_state.MM[cpu_reg].b[7] = USATB(src.sw[3]);
        
        return 0;
}
static int opPACKUSWB_a32(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];

        cpu_state.MM[cpu_reg].b[0] = USATB(dst.sw[0]);
        cpu_state.MM[cpu_reg].b[1] = USATB(dst.sw[1]);
        cpu_state.MM[cpu_reg].b[2] = USATB(dst.sw[2]);
        cpu_state.MM[cpu_reg].b[3] = USATB(dst.sw[3]);
        cpu_state.MM[cpu_reg].b[4] = USATB(src.sw[0]);
        cpu_state.MM[cpu_reg].b[5] = USATB(src.sw[1]);
        cpu_state.MM[cpu_reg].b[6] = USATB(src.sw[2]);
        cpu_state.MM[cpu_reg].b[7] = USATB(src.sw[3]);

        return 0;
}

static int opPACKSSDW_a16(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];
        
        cpu_state.MM[cpu_reg].sw[0] = SSATW(dst.sl[0]);
        cpu_state.MM[cpu_reg].sw[1] = SSATW(dst.sl[1]);
        cpu_state.MM[cpu_reg].sw[2] = SSATW(src.sl[0]);
        cpu_state.MM[cpu_reg].sw[3] = SSATW(src.sl[1]);
        
        return 0;
}
static int opPACKSSDW_a32(uint32_t fetchdat)
{
        MMX_REG src, dst;
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSRC();
        dst = cpu_state.MM[cpu_reg];
        
        cpu_state.MM[cpu_reg].sw[0] = SSATW(dst.sl[0]);
        cpu_state.MM[cpu_reg].sw[1] = SSATW(dst.sl[1]);
        cpu_state.MM[cpu_reg].sw[2] = SSATW(src.sl[0]);
        cpu_state.MM[cpu_reg].sw[3] = SSATW(src.sl[1]);
        
        return 0;
}
