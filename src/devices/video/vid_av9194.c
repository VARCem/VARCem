/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		AV9194 clock generator emulation..
 *
 * Version:	@(#)vid_av9194.c	1.0.1	2019/01/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2019 Fred N. van Kempen.
 *		Copyright 2019 Miran Grca.
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
#include "../../emu.h"
#include "../../device.h"
#include "video.h"
#include "vid_av9194.h"


float
av9194_getclock(int clock, void *priv)
{
    float ret = 0.0;

    switch (clock & 0x0f) {
	case 0:
		ret = 25175000.0;
		break;

	case 1:
		ret = 28322000.0;
		break;

	case 2:
		ret = 40000000.0;
		break;

	case 4:
		ret = 50000000.0;
		break;
	case 5:
		ret = 77000000.0;
		break;

	case 6:
		ret = 36000000.0;
		break;

	case 7:
		ret = 44900000.0;
		break;

	case 8:
		ret = 130000000.0;
		break;

	case 9:
		ret = 120000000.0;
		break;

	case 0xa:
		ret = 80000000.0;
		break;

	case 0xb:
		ret = 31500000.0;
		break;

	case 0xc:
		ret = 110000000.0;
		break;

	case 0xd:
		ret = 65000000.0;
		break;

	case 0xe:
		ret = 75000000.0;
		break;

	case 0xf:
		ret = 94500000.0;
		break;
    }

    return ret;
}


static void *
av9194_init(const device_t *info)
{
    /* Return something non-NULL. */
    return (void *)&av9194_device;
}


const device_t av9194_device = {
    "AV9194 Clock Generator",
    0,
    0,
    av9194_init, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
