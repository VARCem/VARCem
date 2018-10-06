/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of the NatSemi DP8390 ethernet controller chip.
 *
 * Version:	@(#)net_dp8390.c	1.0.1	2018/09/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Peter Grehan, <grehan@iprg.nokia.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Portions Copyright (C) 2002  MandrakeSoft S.A.
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
#include <stdlib.h>
#include <wchar.h>
#define dbglog network_dev_log
#include "../../emu.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "network.h"
#include "net_dp8390.h"
#include "bswap.h"


/*
 * Return the 6-bit index into the multicast table.
 *
 * Stolen unashamedly from FreeBSD's if_ed.c
 */
int
mcast_index(const void *dst)
{
#define POLYNOMIAL 0x04c11db6
    uint32_t crc = 0xffffffffL;
    int carry, i, j;
    uint8_t b;
    uint8_t *ep = (uint8_t *)dst;

    for (i=6; --i>=0;) {
	b = *ep++;
	for (j = 8; --j >= 0;) {
		carry = ((crc & 0x80000000L) ? 1 : 0) ^ (b & 0x01);
		crc <<= 1;
		b >>= 1;
		if (carry)
			crc = ((crc ^ POLYNOMIAL) | carry);
	}
    }
    return(crc >> 26);
#undef POLYNOMIAL
}
