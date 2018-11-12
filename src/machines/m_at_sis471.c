/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the SiS 85c471 Super I/O chip.
 *
 * Version:	@(#)m_at_sis471.c	1.0.12	2018/11/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../io.h"
#include "../device.h"
#include "../devices/system/memregs.h"
#include "../devices/ports/serial.h"
#include "../devices/ports/parallel.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "machine.h"


typedef struct {
    uint8_t	cur_reg,
		regs[39];
} sis471_t;


static void
sis_write(uint16_t port, uint8_t val, void *priv)
{
    sis471_t *dev = (sis471_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t valxor;

    if (indx) {
	if ((val >= 0x50) && (val <= 0x76))
		dev->cur_reg = val;
	return;
    }

    if ((dev->cur_reg < 0x50) || (dev->cur_reg > 0x76))
	return;
    valxor = val ^ dev->regs[dev->cur_reg - 0x50];
	/* Writes to 0x52 are blocked as otherwise, large hard disks don't read correctly. */
    if (dev->cur_reg != 0x52)
	dev->regs[dev->cur_reg - 0x50] = val;

    switch (dev->cur_reg) {
	case 0x73:
		if (valxor & 0x40) {
			ide_pri_disable();
			if (val & 0x40)
				ide_pri_enable();
		}
		if (valxor & 0x20) {
			if (val & 0x20) {
				serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
				serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);
			} else {
#if 0
				serial_remove(0);
				serial_remove(1);
#endif
			}
		}
		if (valxor & 0x10) {
//			parallel_remove(0);
			if (val & 0x10)
				parallel_setup(0, 0x0378);
		}
		break;
    }

    dev->cur_reg = 0;
}


static uint8_t
sis_read(uint16_t port, void *priv)
{
    sis471_t *dev = (sis471_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t ret = 0xff;;

    if (indx)
	ret = dev->cur_reg;
    else {
	if ((dev->cur_reg >= 0x50) && (dev->cur_reg <= 0x76)) {
		ret = dev->regs[dev->cur_reg - 0x50];
		dev->cur_reg = 0;
	}
    }

    return ret;
}


static void
sis_close(void *priv)
{
    sis471_t *dev = (sis471_t *)priv;

    free(dev);
}


static void *
sis_init(const device_t *info)
{
    int mem_size_mb, i;

    sis471_t *dev = (sis471_t *)mem_alloc(sizeof(sis471_t));
    memset(dev, 0x00, sizeof(sis471_t));

//    lpt2_remove();

    dev->cur_reg = 0;
    for (i = 0; i < 0x27; i++)
	dev->regs[i] = 0x00;

    dev->regs[9] = 0x40;

    mem_size_mb = mem_size >> 10;
    switch (mem_size_mb) {
	case 0:
	case 1:
		dev->regs[9] |= 0x00;
		break;

	case 2:
	case 3:
		dev->regs[9] |= 0x01;
		break;

	case 4:
		dev->regs[9] |= 0x02;
		break;

	case 5:
		dev->regs[9] |= 0x20;
		break;

	case 6: case 7:
		dev->regs[9] |= 0x09;
		break;

	case 8: case 9:
		dev->regs[9] |= 0x04;
		break;

	case 10:
	case 11:
		dev->regs[9] |= 0x05;
		break;

	case 12:
	case 13:
	case 14:
	case 15:
		dev->regs[9] |= 0x0b;
		break;

	case 16:
		dev->regs[9] |= 0x13;
		break;

	case 17:
		dev->regs[9] |= 0x21;
		break;

	case 18:
	case 19:
		dev->regs[9] |= 0x06;
		break;

	case 20:
	case 21:
	case 22:
	case 23:
		dev->regs[9] |= 0x0d;
		break;

	case 24:
	case 25:
	case 26:
	case 27:
	case 28:
	case 29:
	case 30:
	case 31:
		dev->regs[9] |= 0x0e;
		break;

	case 32:
	case 33:
	case 34:
	case 35:
		dev->regs[9] |= 0x1b;
		break;

	case 36:
	case 37:
	case 38:
	case 39:
		dev->regs[9] |= 0x0f;
		break;

	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
		dev->regs[9] |= 0x17;
		break;

	case 48:
		dev->regs[9] |= 0x1e;
		break;

	default:
		if (mem_size_mb < 64)
			dev->regs[9] |= 0x1e;
		else if ((mem_size_mb >= 65) && (mem_size_mb < 68))
			dev->regs[9] |= 0x22;
		else
			dev->regs[9] |= 0x24;
		break;
    }

    dev->regs[0x11] = 0x09;
    dev->regs[0x12] = 0xff;
    dev->regs[0x23] = 0xf0;
    dev->regs[0x26] = 1;

    io_sethandler(0x0022, 2,
		  sis_read,NULL,NULL, sis_write,NULL,NULL, dev);

    return dev;
}


const device_t sis_85c471_device = {
    "SiS 85c471",
    0,
    0,
    sis_init, sis_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


void
machine_at_dtk486_init(const machine_t *model, void *arg)
{
    machine_at_ide_init(model, arg);

    device_add(&fdc_at_device);

    memregs_init();

    device_add(&sis_85c471_device);
}
