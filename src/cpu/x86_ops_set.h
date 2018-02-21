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
 * Version:	@(#)x86_ops_set.h	1.0.1	2018/02/14
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

#define opSET(condition)                                                \
        static int opSET ## condition ## _a16(uint32_t fetchdat)        \
        {                                                               \
                fetch_ea_16(fetchdat);                                  \
                seteab((cond_ ## condition) ? 1 : 0);                   \
                CLOCK_CYCLES(4);                                        \
                return cpu_state.abrt;                                            \
        }                                                               \
                                                                        \
        static int opSET ## condition ## _a32(uint32_t fetchdat)        \
        {                                                               \
                fetch_ea_32(fetchdat);                                  \
                seteab((cond_ ## condition) ? 1 : 0);                   \
                CLOCK_CYCLES(4);                                        \
                return cpu_state.abrt;                                            \
        }

opSET(O)
opSET(NO)
opSET(B)
opSET(NB)
opSET(E)
opSET(NE)
opSET(BE)
opSET(NBE)
opSET(S)
opSET(NS)
opSET(P)
opSET(NP)
opSET(L)
opSET(NL)
opSET(LE)
opSET(NLE)
