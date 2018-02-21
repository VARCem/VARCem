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
 * Version:	@(#)x86_ops_int.h	1.0.1	2018/02/14
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

static int opINT3(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 1;
        }
        x86_int_sw(3);
        CLOCK_CYCLES((is486) ? 44 : 59);
        PREFETCH_RUN(cycles_old-cycles, 1, -1, 0,0,0,0, 0);
        return 1;
}

static int opINT1(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 1;
        }
        x86_int_sw(1);
        CLOCK_CYCLES((is486) ? 44 : 59);
        PREFETCH_RUN(cycles_old-cycles, 1, -1, 0,0,0,0, 0);
        return 1;
}

static int opINT(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        uint8_t temp = getbytef();

        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                if (cr4 & CR4_VME)
                {
                        uint16_t t;
                        uint8_t d;

                        cpl_override = 1;
                        t = readmemw(tr.base, 0x66) - 32;
                        cpl_override = 0;
                        if (cpu_state.abrt) return 1;

                        t += (temp >> 3);
                        if (t <= tr.limit)
                        {
                                cpl_override = 1;
                                d = readmemb(tr.base, t);// + (temp >> 3));
                                cpl_override = 0;
                                if (cpu_state.abrt) return 1;

                                if (!(d & (1 << (temp & 7))))
                                {
                                        x86_int_sw_rm(temp);
                                        PREFETCH_RUN(cycles_old-cycles, 2, -1, 0,0,0,0, 0);
                                        return 1;
                                }
                        }
                }
                x86gpf(NULL,0);
                return 1;
        }

        x86_int_sw(temp);
        PREFETCH_RUN(cycles_old-cycles, 2, -1, 0,0,0,0, 0);
        return 1;
}

static int opINTO(uint32_t fetchdat)
{
        int cycles_old = cycles; UN_USED(cycles_old);
        
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 1;
        }
        if (VF_SET())
        {
                cpu_state.oldpc = cpu_state.pc;
                x86_int_sw(4);
                PREFETCH_RUN(cycles_old-cycles, 1, -1, 0,0,0,0, 0);
                return 1;
        }
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 1, -1, 0,0,0,0, 0);
        return 0;
}

