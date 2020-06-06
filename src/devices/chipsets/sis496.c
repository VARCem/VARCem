/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SiS 85C496/497 chipset.
 *
 * Version:	@(#)sis49x.c	1.0.13	2020/01/29
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/pci.h"
#include "sis496.h"


typedef struct {
    uint8_t	cur_reg,
		regs[39],
		pci_conf[256];
} sis49x_t;


static void
sis497_write(uint16_t port, uint8_t val, priv_t priv)
{
    sis49x_t *dev = (sis49x_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;

    if (indx) {
	if ((val >= 0x50) && (val <= 0x76))
		dev->cur_reg = val;
	return;
    }

    if ((dev->cur_reg < 0x50) || (dev->cur_reg > 0x76))
	return;
    /* Writes to 0x52 are blocked as otherwise, large hard disks don't read correctly. */
    if (dev->cur_reg != 0x52)
	dev->regs[dev->cur_reg - 0x50] = val;

    dev->cur_reg = 0;
}


static uint8_t
sis497_read(uint16_t port, priv_t priv)
{
    sis49x_t *dev = (sis49x_t *)priv;
    uint8_t indx = (port & 1) ? 0 : 1;
    uint8_t ret = 0xff;

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
sis497_reset(sis49x_t *dev)
{
    int i;

    memset(dev->regs, 0x00, sizeof(dev->regs));
    dev->cur_reg = 0;

    for (i = 0; i < 0x27; i++)
	dev->regs[i] = 0x00;
    dev->regs[9] = 0x40;

    i = mem_size >> 10;
    switch (i) {
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

	case 6:
	case 7:
		dev->regs[9] |= 0x09;
		break;

	case 8:
	case 9:
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
		if (i < 64)
			dev->regs[9] |= 0x1e;
		else if ((i >= 65) && (i < 68))
			dev->regs[9] |= 0x22;
		else
			dev->regs[9] |= 0x24;
		break;
    }

    dev->regs[0x11] = 0x09;
    dev->regs[0x12] = 0xff;
    dev->regs[0x23] = 0xf0;
    dev->regs[0x26] = 0x01;

    io_removehandler(0x0022, 2,
		     sis497_read,NULL,NULL, sis497_write,NULL,NULL, dev);
    io_sethandler(0x0022, 2,
		  sis497_read,NULL,NULL, sis497_write,NULL,NULL, dev);
}


static void
recalc_mapping(sis49x_t *dev)
{
    uint32_t base;
    int c;

    for (c = 0; c < 8; c++) {
	base = 0xc0000 + (c << 15);
	if (dev->pci_conf[0x44] & (1 << c)) {
		switch (dev->pci_conf[0x45] & 3) {
			case 0:
				mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
				break;

			case 1:
				mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
				break;

			case 2:
				mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
				break;

			case 3:
				mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
				break;
		}
	} else
		mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    }

    flushmmucache();
    shadowbios = (dev->pci_conf[0x44] & 0xf0);
}


static void
sis496_write(int func, int addr, uint8_t val, priv_t priv)
{
    sis49x_t *dev = (sis49x_t *)priv;

    switch (addr) {
	case 0x42: /*Cache configure*/
		cpu_cache_ext_enabled = (val & 0x01);
		cpu_update_waitstates();
		break;
	
	case 0x44: /*Shadow configure*/
		if ((dev->pci_conf[0x44] & val) ^ 0xf0) {
			dev->pci_conf[0x44] = val;
			recalc_mapping(dev);
		}
		break;

	case 0x45: /*Shadow configure*/
		if ((dev->pci_conf[0x45] & val) ^ 0x01) {
			dev->pci_conf[0x45] = val;
			recalc_mapping(dev);
		}
		break;


	case 0x82:
		sis497_write(0x22, val, priv);
		break;

	case 0xc0:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTA, val & 0xf);
		else
			pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
		break;

	case 0xc1:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTB, val & 0xf);
		else
			pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
		break;

	case 0xc2:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTC, val & 0xf);
		else
			pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
		break;

	case 0xc3:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTD, val & 0xf);
		else
			pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
		break;
    }
  
    if ((addr >= 4 && addr < 8) || addr >= 0x40)
	dev->pci_conf[addr] = val;
}


static uint8_t
sis496_read(int func, int addr, priv_t priv)
{
    sis49x_t *dev = (sis49x_t *)priv;

    return dev->pci_conf[addr];
}
 

static void
sis496_reset(void *priv)
{
    uint8_t val;

    /* Read current value of 0x44. */
    val = sis496_read(0, 0x44, priv);

    /* Turn off shadow BIOS but keep the lower 4 bits. */
    sis496_write(0, 0x44, val & 0x0f, priv);

    sis497_reset((sis49x_t *)priv);
}


static void
sis496_close(priv_t priv)
{
    sis49x_t *dev = (sis49x_t *)priv;

    free(dev);
}


static priv_t
sis496_init(const device_t *info, UNUSED(void *parent))
{
    sis49x_t *dev = (sis49x_t *)mem_alloc(sizeof(sis49x_t));
    memset(dev, 0x00, sizeof(sis49x_t));

    dev->pci_conf[0x00] = 0x39; /*SiS*/
    dev->pci_conf[0x01] = 0x10; 
    dev->pci_conf[0x02] = 0x96; /*496/497*/
    dev->pci_conf[0x03] = 0x04; 
    dev->pci_conf[0x04] = 7;
    dev->pci_conf[0x05] = 0;

    dev->pci_conf[0x06] = 0x80;
    dev->pci_conf[0x07] = 0x02;

    dev->pci_conf[0x08] = 2; /*Device revision*/

    dev->pci_conf[0x09] = 0x00; /*Device class (PCI bridge)*/
    dev->pci_conf[0x0a] = 0x00;
    dev->pci_conf[0x0b] = 0x06;

    dev->pci_conf[0x0e] = 0x00; /*Single function device*/

    pci_add_card(5, sis496_read, sis496_write, dev);

    sis497_reset(dev);

    return((priv_t)dev);
}


const device_t sis_85c496_device = {
    "SiS 85C496/85C497",
    DEVICE_PCI,
    0,
    NULL,
    sis496_init, sis496_close, sis496_reset,
    NULL, NULL, NULL, NULL,
    NULL
};
