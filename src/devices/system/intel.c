/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Intel mainboards.
 *
 * Version:	@(#)intel.c	1.0.11	2019/05/05
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "intel.h"
#include "clk.h"


typedef struct {
    uint16_t	timer_latch;
    int64_t	timer;
} batman_t;


static uint8_t
config_read(uint16_t port, void *priv)
{
    uint8_t ret = 0x00;

    switch (port & 0x000f) {
	case 3:
		ret = 0xff;
		break;

	case 5:
		ret = 0xdf;
		break;
    }

    return ret;
}


static void
timer_over(void *priv)
{
    batman_t *dev = (batman_t *)priv;

    dev->timer = 0;
}


static void
timer_write(uint16_t addr, uint8_t val, void *priv)
{
    batman_t *dev = (batman_t *)priv;

    if (addr & 1)
	dev->timer_latch = (dev->timer_latch & 0xff) | (val << 8);
    else
	dev->timer_latch = (dev->timer_latch & 0xff00) | val;

    dev->timer = dev->timer_latch * TIMER_USEC;
}


static uint8_t
timer_read(uint16_t addr, void *priv)
{
    batman_t *dev = (batman_t *)priv;
    uint16_t latch;
    uint8_t ret;

    cycles -= (int)PITCONST;

    timer_clock();

    if (dev->timer < 0)
	return 0;

    latch = (uint16_t)(dev->timer / TIMER_USEC);

    if (addr & 1)
	ret = latch >> 8;
    else
	ret = latch & 0xff;

    return ret;
}


static void
batman_close(void *priv)
{
    batman_t *dev = (batman_t *) priv;

    free(dev);
}


static void *
batman_init(const device_t *info, UNUSED(void *parent))
{
    batman_t *dev = (batman_t *)mem_alloc(sizeof(batman_t));
    memset(dev, 0x00, sizeof(batman_t));

    io_sethandler(0x0073, 0x0001,
		  config_read,NULL,NULL, NULL,NULL,NULL, dev);
    io_sethandler(0x0075, 0x0001,
		  config_read,NULL,NULL, NULL,NULL,NULL, dev);

    io_sethandler(0x0078, 0x0002,
		  timer_read,NULL,NULL, timer_write,NULL,NULL, dev);

    timer_add(timer_over, dev, &dev->timer, &dev->timer);

    return(dev);
}


const device_t intel_batman_device = {
    "Intel Batman board chip",
    0, 0, NULL,
    batman_init, batman_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
