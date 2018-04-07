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
 * NOTE:	Have to re-do the "attach" and "detach" stuff for the
 *		 "device" that use the ports. --FvK
 *
 * Version:	@(#)parallel.c	1.0.4	2018/04/05
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
#include <wchar.h>
#include "emu.h"
#include "io.h"
#include "parallel.h"
#include "sound/snd_lpt_dac.h"
#include "sound/snd_lpt_dss.h"


static const struct {
    const char	*name;
    const char	*internal_name;
    const lpt_device_t *device;
} parallel_devices[] = {
    { "None",				"none",			NULL			},
    { "Disney Sound Source",		"dss",			&dss_device		},
    { "LPT DAC / Covox Speech Thing",	"lpt_dac",		&lpt_dac_device		},
    { "Stereo LPT DAC",			"lpt_dac_stereo",	&lpt_dac_stereo_device	},
    { "",				"",			NULL			}
};

static const uint16_t addr_list[] = {		/* valid port addresses */
    0x0378, 0x0278, 0x03bc
};


typedef struct {
    /* Standard port stuff. */
    uint16_t		base;			/* port base address */

    uint8_t		dat,			/* port data register */
			ctrl;			/* port control register */

    /* Device stuff. */
    char		device_name[16];	/* name of attached device */
    const lpt_device_t	*device_ts;
    void		*device_ps;
} parallel_t;



static parallel_t	ports[PARALLEL_MAX];	/* the ports */


/* Write a value to a port (and/or its attached device.) */
static void
parallel_write(uint16_t port, uint8_t val, void *priv)
{
    parallel_t *dev = (parallel_t *)priv;

    switch (port & 3) {
	case 0:
		if (dev->device_ts != NULL)
			dev->device_ts->write_data(val, dev->device_ps);
		dev->dat = val;
		break;

	case 2:
		if (dev->device_ts != NULL)
			dev->device_ts->write_ctrl(val, dev->device_ps);
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
	case 0:
		ret = dev->dat;
		break;

	case 1:
		if (dev->device_ts != NULL)
			ret = dev->device_ts->read_status(dev->device_ps);
		  else ret = 0x00;
		break;

	case 2:
		ret = dev->ctrl;
		break;
    }

    return(ret);
}


/* Initialize (all) the parallel ports. */
void
parallel_init(void)
{
    parallel_t *dev;
    int i;

    pclog("LPT: initializing ports...");

    for (i = 0; i < PARALLEL_MAX; i++) {
	dev = &ports[i];

	memset(dev, 0x00, sizeof(parallel_t));

	dev->base = addr_list[i];

	if (parallel_enabled[i]) {
		io_sethandler(dev->base, 3,
			      parallel_read,NULL,NULL,
			      parallel_write,NULL,NULL, dev);

		pclog(" [%i=%04x]", i, dev->base);
	}
    }

    pclog("\n");
}


/* Disable one of the parallel ports. */
void
parallel_remove(int id)
{
    parallel_t *dev = &ports[id-1];

    if (! parallel_enabled[id-1]) return;

    io_removehandler(dev->base, 3,
		     parallel_read,NULL,NULL,
		     parallel_write,NULL,NULL, dev);

    dev->base = addr_list[id-1];
}


void
parallel_remove_amstrad(void)
{
    parallel_t *dev = &ports[1];

    if (parallel_enabled[1]) {
	io_removehandler(dev->base+1, 2,
			 parallel_read,NULL,NULL,
			 parallel_write,NULL,NULL, dev);
    }
}


/* Set up (the address of) one of the parallel ports. */
void
parallel_setup(int id, uint16_t port)
{
    parallel_t *dev = &ports[id-1];

    if (! parallel_enabled[id-1]) return;

    dev->base = port;
    io_sethandler(dev->base, 3,
		  parallel_read,NULL,NULL,
		  parallel_write,NULL,NULL, dev);
}


const char *
parallel_device_get_name(int id)
{
    if (strlen((char *)parallel_devices[id].name) == 0)
	return(NULL);

    return((char *)parallel_devices[id].name);
}


char *
parallel_device_get_internal_name(int id)
{
    if (strlen((char *)parallel_devices[id].internal_name) == 0)
	return(NULL);

    return((char *)parallel_devices[id].internal_name);
}


/* Attach the configured "LPT" devices. */
void
parallel_devices_init(void)
{
    parallel_t *dev;
    int c, i;

    for (i = 0; i < PARALLEL_MAX; i++) {
	dev = &ports[i];

	c = 0;

#if 0
	/* FIXME: Gotta re-think this junk... --FvK */
	while (! strmpdev->device_name, (char *)strcmp(lpt_devices[c].internal_name, lpt_device_names[i]) && strlen((char *)lpt_devices[c].internal_name) != 0)
		c++;

	if (strlen((char *)lpt_devices[c].internal_name) == 0)
		lpt_device_ts[i] = NULL;
	  else {
		lpt_device_ts[i] = lpt_devices[c].device;
		if (lpt_device_ts[i])
			lpt_device_ps[i] = lpt_device_ts[i]->init(lpt_devices[c].device);
	}
#endif
    }
}


void
parallel_devices_close(void)
{
    parallel_t *dev;
    int i;

    for (i = 0; i < PARALLEL_MAX; i++) {
	dev = &ports[i];

	if (dev->device_ts != NULL) {
		dev->device_ts->close(dev->device_ps);
		dev->device_ts = NULL;
		dev->device_ps = NULL;
	}
    }
}
