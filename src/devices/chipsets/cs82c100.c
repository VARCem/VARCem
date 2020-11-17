/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the C&T 82c100 chipset.
 *
 * Version:	@(#)cs82c100.c	1.0.0	2020/11/16
 *
 * Authors:	Altheos,
 * 		Davide78
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2020 Altheos.
 *		Copyright 2020 Davide78.
 *		Copyright 2020 Sarah Walker.
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
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "cs82c100.h"


typedef struct {
    int		indx;
    uint8_t	regs[256];
    int     emspage[4];
} cs82c100_t;


static void 
cs82c100_write(uint16_t addr, uint8_t val, priv_t priv)
{
    cs82c100_t *dev = (cs82c100_t *)priv;

  
    switch (addr) {
        case 0x22:
            dev->indx = val;
            break;

        case 0x23:
            dev->regs[dev->indx] = val;
            break;

        case 0x0208: case 0x4208:
        case 0x8208: case 0xc208:
            dev->emspage[addr >> 14] = val;
            break;
    }
}


static uint8_t 
cs82c100_read(uint16_t addr, priv_t priv)
{
    cs82c100_t *dev = (cs82c100_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
        case 0x22:
            ret = dev->indx;

        case 0x23:
            ret = dev->regs[dev->indx];

        case 0x0208: case 0x4208:
        case 0x8208: case 0xc208:
            ret = dev->emspage[addr >> 14];
    }

    return ret;
}


static void
cs82c100_close(priv_t priv)
{
    cs82c100_t *dev = (cs82c100_t *)priv;

    free(dev);
}


static priv_t
cs82c100_init(const device_t *info, UNUSED(void *parent))
{
    cs82c100_t *dev;

    dev = (cs82c100_t *)mem_alloc(sizeof(cs82c100_t));
    memset(dev, 0x00, sizeof(cs82c100_t));
	
    /* Set register 0x42 to invalid configuration at startup */
    dev->regs[0x42] = 0;

    io_sethandler(0x0022, 0x0002, cs82c100_read, NULL, NULL, cs82c100_write, NULL, NULL,  dev);
    io_sethandler(0x0208, 0x0001, cs82c100_read, NULL, NULL, cs82c100_write, NULL, NULL,  dev);
    io_sethandler(0x4208, 0x0001, cs82c100_read, NULL, NULL, cs82c100_write, NULL, NULL,  dev);
    io_sethandler(0x8208, 0x0001, cs82c100_read, NULL, NULL, cs82c100_write, NULL, NULL,  dev);
    io_sethandler(0xc208, 0x0001, cs82c100_read, NULL, NULL, cs82c100_write, NULL, NULL,  dev);

    return((priv_t)dev);
}
const device_t cs82c100_device = {
    "C&T 82c100",
    0, 0, NULL,
    cs82c100_init, cs82c100_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
