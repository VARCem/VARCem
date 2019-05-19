/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the NatSemi PC87306 Super I/O chip.
 *
 * Version:	@(#)sio_pc87306.c	1.0.13	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
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
#include "../../plat.h"
#include "sio.h"


typedef struct {
    uint8_t	tries,
		cur_reg;

    fdc_t	*fdc;

    uint8_t	regs[29],
		gpio[2];
} pc87306_t;


static void
gpio_write(uint16_t port, uint8_t val, priv_t priv)
{
    pc87306_t *dev = (pc87306_t *)priv;

    dev->gpio[port & 1] = val;
}


static uint8_t
gpio_read(uint16_t port, priv_t priv)
{
    pc87306_t *dev = (pc87306_t *)priv;

    return dev->gpio[port & 1];
}


static void
gpio_remove(pc87306_t *dev)
{
    io_removehandler(dev->regs[0x0f] << 2, 1,
		     gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);
    io_removehandler((dev->regs[0x0f] << 2) + 1, 1,
		     gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);
}


static void
gpio_init(pc87306_t *dev)
{
    if ((dev->regs[0x12]) & 0x10)
        io_sethandler(dev->regs[0x0f] << 2, 1,
		      gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);

    if ((dev->regs[0x12]) & 0x20)
        io_sethandler((dev->regs[0x0f] << 2) + 1, 1,
		      gpio_read,NULL,NULL, gpio_write,NULL,NULL, dev);
}


static void
lpt1_handler(pc87306_t *dev)
{
    uint16_t lptba, lpt_port = 0x0378;
    int temp;

    temp = dev->regs[0x01] & 3;
    lptba = ((uint16_t) dev->regs[0x19]) << 2;

    if (dev->regs[0x1b] & 0x10) {
	if (dev->regs[0x1b] & 0x20)
		lpt_port = 0x0278;
	else
		lpt_port = 0x0378;
    } else {
	switch (temp) {
		case 0:
			lpt_port = 0x0378;
			break;

		case 1:
			lpt_port = lptba;
			break;

		case 2:
			lpt_port = 0x0278;
			break;

		case 3:
			lpt_port = 0x0000;
			break;
	}
    }

    if (lpt_port)
	parallel_setup(0, lpt_port);
}


static void
serial_handler(pc87306_t *dev, int uart)
{
    uint8_t fer_irq, pnp1_irq;
    uint8_t fer_shift, pnp_shift;
    uint8_t irq;
    int temp;

    temp = (dev->regs[1] >> (2 << uart)) & 3;

    fer_shift = 2 << uart;		/* 2 for UART1, 4 for UART2 */
    pnp_shift = 2 + (uart << 2);	/* 2 for UART1, 6 for UART2 */

    /* 0 = COM1 (IRQ 4), 1 = COM2 (IRQ 3), 2 = COM3 (IRQ 4), 3 = COM4 (IRQ 3) */
    fer_irq = ((dev->regs[1] >> fer_shift) & 1) ? 3 : 4;
    pnp1_irq = ((dev->regs[0x1c] >> pnp_shift) & 1) ? 4 : 3;

    irq = (dev->regs[0x1c] & 1) ? pnp1_irq : fer_irq;

    switch (temp) {
	case 0:
		serial_setup(uart, SERIAL1_ADDR, irq);
		break;

	case 1:
		serial_setup(uart, SERIAL2_ADDR, irq);
		break;

	case 2:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(uart, 0x03e8, irq);
				break;

			case 1:
				serial_setup(uart, 0x0338, irq);
				break;

			case 2:
				serial_setup(uart, 0x02e8, irq);
				break;

			case 3:
				serial_setup(uart, 0x0220, irq);
				break;
		}
		break;

	case 3:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(uart, 0x02e8, irq);
				break;

			case 1:
				serial_setup(uart, 0x0238, irq);
				break;

			case 2:
				serial_setup(uart, 0x02e0, irq);
				break;

			case 3:
				serial_setup(uart, 0x0228, irq);
				break;
		}
		break;
    }
}


