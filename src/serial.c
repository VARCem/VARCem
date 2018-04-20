/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of 8250-style serial port.
 *
 * Version:	@(#)serial.c	1.0.3	2018/04/20
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
#include "emu.h"
#include "machine/machine.h"
#include "io.h"
#include "pic.h"
#include "mem.h"
#include "rom.h"
#include "timer.h"
#include "device.h"
#include "serial.h"


enum {
    SERIAL_INT_LSR = 1,
    SERIAL_INT_RECEIVE = 2,
    SERIAL_INT_TRANSMIT = 4,
    SERIAL_INT_MSR = 8
};


#ifdef ENABLE_SERIAL_LOG
int		serial_do_log = ENABLE_SERIAL_LOG;
#endif


static const struct {
    uint16_t	addr;
    int8_t	irq;
    int8_t	pad;
}		addr_list[] = {		/* valid port addresses */
    { SERIAL1_ADDR,	4 },
    { SERIAL2_ADDR,	3 }
};
static SERIAL	ports[SERIAL_MAX];	/* the ports */


static void
update_ints(SERIAL *dev)
{
    int stat = 0;

    dev->iir = 1;

    if ((dev->ier & 4) && (dev->int_status & SERIAL_INT_LSR)) {
	/*Line status interrupt*/
	stat = 1;
	dev->iir = 6;
    } else if ((dev->ier & 1) && (dev->int_status & SERIAL_INT_RECEIVE)) {
	/*Recieved data available*/
	stat = 1;
	dev->iir = 4;
    } else if ((dev->ier & 2) && (dev->int_status & SERIAL_INT_TRANSMIT)) {
	/*Transmit data empty*/
	stat = 1;
	dev->iir = 2;
    } else if ((dev->ier & 8) && (dev->int_status & SERIAL_INT_MSR)) {
	/*Modem status interrupt*/
	stat = 1;
	dev->iir = 0;
    }

    if (stat && ((dev->mcr & 8) || PCJR))
	picintlevel(1 << dev->irq);
      else
	picintc(1 << dev->irq);
}


static void
clear_fifo(SERIAL *dev)
{
    memset(dev->fifo, 0x00, 256);
    dev->fifo_read = dev->fifo_write = 0;
}


static void
write_fifo(SERIAL *dev, uint8_t *ptr, uint8_t len)
{
    while (len-- > 0) {
	dev->fifo[dev->fifo_write] = *ptr++;
	dev->fifo_write = (dev->fifo_write + 1) & 0xff;
	/*OVERFLOW NOT DETECTED*/
    }

    if (! (dev->lsr & 1)) {
	dev->lsr |= 1;
	dev->int_status |= SERIAL_INT_RECEIVE;
	update_ints(dev);
    }
}


static uint8_t
read_fifo(SERIAL *dev)
{
    if (dev->fifo_read != dev->fifo_write) {
	dev->dat = dev->fifo[dev->fifo_read];
	dev->fifo_read = (dev->fifo_read + 1) & 0xff;
    }

    return(dev->dat);
}


static void
receive_callback(void *priv)
{
    SERIAL *dev = (SERIAL *)priv;

    dev->delay = 0;

    if (dev->fifo_read != dev->fifo_write) {
	dev->lsr |= 1;
	dev->int_status |= SERIAL_INT_RECEIVE;
	update_ints(dev);
    }
}


static void
reset_port(SERIAL *dev)
{
    dev->iir = dev->ier = dev->lcr = 0;
    dev->fifo_read = dev->fifo_write = 0;

    dev->int_status = 0x00;
}


static void
serial_write(uint16_t addr, uint8_t val, void *priv)
{
    SERIAL *dev = (SERIAL *)priv;

    switch (addr & 7) {
	case 0:
                if (dev->lcr & 0x80) {
                        dev->dlab1 = val;
                        return;
                }
                dev->thr = val;
                dev->lsr |= 0x20;
                dev->int_status |= SERIAL_INT_TRANSMIT;
                update_ints(dev);
                if (dev->mcr & 0x10)
                        write_fifo(dev, &val, 1);
                break;

	case 1:
                if (dev->lcr & 0x80) {
                        dev->dlab2 = val;
                        return;
                }
                dev->ier = val & 0xf;
                update_ints(dev);
                break;

	case 2:
                dev->fcr = val;
                break;

	case 3:
                dev->lcr = val;
                break;

	case 4:
                if ((val & 2) && !(dev->mcr & 2)) {
                        if (dev->rts_callback)
                                dev->rts_callback(dev, dev->rts_callback_p);
                }
                dev->mcr = val;
                if (val & 0x10) {
                        uint8_t new_msr;

                        new_msr = (val & 0x0c) << 4;
                        new_msr |= (val & 0x02) ? 0x10: 0;
                        new_msr |= (val & 0x01) ? 0x20: 0;

                        if ((dev->msr ^ new_msr) & 0x10)
                                new_msr |= 0x01;
                        if ((dev->msr ^ new_msr) & 0x20)
                                new_msr |= 0x02;
                        if ((dev->msr ^ new_msr) & 0x80)
                                new_msr |= 0x08;
                        if ((dev->msr & 0x40) && !(new_msr & 0x40))
                                new_msr |= 0x04;
                        
                        dev->msr = new_msr;
                }
                break;

	case 5:
                dev->lsr = val;
                if (dev->lsr & 0x01)
                        dev->int_status |= SERIAL_INT_RECEIVE;
                if (dev->lsr & 0x1e)
                        dev->int_status |= SERIAL_INT_LSR;
                if (dev->lsr & 0x20)
                        dev->int_status |= SERIAL_INT_TRANSMIT;
                update_ints(dev);
                break;

	case 6:
                dev->msr = val;
                if (dev->msr & 0x0f)
                        dev->int_status |= SERIAL_INT_MSR;
                update_ints(dev);
                break;

	case 7:
                dev->scratch = val;
                break;
    }
}


static uint8_t
serial_read(uint16_t addr, void *priv)
{
    SERIAL *dev = (SERIAL *)priv;
    uint8_t ret = 0x00;

    switch (addr & 7) {
	case 0:
                if (dev->lcr & 0x80) {
                        ret = dev->dlab1;
                        break;
                }

                dev->lsr &= ~1;
                dev->int_status &= ~SERIAL_INT_RECEIVE;
                update_ints(dev);
                ret = read_fifo(dev);
                if (dev->fifo_read != dev->fifo_write) {
                        dev->delay = 1000LL * TIMER_USEC;
		}
                break;

	case 1:
                if (dev->lcr & 0x80)
                        ret = dev->dlab2;
                  else
                        ret = dev->ier;
                break;

	case 2: 
                ret = dev->iir;
                if ((ret & 0xe) == 2) {
                        dev->int_status &= ~SERIAL_INT_TRANSMIT;
                        update_ints(dev);
                }
                if (dev->fcr & 1)
                        ret |= 0xc0;
                break;

	case 3:
                ret = dev->lcr;
                break;

	case 4:
                ret = dev->mcr;
                break;

	case 5:
                if (dev->lsr & 0x20)
                        dev->lsr |= 0x40;
                dev->lsr |= 0x20;
                ret = dev->lsr;
                if (dev->lsr & 0x1f)
                        dev->lsr &= ~0x1e;
                dev->int_status &= ~SERIAL_INT_LSR;
                update_ints(dev);
                break;

	case 6:
                ret = dev->msr;
                dev->msr &= ~0x0f;
                dev->int_status &= ~SERIAL_INT_MSR;
                update_ints(dev);
                break;

	case 7:
                ret = dev->scratch;
                break;
    }

    return(ret);
}


static void *
serial_init(const device_t *info)
{
    SERIAL *dev;

    /* Get the correct device. */
    dev = &ports[info->local - 1];

    /* Set up callback functions. */
    dev->clear_fifo = clear_fifo;
    dev->write_fifo = write_fifo;

    /* Clear port. */
    reset_port(dev);

    /* Enable the I/O handler for this port. */
    io_sethandler(dev->base, 8,
		  serial_read,NULL,NULL, serial_write,NULL,NULL, dev);

    timer_add(receive_callback, &dev->delay, &dev->delay, dev);

    pclog("SERIAL: COM%d (I/O=%04X, IRQ=%d)\n",
		info->local, dev->base, dev->irq);

    return(dev);
}


static void
serial_close(void *priv)
{
    SERIAL *dev = (SERIAL *)priv;

    /* Remove the I/O handler. */
    io_removehandler(dev->base, 8,
		     serial_read,NULL,NULL, serial_write,NULL,NULL, dev);

    /* Clear port. */
    reset_port(dev);
}


const device_t serial_1_device = {
    "COM1:",
    0,
    1,
    serial_init, serial_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


const device_t serial_2_device = {
    "COM2:",
    0,
    2,
    serial_init, serial_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL,
};


/* (Re-)initialize all serial ports. */
void
serial_reset(void)
{
    SERIAL *dev;
    int i;

    pclog("SERIAL: reset ([%d] [%d])\n", serial_enabled[0], serial_enabled[1]);

    for (i = 0; i < SERIAL_MAX; i++) {
	dev = &ports[i];

	memset(dev, 0x00, sizeof(SERIAL));

	dev->base = addr_list[i].addr;
	dev->irq = addr_list[i].irq;

	/* Clear port. */
	reset_port(dev);
    }
}


/* Set up (the address/IRQ of) one of the serial ports. */
void
serial_setup(int id, uint16_t port, int8_t irq)
{
    SERIAL *dev = &ports[id-1];

#ifdef _DEBUG
    pclog("SERIAL: setting up COM%d as %04X [enabled=%d]\n",
			id, port, serial_enabled[id-1]);
#endif
    if (! serial_enabled[id-1]) return;

    dev->base = port;
    dev->irq = irq;
}


/* Attach another device (MOUSE) to a serial port. */
SERIAL *
serial_attach(int port, void *func, void *arg)
{
    SERIAL *dev;

    /* No can do if port not enabled. */
    if (! serial_enabled[port-1]) return(NULL);

    /* Grab the desired port block. */
    dev = &ports[port-1];

    /* Set up callback info. */
    dev->rts_callback = func;
    dev->rts_callback_p = arg;

    return(dev);
}
