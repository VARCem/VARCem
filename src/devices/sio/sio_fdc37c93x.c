/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SMC FDC37C932FR and FDC37C935 Super
 *		I/O Chips.
 *
 * Version:	@(#)sio_fdc37c93x.c	1.0.11	2018/11/11
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
#include "../../io.h"
#include "../../device.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "sio.h"


#define AB_RST	0x80


typedef struct {
    uint16_t	base;
    uint8_t	control;
    uint8_t	status;
    uint8_t	own_addr;
    uint8_t	data;
    uint8_t	clock;
} access_bus_t;

typedef struct {
    uint8_t	chip_id,
		tries,
		auxio_reg,
		cur_reg;

    uint16_t	gpio_base,	/* Set to EA */
		auxio_base;

    int		locked;

    fdc_t	*fdc;

    access_bus_t *access_bus;

    uint8_t	regs[48],
		gpio_regs[2],
		ld_regs[10][256];
} fdc37c93x_t;


static uint16_t
make_port(fdc37c93x_t *dev, uint8_t ld)
{
    uint16_t r0 = dev->ld_regs[ld][0x60];
    uint16_t r1 = dev->ld_regs[ld][0x61];

    uint16_t p = (r0 << 8) + r1;

    return p;
}


static uint8_t
auxio_read(uint16_t port, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;

    return dev->auxio_reg;
}


static void
auxio_write(uint16_t port, uint8_t val, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;

    dev->auxio_reg = val;
}


static void
auxio_handler(fdc37c93x_t *dev)
{
    uint8_t local_enable = !!dev->ld_regs[8][0x30];
    uint16_t ld_port;

    io_removehandler(dev->auxio_base, 1,
		     auxio_read,NULL,NULL, auxio_write,NULL,NULL, dev);

    if (local_enable) {
	dev->auxio_base = ld_port = make_port(dev, 8);
	if ((ld_port >= 0x0100) && (ld_port <= 0x0fff))
	        io_sethandler(dev->auxio_base, 0x0001,
			      auxio_read,NULL,NULL, auxio_write,NULL,NULL, dev);
    }
}


static uint8_t
gpio_read(uint16_t port, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;

    return dev->gpio_regs[port & 1];
}


static void
gpio_write(uint16_t port, uint8_t val, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;

    dev->gpio_regs[port & 1] = val;
}


static void
gpio_handler(fdc37c93x_t *dev)
{
    uint8_t local_enable = !!(dev->regs[0x03] & 0x80);
    uint16_t ld_port = 0x0000;

    io_removehandler(dev->gpio_base, 0x0002,
		     gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);

    if (local_enable) {
	switch (dev->regs[0x03] & 0x03) {
		case 0:
			ld_port = 0xe0;
			break;

		case 1:
			ld_port = 0xe2;
			break;

		case 2:
			ld_port = 0xe4;
			break;

		case 3:
			ld_port = 0xea;	/* Default */
			break;
	}
	dev->gpio_base = ld_port;

	if ((ld_port >= 0x0100) && (ld_port <= 0x0ffe))
	        io_sethandler(dev->gpio_base, 0x0002,
			      gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);
    }
}


static void
fdc_handler(fdc37c93x_t *dev)
{
    uint8_t global_enable = !!(dev->regs[0x22] & (1 << 0));
    uint8_t local_enable = !!dev->ld_regs[0][0x30];
    uint16_t ld_port;

    fdc_remove(dev->fdc);
    if (global_enable && local_enable) {
	ld_port = make_port(dev, 0);
	if ((ld_port >= 0x0100) && (ld_port <= 0x0ff8))
		fdc_set_base(dev->fdc, ld_port);
    }
}


static void
lpt_handler(fdc37c93x_t *dev)
{
    uint8_t global_enable = !!(dev->regs[0x22] & (1 << 3));
    uint8_t local_enable = !!dev->ld_regs[3][0x30];
    uint16_t ld_port;

//   parallel_remove(0);
    if (global_enable && local_enable) {
	ld_port = make_port(dev, 3);
	if ((ld_port >= 0x0100) && (ld_port <= 0x0ffc))
		parallel_setup(0, ld_port);
    }
}


static void
serial_handler(fdc37c93x_t *dev, int uart)
{
    uint8_t uart_no = 4 + uart;
    uint8_t global_enable = !!(dev->regs[0x22] & (1 << uart_no));
    uint8_t local_enable = !!dev->ld_regs[uart_no][0x30];
    uint16_t ld_port;

//    serial_remove(uart);
    if (global_enable && local_enable) {
	ld_port = make_port(dev, uart_no);
	if ((ld_port >= 0x0100) && (ld_port <= 0x0ff8))
		serial_setup(uart, ld_port, dev->ld_regs[uart_no][0x70]);
    }
}


static uint8_t
access_bus_read(uint16_t port, void *priv)
{
    access_bus_t *dev = (access_bus_t *)priv;
    uint8_t ret = 0xff;

    switch(port & 3) {
	case 0:
		ret = (dev->status & 0xbf);
		break;

	case 1:
		ret = (dev->own_addr & 0x7f);
		break;

	case 2:
		ret = dev->data;
		break;

	case 3:
		ret = (dev->clock & 0x87);
		break;
    }

    return ret;
}


static void
access_bus_write(uint16_t port, uint8_t val, void *priv)
{
    access_bus_t *dev = (access_bus_t *)priv;

    switch(port & 3) {
	case 0:
		dev->control = (val & 0xcf);
		break;

	case 1:
		dev->own_addr = (val & 0x7f);
		break;

	case 2:
		dev->data = val;
		break;

	case 3:
		dev->clock &= 0x80;
		dev->clock |= (val & 0x07);
		break;
    }
}


static void
access_bus_handler(fdc37c93x_t *dev)
{
    uint8_t global_enable = !!(dev->regs[0x22] & (1 << 6));
    uint8_t local_enable = !!dev->ld_regs[9][0x30];
    uint16_t ld_port;

    io_removehandler(dev->access_bus->base, 4,
		     access_bus_read,NULL,NULL,
		     access_bus_write,NULL,NULL, dev->access_bus);

    if (global_enable && local_enable) {
	dev->access_bus->base = ld_port = make_port(dev, 9);
	if ((ld_port >= 0x0100) && (ld_port <= 0x0ffc))
	        io_sethandler(dev->access_bus->base, 4,
			      access_bus_read,NULL,NULL,
			      access_bus_write,NULL,NULL, dev->access_bus);
    }
}


static void
fdc37c93x_write(uint16_t port, uint8_t val, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t valxor = 0x00;

    if (indx) {
	if ((val == 0x55) && !dev->locked) {
		if (dev->tries) {
			dev->locked = 1;
			fdc_3f1_enable(dev->fdc, 0);
			dev->tries = 0;
		} else
			dev->tries++;
	} else {
		if (dev->locked) {
			if (val == 0xaa) {
				dev->locked = 0;
				fdc_3f1_enable(dev->fdc, 1);
				return;
			}
			dev->cur_reg = val;
		} else {
			if (dev->tries)
				dev->tries = 0;
		}
	}
	return;
    } else {
	if (dev->locked) {
		if (dev->cur_reg < 48) {
			valxor = val ^ dev->regs[dev->cur_reg];
			if ((val == 0x20) || (val == 0x21))
				return;
			dev->regs[dev->cur_reg] = val;
		} else {
			valxor = val ^ dev->ld_regs[dev->regs[7]][dev->cur_reg];
			if (((dev->cur_reg & 0xF0) == 0x70) && (dev->regs[7] < 4))
				return;
			/* Block writes to some logical devices. */
			if (dev->regs[7] > 9)
				return;
			else switch (dev->regs[7]) {
				case 1:
				case 2:
				case 6:
				case 7:
					return;

				case 9:
					/*
					 * If we are on the FDC37C935, return
					 * as this is not a valid logical
					 * device there.
					 */
					if (dev->chip_id == 0x02)
						return;
					break;
			}
			dev->ld_regs[dev->regs[7]][dev->cur_reg] = val;
		}
	} else
		return;
    }

    if (dev->cur_reg < 48) {
	switch(dev->cur_reg) {
		case 0x03:
			if (valxor & 0x83)
				gpio_handler(dev);
			dev->regs[0x03] &= 0x83;
			break;

		case 0x22:
			if (valxor & 0x01)
				fdc_handler(dev);
			if (valxor & 0x08)
				lpt_handler(dev);
			if (valxor & 0x10)
				serial_handler(dev, 0);
			if (valxor & 0x20)
				serial_handler(dev, 1);
			break;
	}

	return;
    }

    switch(dev->regs[7]) {
	case 0:
		/* FDD */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
				if (valxor)
					fdc_handler(dev);
				break;

			case 0xf0:
				if (valxor & 0x01)
					fdc_update_enh_mode(dev->fdc, val & 0x01);
				if (valxor & 0x10)
					fdc_set_swap(dev->fdc, (val & 0x10) >> 4);
				break;

			case 0xf1:
				if (valxor & 0xc)
					fdc_update_densel_force(dev->fdc, (val & 0xC) >> 2);
				break;

			case 0xf2:
				if (valxor & 0xc0)
					fdc_update_rwc(dev->fdc, 3, (valxor & 0xC0) >> 6);
				if (valxor & 0x30)
					fdc_update_rwc(dev->fdc, 2, (valxor & 0x30) >> 4);
				if (valxor & 0x0c)
					fdc_update_rwc(dev->fdc, 1, (valxor & 0x0C) >> 2);
				if (valxor & 0x03)
					fdc_update_rwc(dev->fdc, 0, (valxor & 0x03));
				break;

			case 0xf4:
				if (valxor & 0x18)
					fdc_update_drvrate(dev->fdc, 0, (val & 0x18) >> 3);
				break;

			case 0xf5:
				if (valxor & 0x18)
					fdc_update_drvrate(dev->fdc, 1, (val & 0x18) >> 3);
				break;

			case 0xf6:
				if (valxor & 0x18)
					fdc_update_drvrate(dev->fdc, 2, (val & 0x18) >> 3);
				break;

			case 0xf7:
				if (valxor & 0x18)
					fdc_update_drvrate(dev->fdc, 3, (val & 0x18) >> 3);
				break;
		}
		break;

	case 3:
		/* Parallel port */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
				if (valxor)
					lpt_handler(dev);
				break;
		}
		break;

	case 4:
		/* Serial port 1 */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
			case 0x70:
				if (valxor)
					serial_handler(dev, 0);
				break;
		}
		break;

	case 5:
		/* Serial port 2 */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
			case 0x70:
				if (valxor)
					serial_handler(dev, 1);
				break;
		}
		break;

	case 8:
		/* Auxiliary I/O */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
			case 0x70:
				if (valxor)
					auxio_handler(dev);
				break;
		}
		break;

	case 9:
		/* Access bus (FDC37C932FR only) */
		switch(dev->cur_reg) {
			case 0x30:
			case 0x60:
			case 0x61:
			case 0x70:
				if (valxor)
					access_bus_handler(dev);
				break;
		}
		break;
    }
}


