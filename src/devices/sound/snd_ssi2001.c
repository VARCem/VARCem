/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SSI2001 sound device.
 *
 * Version:	@(#)snd_ssi2001.c	1.0.7	2018/10/16
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
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "sound.h"
#include "snd_resid.h"


typedef struct {
    uint16_t	base;
    int16_t	game;

    void	*psid;

    int		pos;
    int16_t	buffer[SOUNDBUFLEN * 2];
} ssi2001_t;


static void
ssi_update(ssi2001_t *dev)
{
    if (dev->pos >= sound_pos_global) return;

    sid_fillbuf(&dev->buffer[dev->pos], sound_pos_global-dev->pos, dev->psid);

    dev->pos = sound_pos_global;
}


static void
get_buffer(int32_t *buffer, int len, void *priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;
    int c;

    ssi_update(dev);

    for (c = 0; c < len * 2; c++)
	buffer[c] += dev->buffer[c >> 1] / 2;

    dev->pos = 0;
}


static uint8_t
ssi_read(uint16_t addr, void *priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;
	
    ssi_update(dev);
	
    return(sid_read(addr, priv));
}


static void
ssi_write(uint16_t addr, uint8_t val, void *priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;
	
    ssi_update(dev);	

    sid_write(addr, val, priv);
}


static void *
ssi_init(const device_t *info)
{
    ssi2001_t *dev;

    dev = (ssi2001_t *)mem_alloc(sizeof(ssi2001_t));
    memset(dev, 0x00, sizeof(ssi2001_t));

    /* Get the device configuration. We ignore the game port for now. */
    dev->base = device_get_config_hex16("base");
    dev->game = !!device_get_config_int("game_port");

    /* Initialize the 6581 SID. */
    dev->psid = sid_init();
    sid_reset(dev->psid);

    /* Set up our I/O handler. */
    io_sethandler(dev->base, 32,
		  ssi_read,NULL,NULL, ssi_write,NULL,NULL, dev);

    sound_add_handler(get_buffer, dev);

    return(dev);
}


static void
ssi_close(void *priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;

    /* Remove our I/O handler. */
    io_removehandler(dev->base, 32,
		     ssi_read,NULL,NULL, ssi_write,NULL,NULL, dev);

    /* Close the SID. */
    sid_close(dev->psid);

    free(dev);
}


static const device_config_t ssi2001_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x280,
	{
		{
			"0x280", 0x280
		},
		{
			"0x2A0", 0x2a0
		},
		{
			"0x2C0", 0x2c0
		},
		{
			"0x2E0", 0x2e0
		},
		{
			""
		}
	}
    },
    {
	"game_port", "Game Port", CONFIG_SELECTION, "", 0,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled", 1
		},
		{
			""
		}
	}
    },
    {
	"", "", -1
    }
};

const device_t ssi2001_device = {
    "Innovation SSI-2001",
    DEVICE_ISA,
    0,
    ssi_init, ssi_close, NULL,
    NULL, NULL, NULL, NULL,
    ssi2001_config
};
