/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Super I/O chip detection code.
 *
 * Version:	@(#)sio_detect.c	1.0.7	2019/05/13
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../device.h"
#include "../../io.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../../plat.h"
#include "sio.h"


typedef struct {
    uint8_t	regs[2];
} sio_detect_t;


static void
detect_write(uint16_t port, uint8_t val, priv_t priv)
{
    sio_detect_t *dev = (sio_detect_t *)priv;

    DEBUG("superio_detect_write : port=%04x = %02X\n", port, val);

    dev->regs[port & 1] = val;
}


static uint8_t
detect_read(uint16_t port, priv_t priv)
{
    sio_detect_t *dev = (sio_detect_t *)priv;
    uint8_t ret;

    ret = dev->regs[port & 1];

    DEBUG("superio_detect_read : port=%04x = %02X\n", port, ret);

    return ret;
}


static void
detect_close(priv_t priv)
{
    sio_detect_t *dev = (sio_detect_t *)priv;

    free(dev);
}


static priv_t
detect_init(const device_t *info, UNUSED(void *parent))
{
    sio_detect_t *dev = (sio_detect_t *)mem_alloc(sizeof(sio_detect_t));
    memset(dev, 0x00, sizeof(sio_detect_t));

    device_add(&fdc_at_smc_device);

    io_sethandler(0x0024, 4,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x002e, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x0044, 4,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x004e, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x0108, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x0250, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x0370, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);
    io_sethandler(0x03f0, 2,
		  detect_read,NULL,NULL, detect_write,NULL,NULL, dev);

    return((priv_t)dev);
}


const device_t sio_detect_device = {
    "Super I/O Detection Helper",
    0, 0, NULL,
    detect_init, detect_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
