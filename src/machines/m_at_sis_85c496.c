/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SiS 85C496 chipset.
 *
 * Version:	@(#)m_at_sis_85c496.c	1.0.6	2018/10/05
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
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../io.h"
#include "../device.h"
#include "../mem.h"
#include "../devices/system/pci.h"
#include "../devices/system/memregs.h"
#include "../devices/sio/sio.h"
#include "../devices/disk/hdc.h"
#include "../devices/input/keyboard.h"
#include "machine.h"


typedef struct sis_85c496_t {
    uint8_t pci_conf[256];
} sis_85c496_t;


static void
sis_85c496_recalcmapping(sis_85c496_t *dev)
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
sis_85c496_write(int func, int addr, uint8_t val, void *priv)
{
    sis_85c496_t *dev = (sis_85c496_t *)priv;

    switch (addr) {
	case 0x44: /*Shadow configure*/
		if ((dev->pci_conf[0x44] & val) ^ 0xf0) {
			dev->pci_conf[0x44] = val;
			sis_85c496_recalcmapping(dev);
		}
		break;

	case 0x45: /*Shadow configure*/
		if ((dev->pci_conf[0x45] & val) ^ 0x01) {
			dev->pci_conf[0x45] = val;
			sis_85c496_recalcmapping(dev);
		}
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
sis_85c496_read(int func, int addr, void *priv)
{
    sis_85c496_t *dev = (sis_85c496_t *)priv;

    return dev->pci_conf[addr];
}
 

static void
sis_85c496_reset(void *priv)
{
    uint8_t val = 0;

    val = sis_85c496_read(0, 0x44, priv);	/* Read current value of 0x44. */
    sis_85c496_write(0, 0x44, val & 0xf, priv);	/* Turn off shadow BIOS but keep the lower 4 bits. */
}


static void
sis_85c496_close(void *priv)
{
    sis_85c496_t *dev = (sis_85c496_t *)priv;

    free(dev);
}


static void *
sis_85c496_init(const device_t *info)
{
    sis_85c496_t *dev = (sis_85c496_t *)mem_alloc(sizeof(sis_85c496_t));
    memset(dev, 0, sizeof(sis_85c496_t));

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

    pci_add_card(5, sis_85c496_read, sis_85c496_write, dev);

    return dev;
}


const device_t sis_85c496_device = {
    "SiS 85c496/85c497",
    DEVICE_PCI,
    0,
    sis_85c496_init, sis_85c496_close, sis_85c496_reset,
    NULL, NULL, NULL, NULL,
    NULL
};


static void
machine_at_sis_85c496_common_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);
    device_add(&keyboard_ps2_pci_device);

    device_add(&ide_pci_device);

    memregs_init();
    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x05, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x07, PCI_CARD_NORMAL, 4, 1, 2, 3);

    pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);

    device_add(&sis_85c496_device);
}


void
machine_at_r418_init(const machine_t *model, void *arg)
{
    machine_at_sis_85c496_common_init(model, arg);

    fdc37c665_init();
}
