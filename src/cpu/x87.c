/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of 8087 opcodes.
 *
 * Version:	@(#)x87.c	1.0.3	2018/10/05
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include "../emu.h"
#include "cpu.h"
#include "../mem.h"
#include "../devices/system/pic.h"
#include "x86.h"
#include "x86_flags.h"
#include "x86_ops.h"
#include "x87.h"
#include "386_common.h"


uint32_t	x87_pc_off,
		x87_op_off;
uint16_t	x87_pc_seg,
		x87_op_seg;


uint16_t x87_gettag(void)
{
        uint16_t ret = 0;
        int c;
        
        for (c = 0; c < 8; c++)
        {
                if (cpu_state.tag[c] & TAG_UINT64)
                        ret |= 2 << (c*2);
                else
                        ret |= (cpu_state.tag[c] << (c*2));
        }

        return ret;
}

void x87_settag(uint16_t new_tag)
{
        cpu_state.tag[0] = new_tag & 3;
        cpu_state.tag[1] = (new_tag >> 2) & 3;
        cpu_state.tag[2] = (new_tag >> 4) & 3;
        cpu_state.tag[3] = (new_tag >> 6) & 3;
        cpu_state.tag[4] = (new_tag >> 8) & 3;
        cpu_state.tag[5] = (new_tag >> 10) & 3;
        cpu_state.tag[6] = (new_tag >> 12) & 3;
        cpu_state.tag[7] = (new_tag >> 14) & 3;
}

void x87_dumpregs(void)
{
        if (cpu_state.ismmx)
        {
                ERRLOG("MM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\n", cpu_state.MM[0].q, cpu_state.MM[1].q, cpu_state.MM[2].q, cpu_state.MM[3].q);
                ERRLOG("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", cpu_state.MM[4].q, cpu_state.MM[5].q, cpu_state.MM[6].q, cpu_state.MM[7].q);
        }
        else
        {
                ERRLOG("ST(0)=%f\tST(1)=%f\tST(2)=%f\tST(3)=%f\t\n",cpu_state.ST[cpu_state.TOP],cpu_state.ST[(cpu_state.TOP+1)&7],cpu_state.ST[(cpu_state.TOP+2)&7],cpu_state.ST[(cpu_state.TOP+3)&7]);
                ERRLOG("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n",cpu_state.ST[(cpu_state.TOP+4)&7],cpu_state.ST[(cpu_state.TOP+5)&7],cpu_state.ST[(cpu_state.TOP+6)&7],cpu_state.ST[(cpu_state.TOP+7)&7]);
        }
        ERRLOG("Status = %04X  Control = %04X  Tag = %04X\n", cpu_state.npxs, cpu_state.npxc, x87_gettag());
}

void x87_print(void)
{
        if (cpu_state.ismmx)
        {
                ERRLOG("\tMM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\t", cpu_state.MM[0].q, cpu_state.MM[1].q, cpu_state.MM[2].q, cpu_state.MM[3].q);
                ERRLOG("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", cpu_state.MM[4].q, cpu_state.MM[5].q, cpu_state.MM[6].q, cpu_state.MM[7].q);
        }
        else
        {
                ERRLOG("\tST(0)=%.20f\tST(1)=%.20f\tST(2)=%f\tST(3)=%f\t",cpu_state.ST[cpu_state.TOP&7],cpu_state.ST[(cpu_state.TOP+1)&7],cpu_state.ST[(cpu_state.TOP+2)&7],cpu_state.ST[(cpu_state.TOP+3)&7]);
                ERRLOG("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\tTOP=%i CR=%04X SR=%04X TAG=%04X\n",cpu_state.ST[(cpu_state.TOP+4)&7],cpu_state.ST[(cpu_state.TOP+5)&7],cpu_state.ST[(cpu_state.TOP+6)&7],cpu_state.ST[(cpu_state.TOP+7)&7], cpu_state.TOP, cpu_state.npxc, cpu_state.npxs, x87_gettag());
        }
}

void x87_reset(void)
{
}
