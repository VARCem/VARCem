/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the UM8669f super-io chip.
 *
 *		  aa to 108 unlocks
 *		  next 108 write is register select (Cx?)
 *		  data read/write to 109
 *		  55 to 108 locks
 *
 *		  C1
 *			bit 7 - enable PnP registers
 *
 *		  PnP registers:
 *
 *			07 - device :
 *		        0 = FDC
 *		        1 = COM1
 *		        2 = COM2
 *		        3 = LPT1
 *		        5 = Game port
 *			30 - enable
 *			60/61 - addr
 *			70 - IRQ
 *			74 - DMA
 *
 * Version:	@(#)sio_um8669f.c	1.0.11	2019/04/09
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
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
#include "../../io.h"
#include "../../device.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../../plat.h"
#include "sio.h"


#define DEV_FDC		0
#define DEV_COM1	1
#define DEV_COM2	2
#define DEV_LPT1	3
#define DEV_GAME	5

#define REG_DEVICE	0x07
#define REG_ENABLE	0x30
#define REG_ADDRHI	0x60
#define REG_ADDRLO	0x61
#define REG_IRQ		0x70
#define REG_DMA		0x74


typedef struct {
    int8_t	locked,
		pnp_active;

    uint8_t	cur_reg_108,
		cur_reg,
		cur_device;

    struct {
	int8_t	enable;
	uint16_t addr;
	int8_t	irq;
	int8_t	dma;
    }		dev[8];

    fdc_t	*fdc;

    uint8_t	regs_108[256];
} um8669f_t;


static void
pnp_write(uint16_t port, uint8_t val, void *priv)
{
    um8669f_t *dev = (um8669f_t *)priv;
    uint8_t valxor = 0;

    if (port == 0x0279)
	dev->cur_reg = val;
    else {
	if (dev->cur_reg == REG_DEVICE)
		dev->cur_device = val & 7;
	else {
		switch (dev->cur_reg) {
			case REG_ENABLE:
				valxor = dev->dev[dev->cur_device].enable ^ val;
				dev->dev[dev->cur_device].enable = val;
				break;

			case REG_ADDRLO:
				valxor = (dev->dev[dev->cur_device].addr & 0xff) ^ val;
				dev->dev[dev->cur_device].addr = (dev->dev[dev->cur_device].addr & 0xff00) | val;
				break;

			case REG_ADDRHI:
				valxor = ((dev->dev[dev->cur_device].addr >> 8) & 0xff) ^ val;
				dev->dev[dev->cur_device].addr = (dev->dev[dev->cur_device].addr & 0x00ff) | (val << 8);
				break;

			case REG_IRQ:
				valxor = dev->dev[dev->cur_device].irq ^ val;
				dev->dev[dev->cur_device].irq = val;
				break;

			case REG_DMA:
				valxor = dev->dev[dev->cur_device].dma ^ val;
				dev->dev[dev->cur_device].dma = val;
				break;

			default:
				valxor = 0;
				break;
		}

		switch (dev->cur_device) {
			case DEV_FDC:
				if ((dev->cur_reg == REG_ENABLE) && valxor) {
					fdc_remove(dev->fdc);
					if (dev->dev[DEV_FDC].enable & 1)
						fdc_set_base(dev->fdc, 0x03f0);
				}
				break;

			case DEV_COM1:
				if ((dev->cur_reg == REG_ENABLE) && valxor) {
//	                                serial_remove(0);
					if (dev->dev[DEV_COM1].enable & 1)
						serial_setup(0, dev->dev[DEV_COM1].addr, dev->dev[DEV_COM1].irq);
				}
				break;

			case DEV_COM2:
				if ((dev->cur_reg == REG_ENABLE) && valxor) {
//					serial_remove(1);
					if (dev->dev[DEV_COM2].enable & 1)
						serial_setup(1, dev->dev[DEV_COM2].addr, dev->dev[DEV_COM2].irq);
				}
				break;

			case DEV_LPT1:
				if ((dev->cur_reg == REG_ENABLE) && valxor) {
//					parallel_remove(0);
					if (dev->dev[DEV_LPT1].enable & 1)
						parallel_setup(0, dev->dev[DEV_LPT1].addr);
				}
				break;
		}
	}
    }
}


static uint8_t
pnp_read(uint16_t port, void *priv)
{
    um8669f_t *dev = (um8669f_t *)priv;
    uint8_t ret = 0xff;

    switch (dev->cur_reg) {
	case REG_DEVICE:
		ret = dev->cur_device;
		break;

	case REG_ENABLE:
		ret = dev->dev[dev->cur_device].enable;
		break;

	case REG_ADDRLO:
		ret = dev->dev[dev->cur_device].addr & 0xff;
		break;

	case REG_ADDRHI:
		ret = dev->dev[dev->cur_device].addr >> 8;
		break;

	case REG_IRQ:
		ret = dev->dev[dev->cur_device].irq;
		break;

	case REG_DMA:
		ret = dev->dev[dev->cur_device].dma;
		break;
    }

    return ret;
}


static void
um8669f_write(uint16_t port, uint8_t val, void *priv)
{
    um8669f_t *dev = (um8669f_t *)priv;
    int new_pnp_active;

    if (dev->locked) {
	if ((port == 0x0108) && (val == 0xaa))
		dev->locked = 0;
    } else {
	if (port == 0x0108) {
		if (val == 0x55)
			dev->locked = 1;
		else
			dev->cur_reg_108 = val;
	} else {
		dev->regs_108[dev->cur_reg_108] = val;

		if (dev->cur_reg_108 == 0xc1) {
			new_pnp_active = !!(dev->regs_108[0xc1] & 0x80);
			if (new_pnp_active != dev->pnp_active) {
				if (new_pnp_active) {
					io_sethandler(0x0279, 1,
						      NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
					io_sethandler(0x0a79, 1,
						      NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
					io_sethandler(0x03e3, 1,
						      pnp_read,NULL,NULL, NULL,NULL,NULL, dev);
				} else {
					io_removehandler(0x0279, 1,
							 NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
					io_removehandler(0x0a79, 1,
							 NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
					io_removehandler(0x03e3, 1,
							 pnp_read,NULL,NULL, NULL,NULL,NULL, dev);
				}
				dev->pnp_active = new_pnp_active;
			}
		}
	}
    }
}


static uint8_t
um8669f_read(uint16_t port, void *priv)
{
    um8669f_t *dev = (um8669f_t *)priv;
    uint8_t ret = 0xff;

    if (! dev->locked) {
	if (port == 0x0108)
		ret = dev->cur_reg_108;	/* ??? */
	else
		ret = dev->regs_108[dev->cur_reg_108];
    }

    return ret;
}


static void
um8669f_reset(um8669f_t *dev)
{
    fdc_reset(dev->fdc);

#if 0
    serial_remove(0);
#endif
    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);

#if 0
    serial_remove(1);
#endif
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

#if 0
    parallel_remove(1);

    parallel_remove(0);
#endif
    parallel_setup(0, 0x0378);

    if (dev->pnp_active) {
	io_removehandler(0x0279, 1, NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
	io_removehandler(0x0a79, 1, NULL,NULL,NULL, pnp_write,NULL,NULL, dev);
	io_removehandler(0x03e3, 1, pnp_read,NULL,NULL, NULL,NULL,NULL, dev);
	dev->pnp_active = 0;
    }

    dev->locked = 1;

    dev->dev[DEV_FDC].enable = 1;
    dev->dev[DEV_FDC].addr = 0x03f0;
    dev->dev[DEV_FDC].irq = 6;
    dev->dev[DEV_FDC].dma = 2;

    dev->dev[DEV_COM1].enable = 1;
    dev->dev[DEV_COM1].addr = 0x03f8;
    dev->dev[DEV_COM1].irq = 4;

    dev->dev[DEV_COM2].enable = 1;
    dev->dev[DEV_COM2].addr = 0x02f8;
    dev->dev[DEV_COM2].irq = 3;

    dev->dev[DEV_LPT1].enable = 1;
    dev->dev[DEV_LPT1].addr = 0x0378;
    dev->dev[DEV_LPT1].irq = 7;
}


static void
um8669f_close(void *priv)
{
    um8669f_t *dev = (um8669f_t *)priv;

    free(dev);
}


static void *
um8669f_init(const device_t *info, UNUSED(void *parent))
{
    um8669f_t *dev = (um8669f_t *)mem_alloc(sizeof(um8669f_t));
    memset(dev, 0x00, sizeof(um8669f_t));

    dev->fdc = device_add(&fdc_at_device);

    io_sethandler(0x0108, 2,
		  um8669f_read,NULL,NULL, um8669f_write,NULL,NULL, dev);

    um8669f_reset(dev);

    return dev;
}


const device_t um8669f_device = {
    "UMC UM8669F Super I/O",
    0, 0, NULL,
    um8669f_init, um8669f_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
