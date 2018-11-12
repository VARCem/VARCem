/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Winbond W83877F Super I/O Chip.
 *
 *		Winbond W83877F Super I/O Chip
 *
 * Version:	@(#)sio_w83877f.c	1.0.9	2018/11/11
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
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "sio.h"


#define FDDA_TYPE	(dev->regs[7] & 3)
#define FDDB_TYPE	((dev->regs[7] >> 2) & 3)
#define FDDC_TYPE	((dev->regs[7] >> 4) & 3)
#define FDDD_TYPE	((dev->regs[7] >> 6) & 3)

#define FD_BOOT		(dev->regs[8] & 3)
#define SWWP		((dev->regs[8] >> 4) & 1)
#define DISFDDWR	((dev->regs[8] >> 5) & 1)

#define EN3MODE		((dev->regs[9] >> 5) & 1)

#define DRV2EN_NEG	(dev->regs[0x0b] & 1)        /* 0=drive 2 installed */
#define INVERTZ		((dev->regs[0x0b] >> 1) & 1) /* 0=inv DENSEL polarity */
#define IDENT		((dev->regs[0x0b] >> 3) & 1)

#define HEFERE		((dev->regs[0x0c] >> 5) & 1)

#define HEFRAS		(dev->regs[0x16] & 1)


typedef struct {
    int8_t	locked,
		rw_locked;

    uint8_t	tries,
		reg16_init,
		cur_reg,
		key,
		key_times;

    uint16_t	base_address;

    fdc_t	*fdc;

    uint8_t	regs[42];
} w83877f_t;


static void	w83877f_write(uint16_t port, uint8_t val, void *priv);
static uint8_t	w83877f_read(uint16_t port, void *priv);


static void
w83877f_remap(w83877f_t *dev)
{
    uint8_t hefras = HEFRAS;

    io_removehandler(0x0250, 2,
		     w83877f_read,NULL,NULL, w83877f_write,NULL,NULL, dev);
    io_removehandler(0x03f0, 2,
		     w83877f_read,NULL,NULL, w83877f_write,NULL,NULL, dev);

    dev->base_address = (hefras ? 0x03f0 : 0x0250);
    io_sethandler(dev->base_address, 2,
		  w83877f_read,NULL,NULL, w83877f_write,NULL,NULL, dev);

    dev->key_times = hefras + 1;
    dev->key = (hefras ? 0x86 : 0x88) | HEFERE;
}


static uint8_t
get_lpt_length(w83877f_t *dev)
{
    uint8_t length = 4;

    if (dev->regs[9] & 0x80) {
	if (dev->regs[0] & 0x04)
		length = 8;	/* EPP mode. */
	if (dev->regs[0] & 0x08)
		length |= 0x80;	/* ECP mode. */
    }

    return length;
}


static uint16_t
make_port(w83877f_t *dev, uint8_t reg)
{
    uint16_t p = 0;
    uint8_t l;

    switch (reg) {
	case 0x20:
		p = ((uint16_t) (dev->regs[reg] & 0xfc)) << 2;
		p &= 0xff0;
		if ((p < 0x100) || (p > 0x3f0))  p = 0x3f0;
		break;

	case 0x23:
		l = get_lpt_length(dev);
		p = ((uint16_t) (dev->regs[reg] & 0xff)) << 2;

		/* 8 ports in EPP mode, 4 in non-EPP mode. */
		if ((l & 0x0f) == 8)
			p &= 0x03f8;
		else
			p &= 0x03fc;
		if ((p < 0x0100) || (p > 0x03ff)) p = 0x0378;

		/* In ECP mode, A10 is active. */
		if (l & 0x80)
			p |= 0x0400;
		break;

	case 0x24:
		p = ((uint16_t) (dev->regs[reg] & 0xfe)) << 2;
		p &= 0x0ff8;
		if ((p < 0x0100) || (p > 0x3f8)) p = 0x03f8;
		break;

	case 0x25:
		p = ((uint16_t) (dev->regs[reg] & 0xfe)) << 2;
		p &= 0x0ff8;
		if ((p < 0x0100) || (p > 0x3f8)) p = 0x02f8;
		break;
    }

    return p;
}


static void
serial_handler(w83877f_t *dev, int uart)
{
    int reg_mask = uart ? 0x10 : 0x20;
    int reg_id = uart ? 0x24 : 0x25;
    int irq_mask = uart ? 0x0f : 0xf0;
    int irq_shift = uart ? 4 : 0;

    if ((dev->regs[4] & reg_mask) || !(dev->regs[reg_id] & 0xc0)) {
#if 0
	serial_remove(uart);
#endif
    } else
	serial_setup(uart, make_port(dev, reg_id), (dev->regs[0x28] & irq_mask) >> irq_shift);
}


static void
w83877f_write(uint16_t port, uint8_t val, void *priv)
{
    w83877f_t *dev = (w83877f_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t valxor = 0;
    uint8_t max = 0x2a;

    if (indx) {
	if ((val == dev->key) && !dev->locked) {
		if (dev->key_times == 2) {
			if (dev->tries) {
				dev->locked = 1;
				dev->tries = 0;
			} else
				dev->tries++;
		} else {
			dev->locked = 1;
			dev->tries = 0;
		}
	} else {
		if (dev->locked) {
			if (val < max)
				dev->cur_reg = val;
			if (val == 0xaa)
				dev->locked = 0;
		} else {
			if (dev->tries)
				dev->tries = 0;
		}
	}

	return;
    }

    if (dev->locked) {
	if (dev->rw_locked)
		return;
	if ((dev->cur_reg >= 0x26) && (dev->cur_reg <= 0x27))
		return;
	if (dev->cur_reg == 0x29)
		return;
	if (dev->cur_reg == 6)
		val &= 0xf3;
	valxor = val ^ dev->regs[dev->cur_reg];
	dev->regs[dev->cur_reg] = val;
    } else
	return;

    switch (dev->cur_reg) {
	case 0:
		if (valxor & 0xc0) {
#if 0
			parallel_remove(0);
#endif
			if (!(dev->regs[4] & 0x80))
				parallel_setup(0, make_port(dev, 0x23));
		}
		break;

	case 1:
		if (valxor & 0x80)
			fdc_set_swap(dev->fdc, (dev->regs[1] & 0x80) ? 1 : 0);
		break;

	case 4:
		if (valxor & 0x10)
			serial_handler(dev, 1);
		if (valxor & 0x20)
			serial_handler(dev, 0);
		if (valxor & 0x80) {
#if 0
			parallel_remove(0);
#endif
			if (!(dev->regs[4] & 0x80))
				parallel_setup(0, make_port(dev, 0x23));
		}
		break;

	case 6:
		if (valxor & 0x08) {
			fdc_remove(dev->fdc);
			if (!(dev->regs[6] & 0x08))
				fdc_set_base(dev->fdc, 0x03f0);
		}
		break;

	case 7:
		if (valxor & 0x03)
			fdc_update_rwc(dev->fdc, 0, FDDA_TYPE);
		if (valxor & 0x0c)
			fdc_update_rwc(dev->fdc, 1, FDDB_TYPE);
		if (valxor & 0x30)
			fdc_update_rwc(dev->fdc, 2, FDDC_TYPE);
		if (valxor & 0xc0)
			fdc_update_rwc(dev->fdc, 3, FDDD_TYPE);
		break;

	case 8:
		if (valxor & 0x03)
			fdc_update_boot_drive(dev->fdc, FD_BOOT);
		if (valxor & 0x10)
			fdc_set_swwp(dev->fdc, SWWP ? 1 : 0);
		if (valxor & 0x20)
			fdc_set_diswr(dev->fdc, DISFDDWR ? 1 : 0);
		break;

	case 9:
		if (valxor & 0x20)
			fdc_update_enh_mode(dev->fdc, EN3MODE ? 1 : 0);
		if (valxor & 0x40)
			dev->rw_locked = (val & 0x40) ? 1 : 0;
		if (valxor & 0x80) {
#if 0
			parallel_remove(0);
#endif
			if (!(dev->regs[4] & 0x80))
				parallel_setup(0, make_port(dev, 0x23));
		}
		break;

	case 0x0b:
		if (valxor & 1)
			fdc_update_drv2en(dev->fdc, DRV2EN_NEG ? 0 : 1);
		if (valxor & 2)
			fdc_update_densel_polarity(dev->fdc, INVERTZ ? 1 : 0);
		break;

	case 0x0c:
		if (valxor & 0x20)
			w83877f_remap(dev);
		break;

	case 0x16:
		if (valxor & 1)
			w83877f_remap(dev);
		break;

	case 0x20:
		if (valxor) {
			fdc_remove(dev->fdc);
			if (!(dev->regs[4] & 0x80))
				fdc_set_base(dev->fdc, make_port(dev, 0x20));
		}
		break;

	case 0x23:
		if (valxor) {
#if 0
			parallel_remove(0);
#endif
			if (! (dev->regs[4] & 0x80))
				parallel_setup(0, make_port(dev, 0x23));
		}
		break;

	case 0x24:
		if (valxor & 0xfe)
			serial_handler(dev, 0);
		break;

	case 0x25:
		if (valxor & 0xfe)
			serial_handler(dev, 1);
		break;

	case 0x28:
		if (valxor & 0x0f) {
			if ((dev->regs[0x28] & 0x0f) == 0)
				dev->regs[0x28] |= 0x03;
			if (!(dev->regs[2] & 0x10))
				serial_setup(1, make_port(dev, 0x25), dev->regs[0x28] & 0x0f);
		}
		if (valxor & 0xf0) {
			if ((dev->regs[0x28] & 0xf0) == 0)
				dev->regs[0x28] |= 0x40;
			if (!(dev->regs[4] & 0x20))
				serial_setup(0, make_port(dev, 0x24), (dev->regs[0x28] & 0xf0) >> 4);
		}
		break;
    }
}


static uint8_t
w83877f_read(uint16_t port, void *priv)
{
    w83877f_t *dev = (w83877f_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t ret = 0xff;

    if (dev->locked) {
	if (indx)
		ret = dev->cur_reg;
	else {
		if (dev->cur_reg == 7)
			ret = (fdc_get_rwc(dev->fdc, 0) | (fdc_get_rwc(dev->fdc, 1) << 2));
		else if ((dev->cur_reg >= 0x18) || !dev->rw_locked)
			ret = dev->regs[dev->cur_reg];
	}
    }

    return ret;
}


static void
w83877f_reset(w83877f_t *dev)
{
#if 0
    parallel_remove(1);

    parallel_remove(0);
#endif
    parallel_setup(0, 0x0378);

    fdc_reset(dev->fdc);

    memset(dev->regs, 0x00, sizeof(dev->regs));
    dev->regs[0x03] = 0x30;
    dev->regs[0x07] = 0xf5;
    dev->regs[0x09] = 0x0a;
    dev->regs[0x0a] = 0x1f;
    dev->regs[0x0c] = 0x28;
    dev->regs[0x0d] = 0xa3;
    dev->regs[0x16] = dev->reg16_init;
    dev->regs[0x1e] = 0x81;
    dev->regs[0x20] = (0x03f0 >> 2) & 0xfc;
    dev->regs[0x21] = (0x01f0 >> 2) & 0xfc;
    dev->regs[0x22] = ((0x03f6 >> 2) & 0xfc) | 1;
    dev->regs[0x23] = (0x0378 >> 2);
    dev->regs[0x24] = (0x03f8 >> 2) & 0xfe;
    dev->regs[0x25] = (0x02f8 >> 2) & 0xfe;
    dev->regs[0x26] = (2 << 4) | 4;
    dev->regs[0x27] = (6 << 4) | 7;
    dev->regs[0x28] = (4 << 4) | 3;
    dev->regs[0x29] = 0x62;

    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

    dev->base_address = 0x03f0;
    dev->key = 0x89;
    dev->key_times = 1;

    w83877f_remap(dev);

    dev->locked = 0;
    dev->rw_locked = 0;
}


static void
w83877f_close(void *priv)
{
    w83877f_t *dev = (w83877f_t *)priv;

    free(dev);
}


static void *
w83877f_init(const device_t *info)
{
    w83877f_t *dev = (w83877f_t *)mem_alloc(sizeof(w83877f_t));
    memset(dev, 0x00, sizeof(w83877f_t));
    dev->reg16_init = info->local;

    dev->fdc = device_add(&fdc_at_winbond_device);

    w83877f_reset(dev);

    return dev;
}


const device_t w83877f_device = {
    "Winbond W83877F Super I/O",
    0,
    5,
    w83877f_init, w83877f_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


const device_t w83877f_president_device = {
    "Winbond W83877F Super I/O (President)",
    0,
    4,
    w83877f_init, w83877f_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
