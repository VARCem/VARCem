/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the memory I/O scratch registers on ports 0xE1
 *		and 0xE2, used by just about any emulated machine.
 *
 * Version:	@(#)memregs.c	1.0.4	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2019 Fred N. van Kempen.
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
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "memregs.h"


typedef struct {
    uint8_t	regs[16];

    uint8_t	reg_ffff;
} memregs_t;


static void
memregs_write(uint16_t port, uint8_t val, priv_t priv)
{
    memregs_t *dev = (memregs_t *)priv;

    if (port == 0xffff)
	dev->reg_ffff = 0;

    dev->regs[port & 0x0f] = val;
}


static uint8_t
memregs_read(uint16_t port, priv_t priv)
{
    memregs_t *dev = (memregs_t *)priv;

    if (port == 0xffff)
	return dev->reg_ffff;

    return dev->regs[port & 0xf];
}


static void
memregs_close(priv_t priv)
{
    memregs_t *dev = (memregs_t *)priv;

    free(dev);
}


static priv_t
memregs_init(const device_t *info, UNUSED(void *parent))
{
    memregs_t *dev;

    dev = (memregs_t *)mem_alloc(sizeof(memregs_t));
    memset(dev, 0xff, sizeof(memregs_t));
    dev->reg_ffff = 0;

    switch(info->local) {
	case 0:		/* default */
		io_sethandler(0x00e1, 2,
			      memregs_read,NULL,NULL,
			      memregs_write,NULL,NULL, dev);
		break;

	case 1:		/* powermate */
		io_sethandler(0x00ed, 2,
			      memregs_read,NULL,NULL,
			      memregs_write,NULL,NULL, dev);
		io_sethandler(0xffff, 1,
			      memregs_read,NULL,NULL,
			      memregs_write,NULL,NULL, dev);
		break;
    }

    return((priv_t)dev);
}


const device_t memregs_device = {
    "Memory Registers",
    0, 0, NULL,
    memregs_init, memregs_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t memregs_powermate_device = {
    "Memory Registers (PowerMate)",
    0, 1, NULL,
    memregs_init, memregs_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
