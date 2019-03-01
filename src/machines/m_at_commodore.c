/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Commodore PC3 system.
 *
 * Version:	@(#)m_at_commodore.c	1.0.12	2019/02/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include <wchar.h>
#include "../emu.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static void
pc3_write(uint16_t port, uint8_t val, void *priv)
{
    switch (val & 0x03) {
	case 1:
		parallel_setup(0, 0x03bc);
		break;

	case 2:
       		parallel_setup(0, 0x0378);
		break;

	case 3:
		parallel_setup(0, 0x0278);
		break;
    }

    switch (val & 0x0c) {
	case 0x04:
		serial_setup(0, 0x02f8, 3);
		break;

	case 0x08:
		serial_setup(0, 0x03f8, 4);
		break;
    }
}


static void
pc3_init(void)
{
    io_sethandler(0x0230, 1,
		  NULL,NULL,NULL, pc3_write,NULL,NULL, NULL);
}


void
m_at_cmdpc_init(const machine_t *model, void *arg)
{
    m_at_ide_init(model, arg);

    mem_remap_top(384);

    device_add(&fdc_at_device);

    pc3_init();
}
