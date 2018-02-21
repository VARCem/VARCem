/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of (dummy) PPI module.
 *
 *		IBM 5150 cassette nonsense:
 *		  Calls F979 twice
 *		  Expects CX to be nonzero, BX >$410 and <$540
 *		  CX is loops between bit 4 of $62 changing
 *		  BX is timer difference between calls
 *
 * Version:	@(#)ppi.c	1.0.1	2018/02/14
 *
 * Author:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2008-2018 Sarah Walker.
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
#include "pit.h"
#include "ppi.h"


PPI ppi;
int ppispeakon;


void ppi_reset(void)
{
        ppi.pa=0x0;
        ppi.pb=0x40;
}
