/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the parallel-port-attached devices.
 *
 * Version:	@(#)parallel_dev.c	1.0.1	2018/04/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "emu.h"
#include "io.h"
#include "parallel.h"
#include "parallel_dev.h"

#include "sound/snd_lpt_dac.h"
#include "sound/snd_lpt_dss.h"


static const struct {
    const char	*name;
    const char	*internal_name;
    const lpt_device_t *device;
} devices[] = {
    {"None",
     "none",			NULL					},
    {"Disney Sound Source",
     "dss",			&dss_device				},
    {"LPT DAC / Covox Speech Thing",
     "lpt_dac",			&lpt_dac_device				},
    {"Stereo LPT DAC",
     "lpt_dac_stereo",		&lpt_dac_stereo_device			},
    {NULL,			NULL,
     NULL								}
};


const char *
parallel_device_get_name(int id)
{
    return(devices[id].name);
}


const char *
parallel_device_get_internal_name(int id)
{
    return(devices[id].internal_name);
}


const lpt_device_t *
parallel_device_get_device(int id)
{
    return(devices[id].device);
}


int
parallel_device_get_from_internal_name(char *s)
{
    int c = 0;

    while (devices[c].internal_name != NULL) {
	if (! strcmp((char *)devices[c].internal_name, s))
		return(c);
	c++;
    }

    return(-1);
}
