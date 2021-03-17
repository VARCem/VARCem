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
 * Version:	@(#)x86_ops_msr.h	1.0.2	2020/12/15
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

static int opRDTSC(uint32_t fetchdat)
{
        if (!cpu_has_feature(CPU_FEATURE_RDTSC))
        {
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                return 1;
        }
        if ((cr4 & CR4_TSD) && CPL)
        {
                x86gpf("RDTSC when TSD set and CPL != 0", 0);
                return 1;
        }
        EAX = tsc & 0xffffffff;
        EDX = tsc >> 32;
        CLOCK_CYCLES(1);
        return 0;
}

static int opRDPMC(uint32_t fetchdat)
{
        if (ECX > 1 || (!(cr4 & CR4_PCE) && (cr0 & 1) && CPL))
        {
                x86gpf("RDPMC not allowed", 0);
                return 1;
        }
        EAX = EDX = 0;
        CLOCK_CYCLES(1);
        return 0;
}
