/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of Intel System I/O PCI chip.
 *
 * Version:	@(#)intel_sio.c	1.0.4	2018/10/05
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../device.h"
#include "dma.h"
#include "pci.h"
#include "intel_sio.h"


typedef struct {
    uint8_t regs[256];
} sio_t;


static void
sio_write(int func, int addr, uint8_t val, void *priv)
{
    sio_t *dev = (sio_t *) priv;

    if (func > 0)
	return;

    if (addr >= 0x0f && addr < 0x4c)
	return;

    switch (addr) {
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x0e:
		return;

	case 0x04: /*Command register*/
		val &= 0x08;
		val |= 0x07;
		break;
	case 0x05:
		val = 0;
		break;

	case 0x06: /*Status*/
		val = 0;
		break;
	case 0x07:
		val = 0x02;
		break;

	case 0x40:
		if (!((val ^ dev->regs[addr]) & 0x40))
			return;

		if (val & 0x40)
			dma_alias_remove();
		else
			dma_alias_set();
		break;

	case 0x4f:
		if (!((val ^ dev->regs[addr]) & 0x40))
			return;

		if (val & 0x40)
			port_92_add();
		else
			port_92_remove();

	case 0x60:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
		else
			pci_set_irq_routing(PCI_INTA, val & 0xf);
		break;
	case 0x61:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
		else
			pci_set_irq_routing(PCI_INTC, val & 0xf);
		break;
	case 0x62:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
		else
			pci_set_irq_routing(PCI_INTB, val & 0xf);
		break;
	case 0x63:
		if (val & 0x80)
			pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
		else
			pci_set_irq_routing(PCI_INTD, val & 0xf);
		break;
    }
    dev->regs[addr] = val;
}


static uint8_t
sio_read(int func, int addr, void *priv)
{
    sio_t *dev = (sio_t *) priv;
    uint8_t ret;

    ret = 0xff;

    if (func == 0)
        ret = dev->regs[addr];

    return ret;
}


static void
sio_reset(void *priv)
{
    sio_t *dev = (sio_t *) priv;

    memset(dev->regs, 0, 256);

    dev->regs[0x00] = 0x86; dev->regs[0x01] = 0x80; /*Intel*/
    dev->regs[0x02] = 0x84; dev->regs[0x03] = 0x04; /*82378IB (SIO)*/
    dev->regs[0x04] = 0x07; dev->regs[0x05] = 0x00;
    dev->regs[0x06] = 0x00; dev->regs[0x07] = 0x02;
    dev->regs[0x08] = 0x03; /*A0 stepping*/

    dev->regs[0x40] = 0x20; dev->regs[0x41] = 0x00;
    dev->regs[0x42] = 0x04; dev->regs[0x43] = 0x00;
    dev->regs[0x44] = 0x20; dev->regs[0x45] = 0x10;
    dev->regs[0x46] = 0x0f; dev->regs[0x47] = 0x00;
    dev->regs[0x48] = 0x01; dev->regs[0x49] = 0x10;
    dev->regs[0x4a] = 0x10; dev->regs[0x4b] = 0x0f;
    dev->regs[0x4c] = 0x56; dev->regs[0x4d] = 0x40;
    dev->regs[0x4e] = 0x07; dev->regs[0x4f] = 0x4f;
    dev->regs[0x54] = 0x00; dev->regs[0x55] = 0x00; dev->regs[0x56] = 0x00;
    dev->regs[0x60] = 0x80; dev->regs[0x61] = 0x80; dev->regs[0x62] = 0x80; dev->regs[0x63] = 0x80;
    dev->regs[0x80] = 0x78; dev->regs[0x81] = 0x00;
    dev->regs[0xa0] = 0x08;
    dev->regs[0xa8] = 0x0f;

    pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
    pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
}


static void
sio_close(void *p)
{
    sio_t *sio = (sio_t *)p;

    free(sio);
}


static void *
sio_init(const device_t *info)
{
    sio_t *sio = (sio_t *)mem_alloc(sizeof(sio_t));
    memset(sio, 0, sizeof(sio_t));

    pci_add_card(2, sio_read, sio_write, sio);
        
    sio_reset(sio);

    port_92_reset();

    port_92_add();

    dma_alias_set();

    return sio;
}


const device_t sio_device = {
    "Intel 82378IB (SIO)",
    DEVICE_PCI,
    0,
    sio_init, sio_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
