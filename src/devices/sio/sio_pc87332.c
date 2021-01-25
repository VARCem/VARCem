/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the NatSemi PC87332 Super I/O chip.
 *
 * Version:	@(#)sio_pc87332.c	1.0.0	2021/01/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		    Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018,2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
            regs[15];
	
    int     cur_reg;

    fdc_t	*fdc;
} pc87332_t;


static void
lpt1_handler(pc87332_t *dev)
{

    int temp;
    uint16_t lpt_port = 0x378;
    uint8_t lpt_irq = 5;

    temp = dev->regs[0x01] & 3;

    switch (temp) {
	case 0:
		lpt_port = 0x378;
		lpt_irq = (dev->regs[0x02] & 0x08) ? 7 : 5;
		break;
	case 1:
		lpt_port = 0x3bc;
		lpt_irq = 7;
		break;
	case 2:
		lpt_port = 0x278;
		lpt_irq = 5;
		break;
	case 3:
		lpt_port = 0x000;
		lpt_irq = 0xff;
		break;
    }

    if (lpt_port)
	    parallel_setup(0, lpt_port);

    //lpt1_irq(lpt_irq);
}


static void
serial_handler(pc87332_t *dev, int uart)
{
    int temp;

    temp = (dev->regs[1] >> (2 << uart)) & 3;

    switch (temp) {
	case 0:
		serial_setup(uart, SERIAL1_ADDR, 4);
		break;
	case 1:
		serial_setup(uart, SERIAL2_ADDR, 3);
		break;
	case 2:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(uart, 0x3e8, 4);
				break;
			case 1:
				serial_setup(uart, 0x338, 4);
				break;
			case 2:
				serial_setup(uart, 0x2e8, 4);
				break;
			case 3:
				serial_setup(uart, 0x220, 4);
				break;
		}
		break;
	case 3:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(uart, 0x2e8, 3);
				break;
			case 1:
				serial_setup(uart, 0x238, 3);
				break;
			case 2:
				serial_setup(uart, 0x2e0, 3);
				break;
			case 3:
				serial_setup(uart, 0x228, 3);
				break;
		}
		break;
    }
    
}


static void
pc87332_write(uint16_t port, uint8_t val, priv_t priv)
{
    pc87332_t *dev = (pc87332_t *)priv;
    uint8_t index, valxor;

    index = (port & 1) ? 0 : 1;

    if (index) {
    	dev->cur_reg = val & 0x1f;
    	dev->tries = 0;
	    return;
    } else {
	    if (dev->tries) {
		    valxor = val ^ dev->regs[dev->cur_reg];
		    dev->tries = 0;
	    	if ((dev->cur_reg <= 14) && (dev->cur_reg != 8))
		    	dev->regs[dev->cur_reg] = val;
		    else
			    return;
	    } else {
		    dev->tries++;
		    return;
	    }
    }

    switch(dev->cur_reg) {
	case 0:
		if (valxor & 1) {
			if ((val & 1) && !(dev->regs[2] & 1))
				lpt1_handler(dev);
		}
		if (valxor & 2) {
			if ((val & 2) && !(dev->regs[2] & 1))
				serial_handler(dev, 0);
		}
		if (valxor & 4) {
			if ((val & 4) && !(dev->regs[2] & 1))
				serial_handler(dev, 1);
		}
		if (valxor & 0x28) {
			fdc_remove(dev->fdc);
			if ((val & 8) && !(dev->regs[2] & 1))
				fdc_set_base(dev->fdc, (val & 0x20) ? 0x370 : 0x3f0);
		}
		break;
	case 1:
		if (valxor & 3) {
			if ((dev->regs[0] & 1) && !(dev->regs[2] & 1))
				lpt1_handler(dev);
		}
		if (valxor & 0xcc) {
			if ((dev->regs[0] & 2) && !(dev->regs[2] & 1))
				serial_handler(dev, 0);
		}
		if (valxor & 0xf0) {
			if ((dev->regs[0] & 4) && !(dev->regs[2] & 1))
				serial_handler(dev, 1);
		}
		break;
	case 2:
		if (valxor & 1) {
			fdc_remove(dev->fdc);

			if (!(val & 1)) {
				if (dev->regs[0] & 1)
					lpt1_handler(dev);
				if (dev->regs[0] & 2)
					serial_handler(dev, 0);
				if (dev->regs[0] & 4)
					serial_handler(dev, 1);
				if (dev->regs[0] & 8)
					fdc_set_base(dev->fdc, (dev->regs[0] & 0x20) ? 0x370 : 0x3f0);
			}
		}
		if (valxor & 8) {
			if ((dev->regs[0] & 1) && !(dev->regs[2] & 1))
				lpt1_handler(dev);
		}
		break;
    }
}


uint8_t
pc87332_read(uint16_t port, priv_t priv)
{
    pc87332_t *dev = (pc87332_t *)priv;
    uint8_t ret = 0xff, index;

    index = (port & 1) ? 0 : 1;

    dev->tries = 0;

    if (index)
	    ret = dev->cur_reg & 0x1f;
    else {
	if (dev->cur_reg == 8)
		ret = 0x10;
	else if (dev->cur_reg < 14)
		ret = dev->regs[dev->cur_reg];
    }

    return ret;
}


void
pc87332_reset(pc87332_t *dev)
{
    memset(dev->regs, 0x00, sizeof(dev->regs));

    dev->regs[0x00] = 0x0F;
    dev->regs[0x01] = 0x10;
    dev->regs[0x03] = 0x01;
    dev->regs[0x05] = 0x0d;
    dev->regs[0x08] = 0x70;

    /*
	0 = 360 rpm @ 500 kbps for 3.5"
	1 = Default, 300 rpm @ 500,300,250,1000 kbps for 3.5"
    */

    lpt1_handler(dev);
    serial_handler(dev, 0);
    serial_handler(dev, 1);

    fdc_reset(dev->fdc);
}


static void
pc87332_close(priv_t priv)
{
    pc87332_t *dev = (pc87332_t *)priv;

    free(dev);
}


static priv_t
pc87332_init(const device_t *info, UNUSED(void *parent))
{
    pc87332_t *dev = (pc87332_t *)mem_alloc(sizeof(pc87332_t));
    memset(dev, 0x00, sizeof(pc87332_t));

    dev->fdc = device_add(&fdc_at_nsc_device);

    pc87332_reset(dev);

    if (info->local == 1) {
	    io_sethandler(0x398, 0x0002,
		      pc87332_read, NULL, NULL, pc87332_write, NULL, NULL, dev);
    } else {
        io_sethandler(0x02e, 2,
		      pc87332_read, NULL, NULL, pc87332_write, NULL, NULL, dev);
    }

    return((priv_t)dev);
}


const device_t pc87332_device = {
    "National Semiconductor PC87332 Super I/O",
    0, 0, NULL,
    pc87332_init, pc87332_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t pc87332_ps1_device = {
    "National Semiconductor PC87332 Super I/O (IBM PS/1 Model 2133 EMEA 451)",
    0, 1, NULL,
    pc87332_init, pc87332_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

