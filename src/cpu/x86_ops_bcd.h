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
 * Version:	@(#)x86_ops_bcd.h	1.0.3	2020/12/05
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

static int opAAA(uint32_t fetchdat)
{
        flags_rebuild();
        if ((cpu_state.flags & A_FLAG) || ((AL & 0xF) > 9))
        {
                AL += 6;
                AH++;
                cpu_state.flags |= (A_FLAG | C_FLAG);
        }
        else
                cpu_state.flags &= ~(A_FLAG | C_FLAG);
        AL &= 0xF;
        CLOCK_CYCLES(is486 ? 3 : 4);
        PREFETCH_RUN(is486 ? 3 : 4, 1, -1, 0,0,0,0, 0);
        return 0;
}

static int opAAD(uint32_t fetchdat)
{
        int base = getbytef();
        if (cpu_manuf != MANU_INTEL) base = 10;
        AL = (AH * base) + AL;
        AH = 0;
        setznp16(AX);
        CLOCK_CYCLES((is486) ? 14 : 19);
        PREFETCH_RUN(is486 ? 14 : 19, 2, -1, 0,0,0,0, 0);
        return 0;
}

static int opAAM(uint32_t fetchdat)
{
        int base = getbytef();
        if (!base || cpu_manuf != MANU_INTEL) base = 10;
        AH = AL / base;
        AL %= base;
        setznp16(AX);
        CLOCK_CYCLES((is486) ? 15 : 17);
        PREFETCH_RUN(is486 ? 15 : 17, 2, -1, 0,0,0,0, 0);
        return 0;
}

static int opAAS(uint32_t fetchdat)
{
        flags_rebuild();
        if ((cpu_state.flags & A_FLAG) || ((AL & 0xF) > 9))
        {
                AL -= 6;
                AH--;
                cpu_state.flags |= (A_FLAG | C_FLAG);
        }
        else
                cpu_state.flags &= ~(A_FLAG | C_FLAG);
        AL &= 0xF;
        CLOCK_CYCLES(is486 ? 3 : 4);
        PREFETCH_RUN(is486 ? 3 : 4, 1, -1, 0,0,0,0, 0);
        return 0;
}

static int opDAA(uint32_t fetchdat)
{
        uint16_t tempw;
        
        flags_rebuild();
        if ((cpu_state.flags & A_FLAG) || ((AL & 0xf) > 9))
        {
                int tempi = ((uint16_t)AL) + 6;
                AL += 6;
                cpu_state.flags |= A_FLAG;
                if (tempi & 0x100) cpu_state.flags |= C_FLAG;
        }
        if ((cpu_state.flags & C_FLAG) || (AL > 0x9f))
        {
                AL += 0x60;
                cpu_state.flags |= C_FLAG;
        }

        tempw = cpu_state.flags & (C_FLAG | A_FLAG);
        setznp8(AL);
        flags_rebuild();
        cpu_state.flags |= tempw;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,0,0, 0);
        
        return 0;
}

static int opDAS(uint32_t fetchdat)
{
        uint16_t tempw;

        flags_rebuild();
        if ((cpu_state.flags & A_FLAG) || ((AL & 0xf) > 9))
        {
                int tempi = ((uint16_t)AL) - 6;
                AL -= 6;
                cpu_state.flags |= A_FLAG;
                if (tempi & 0x100) cpu_state.flags |= C_FLAG;
        }
        if ((cpu_state.flags & C_FLAG) || (AL > 0x9f))
        {
                AL -= 0x60;
                cpu_state.flags |= C_FLAG;
        }

        tempw = cpu_state.flags & (C_FLAG | A_FLAG);
        setznp8(AL);
        flags_rebuild();
        cpu_state.flags |= tempw;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,0,0, 0);
        
        return 0;
}