static void
pc87306_write(uint16_t port, uint8_t val, priv_t priv)
{
    pc87306_t *dev = (pc87306_t *)priv;
    uint8_t indx, valxor;

    indx = (port & 1) ? 0 : 1;

    if (indx) {
	dev->cur_reg = val & 0x1f;
	dev->tries = 0;
	return;
    }

    if (dev->tries) {
	if ((dev->cur_reg == 0) && (val == 8))
		val = 0x4b;
	valxor = val ^ dev->regs[dev->cur_reg];
	dev->tries = 0;
	if ((dev->cur_reg == 0x19) && !(dev->regs[0x1b] & 0x40))
		return;
	if ((dev->cur_reg <= 28) && (dev->cur_reg != 8)) {
		if (dev->cur_reg == 0)
			val &= 0x5f;
		if (((dev->cur_reg == 0x0f) || (dev->cur_reg == 0x12)) && valxor)
			gpio_remove(dev);
		dev->regs[dev->cur_reg] = val;
	} else
		return;
    } else {
	dev->tries++;
	return;
    }

    switch(dev->cur_reg) {
	case 0:
		if (valxor & 0x01) {
//			lpt1_remove();
			if (val & 0x01)
				lpt1_handler(dev);
		}
		if (valxor & 0x02) {
//			serial_remove(0);
			if (val & 0x02)
				serial_handler(dev, 0);
		}
		if (valxor & 0x04) {
//			serial_remove(1);
			if (val & 0x04)
				serial_handler(dev, 1);
		}
		if (valxor & 0x28) {
			fdc_remove(dev->fdc);
			if (val & 0x08)
				fdc_set_base(dev->fdc, (val & 0x20) ? 0x0370 : 0x03f0);
		}
		break;

	case 1:
		if (valxor & 0x03) {
#if 0
			parallel_remove(0);
#endif
			if (dev->regs[0] & 1)
				lpt1_handler(dev);
		}
		if (valxor & 0xcc) {
			if (dev->regs[0] & 2)
				serial_handler(dev, 0);
#if 0
			else
				serial_remove(0);
#endif
		}
		if (valxor & 0xf0) {
			if (dev->regs[0] & 4)
				serial_handler(dev, 1);
#if 0
			else
				serial_remove(1);
#endif
		}
		break;

	case 2:
		if (valxor & 1) {
#if 0
			parallel_remove(0);
			serial_remove(0);
			serial_remove(1);
#endif
			fdc_remove(dev->fdc);

			if (! (val & 0x01)) {
				if (dev->regs[0] & 0x01)
					lpt1_handler(dev);
				if (dev->regs[0] & 0x02)
					serial_handler(dev, 0);
				if (dev->regs[0] & 0x04)
					serial_handler(dev, 1);
				if (dev->regs[0] & 0x08)
					fdc_set_base(dev->fdc, (dev->regs[0] & 0x20) ? 0x0370 : 0x03f0);
			}
		}
		break;

	case 9:
		if (valxor & 0x44) {
			fdc_update_enh_mode(dev->fdc, (val & 4) ? 1 : 0);
			fdc_update_densel_polarity(dev->fdc, (val & 0x40) ? 1 : 0);
		}
		break;

	case 0x0f:
		if (valxor)
			gpio_init(dev);
		break;

	case 0x12:
		if (valxor & 0x30)
			gpio_init(dev);
		break;

	case 0x19:
		if (valxor) {
//			parallel_remove(0);
			if (dev->regs[0] & 1)
				lpt1_handler(dev);
		}
		break;

	case 0x1b:
		if (valxor & 0x70) {
//			parallel_remove(0);
			if (! (val & 0x40))
				dev->regs[0x19] = 0xef;
			if (dev->regs[0] & 0x01)
				lpt1_handler(dev);
		}
		break;

	case 0x1c:
		if (valxor) {
			if (dev->regs[0] & 0x02)
				serial_handler(dev, 0);
			if (dev->regs[0] & 0x04)
				serial_handler(dev, 1);
		}
		break;
    }
}


uint8_t
pc87306_read(uint16_t port, priv_t priv)
{
    pc87306_t *dev = (pc87306_t *)priv;
    uint8_t ret = 0xff, indx;

    indx = (port & 1) ? 0 : 1;

    dev->tries = 0;

    if (indx)
	ret = dev->cur_reg & 0x1f;
    else {
	if (dev->cur_reg == 8)
		ret = 0x70;
	else if (dev->cur_reg < 28)
		ret = dev->regs[dev->cur_reg];
    }

    return ret;
}


void
pc87306_reset(pc87306_t *dev)
{
    memset(dev->regs, 0x00, sizeof(dev->regs));

    dev->regs[0x00] = 0x0B;
    dev->regs[0x01] = 0x01;
    dev->regs[0x03] = 0x01;
    dev->regs[0x05] = 0x0d;
    dev->regs[0x08] = 0x70;
    dev->regs[0x09] = 0xc0;
    dev->regs[0x0b] = 0x80;
    dev->regs[0x0f] = 0x1e;
    dev->regs[0x12] = 0x30;
    dev->regs[0x19] = 0xef;

    dev->gpio[0] = 0xff;
    dev->gpio[1] = 0xfb;

    /*
	0 = 360 rpm @ 500 kbps for 3.5"
	1 = Default, 300 rpm @ 500,300,250,1000 kbps for 3.5"
    */

#if 0
    parallel_remove(0);
    parallel_remove(1);
#endif
    lpt1_handler(dev);
#if 0
    serial_remove(0);
    serial_remove(1);
#endif
    serial_handler(dev, 0);
    serial_handler(dev, 1);

    fdc_reset(dev->fdc);

    gpio_init(dev);
}


static void
pc87306_close(priv_t priv)
{
    pc87306_t *dev = (pc87306_t *)priv;

    free(dev);
}


static priv_t
pc87306_init(const device_t *info, UNUSED(void *parent))
{
    pc87306_t *dev = (pc87306_t *)mem_alloc(sizeof(pc87306_t));
    memset(dev, 0x00, sizeof(pc87306_t));

    dev->fdc = device_add(&fdc_at_nsc_device);

    pc87306_reset(dev);

    io_sethandler(0x02e, 2,
		  pc87306_read,NULL,NULL, pc87306_write,NULL,NULL, dev);

    return((priv_t)dev);
}


const device_t pc87306_device = {
    "National Semiconductor PC87306 Super I/O",
    0, 0, NULL,
    pc87306_init, pc87306_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
