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
 * Version:	@(#)x86_ops_mmx.h	1.0.1	2018/02/14
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

#define SSATB(val) (((val) < -128) ? -128 : (((val) > 127) ? 127 : (val)))
#define SSATW(val) (((val) < -32768) ? -32768 : (((val) > 32767) ? 32767 : (val)))
#define USATB(val) (((val) < 0) ? 0 : (((val) > 255) ? 255 : (val)))
#define USATW(val) (((val) < 0) ? 0 : (((val) > 65535) ? 65535 : (val)))

#define MMX_GETSRC()                                                            \
        if (cpu_mod == 3)                                                           \
        {                                                                       \
                src = cpu_state.MM[cpu_rm];                                                   \
                CLOCK_CYCLES(1);                                                \
        }                                                                       \
        else                                                                    \
        {                                                                       \
                src.q = readmemq(easeg, cpu_state.eaaddr); if (cpu_state.abrt) return 1;            \
                CLOCK_CYCLES(2);                                                \
        }

#define MMX_ENTER()                                                     \
        if (!cpu_hasMMX)                                                \
        {                                                               \
                cpu_state.pc = cpu_state.oldpc;                                   \
                x86illegal();                                           \
                return 1;                                               \
        }                                                               \
        if (cr0 & 0xc)                                                  \
        {                                                               \
                x86_int(7);                                             \
                return 1;                                               \
        }                                                               \
        x87_set_mmx()

static int opEMMS(uint32_t fetchdat)
{
        if (!cpu_hasMMX)
        {
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                return 1;
        }
        if (cr0 & 4)
        {
                x86_int(7);
                return 1;
        }
        x87_emms();
        CLOCK_CYCLES(100); /*Guess*/
        return 0;
}
