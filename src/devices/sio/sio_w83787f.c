/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Winbond W83787F Super I/O Chip.
 *
 *		Winbond W83787F Super I/O Chip
 *
 * Version:	@(#)sio_w83787f.c	1.0.1	2021/03/24
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2021 Fred N. van Kempen.
 *		Copyright 2021 Miran Grca.
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
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../timer.h"
#include "../system/pci.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc_ide.h"
#include "../../plat.h"
#include "sio.h"



#define FDDA_TYPE	(dev->regs[7] & 3)
#define FDDB_TYPE	((dev->regs[7] >> 2) & 3)
#define FDDC_TYPE	((dev->regs[7] >> 4) & 3)
#define FDDD_TYPE	((dev->regs[7] >> 6) & 3)

#define FD_BOOT		(dev->regs[8] & 3)
#define SWWP		((dev->regs[8] >> 4) & 1)
#define DISFDDWR	((dev->regs[8] >> 5) & 1)

#define EN3MODE		((dev->regs[9] >> 5) & 1)

#define DRV2EN_NEG	(dev->regs[0x0b] & 1)        /* 0=drive 2 installed */
#define INVERTZ		((dev->regs[0x0b] >> 1) & 1) /* 0=inv DENSEL polarity */
#define IDENT		((dev->regs[0x0b] >> 3) & 1)

#define HEFERE		((dev->regs[0x0c] >> 5) & 1)

#define HAS_IDE_FUNCTIONALITY dev->ide_function

typedef struct {
    int8_t	locked,
		rw_locked;

    uint8_t tries,
        cur_reg,
        ide_function,
        key;

    uint16_t    reg16_init;

    fdc_t	*fdc;

    uint8_t	regs[42];
} w83787f_t;


static void	w83787f_write(uint16_t port, uint8_t val, priv_t priv);
static uint8_t	w83787f_read(uint16_t port, priv_t priv);


static void
w83787f_remap(w83787f_t *dev)
{

    io_removehandler(0x0250, 3,
		     w83787f_read,NULL,NULL, w83787f_write,NULL,NULL, dev);

    io_sethandler(0x0250, 3,
		  w83787f_read,NULL,NULL, w83787f_write,NULL,NULL, dev);

    dev->key = 0x88 | HEFERE;
}

/* FIXME: Implement EPP (and ECP) parallel port modes. */
#if 0
static uint8_t
get_lpt_length(w83787f_t *dev)
{
    uint8_t length = 4;

    if (dev->regs[9] & 0x80) {
	    if (dev->regs[0] & 0x04)
		    length = 8;	/* EPP mode. */
	    if (dev->regs[0] & 0x08)
		    length |= 0x80;	/* ECP mode. */
    }

    return length;
}
#endif

static void
serial_handler(w83787f_t *dev, int uart)
{
    int urs0 = !!(dev->regs[1] & (1 << uart));
    int urs1 = !!(dev->regs[1] & (4 << uart));
    int urs2 = !!(dev->regs[3] & (8 >> uart));
    int urs, irq = 4;
    uint16_t addr = 0x3f8, enable = 1;

    urs = (urs1 << 1) | urs0;

    if (urs2) {
	    addr = uart ? 0x3f8 : 0x2f8;
	    irq = uart ? 4 : 3;
    } else {
	    switch (urs) {
		    case 0:
			    addr = uart ? 0x3e8 : 0x2e8;
			    irq = uart ? 4 : 3;
			    break;
		    case 1:
			    addr = uart ? 0x2e8 : 0x3e8;
			    irq = uart ? 3 : 4;
			    break;
		    case 2:
			    addr = uart ? 0x2f8 : 0x3f8;
			    irq = uart ? 3 : 4;
			    break;
		    case 3:
		    default:
			    enable = 0;
			    break;
	    }
    }

    if (dev->regs[4] & (0x20 >> uart))
        enable = 0;
#if 0
	serial_remove(uart);
#endif
    if (enable)
	    serial_setup(uart, addr, irq);
}

static void
lpt_handler(w83787f_t *dev)
{
    int ptras = (dev->regs[1] >> 4) & 0x03;
    //int irq = 7;
    uint16_t addr = 0x378, enable = 1;

    switch (ptras) {
        case 0x00:
            addr = 0x3bc;
            //irq = 7;
            break;
        case 0x01:
            addr = 0x278;
            //irq = 5;
            break;
        case 0x02:
            addr = 0x378;
            //irq = 7;
            break;
        case 0x03:
        default:
            enable = 0;
            break;
    }

    if (dev->regs[4] & 0x80)
        enable = 0;
    
//    parallel_remove(0);
    if (enable)
		parallel_setup(0, addr);
}

static void
fdc_handler(w83787f_t *dev)
{
    fdc_remove(dev->fdc);
    if (!(dev->regs[0] & 0x20) && !(dev->regs[6] & 0x08))
        fdc_set_base(dev->fdc, (dev->regs[0] & 0x10) ? 0x03f0 : 0x0370);
}

static void
ide_handler(w83787f_t *dev)
{
    ide_pri_disable();
    if (dev->regs[0] & 0x80) {
        ide_set_base(0, (dev->regs[0] & 0x40) ? 0x1f0 : 0x170);
        ide_set_side(0, (dev->regs[0] & 0x40) ? 0x3f6 : 0x376);
        ide_pri_enable();
    }
}

static void
w83787f_write(uint16_t port, uint8_t val, priv_t priv)
{
    w83787f_t *dev = (w83787f_t *)priv;
    uint8_t valxor = 0;
    uint8_t max = 0x15;

    if (port == 0x250) {
        if (val == dev->key)
            dev->locked = 1;
        else
            dev->locked = 0;
        return;
    } else if (port == 0x251) {
        if (val <= max)
            dev->cur_reg = val;
        return;
    } else {
        if (dev->locked) {
            if (dev->rw_locked)
                return;
            if (dev->cur_reg == 6)
                val &= 0xf3;
            valxor = val ^ dev->regs[dev->cur_reg];
            dev->regs[dev->cur_reg] = val;
        } else
            return;
    }

    switch (dev->cur_reg) {
	case 0:
        if ((valxor & 0xc0) && (HAS_IDE_FUNCTIONALITY))
            ide_handler(dev);
        if (valxor & 0x30)
            fdc_handler(dev);
        if (valxor & 0x0c)
            lpt_handler(dev);
        break;

    case 1:
		if (valxor & 0x80)
			fdc_set_swap(dev->fdc, (dev->regs[1] & 0x80) ? 1 : 0);
        if (valxor & 0x30)
            lpt_handler(dev);
        if (valxor & 0x0a)
            serial_handler(dev, 1);
        if (valxor & 0x05)
            serial_handler(dev, 0);
        break;

    case 3:
        if (valxor & 0x80)
            lpt_handler(dev);
        if (valxor & 0x08)
            serial_handler(dev, 0);
        if (valxor & 0x04)
            serial_handler(dev, 1);
        break;

    case 4:
        if (valxor & 0x10)
			serial_handler(dev, 1);
		if (valxor & 0x20)
			serial_handler(dev, 0);
		if (valxor & 0x80)
            lpt_handler(dev);
            break;

    case 6:
        if (valxor & 0x08)
            fdc_handler(dev);
        break;

    case 7:
        if (valxor & 0x03)
            fdc_update_rwc(dev->fdc, 0, FDDA_TYPE);
        if (valxor & 0x0c)
            fdc_update_rwc(dev->fdc, 1, FDDB_TYPE);
        if (valxor & 0x30)
            fdc_update_rwc(dev->fdc, 2, FDDC_TYPE);
        if (valxor & 0xc0)
            fdc_update_rwc(dev->fdc, 3, FDDD_TYPE);
        break;

    case 8:
        if (valxor & 0x03)
            fdc_update_boot_drive(dev->fdc, FD_BOOT);
        if (valxor & 0x10)
            fdc_set_swwp(dev->fdc, SWWP ? 1 : 0);
        if (valxor & 0x20)
            fdc_set_diswr(dev->fdc, DISFDDWR ? 1 : 0);
        break;

    case 9:
        if (valxor & 0x20)
            fdc_update_enh_mode(dev->fdc, EN3MODE ? 1 : 0);
        if (valxor & 0x40)
            dev->rw_locked = (val & 0x40) ? 1 : 0;
        if (valxor & 0x80)
            lpt_handler(dev);
        break;

	case 0x0c:
		if (valxor & 0x20)
			w83787f_remap(dev);
		break;
    }
}


static uint8_t
w83787f_read(uint16_t port, priv_t priv)
{
    w83787f_t *dev = (w83787f_t *)priv;
    uint8_t ret = 0xff;

    if (dev->locked) {
	    if (port == 0x251)
		    ret = dev->cur_reg;
	    else if (port == 0x252) {
            if (dev->cur_reg == 7)
                ret = (fdc_get_rwc(dev->fdc, 0) | (fdc_get_rwc(dev->fdc, 1) << 2));
            else if (!dev->rw_locked)
                ret = dev->regs[dev->cur_reg];
        }
    } 

    return ret;
}


static void
w83787f_reset(w83787f_t *dev)
{
#if 0
    parallel_remove(1);

    parallel_remove(0);
#endif
    parallel_setup(0, 0x0378);

    if(HAS_IDE_FUNCTIONALITY) {
        ide_pri_disable();
        ide_set_base(0, 0x1f0);
        ide_set_side(0, 0x3f6);
        ide_pri_enable();
    }

    fdc_reset(dev->fdc);

    memset(dev->regs, 0x00, sizeof(dev->regs));
    dev->regs[0x00] = 0x50;
    dev->regs[0x01] = 0x2C;
    dev->regs[0x03] = 0x30;
    dev->regs[0x07] = 0xf5;
    dev->regs[0x09] = dev->reg16_init & 0xff;
    dev->regs[0x0a] = 0x1f;
    dev->regs[0x0c] = 0x2c;
    dev->regs[0x0d] = 0xa3;

    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

    dev->key = 0x89;

    w83787f_remap(dev);

    dev->locked = 0;
    dev->rw_locked = 0;
}


static void
w83787f_close(priv_t priv)
{
    w83787f_t *dev = (w83787f_t *)priv;

    free(dev);
}


static priv_t
w83787f_init(const device_t *info, UNUSED(void *parent))
{
    w83787f_t *dev = (w83787f_t *)mem_alloc(sizeof(w83787f_t));
    memset(dev, 0x00, sizeof(w83787f_t));
    dev->reg16_init = info->local;

    dev->fdc = device_add(&fdc_at_winbond_device);

    w83787f_reset(dev);

    return((priv_t)dev);
}


const device_t w83787f_device = {
    "Winbond W83787F Super I/O",
    0,
    0x09,
    NULL,
    w83787f_init, w83787f_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
