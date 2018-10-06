/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the NatSemi PC87306 Super I/O chip.
 *
 * Version:	@(#)sio_pc87306.c	1.0.8	2018/10/05
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
#include "../../io.h"
#include "../../device.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "sio.h"


static int pc87306_curreg;
static uint8_t pc87306_regs[29];
static uint8_t pc87306_gpio[2] = {0xFF, 0xFB};
static fdc_t *pc87306_fdc;
static uint8_t tries;
static uint16_t lpt_port;

void pc87306_gpio_remove();
void pc87306_gpio_init();

void pc87306_gpio_write(uint16_t port, uint8_t val, void *priv)
{
	pc87306_gpio[port & 1] = val;
}

uint8_t uart1_int()
{
	uint8_t fer_irq, pnp1_irq;
	fer_irq = ((pc87306_regs[1] >> 2) & 1) ? 3 : 4;	/* 0 = COM1 (IRQ 4), 1 = COM2 (IRQ 3), 2 = COM3 (IRQ 4), 3 = COM4 (IRQ 3) */
	pnp1_irq = ((pc87306_regs[0x1C] >> 2) & 1) ? 4 : 3;
	return (pc87306_regs[0x1C] & 1) ? pnp1_irq : fer_irq;
}

uint8_t uart2_int()
{
	uint8_t fer_irq, pnp1_irq;
	fer_irq = ((pc87306_regs[1] >> 4) & 1) ? 3 : 4;	/* 0 = COM1 (IRQ 4), 1 = COM2 (IRQ 3), 2 = COM3 (IRQ 4), 3 = COM4 (IRQ 3) */
	pnp1_irq = ((pc87306_regs[0x1C] >> 6) & 1) ? 4 : 3;
	return (pc87306_regs[0x1C] & 1) ? pnp1_irq : fer_irq;
}

void lpt1_handler()
{
        int temp;
        uint16_t lptba;

	temp = pc87306_regs[0x01] & 3;
        lptba = ((uint16_t) pc87306_regs[0x19]) << 2;
	if (pc87306_regs[0x1B] & 0x10) {
		if (pc87306_regs[0x1B] & 0x20)
			lpt_port = 0x278;
		else
			lpt_port = 0x378;
	} else {
		switch (temp) {
			case 0:
				lpt_port = 0x378;
				break;
			case 1:
				lpt_port = lptba;
				break;
			case 2:
				lpt_port = 0x278;
				break;
			case 3:
				// pclog("PNP0 Bits 4,5 = 00, FAR Bits 1,0 = 3 - reserved\n");
				lpt_port = 0x000;
				break;
		}
	}
	if (lpt_port)
		parallel_setup(1, lpt_port);
}

void serial1_handler()
{
        int temp;
	temp = (pc87306_regs[1] >> 2) & 3;
	switch (temp)
	{
		case 0: serial_setup(1, SERIAL1_ADDR, uart1_int()); break;
		case 1: serial_setup(1, SERIAL2_ADDR, uart1_int()); break;
		case 2:
			switch ((pc87306_regs[1] >> 6) & 3)
			{
				case 0: serial_setup(1, 0x3e8, uart1_int()); break;
				case 1: serial_setup(1, 0x338, uart1_int()); break;
				case 2: serial_setup(1, 0x2e8, uart1_int()); break;
				case 3: serial_setup(1, 0x220, uart1_int()); break;
			}
			break;
		case 3:
			switch ((pc87306_regs[1] >> 6) & 3)
			{
				case 0: serial_setup(1, 0x2e8, uart1_int()); break;
				case 1: serial_setup(1, 0x238, uart1_int()); break;
				case 2: serial_setup(1, 0x2e0, uart1_int()); break;
				case 3: serial_setup(1, 0x228, uart1_int()); break;
			}
			break;
	}
}

void serial2_handler()
{
        int temp;
	temp = (pc87306_regs[1] >> 4) & 3;
	switch (temp)
	{
		case 0: serial_setup(2, SERIAL1_ADDR, uart2_int()); break;
		case 1: serial_setup(2, SERIAL2_ADDR, uart2_int()); break;
		case 2:
			switch ((pc87306_regs[1] >> 6) & 3)
			{
				case 0: serial_setup(2, 0x3e8, uart2_int()); break;
				case 1: serial_setup(2, 0x338, uart2_int()); break;
				case 2: serial_setup(2, 0x2e8, uart2_int()); break;
				case 3: serial_setup(2, 0x220, uart2_int()); break;
			}
			break;
		case 3:
			switch ((pc87306_regs[1] >> 6) & 3)
			{
				case 0: serial_setup(2, 0x2e8, uart2_int()); break;
				case 1: serial_setup(2, 0x238, uart2_int()); break;
				case 2: serial_setup(2, 0x2e0, uart2_int()); break;
				case 3: serial_setup(2, 0x228, uart2_int()); break;
			}
			break;
	}
}

void pc87306_write(uint16_t port, uint8_t val, void *priv)
{
	uint8_t index, valxor;

	index = (port & 1) ? 0 : 1;

	if (index)
	{
		pc87306_curreg = val & 0x1f;
		tries = 0;
		return;
	}
	else
	{
		if (tries)
		{
			if ((pc87306_curreg == 0) && (val == 8))
			{
				val = 0x4b;
			}
			valxor = val ^ pc87306_regs[pc87306_curreg];
			tries = 0;
			if ((pc87306_curreg == 0x19) && !(pc87306_regs[0x1B] & 0x40))
			{
				return;
			}
			if ((pc87306_curreg <= 28) && (pc87306_curreg != 8)/* && (pc87306_curreg != 0x18)*/)
			{
				if (pc87306_curreg == 0)
				{
					val &= 0x5f;
				}
				if (((pc87306_curreg == 0x0F) || (pc87306_curreg == 0x12)) && valxor)
				{
					pc87306_gpio_remove();
				}
				pc87306_regs[pc87306_curreg] = val;
				goto process_value;
			}
		}
		else
		{
			tries++;
			return;
		}
	}
	return;

process_value:
	switch(pc87306_curreg)
	{
		case 0:
			if (valxor & 1)
			{
//FIXME:				parallel_remove(1);
				if (val & 1)
				{
					lpt1_handler();
				}
			}

			if (valxor & 2)
			{
#if 0
				serial_remove(1);
#endif
				if (val & 2)
				{
					serial1_handler();
				}
			}
			if (valxor & 4)
			{
#if 0
				serial_remove(2);
#endif
				if (val & 4)
				{
					serial2_handler();
				}
			}
			if (valxor & 0x28)
			{
				fdc_remove(pc87306_fdc);
				if (val & 8)
				{
					fdc_set_base(pc87306_fdc, (val & 0x20) ? 0x370 : 0x3f0);
				}
			}
			break;
		case 1:
			if (valxor & 3)
			{
//FIXME:				parallel_remove(1);
				if (pc87306_regs[0] & 1)
				{
					lpt1_handler();
				}
			}

			if (valxor & 0xcc)
			{
				if (pc87306_regs[0] & 2)
				{
					serial1_handler();
				}
#if 0
				else
				{
					serial_remove(1);
				}
#endif
			}

			if (valxor & 0xf0)
			{
				if (pc87306_regs[0] & 4)
				{
					serial2_handler();
				}
#if 0
				else
				{
					serial_remove(2);
				}
#endif
			}
			break;
		case 2:
			if (valxor & 1)
			{
				if (val & 1)
				{
#if 0
//FIXME:					parallel_remove(1);
					serial_remove(1);
					serial_remove(2);
#endif
					fdc_remove(pc87306_fdc);
				}
				else
				{
					if (pc87306_regs[0] & 1)
					{
						lpt1_handler();
					}
					if (pc87306_regs[0] & 2)
					{
						serial1_handler();
					}
					if (pc87306_regs[0] & 4)
					{
						serial2_handler();
					}
					if (pc87306_regs[0] & 8)
					{
						fdc_set_base(pc87306_fdc, (pc87306_regs[0] & 0x20) ? 0x370 : 0x3f0);
					}
				}
			}
			break;
		case 9:
			if (valxor & 0x44)
			{
				fdc_update_enh_mode(pc87306_fdc, (val & 4) ? 1 : 0);
				fdc_update_densel_polarity(pc87306_fdc, (val & 0x40) ? 1 : 0);
			}
			break;
		case 0xF:
			if (valxor)
			{
				pc87306_gpio_init();
			}
			break;
		case 0x12:
			if (valxor & 0x30)
			{
				pc87306_gpio_init();
			}
			break;
		case 0x19:
			if (valxor)
			{
//FIXME:				parallel_remove(1);
				if (pc87306_regs[0] & 1)
				{
					lpt1_handler();
				}
			}
			break;
		case 0x1B:
			if (valxor & 0x70)
			{
//FIXME:				parallel_remove(1);
				if (!(val & 0x40))
				{
					pc87306_regs[0x19] = 0xEF;
				}
				if (pc87306_regs[0] & 1)
				{
					lpt1_handler();
				}
			}
			break;
		case 0x1C:
			if (valxor)
			{
				if (pc87306_regs[0] & 2)
				{
					serial1_handler();
				}
				if (pc87306_regs[0] & 4)
				{
					serial2_handler();
				}
			}
			break;
	}
}

uint8_t pc87306_gpio_read(uint16_t port, void *priv)
{
	return pc87306_gpio[port & 1];
}

uint8_t pc87306_read(uint16_t port, void *priv)
{
	uint8_t index;
	index = (port & 1) ? 0 : 1;

	tries = 0;

	if (index)
	{
		return pc87306_curreg & 0x1f;
	}
	else
	{
	        if (pc87306_curreg >= 28)
		{
			return 0xff;
		}
		else if (pc87306_curreg == 8)
		{
			return 0x70;
		}
		else
		{
			return pc87306_regs[pc87306_curreg];
		}
	}
}

void pc87306_gpio_remove()
{
        io_removehandler(pc87306_regs[0xF] << 2, 0x0002, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL,  NULL);
}

void pc87306_gpio_init()
{
	if ((pc87306_regs[0x12]) & 0x10)
	{
	        io_sethandler(pc87306_regs[0xF] << 2, 0x0001, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL,  NULL);
	}

	if ((pc87306_regs[0x12]) & 0x20)
	{
	        io_sethandler((pc87306_regs[0xF] << 2) + 1, 0x0001, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL,  NULL);
	}
}

void pc87306_reset(void)
{
	memset(pc87306_regs, 0, 29);

	pc87306_regs[0] = 0x0B;
	pc87306_regs[1] = 0x01;
	pc87306_regs[3] = 0x01;
	pc87306_regs[5] = 0x0D;
	pc87306_regs[8] = 0x70;
	pc87306_regs[9] = 0xC0;
	pc87306_regs[0xB] = 0x80;
	pc87306_regs[0xF] = 0x1E;
	pc87306_regs[0x12] = 0x30;
	pc87306_regs[0x19] = 0xEF;
	/*
		0 = 360 rpm @ 500 kbps for 3.5"
		1 = Default, 300 rpm @ 500,300,250,1000 kbps for 3.5"
	*/
//FIXME:	parallel_remove(1);
//FIXME:	parallel_remove(2);
	lpt1_handler();
#if 0
	serial_remove(1);
	serial_remove(2);
#endif
	serial1_handler();
	serial2_handler();
	fdc_reset(pc87306_fdc);
	pc87306_gpio_init();
}

void pc87306_init()
{
	pc87306_fdc = (fdc_t *)device_add(&fdc_at_nsc_device);

//FIXME:	parallel_remove(2);

	pc87306_reset();

        io_sethandler(0x02e, 0x0002, pc87306_read, NULL, NULL, pc87306_write, NULL, NULL,  NULL);
}
