/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the "LPT" style parallel ports.
 *
 * Version:	@(#)parallel.c	1.0.17 	2019/04/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog parallel_log
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "parallel.h"
#include "parallel_dev.h"


typedef struct {
    /* Standard port stuff. */
    uint16_t		base;			/* port base address */

    uint8_t		dat,			/* port data register */
			ctrl;			/* port control register */

    /* Port overloading stuff. */
    void		*func_priv;
    uint8_t		(*func_read)(uint16_t, void *);

    /* Device stuff. */
    int			dev_id;			/* attached device */
    const lpt_device_t	*dev_ts;
    void		*dev_ps;
} parallel_t;


#ifdef ENABLE_PARALLEL_LOG
int			parallel_do_log = ENABLE_PARALLEL_LOG;
#endif


static const uint16_t addr_list[] = {		/* valid port addresses */
    PARALLEL1_ADDR,
    PARALLEL2_ADDR,
    PARALLEL3_ADDR
};
static parallel_t	ports[PARALLEL_MAX];	/* the ports */


#ifdef _LOGGING
void
parallel_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_PARALLEL_LOG
    va_list ap;

    if (parallel_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


/* Write a value to a port (and/or its attached device.) */
static void
parallel_write(uint16_t port, uint8_t val, void *priv)
{
    parallel_t *dev = (parallel_t *)priv;

    DBGLOG(2, "PARALLEL: write(%04X, %02X)\n", port, val);

    switch (port & 3) {
	case 0:		/* data */
		if (dev->dev_ts != NULL)
			dev->dev_ts->write_data(val, dev->dev_ps);
		dev->dat = val;
		break;

	case 1:		/* status */
		break;

	case 2:		/* control */
		if (dev->dev_ts != NULL)
			dev->dev_ts->write_ctrl(val, dev->dev_ps);
		dev->ctrl = val;
		break;
    }
}


/* Read a value from a port (and/or its attached device.) */
static uint8_t
parallel_read(uint16_t port, void *priv)
{
    parallel_t *dev = (parallel_t *)priv;
    uint8_t ret = 0xff;

    switch (port & 3) {
	case 0:		/* data */
		if (dev->dev_ts != NULL)
			ret = dev->dev_ts->read_data(dev->dev_ps);
		  else
			ret = dev->dat;
		break;

	case 1:		/* status */
		if (dev->dev_ts != NULL)
			ret = dev->dev_ts->read_status(dev->dev_ps);
		  else
			ret = 0x00;

		if (dev->func_read != NULL)
			ret |= dev->func_read(port, dev->func_priv);
		break;

	case 2:		/* control */
		if (dev->dev_ts != NULL)
			ret = dev->dev_ts->read_ctrl(dev->dev_ps);
		  else
			ret = dev->ctrl;

		if (dev->func_read != NULL)
			ret |= dev->func_read(port, dev->func_priv);
		break;
    }

    DBGLOG(2, "PARALLEL: read(%04X) => %02X\n", port, ret);

    return(ret);
}


static void
parallel_close(void *priv)
{
    parallel_t *dev = (parallel_t *)priv;

    /* Unlink the attached device if there is one. */
    if (dev->dev_ts != NULL) {
	dev->dev_ts->close(dev->dev_ps);
	dev->dev_ts = NULL;
	dev->dev_ps = NULL;
    }

    /* Remove overloads. */
    dev->func_priv = NULL;
    dev->func_read = NULL;

    /* Remove the I/O handler. */
    io_removehandler(dev->base, 3,
		     parallel_read,NULL,NULL,
		     parallel_write,NULL,NULL, dev);

    /* Clear port. */
    dev->dat = 0x00;
    dev->ctrl = 0x04;
}


static void *
parallel_init(const device_t *info, UNUSED(void *parent))
{
    parallel_t *dev;
    int port = info->local;

    /* Get the correct device. */
    dev = &ports[port];

    /* Clear port. */
    dev->dat = 0x00;
    dev->ctrl = 0x04;

    /* Enable the I/O handler for this port. */
    io_sethandler(dev->base, 4,
		  parallel_read,NULL,NULL,
		  parallel_write,NULL,NULL, dev);

    /* If the user configured a device for this port, attach it. */
    if (parallel_device[port] != 0) {
	dev->dev_ts = parallel_device_get_device(parallel_device[port]);
	if (dev->dev_ts != NULL)
		dev->dev_ps = dev->dev_ts->init(dev->dev_ts);
    }

    INFO("PARALLEL: %s (I/O=%04X, device=%i)\n",
	info->name, dev->base, parallel_device[port]);

    return(dev);
}


const device_t parallel_1_device = {
    "LPT1",
    0, 0, NULL,
    parallel_init, parallel_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t parallel_2_device = {
    "LPT2",
    0, 1, NULL,
    parallel_init, parallel_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t parallel_3_device = {
    "LPT3",
    0, 2, NULL,
    parallel_init, parallel_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


/* (Re-)initialize all parallel ports. */
void
parallel_reset(void)
{
    parallel_t *dev;
    int i;

    DEBUG("PARALLEL: reset ([%i] [%i] [%i])\n",
	  parallel_enabled[0], parallel_enabled[1], parallel_enabled[2]);

    for (i = 0; i < PARALLEL_MAX; i++) {
	dev = &ports[i];

	memset(dev, 0x00, sizeof(parallel_t));

	dev->base = addr_list[i];
    }
}


/* Set up (the address of) one of the parallel ports. */
void
parallel_setup(int id, uint16_t port)
{
    parallel_t *dev = &ports[id];

    DEBUG("PARALLEL: setting up LPT%i as %04X [enabled=%i]\n",
			id+1, port, parallel_enabled[id]);
    if (! parallel_enabled[id]) return;

    dev->base = port;
}


void
parallel_set_func(void *arg, uint8_t (*rfunc)(uint16_t, void *), void *priv)
{
    parallel_t *dev = (parallel_t *)arg;

    dev->func_priv = priv;
    dev->func_read = rfunc;
}
