/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Super I/O chip detection code.
 *
 * Version:	@(#)sio_detect.c	1.0.3	2018/05/06
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
#include <wchar.h>
#include "../../emu.h"
#include "../../device.h"
#include "../../io.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "sio.h"


static uint8_t detect_regs[2];


static void superio_detect_write(uint16_t port, uint8_t val, void *priv)
{
        pclog(LOG_INFO, "superio_detect_write : port=%04x = %02X\n", port, val);

	detect_regs[port & 1] = val;

	return;
}


static uint8_t superio_detect_read(uint16_t port, void *priv)
{
        pclog(LOG_INFO, "superio_detect_read : port=%04x = %02X\n", port, detect_regs[port & 1]);

	return detect_regs[port & 1];
}


void superio_detect_init(void)
{
	device_add(&fdc_at_smc_device);

        io_sethandler(0x24, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x26, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x2e, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x44, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x46, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x4e, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x108, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x250, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x370, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
        io_sethandler(0x3f0, 0x0002, superio_detect_read, NULL, NULL, superio_detect_write, NULL, NULL,  NULL);
}