static uint8_t
fdc37c93x_read(uint16_t port, void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t ret = 0xff;

    if (! dev->locked)
	return ret;

    if (indx) {
	ret = dev->cur_reg;
	return ret;
    }

    if (dev->cur_reg < 0x30) {
	if (dev->cur_reg == 0x20)
		ret = dev->chip_id;
	else
		ret = dev->regs[dev->cur_reg];
    } else {
	if ((dev->regs[7] == 0) && (dev->cur_reg == 0xF2)) {
		ret = (fdc_get_rwc(dev->fdc, 0) | (fdc_get_rwc(dev->fdc, 1) << 2) |
		      (fdc_get_rwc(dev->fdc, 2) << 4) | (fdc_get_rwc(dev->fdc, 3) << 6));
	} else
		ret = dev->ld_regs[dev->regs[7]][dev->cur_reg];
    }

    return ret;
}


static void
fdc37c93x_reset(fdc37c93x_t *dev)
{
//    parallel_remove(1);

    memset(dev->regs, 0, 48);

    dev->regs[0x03] = 0x03;
    dev->regs[0x21] = 0x01;
    dev->regs[0x20] = dev->chip_id;
    dev->regs[0x22] = 0x39;
    dev->regs[0x24] = 0x04;
    dev->regs[0x26] = 0xf0;
    dev->regs[0x27] = 0x03;

    memset(dev->ld_regs, 0x00, sizeof(dev->ld_regs));

    /* Logical device 0: FDD */
    dev->ld_regs[0][0x30] = 0x01;
    dev->ld_regs[0][0x60] = 0x03;	/* 0x03f0 */
    dev->ld_regs[0][0x61] = 0xf0;
    dev->ld_regs[0][0x70] = 0x06;
    dev->ld_regs[0][0x74] = 0x02;
    dev->ld_regs[0][0xf0] = 0x0e;
    dev->ld_regs[0][0xf2] = 0xff;

    /* Logical device 1: IDE1 */
    dev->ld_regs[1][0x30] = 0x00;
    dev->ld_regs[1][0x60] = 0x01;	/* 0x01f0 */
    dev->ld_regs[1][0x61] = 0xf0;
    dev->ld_regs[1][0x62] = 0x03;
    dev->ld_regs[1][0x63] = 0xf6;
    dev->ld_regs[1][0x70] = 0x0e;
    dev->ld_regs[1][0xf0] = 0x0c;

    /* Logical device 2: IDE2 */
    dev->ld_regs[2][0x30] = 0x00;
    dev->ld_regs[2][0x60] = 0x01;	/* 0x0170 */
    dev->ld_regs[2][0x61] = 0x70;
    dev->ld_regs[2][0x62] = 0x03;
    dev->ld_regs[2][0x63] = 0x76;
    dev->ld_regs[2][0x70] = 0x0f;

    /* Logical device 3: Parallel Port */
    dev->ld_regs[3][0x30] = 0x01;
    dev->ld_regs[3][0x60] = 0x03;	/* 0x0378 */
    dev->ld_regs[3][0x61] = 0x78;
    dev->ld_regs[3][0x70] = 0x07;	/* IRQ7 */
    dev->ld_regs[3][0x74] = 0x04;
    dev->ld_regs[3][0xf0] = 0x3c;

    /* Logical device 4: Serial Port 1 */
    dev->ld_regs[4][0x30] = 0x01;
    dev->ld_regs[4][0x60] = 0x03;	/* 0x03f8 */
    dev->ld_regs[4][0x61] = 0xf8;
    dev->ld_regs[4][0x70] = 0x04;	/* IRQ4 */
    dev->ld_regs[4][0xf0] = 0x03;
    serial_setup(0, 0x03f8, dev->ld_regs[4][0x70]);

    /* Logical device 5: Serial Port 2 */
    dev->ld_regs[5][0x30] = 0x01;
    dev->ld_regs[5][0x60] = 0x02;	/* 0x02f8 */
    dev->ld_regs[5][0x61] = 0xf8;
    dev->ld_regs[5][0x70] = 0x03;	/* IRQ3 */
    dev->ld_regs[5][0x74] = 0x04;
    dev->ld_regs[5][0xf1] = 0x02;
    dev->ld_regs[5][0xf2] = 0x03;
    serial_setup(1, 0x02f8, dev->ld_regs[5][0x70]);

    /* Logical device 6: RTC */
    dev->ld_regs[6][0x63] = 0x70;	/* [0x60] ? */
    dev->ld_regs[6][0xf4] = 0x03;

    /* Logical device 7: Keyboard */
    dev->ld_regs[7][0x30] = 1;
    dev->ld_regs[7][0x61] = 0x60;	/* 0x0060 */
    dev->ld_regs[7][0x70] = 1;		/* IRQ1 */

    /* Logical device 8: Auxiliary I/O */
    io_removehandler(dev->access_bus->base, 4,
		     access_bus_read,NULL,NULL, access_bus_write,NULL,NULL, dev->access_bus);

    /* Logical device 9: ACCESS.bus */

    gpio_handler(dev);
    lpt_handler(dev);
    serial_handler(dev, 0);
    serial_handler(dev, 1);
    auxio_handler(dev);
    if (dev->chip_id == 0x03)
	access_bus_handler(dev);

    fdc_reset(dev->fdc);

    dev->locked = 0;
}


