/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the WD76C10 System Controller chip.
 *
 * Version:	@(#)wd76c10.c	1.0.12	2019/04/08
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "wd76c10.h"


typedef struct {
    int		type;

    uint16_t	reg_0092;
    uint16_t	reg_2072;
    uint16_t	reg_2872;
    uint16_t	reg_5872;

    fdc_t	*fdc;
} wd76c10_t;


static uint16_t
wd76c10_read(uint16_t port, void *priv)
{
    wd76c10_t *dev = (wd76c10_t *)priv;
    int16_t ret = 0xffff;

    switch (port) {
	case 0x0092:
		ret = dev->reg_0092;
		break;

	case 0x2072:
		ret = dev->reg_2072;
		break;

	case 0x2872:
		ret = dev->reg_2872;
		break;

	case 0x5872:
		ret = dev->reg_5872;
		break;
    }

    return(ret);
}


static void
wd76c10_write(uint16_t port, uint16_t val, void *priv)
{
    wd76c10_t *dev = (wd76c10_t *)priv;

    switch (port) {
	case 0x0092:
		dev->reg_0092 = val;

		mem_a20_alt = val & 2;
		mem_a20_recalc();
		break;

	case 0x2072:
		dev->reg_2072 = val;

#if 0
		serial_remove(0);
#endif
		if (! (val & 0x10)) {
			switch ((val >> 5) & 7) {
				case 1:
					serial_setup(0, 0x3f8, 4);
					break;

				case 2:
					serial_setup(0, 0x2f8, 4);
					break;

				case 3:
					serial_setup(0, 0x3e8, 4);
					break;

				case 4:
					serial_setup(0, 0x2e8, 4);
					break;

				default:
					break;
			}
		}
#if 0
		serial_remove(1);
#endif
		if (! (val & 0x01)) {
			switch ((val >> 1) & 7) {
				case 1:
					serial_setup(1, 0x3f8, 3);
					break;

				case 2:
					serial_setup(1, 0x2f8, 3);
					break;

				case 3:
					serial_setup(1, 0x3e8, 3);
					break;

				case 4:
					serial_setup(1, 0x2e8, 3);
					break;

				default:
					break;
			}
		}
		break;

	case 0x2872:
		dev->reg_2872 = val;

		fdc_remove(dev->fdc);
		if (! (val & 1))
			fdc_set_base(dev->fdc, 0x03f0);
		break;

	case 0x5872:
		dev->reg_5872 = val;
		break;
    }
}


static uint8_t
wd76c10_readb(uint16_t port, void *priv)
{
    if (port & 1)
	return(wd76c10_read(port & ~1, priv) >> 8);

    return(wd76c10_read(port, priv) & 0xff);
}


static void
wd76c10_writeb(uint16_t port, uint8_t val, void *priv)
{
    uint16_t temp = wd76c10_read(port, priv);

    if (port & 1)
	wd76c10_write(port & ~1, (temp & 0x00ff) | (val << 8), priv);
    else
	wd76c10_write(port     , (temp & 0xff00) | val, priv);
}


static void
wd76c10_close(void *priv)
{
    wd76c10_t *dev = (wd76c10_t *)priv;

    free(dev);
}


static void *
wd76c10_init(const device_t *info, UNUSED(void *parent))
{
    wd76c10_t *dev;

    dev = (wd76c10_t *)mem_alloc(sizeof(wd76c10_t));
    memset(dev, 0x00, sizeof(wd76c10_t));
    dev->type = info->local;

    dev->fdc = (fdc_t *)device_add(&fdc_at_device);

    io_sethandler(0x0092, 2,
		  wd76c10_readb,wd76c10_read,NULL,
		  wd76c10_writeb,wd76c10_write,NULL, dev);
    io_sethandler(0x2072, 2,
		  wd76c10_readb,wd76c10_read,NULL,
		  wd76c10_writeb,wd76c10_write,NULL, dev);
    io_sethandler(0x2872, 2,
		  wd76c10_readb,wd76c10_read,NULL,
		  wd76c10_writeb,wd76c10_write,NULL, dev);
    io_sethandler(0x5872, 2,
		  wd76c10_readb,wd76c10_read,NULL,
		  wd76c10_writeb,wd76c10_write,NULL, dev);

    return(dev);
}


const device_t wd76c10_device = {
    "WD 76C10",
    0,
    0,
    NULL,
    wd76c10_init, wd76c10_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
