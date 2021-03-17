/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Universal Serial Bus emulation.
 *
 * **NOTE**	Currently dummy UHCI and OHCI only!
 *
 * Version:	@(#)usb.c	1.0.1   2020/12/30
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020 Miran Grca.
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
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../../emu.h"
#include "../../plat.h"
#include "../../device.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../cpu/cpu.h"
#include "usb_port.h"


#ifdef ENABLE_USB_LOG
int usb_do_log = ENABLE_USB_LOG;
#endif


#ifdef _LOGGING
static void
usb_log(const char *fmt, ...)
{
# ifdef ENABLE_USB_LOG
    va_list ap;

    if (usb_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


static uint8_t
uhci_in(uint16_t port, priv_t priv)
{
    usb_t *dev = (usb_t *)priv;
    uint8_t ret, *regs = dev->uhci_io;

    ret = regs[port & 0x01f];

    DEBUG("USB: in(%04x) = %02x\n", port, ret);

    return(ret);
}


static void
uhci_out(uint16_t port, uint8_t val, priv_t priv)
{
    usb_t *dev = (usb_t *)priv;
    uint8_t *regs = dev->uhci_io;

    DEBUG("USB: out(%04x, %02x)\n", port, val);

    switch (port & 0x001f) {
	case 0x02:
		regs[0x02] &= ~(val & 0x3f);
		break;

	case 0x04:
		regs[0x04] = (val & 0x0f);
		break;

	case 0x09:
		regs[0x09] = (val & 0xf0);
		break;

	case 0x0a: case 0x0b:
		regs[addr] = val;
		break;

	case 0x0c:
		regs[0x0c] = (val & 0x7f);
		break;
    }
}


static void
uhci_outw(uint16_t port, uint16_t val, priv_t priv)
{
    usb_t *dev = (usb_t *)priv;
    uint16_t *regs = (uint16_t *)dev->uhci_io;

    DEBUG("USB: outw(%04x, %04x)\n", port, val);

    switch (port & 0x001f) {
	case 0x00:
		if ((val & 0x0001) && !(regs[0x00] & 0x0001))
			regs[0x01] &= ~0x20;
		else if (!(val & 0x0001))
			regs[0x01] |= 0x20;
		regs[0x00] = (val & 0x00ff);
		break;

	case 0x06:
		regs[0x03] = (val & 0x07ff);
		break;

	case 0x10: case 0x12:
		regs[addr >> 1] = ((regs[addr >> 1] & 0xedbb) | (val & 0x1244)) & ~(val & 0x080a);
		break;

	default:
		uhci_reg_write(addr, val & 0xff, p);
		uhci_reg_write(addr + 1, (val >> 8) & 0xff, p);
		break;
    }
}


static void
update_io_mapping(usb_t *dev, uint8_t base_l, uint8_t base_h, int enable)
{
    if (dev->uhci_enable && (dev->uhci_io_base != 0x0000))
	io_removehandler(dev->uhci_io_base, 32,
			 uhci_in,NULL,NULL, uhci_out,uhci_outw,NULL, dev);

    dev->uhci_io_base = base_l | (base_h << 8);
    dev->uhci_enable = enable;

    if (dev->uhci_enable && (dev->uhci_io_base != 0x0000))
	io_sethandler(dev->uhci_io_base, 32,
		      uhci_in,NULL,NULL, uhci_out, uhci_outw,NULL, dev);
}


static uint8_t
ohci_mmio_read(uint32_t addr, priv_t priv)
{
    usb_t *dev = (usb_t *)priv;
    uint8_t ret;

    ret = dev->ohci_mmio[addr & 0x0fff];

    DEBUG("USB: mmio_in(%04x) = %02x\n", addr & 0x0fff, ret);

    return(ret);
}


static void
ohci_mmio_write(uint32_t addr, uint8_t val, priv_t priv)
{
    usb_t *dev = (usb_t *)priv;
    uint8_t old;

    addr &= 0x0fff;
    DEBUG("USB: mmio_out(%04x, %02x)\n", addr, val);

    switch (addr) {
	case 0x04:
		if ((val & 0xc0) == 0x00) {
			/* UsbReset */
			dev->ohci_mmio[0x56] = dev->ohci_mmio[0x5a] = 0x16;
		}
		break;

    	case 0x08: /* HCCOMMANDSTATUS */
    		/* bit OwnershipChangeRequest triggers an ownership change (SMM <-> OS) */
    		if (val & 0x08) {
    			dev->ohci_mmio[0x0f] = 0x40;
// No SMM for now
#if 0				
			if ((dev->ohci_mmio[0x13] & 0xc0) == 0xc0)
				smi_line = 1;
#endif		
		}

    		/* bit HostControllerReset must be cleared for the controller to be seen as initialized */
		if (val & 0x01) {
			memset(dev->ohci_mmio, 0x00, 4096);
			dev->ohci_mmio[0x00] = 0x10;
			dev->ohci_mmio[0x01] = 0x01;
			dev->ohci_mmio[0x48] = 0x02;
	    		val &= ~0x01;
		}
    		break;

	case 0x0c:
		dev->ohci_mmio[addr] &= ~(val & 0x7f);
		return;

	case 0x0d: case 0x0e:
		return;

	case 0x0f:
		dev->ohci_mmio[addr] &= ~(val & 0x40);
		return;

	case 0x3b:
		dev->ohci_mmio[addr] = (val & 0x80);
		return;

	case 0x39: case 0x41:
		dev->ohci_mmio[addr] = (val & 0x3f);
		return;

	case 0x45:
		dev->ohci_mmio[addr] = (val & 0x0f);
		return;

	case 0x3a:
	case 0x3e: case 0x3f: case 0x42: case 0x43:
	case 0x46: case 0x47: case 0x48: case 0x4a:
		return;

	case 0x49:
		dev->ohci_mmio[addr] = (val & 0x1b);
		if (val & 0x02) {
			dev->ohci_mmio[0x55] |= 0x01;
			dev->ohci_mmio[0x59] |= 0x01;
		}
		return;

	case 0x4b:
		dev->ohci_mmio[addr] = (val & 0x03);
		return;

	case 0x4c: case 0x4e:
		dev->ohci_mmio[addr] = (val & 0x06);
		if ((addr == 0x4c) && !(val & 0x04)) {
			if (!(dev->ohci_mmio[0x58] & 0x01))
				dev->ohci_mmio[0x5a] |= 0x01;
			dev->ohci_mmio[0x58] |= 0x01;
		} if ((addr == 0x4c) && !(val & 0x02)) {
			if (!(dev->ohci_mmio[0x54] & 0x01))
				dev->ohci_mmio[0x56] |= 0x01;
			dev->ohci_mmio[0x54] |= 0x01;
		}
		return;

	case 0x4d: case 0x4f:
		return;

	case 0x50:
		if (val & 0x01) {
			if ((dev->ohci_mmio[0x49] & 0x03) == 0x00) {
				dev->ohci_mmio[0x55] &= ~0x01;
				dev->ohci_mmio[0x54] &= ~0x17;
				dev->ohci_mmio[0x56] &= ~0x17;
				dev->ohci_mmio[0x59] &= ~0x01;
				dev->ohci_mmio[0x58] &= ~0x17;
				dev->ohci_mmio[0x5a] &= ~0x17;
			} else if ((dev->ohci_mmio[0x49] & 0x03) == 0x01) {
				if (!(dev->ohci_mmio[0x4e] & 0x02)) {
					dev->ohci_mmio[0x55] &= ~0x01;
					dev->ohci_mmio[0x54] &= ~0x17;
					dev->ohci_mmio[0x56] &= ~0x17;
				}
				if (!(dev->ohci_mmio[0x4e] & 0x04)) {
					dev->ohci_mmio[0x59] &= ~0x01;
					dev->ohci_mmio[0x58] &= ~0x17;
					dev->ohci_mmio[0x5a] &= ~0x17;
				}
			}
		}
		return;

	case 0x51:
		if (val & 0x80)
			dev->ohci_mmio[addr] |= 0x80;
		return;

	case 0x52:
		dev->ohci_mmio[addr] &= ~(val & 0x02);
		if (val & 0x01) {
			if ((dev->ohci_mmio[0x49] & 0x03) == 0x00) {
				dev->ohci_mmio[0x55] |= 0x01;
				dev->ohci_mmio[0x59] |= 0x01;
			} else if ((dev->ohci_mmio[0x49] & 0x03) == 0x01) {
				if (!(dev->ohci_mmio[0x4e] & 0x02))
					dev->ohci_mmio[0x55] |= 0x01;
				if (!(dev->ohci_mmio[0x4e] & 0x04))
					dev->ohci_mmio[0x59] |= 0x01;
			}
		}
		return;

	case 0x53:
		if (val & 0x80)
			dev->ohci_mmio[0x51] &= ~0x80;
		return;

	case 0x54: case 0x58:
		old = dev->ohci_mmio[addr];

		if (val & 0x10) {
			if (old & 0x01) {
				dev->ohci_mmio[addr] |= 0x10;
				/* TODO: The clear should be on a 10 ms timer. */
				dev->ohci_mmio[addr] &= ~0x10;
				dev->ohci_mmio[addr + 2] |= 0x10;
			} else
				dev->ohci_mmio[addr + 2] |= 0x01;
		}
		if (val & 0x08)
			dev->ohci_mmio[addr] &= ~0x04;
		if (val & 0x04)
			dev->ohci_mmio[addr] |= 0x04;
		if (val & 0x02) {
			if (old & 0x01)
				dev->ohci_mmio[addr] |= 0x02;
			else
				dev->ohci_mmio[addr + 2] |= 0x01;
		}
		if (val & 0x01) {
			if (old & 0x01)
				dev->ohci_mmio[addr] &= ~0x02;
			else
				dev->ohci_mmio[addr + 2] |= 0x01;
		}

		if (!(dev->ohci_mmio[addr] & 0x04) && (old & 0x04))
			dev->ohci_mmio[addr + 2] |= 0x04;
		/* if (!(dev->ohci_mmio[addr] & 0x02))
			dev->ohci_mmio[addr + 2] |= 0x02; */
		return;

	case 0x55:
		if ((val & 0x02) && ((dev->ohci_mmio[0x49] & 0x03) == 0x00) && (dev->ohci_mmio[0x4e] & 0x02)) {
			dev->ohci_mmio[addr] &= ~0x01;
			dev->ohci_mmio[0x54] &= ~0x17;
			dev->ohci_mmio[0x56] &= ~0x17;
		} if ((val & 0x01) && ((dev->ohci_mmio[0x49] & 0x03) == 0x00) && (dev->ohci_mmio[0x4e] & 0x02)) {
			dev->ohci_mmio[addr] |= 0x01;
			dev->ohci_mmio[0x58] &= ~0x17;
			dev->ohci_mmio[0x5a] &= ~0x17;
		}
		return;

	case 0x59:
		if ((val & 0x02) && ((dev->ohci_mmio[0x49] & 0x03) == 0x00) && (dev->ohci_mmio[0x4e] & 0x04))
			dev->ohci_mmio[addr] &= ~0x01;
		if ((val & 0x01) && ((dev->ohci_mmio[0x49] & 0x03) == 0x00) && (dev->ohci_mmio[0x4e] & 0x04))
			dev->ohci_mmio[addr] |= 0x01;
		return;

	case 0x56: case 0x5a:
		dev->ohci_mmio[addr] &= ~(val & 0x1f);
		return;

	case 0x57: case 0x5b:
		return;
    }

    dev->ohci_mmio[addr] = val;
}


static void
update_mem_mapping(usb_t *dev, uint8_t base1, uint8_t base2, uint8_t base3, int enable)
{
    if (dev->ohci_enable && (dev->ohci_mem_base != 0x00000000))
	mem_map_disable(&dev->ohci_mmio_mapping);

    dev->ohci_mem_base = ((base1 << 8) | (base2 << 16) | (base3 << 24)) & 0xfffff000;
    dev->ohci_enable = enable;

    if (dev->ohci_enable && (dev->ohci_mem_base != 0x00000000))
    	mem_map_set_addr(&dev->ohci_mmio_mapping, dev->ohci_mem_base, 0x1000);
}


static void
usb_reset(priv_t priv)
{
    usb_t *dev = (usb_t *)priv;

    memset(dev->uhci_io, 0x00, 128);
    dev->uhci_io[0x0c] = 0x40;
    dev->uhci_io[0x10] = dev->uhci_io[0x12] = 0x80;

    memset(dev->ohci_mmio, 0x00, 4096);
    dev->ohci_mmio[0x00] = 0x10;
    dev->ohci_mmio[0x01] = 0x01;
    dev->ohci_mmio[0x48] = 0x02;

    io_removehandler(dev->uhci_io_base, 32,
		     uhci_in,NULL,NULL, uhci_out,uhci_outw, NULL, dev);
    dev->uhci_enable = 0;

    mem_map_disable(&dev->ohci_mmio_mapping);
    dev->ohci_enable = 0;
}


static void
usb_close(priv_t priv)
{
    usb_t *dev = (usb_t *)priv;

    free(dev);
}


static priv_t
usb_init(const device_t *info, UNUSED(void *parent))
{
    usb_t *dev;

    dev = (usb_t *)mem_malloc(sizeof(usb_t));
    memset(dev, 0x00, sizeof(usb_t));

    dev->uhci_io[0x0c] = 0x40;
    dev->uhci_io[0x10] = dev->uhci_io[0x12] = 0x80;

    dev->ohci_mmio[0x00] = 0x10;
    dev->ohci_mmio[0x01] = 0x01;
    dev->ohci_mmio[0x48] = 0x02;

    mem_map_add(&dev->ohci_mmio_mapping, 0, 0,
		ohci_mmio_read,NULL,NULL, ohci_mmio_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, dev);

    usb_reset(dev);

    return(dev);
}


const device_t usb_device = {
    "Universal Serial Bus",
    DEVICE_PCI,
    0,
    NULL,
    usb_init, usb_close, usb_reset,
    NULL, NULL, NULL, NULL,
    NULL
};
