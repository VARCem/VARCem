/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ACC 3221 Super I/O Chip.
 *
 * Version:	@(#)sio_acc3221.c	1.0.3	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2019 Fred N. van Kempen.
 *		Copyright 2019 Miran Grca.
 *		Copyright 2019 Sarah Walker.
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
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../../plat.h"
#include "sio.h"


typedef struct {
    int		reg_idx;
    uint8_t	regs[256];

    fdc_t	*fdc;
} acc3221_t;


#define REG_BE_LPT1_DISABLE (1 << 4)

#define REG_DB_SERIAL1_DISABLE (1 << 4)
#define REG_DB_SERIAL2_DISABLE (1 << 1)

#define REG_DE_SIRQ3_SOURCE  (3 << 6)
#define REG_DE_SIRQ3_SERIAL1 (1 << 6)
#define REG_DE_SIRQ3_SERIAL2 (3 << 6)

#define REG_DE_SIRQ4_SOURCE  (3 << 4)
#define REG_DE_SIRQ4_SERIAL1 (1 << 4)
#define REG_DE_SIRQ4_SERIAL2 (3 << 4)

#define REG_FB_FDC_DISABLE (1 << 1)

#define REG_FE_IDE_DISABLE (1 << 1)


static void
lpt_handle(acc3221_t *dev)
{
//    lpt1_remove();
    if (!(dev->regs[0xbe] & REG_BE_LPT1_DISABLE))
	parallel_setup(0, dev->regs[0xbf] << 2);
}


static void
serial1_handler(acc3221_t *dev)
{
    uint16_t com_addr = 0;

//    serial_remove(dev->uart[0]);

    if (!(dev->regs[0xdb] & REG_DB_SERIAL1_DISABLE)) {
	com_addr = ((dev->regs[0xdc]) << 2) - 4;

	if ((dev->regs[0xde] & REG_DE_SIRQ3_SOURCE) == REG_DE_SIRQ3_SERIAL1) {
		serial_setup(0, com_addr, 3);
	} else if ((dev->regs[0xde] & REG_DE_SIRQ4_SOURCE) == REG_DE_SIRQ4_SERIAL1) {
		serial_setup(0, com_addr, 4);
	}
    }
}


static void
serial2_handler(acc3221_t *dev)
{
    uint16_t com_addr = 0;

//    serial_remove(dev->uart[1]);
    if (!(dev->regs[0xdb] & REG_DB_SERIAL2_DISABLE)) {
	com_addr = ((dev->regs[0xdd]) << 2) - 4;

	if ((dev->regs[0xde] & REG_DE_SIRQ3_SOURCE) == REG_DE_SIRQ3_SERIAL2) {
		serial_setup(1, com_addr, 3);
	} else if ((dev->regs[0xde] & REG_DE_SIRQ4_SOURCE) == REG_DE_SIRQ4_SERIAL2) {
		serial_setup(1, com_addr, 4);
	}
    }
}


static void 
acc3221_write(uint16_t addr, uint8_t val, priv_t priv)
{
    acc3221_t *dev = (acc3221_t *)priv; 
    uint8_t old;

    if (addr & 1) {
	old = dev->regs[dev->reg_idx];
	dev->regs[dev->reg_idx] = val;

	switch (dev->reg_idx) {
		case 0xbe:
			if ((old ^ val) & REG_BE_LPT1_DISABLE)
				lpt_handle(dev);
			break;

		case 0xbf:
			if (old != val)
				lpt_handle(dev);
			break;

		case 0xdb:
			if ((old ^ val) & REG_DB_SERIAL2_DISABLE)
				serial2_handler(dev);
			if ((old ^ val) & REG_DB_SERIAL1_DISABLE)
				serial1_handler(dev);
			break;

		case 0xdc:
			if (old != val)
				serial1_handler(dev);
			break;

		case 0xdd:
			if (old != val)
				serial2_handler(dev);
			break;

		case 0xde:
			if ((old ^ val) & (REG_DE_SIRQ3_SOURCE | REG_DE_SIRQ4_SOURCE)) {
				serial2_handler(dev);
				serial1_handler(dev);
			}
			break;

		case 0xfb:
			if ((old ^ val) & REG_FB_FDC_DISABLE) {
				fdc_remove(dev->fdc);
				if (!(dev->regs[0xfb] & REG_FB_FDC_DISABLE))
					fdc_set_base(dev->fdc, 0x03f0);
			}
			break;

		case 0xfe:
			if ((old ^ val) & REG_FE_IDE_DISABLE) {
				ide_pri_disable();
				if (!(dev->regs[0xfe] & REG_FE_IDE_DISABLE))
					ide_pri_enable();
			}
			break;
	}
    } else
	dev->reg_idx = val;
}


static uint8_t 
acc3221_read(uint16_t addr, priv_t priv)
{
    acc3221_t *dev = (acc3221_t *)priv; 
    uint8_t ret = 0xff;

    if (! (addr & 1))
	ret = dev->reg_idx;
    else if (dev->reg_idx < 0xbc)
	ret = 0xff;
    else
	ret = dev->regs[dev->reg_idx];

    return(ret);
}


static void
acc3221_reset(acc3221_t *dev)
{
//    lpt2_remove();

//    serial_remove(dev->uart[0]);
    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);

//    serial_remove(dev->uart[1]);
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

//    lpt1_remove();
    parallel_setup(0, 0x0378);

    fdc_reset(dev->fdc);
}


static void
acc3221_close(priv_t priv)
{
    acc3221_t *dev = (acc3221_t *)priv;

    free(dev);
}


static priv_t
acc3221_init(const device_t *info, UNUSED(void *parent))
{
    acc3221_t *dev;

    dev = (acc3221_t *)mem_alloc(sizeof(acc3221_t));
    memset(dev, 0x00, sizeof(acc3221_t));

    dev->fdc = device_add(&fdc_at_device);

    io_sethandler(0x00f2, 2,
		  acc3221_read,NULL,NULL, acc3221_write,NULL,NULL, dev);

    acc3221_reset(dev);

    return((priv_t)dev);
}


const device_t acc3221_device = {
    "ACC 3221 Super I/O",
    0, 0, NULL,
    acc3221_init, acc3221_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
