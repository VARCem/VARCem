/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Example implementation of a PCI device.
 *
 * Version:	@(#)pci_dummy.c	1.0.3	2018/05/06
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
#include "../../emu.h
#include "../../io.h"
#include "pci.h"
#include "pci_dummy.h"


static uint8_t	pci_regs[256];
static bar_t	pci_bar[2];
static uint8_t	interrupt_on = 0x00;
static uint8_t	card = 0;


static void
dummy_interrupt(int set)
{
    if (set)
	pci_set_irq(card, pci_regs[0x3d]);
      else
	pci_clear_irq(card, pci_regs[0x3d]);
}


static uint8_t
dummy_read(uint16_t port, void *priv)
{
    uint8_t ret = 0;

    switch(port & 0x20) {
	case 0x00:
		return(0x1a);

	case 0x01:
		return(0x07);

	case 0x02:
		return(0x0b);

	case 0x03:
		return(0xab);

	case 0x04:
		return(pci_regs[0x3c]);

	case 0x05:
		return(pci_regs[0x3d]);

	case 0x06:
		ret = interrupt_on;

		if (interrupt_on) {
			pci_dummy_interrupt(0);
			interrupt_on = 0;
		}
		return(ret);

	default:
		return(0x00);
    }
}


static uint16_t
dummy_readw(uint16_t port, void *priv)
{
    return(dummy_read(port, priv));
}


static uint32_t
dummy_readl(uint16_t port, void *priv)
{
    return(dummy_read(port, priv));
}


static void
dummy_write(uint16_t port, uint8_t val, void *priv)
{
    switch(port & 0x20) {
	case 0x06:
		if (! interrupt_on) {
			interrupt_on = 1;
			pci_dummy_interrupt(1);
		}
		return;

	default:
		return;
    }
}


static void
dummy_writew(uint16_t port, uint16_t val, void *priv)
{
    dummy_write(port, val & 0xff, priv);
}


static void
dummy_writel(uint16_t port, uint32_t val, void *priv)
{
    dummy_write(port, val & 0xff, priv);
}


static void
dummy_io_remove(void)
{
    io_removehandler(pci_bar[0].addr, 32,
		     dummy_read,dummy_readw,dummy_readl, dummy_write,dummy_writew,dummy_writel, NULL);
}


static void
dummy_io_set(void)
{
    io_sethandler(pci_bar[0].addr, 32,
		  dummy_read,dummy_readw,dummy_readl, dummy_write,dummy_writew,dummy_writel, NULL);
}


static uint8_t
dummy_pci_read(int func, int addr, void *priv)
{
    pclog("AB0B:071A: PCI_Read(%d, %04x)\n", func, addr);

    switch(addr) {
	case 0x00:
		return(0x1a);

	case 0x01:
		return(0x07);
		break;

	case 0x02:
		return(0x0b);

	case 0x03:
		return(0xab);

	case 0x04:			/* PCI_COMMAND_LO */
	case 0x05:			/* PCI_COMMAND_HI */
		return(pci_regs[addr]);

	case 0x06:			/* PCI_STATUS_LO */
	case 0x07:			/* PCI_STATUS_HI */
		return(pci_regs[addr]);

	case 0x08:
	case 0x09:
		return(0x00);

	case 0x0a:
		return(pci_regs[addr]);

	case 0x0b:
		return(pci_regs[addr]);

	case 0x10:			/* PCI_BAR 7:5 */
		return((pci_bar[0].addr_regs[0] & 0xe0) | 0x01);

	case 0x11:			/* PCI_BAR 15:8 */
		return(pci_bar[0].addr_regs[1]);

	case 0x12:			/* PCI_BAR 23:16 */
		return(pci_bar[0].addr_regs[2]);

	case 0x13:			/* PCI_BAR 31:24 */
		return(pci_bar[0].addr_regs[3]);

	case 0x2c:
		return(0x1a);

	case 0x2d:
		return(0x07);

	case 0x2e:
		return(0x0b);

	case 0x2f:
		return(0xab);

	case 0x3c:			/* PCI_ILR */
		return(pci_regs[addr]);

	case 0x3d:			/* PCI_IPR */
		return(pci_regs[addr]);

	default:
		return(0x00);
    }
}


static void
dummy_pci_write(int func, int addr, uint8_t val, void *priv)
{
    uint8_t valxor;

    pclog("AB0B:071A: PCI_Write(%d, %04x, %02x)\n", func, addr, val);

    switch(addr) {
	case 0x04:			/* PCI_COMMAND_LO */
		valxor = (val & 0x03) ^ pci_regs[addr];
		if (valxor & PCI_COMMAND_IO) {
			dummy_io_remove();
			if (((pci_bar[0].addr & 0xffe0) != 0) && (val & PCI_COMMAND_IO)) {
				dummy_io_set();
			}
		}
		pci_regs[addr] = val & 0x03;
		break;

	case 0x10:			/* PCI_BAR */
		val &= 0xe0;	/* 0xe0 acc to RTL DS */
		val |= 0x01;	/* re-enable IOIN bit */
		/*FALLTHROUGH*/

	case 0x11:			/* PCI_BAR */
	case 0x12:			/* PCI_BAR */
	case 0x13:			/* PCI_BAR */
		/* Remove old I/O. */
		dummy_io_remove();

		/* Set new I/O as per PCI request. */
		pci_bar[0].addr_regs[addr & 3] = val;

		/* Then let's calculate the new I/O base. */
		pci_bar[0].addr &= 0xffe0;

		/* Log the new base. */
		pclog("AB0B:071A: PCI: new I/O base is %04X\n", pci_bar[0].addr);

		/* We're done, so get out of the here. */
		if (pci_regs[4] & PCI_COMMAND_IO) {
			if ((pci_bar[0].addr) != 0) {
				dummy_io_set();
			}
		}
		break;

	case 0x3C:			/* PCI_ILR */
		pclog("AB0B:071A: IRQ now: %i\n", val);
		pci_regs[addr] = val;
		return;
    }
}


void
pci_dummy_init(void)
{
    card = pci_add_card(PCI_ADD_NORMAL,
			dummy_pci_read, dummy_pci_write, NULL);

    pci_bar[0].addr_regs[0] = 0x01;
    pci_regs[0x04] = 0x03;

    pci_regs[0x3D] = PCI_INTD;
}
