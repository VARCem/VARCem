/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SMC FDC37C669 Super I/O Chip.
 *
 * Version:	@(#)sio_fdc37c669.c	1.0.13	2019/05/17
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

    int8_t	locked,
		rw_locked;

    fdc_t	*fdc;

    uint8_t	regs[42];
} fdc37c669_t;


static uint16_t
make_port(fdc37c669_t *dev, uint8_t reg)
{
    uint16_t mask = 0;
    uint16_t p = 0;

    switch(reg) {
	case 0x20:
	case 0x21:
	case 0x22:
		mask = 0xfc;
		break;

	case 0x23:
		mask = 0xff;
		break;

	case 0x24:
	case 0x25:
		mask = 0xfe;
		break;
    }

    p = ((uint16_t) (dev->regs[reg] & mask)) << 2;
    if (reg == 0x22)
	p |= 6;

    return p;
}


static void
fdc37c669_write(uint16_t port, uint8_t val, priv_t priv)
{
    fdc37c669_t *dev = (fdc37c669_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t valxor = 0;
    uint8_t max = 42;

    if (indx) {
	if ((val == 0x55) && !dev->locked) {
		if (dev->tries) {
			dev->locked = 1;
			dev->tries = 0;
		} else
			dev->tries++;
	} else {
		if (dev->locked) {
			if (val < max) dev->cur_reg = val;
			if (val == 0xaa) dev->locked = 0;
		} else
			dev->tries = 0;
	}
	return;
    }

    if (dev->locked) {
	if ((dev->cur_reg < 0x18) && (dev->rw_locked)) return;
	if ((dev->cur_reg >= 0x26) && (dev->cur_reg <= 0x27)) return;
	if (dev->cur_reg == 0x29) return;

	valxor = val ^ dev->regs[dev->cur_reg];
	dev->regs[dev->cur_reg] = val;
    } else
	return;

    switch(dev->cur_reg) {
	case 0:
		if (valxor & 8) {
			fdc_remove(dev->fdc);
			if ((dev->regs[0] & 8) && (dev->regs[0x20] & 0xc0))
				fdc_set_base(dev->fdc, make_port(dev, 0x20));
		}
		break;

	case 1:
		if (valxor & 4) {
//FIXME			parallel_remove(0);
			if ((dev->regs[1] & 4) && (dev->regs[0x23] >= 0x40))
				parallel_setup(0, make_port(dev, 0x23));
		}
		if (valxor & 7)
			dev->rw_locked = (val & 8) ? 0 : 1;
		break;

	case 2:
		if (valxor & 8) {
#if 0
			serial_remove(0);
#endif
			if ((dev->regs[2] & 8) && (dev->regs[0x24] >= 0x40))
				serial_setup(0, make_port(dev, 0x24), (dev->regs[0x28] & 0xF0) >> 4);
		}
		if (valxor & 0x80) {
#if 0
			serial_remove(1);
#endif
			if ((dev->regs[2] & 0x80) && (dev->regs[0x25] >= 0x40))
				serial_setup(1, make_port(dev, 0x25), dev->regs[0x28] & 0x0F);
		}
		break;

	case 3:
		if (valxor & 2)
			fdc_update_enh_mode(dev->fdc, (val & 2) ? 1 : 0);
		break;

	case 5:
		if (valxor & 0x18)
			fdc_update_densel_force(dev->fdc, (val & 0x18) >> 3);
		if (valxor & 0x20)
			fdc_set_swap(dev->fdc, (val & 0x20) >> 5);
		break;

	case 0x0b:
		if (valxor & 3)
			fdc_update_rwc(dev->fdc, 0, val & 3);
		if (valxor & 0x0c)
			fdc_update_rwc(dev->fdc, 1, (val & 0xC) >> 2);
		break;

	case 0x20:
		if (valxor & 0xfc) {
			fdc_remove(dev->fdc);
			if ((dev->regs[0] & 8) && (dev->regs[0x20] & 0xc0))
				fdc_set_base(dev->fdc, make_port(dev, 0x20));
		}
		break;

	case 0x23:
		if (valxor) {
//FIXME:		parallel_remove(0);
			if ((dev->regs[1] & 4) && (dev->regs[0x23] >= 0x40)) 
				parallel_setup(0, make_port(dev, 0x23));
		}
		break;

	case 0x24:
		if (valxor & 0xfe) {
#if 0
			serial_remove(0);
#endif
			if ((dev->regs[2] & 8) && (dev->regs[0x24] >= 0x40))
				serial_setup(0, make_port(dev, 0x24), (dev->regs[0x28] & 0xF0) >> 4);
		}
		break;

	case 0x25:
		if (valxor & 0xfe) {
#if 0
			serial_remove(1);
#endif
			if ((dev->regs[2] & 0x80) && (dev->regs[0x25] >= 0x40))
				serial_setup(1, make_port(dev, 0x25), dev->regs[0x28] & 0x0F);
		}
		break;

	case 0x28:
		if (valxor & 0x0f) {
#if 0
			serial_remove(1);
#endif
			if ((dev->regs[2] & 0x80) && (dev->regs[0x25] >= 0x40))
				serial_setup(1, make_port(dev, 0x25), dev->regs[0x28] & 0x0F);
		}
		if (valxor & 0xf0) {
#if 0
			serial_remove(0);
#endif
			if ((dev->regs[2] & 8) && (dev->regs[0x24] >= 0x40))
				serial_setup(0, make_port(dev, 0x24), (dev->regs[0x28] & 0xF0) >> 4);
		}
		break;
    }
}


static uint8_t
fdc37c669_read(uint16_t port, priv_t priv)
{
    fdc37c669_t *dev = (fdc37c669_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t ret = 0xff;

    if (dev->locked) {
	    if (indx)
		ret = dev->cur_reg;
	    else if ((dev->cur_reg >= 0x18) || !dev->rw_locked)
		ret = dev->regs[dev->cur_reg];
    }

    return ret;
}


static void
fdc37c669_reset(fdc37c669_t *dev)
{
    fdc_reset(dev->fdc);

//    serial_remove(0);
    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);

//    serial_remove(1);
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

//    parallel_remove(0);
//    parallel_remove(1);
    parallel_setup(0, 0x0378);

    memset(dev->regs, 0, 42);
    dev->regs[0x00] = 0x28;
    dev->regs[0x01] = 0x9c;
    dev->regs[0x02] = 0x88;
    dev->regs[0x03] = 0x78;
    dev->regs[0x06] = 0xff;
    dev->regs[0x0d] = 0x03;
    dev->regs[0x0e] = 0x02;
    dev->regs[0x1e] = 0x80;	/* Gameport controller. */
    dev->regs[0x20] = (0x3f0 >> 2) & 0xfc;
    dev->regs[0x21] = (0x1f0 >> 2) & 0xfc;
    dev->regs[0x22] = ((0x3f6 >> 2) & 0xfc) | 1;
    dev->regs[0x23] = (0x378 >> 2);
    dev->regs[0x24] = (0x3f8 >> 2) & 0xfe;
    dev->regs[0x25] = (0x2f8 >> 2) & 0xfe;
    dev->regs[0x26] = (2 << 4) | 3;
    dev->regs[0x27] = (6 << 4) | 7;
    dev->regs[0x28] = (4 << 4) | 3;
    dev->locked = 0;
    dev->rw_locked = 0;
}


static void
fdc37c669_close(priv_t priv)
{
    fdc37c669_t *dev = (fdc37c669_t *)priv;

    free(dev);
}


static priv_t
fdc37c669_init(const device_t *info, UNUSED(void *parent))
{
    fdc37c669_t *dev = (fdc37c669_t *)mem_alloc(sizeof(fdc37c669_t));
    memset(dev, 0x00, sizeof(fdc37c669_t));

    dev->fdc = device_add(&fdc_at_smc_device);

    io_sethandler(0x03f0, 2,
		  fdc37c669_read,NULL,NULL, fdc37c669_write,NULL,NULL, dev);

    fdc37c669_reset(dev);

    return((priv_t)dev);
}


const device_t fdc37c669_device = {
    "SMC FDC37C669 Super I/O",
    0, 0, NULL,
    fdc37c669_init, fdc37c669_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
