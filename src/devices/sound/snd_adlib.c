/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ADLIB sound device.
 *
 * Version:	@(#)snd_adlib.c	1.0.14	2021/03/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/mca.h"
#include "sound.h"
#include "snd_opl.h"


typedef struct {
    opl_t	opl;

    uint8_t	pos_regs[8];
} adlib_t;


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    adlib_t *dev = (adlib_t *)priv;
    int c;

    opl2_update(&dev->opl);

    for (c = 0; c < len * 2; c++)
	buffer[c] += (int32_t)dev->opl.buffer[c];

    dev->opl.pos = 0;
}


static uint8_t
adlib_mca_read(int port, priv_t priv)
{
    adlib_t *dev = (adlib_t *)priv;

    DBGLOG(1, "adlib_mca_read: port=%04x\n", port);

    return dev->pos_regs[port & 7];
}


static void
adlib_mca_write(int port, uint8_t val, priv_t priv)
{
    adlib_t *dev = (adlib_t *)priv;

    if (port < 0x102)
	return;

    DBGLOG(1, "adlib_mca_write: port=%04x val=%02x\n", port, val);

    switch (port) {
	case 0x102:
		if ((dev->pos_regs[2] & 1) && !(val & 1))
			io_removehandler(0x0388, 0x0002,
					 opl2_read, NULL, NULL,
					 opl2_write, NULL, NULL, &dev->opl);
		if (! (dev->pos_regs[2] & 1) && (val & 1))
			io_sethandler(0x0388, 0x0002,
				      opl2_read, NULL, NULL,
				      opl2_write, NULL, NULL, &dev->opl);
		break;
    }

    dev->pos_regs[port & 7] = val;
}


static uint8_t
adlib_mca_feedb(priv_t priv)
{
    adlib_t *dev = (adlib_t *)priv;

    return (dev->pos_regs[2] & 0x01);
}


static priv_t
adlib_init(const device_t *info, UNUSED(void *parent))
{
    adlib_t *dev;

    dev = (adlib_t *)mem_alloc(sizeof(adlib_t));
    memset(dev, 0x00, sizeof(adlib_t));

    switch(info->local) {
	case 0:		/* ISA */
		io_sethandler(0x0388, 2,
			      opl2_read,NULL,NULL,
			      opl2_write,NULL,NULL, &dev->opl);
		break;

	case 1:		/* MCA */
		dev->pos_regs[0] = 0xd7;
		dev->pos_regs[1] = 0x70;
		mca_add(adlib_mca_read, adlib_mca_write, adlib_mca_feedb, NULL, (priv_t)dev);
		break;
    }

    opl2_init(&dev->opl);

    sound_add_handler(get_buffer, (priv_t)dev);

    return((priv_t)dev);
}


static void
adlib_close(priv_t priv)
{
    adlib_t *dev = (adlib_t *)priv;

    free(dev);
}


const device_t adlib_device = {
    "AdLib",
    DEVICE_ISA,
    0,
    NULL,
    adlib_init, adlib_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t adlib_mca_device = {
    "AdLib",
    DEVICE_MCA,
    1,
    NULL,
    adlib_init, adlib_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
