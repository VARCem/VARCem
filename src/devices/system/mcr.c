/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		INTEL 82355 MCR emulation.
 *		This chip was used as part of many 386 chipsets.
 *		It controls memory addressing and shadowing.
 *
 * Version:	@(#)mcr.c	1.0.4	2019/03/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../../emu.h"
#include "../../mem.h"
#include "../../rom.h"


int	nextreg6;
uint8_t	mcr22;
int	mcrlock,
	mcrfirst;


void
resetmcr(void)
{
    mcrlock = 0;
    mcrfirst = 1;
    shadowbios = 0;
}


void
writemcr(uint16_t addr, uint8_t val)
{
    switch (addr) {
	case 0x22:
		if (val == 6 && mcr22 == 6)
			nextreg6 = 1;
		  else
			nextreg6=0;
		break;

	case 0x23:
		if (nextreg6)
			shadowbios = !val;
		break;
    }

    mcr22 = val;
}
