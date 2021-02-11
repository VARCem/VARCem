/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the "port 92" pseudo-device.
 *
 * Version:	@(#)port92.c	1.0.2	2021/02/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2008-2019 Sarah Walker.
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "port92.h"


static void
p92_reset(priv_t priv)
{
    port92_t *dev = (port92_t *)priv;

    dev->reg = 0;

    mem_a20_alt = 0;
    mem_a20_recalc();

    flushmmucache();
}


static uint8_t
p92_read(uint16_t port, priv_t priv)
{
    port92_t *dev = (port92_t *)priv;

    return(dev->reg | dev->mask);
}


static void
p92_write(uint16_t port, uint8_t val, priv_t priv)
{
    port92_t *dev = (port92_t *)priv;

    if ((mem_a20_alt ^ val) & 2) {
	mem_a20_alt = (val & 2);
	mem_a20_recalc();
    }

    if ((~dev->reg & val) & 1) {
	cpu_reset(0);
	cpu_set_edx();
    }

    dev->reg = val | dev->mask;
}


static void
p92_close(priv_t priv)
{
    port92_t *dev = (port92_t *)priv;

    io_removehandler(0x0092, 1,
		     p92_read,NULL,NULL, p92_write,NULL,NULL, dev);

    free(dev);
}


static priv_t
p92_init(const device_t *info, UNUSED(void *parent))
{
    port92_t *dev;

    dev = (port92_t *)mem_alloc(sizeof(port92_t));
    memset(dev, 0x00, sizeof(port92_t));

    dev->mask = (info->local) ? 0xfc : 0x00;

    io_sethandler(0x0092, 1,
		  p92_read,NULL,NULL, p92_write,NULL,NULL, dev);

    p92_reset((priv_t)dev);

    return((priv_t)dev);
}


const device_t port92_device = {
    "Port 92",
    0,
    0,
    NULL,
    p92_init, p92_close, p92_reset,
    NULL, NULL, NULL, NULL,
    NULL
};


const device_t port92_inverted_device = {
    "Port 92 (Inverted)",
    0,
    1,
    NULL,
    p92_init, p92_close, p92_reset,
    NULL, NULL, NULL, NULL,
    NULL
};
