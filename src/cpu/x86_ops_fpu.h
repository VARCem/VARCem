/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x86 FPU Instructions.
 *
 * Version:	@(#)x86_ops_fpu.h	1.0.1	2018/02/14
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

static int opESCAPE_d8_a16(uint32_t fetchdat)
{
        return x86_opcodes_d8_a16[(fetchdat >> 3) & 0x1f](fetchdat);
}
static int opESCAPE_d8_a32(uint32_t fetchdat)
{
        return x86_opcodes_d8_a32[(fetchdat >> 3) & 0x1f](fetchdat);
}

static int opESCAPE_d9_a16(uint32_t fetchdat)
{
        return x86_opcodes_d9_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_d9_a32(uint32_t fetchdat)
{
        return x86_opcodes_d9_a32[fetchdat & 0xff](fetchdat);
}

static int opESCAPE_da_a16(uint32_t fetchdat)
{
        return x86_opcodes_da_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_da_a32(uint32_t fetchdat)
{
        return x86_opcodes_da_a32[fetchdat & 0xff](fetchdat);
}

static int opESCAPE_db_a16(uint32_t fetchdat)
{
        return x86_opcodes_db_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_db_a32(uint32_t fetchdat)
{
        return x86_opcodes_db_a32[fetchdat & 0xff](fetchdat);
}

static int opESCAPE_dc_a16(uint32_t fetchdat)
{
        return x86_opcodes_dc_a16[(fetchdat >> 3) & 0x1f](fetchdat);
}
static int opESCAPE_dc_a32(uint32_t fetchdat)
{
        return x86_opcodes_dc_a32[(fetchdat >> 3) & 0x1f](fetchdat);
}

static int opESCAPE_dd_a16(uint32_t fetchdat)
{
        return x86_opcodes_dd_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_dd_a32(uint32_t fetchdat)
{
        return x86_opcodes_dd_a32[fetchdat & 0xff](fetchdat);
}

static int opESCAPE_de_a16(uint32_t fetchdat)
{
        return x86_opcodes_de_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_de_a32(uint32_t fetchdat)
{
        return x86_opcodes_de_a32[fetchdat & 0xff](fetchdat);
}

static int opESCAPE_df_a16(uint32_t fetchdat)
{
        return x86_opcodes_df_a16[fetchdat & 0xff](fetchdat);
}
static int opESCAPE_df_a32(uint32_t fetchdat)
{
        return x86_opcodes_df_a32[fetchdat & 0xff](fetchdat);
}

static int opWAIT(uint32_t fetchdat)
{
        if ((cr0 & 0xa) == 0xa)
        {
                x86_int(7);
                return 1;
        }
        CLOCK_CYCLES(4);
        return 0;
}