static void
access_bus_close(void *priv)
{
    access_bus_t *dev = (access_bus_t *)priv;

    free(dev);
}


static void *
access_bus_init(const device_t *info)
{
    access_bus_t *dev = (access_bus_t *)mem_alloc(sizeof(access_bus_t));
    memset(dev, 0x00, sizeof(access_bus_t));

    return dev;
}


static const device_t access_bus_device = {
    "SMC FDC37C932FR ACCESS.bus",
    0,
    0x03,
    access_bus_init, access_bus_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static void
fdc37c93x_close(void *priv)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)priv;

    free(dev);
}


static void *
fdc37c93x_init(const device_t *info)
{
    fdc37c93x_t *dev = (fdc37c93x_t *)mem_alloc(sizeof(fdc37c93x_t));
    memset(dev, 0x00, sizeof(fdc37c93x_t));
    dev->chip_id = info->local;

    dev->fdc = device_add(&fdc_at_smc_device);

    dev->gpio_regs[0] = 0xfd;
    dev->gpio_regs[1] = 0xff;

    if (dev->chip_id == 0x03)
	dev->access_bus = device_add(&access_bus_device);

    io_sethandler(0x3f0, 2,
		  fdc37c93x_read,NULL,NULL, fdc37c93x_write,NULL,NULL, dev);

    fdc37c93x_reset(dev);

    return dev;
}


const device_t fdc37c932fr_device = {
    "SMC FDC37C932FR Super I/O",
    0,
    0x03,
    fdc37c93x_init, fdc37c93x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t fdc37c935_device = {
    "SMC FDC37C935 Super I/O",
    0,
    0x02,
    fdc37c93x_init, fdc37c93x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
