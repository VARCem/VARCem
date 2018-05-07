/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the NEAT AT286 chipset.
 *
 *		This is the chipset used in the AMI 286 clone model.
 *
 * Version:	@(#)m_at_neat.c	1.0.6	2018/05/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../device.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static uint8_t	neat_regs[256];
static int	neat_index;
static int	neat_emspage[4];


static void
neat_write(uint16_t port, uint8_t val, void *priv)
{
    switch (port) {
	case 0x22:
		neat_index = val;
		break;

	case 0x23:
		neat_regs[neat_index] = val;
		switch (neat_index) {
			case 0x6E: /*EMS page extension*/
				neat_emspage[3] = (neat_emspage[3] & 0x7F) | (( val       & 3) << 7);
				neat_emspage[2] = (neat_emspage[2] & 0x7F) | (((val >> 2) & 3) << 7);
				neat_emspage[1] = (neat_emspage[1] & 0x7F) | (((val >> 4) & 3) << 7);
				neat_emspage[0] = (neat_emspage[0] & 0x7F) | (((val >> 6) & 3) << 7);
			break;
		}
		break;

	case 0x0208: case 0x0209: case 0x4208: case 0x4209:
	case 0x8208: case 0x8209: case 0xC208: case 0xC209:
		neat_emspage[port >> 14] = (neat_emspage[port >> 14] & 0x180) | (val & 0x7F);		
		break;
    }
}


static uint8_t
neat_read(uint16_t port, void *priv)
{
    switch (port) {
	case 0x22:
		return(neat_index);

	case 0x23:
		return(neat_regs[neat_index]);
    }

    return(0xff);
}


#if 0	/*NOT_USED*/
static void
neat_writeems(uint32_t addr, uint8_t val)
{
    ram[(neat_emspage[(addr >> 14) & 3] << 14) + (addr & 0x3fff)] = val;
}


static uint8_t neat_readems(uint32_t addr)
{
    return(ram[(neat_emspage[(addr >> 14) & 3] << 14) + (addr & 0x3fff)]);
}
#endif


static void
neat_init(void)
{
    io_sethandler(0x0022, 2,
		  neat_read,NULL,NULL, neat_write,NULL,NULL, NULL);
    io_sethandler(0x0208, 2,
		  neat_read,NULL,NULL, neat_write,NULL,NULL, NULL);
    io_sethandler(0x4208, 2,
		  neat_read,NULL,NULL, neat_write,NULL,NULL, NULL);
    io_sethandler(0x8208, 2,
		  neat_read,NULL,NULL, neat_write,NULL,NULL, NULL);
    io_sethandler(0xc208, 2,
		  neat_read,NULL,NULL, neat_write,NULL,NULL, NULL);
}


void
machine_at_neat_init(const machine_t *model, void *arg)
{
    machine_at_init(model, arg);
    device_add(&fdc_at_device);

    neat_init();
}


void
machine_at_neat_ami_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_at_ami_device);
    device_add(&fdc_at_device);

    neat_init();
}
