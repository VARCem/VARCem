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
 * Version:	@(#)snd_ssi2001.c	1.0.11	2020/07/15
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <resid/c_interface.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "../../ui/ui.h"
#include "sound.h"


#ifdef _WIN32
# define PATH_RESID_DLL		"libresid.dll"
#else
# define PATH_RESID_DLL		"libresid.so"
#endif


static void		*sid_handle = NULL;	/* handle to DLL */
#if USE_RESID == 1
# define FUNC(x)	sid_ ## x
#else
# define FUNC(x)	SID_ ## x


/* Pointers to the real functions. */
static char		*const (*SID_version)(void);
static void		*(*SID_init)(void);
static void		(*SID_close)(void *priv);
static void		(*SID_reset)(void *priv);
static uint8_t		(*SID_read)(uint16_t addr, void *priv);
static void		(*SID_write)(uint16_t addr, uint8_t val, void *priv);
static void		(*SID_fillbuf)(int16_t *buf, int len, void *priv);


static const dllimp_t resid_imports[] = {
  { "sid_version",	&SID_version		},
  { "sid_init",		&SID_init		},
  { "sid_close",	&SID_close		},
  { "sid_reset",	&SID_reset		},
  { "sid_read",		&SID_read		},
  { "sid_write",	&SID_write		},
  { "sid_fillbuf",	&SID_fillbuf		},
  { NULL					}
};
#endif


typedef struct {
    uint16_t	base;
    int16_t	game;

    priv_t	psid;

    int		pos;
    int16_t	buffer[SOUNDBUFLEN * 2];
} ssi2001_t;


static void
ssi_update(ssi2001_t *dev)
{
    if (dev->pos >= sound_pos_global) return;

    FUNC(fillbuf)(&dev->buffer[dev->pos],
		  sound_pos_global - dev->pos, dev->psid);

    dev->pos = sound_pos_global;
}


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;
    int c;

    ssi_update(dev);

    for (c = 0; c < len * 2; c++)
	buffer[c] += dev->buffer[c >> 1] / 2;

    dev->pos = 0;
}


static uint8_t
ssi_read(uint16_t addr, priv_t priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;

    ssi_update(dev);

    return(FUNC(read)(addr, priv));
}


static void
ssi_write(uint16_t addr, uint8_t val, priv_t priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;

    ssi_update(dev);

    FUNC(write)(addr, val, priv);
}


static void
ssi_close(priv_t priv)
{
    ssi2001_t *dev = (ssi2001_t *)priv;

    /* Remove our I/O handler. */
    io_removehandler(dev->base, 32,
		     ssi_read,NULL,NULL, ssi_write,NULL,NULL, (priv_t)dev);

    /* Close the SID. */
    FUNC(close)(dev->psid);

    free(dev);
}


static priv_t
ssi_init(const device_t *info, UNUSED(void *parent))
{
    ssi2001_t *dev;

    /* If we do not have the reSID library, no can do. */
    if (sid_handle == NULL) {
	ERRLOG("SSI2001: reSID module not available, cannot initialize!");
	return(NULL);
    }

    dev = (ssi2001_t *)mem_alloc(sizeof(ssi2001_t));
    memset(dev, 0x00, sizeof(ssi2001_t));

    /* Get the device configuration. We ignore the game port for now. */
    dev->base = device_get_config_hex16("base");
    dev->game = !!device_get_config_int("game_port");

    /* Initialize the 6581 SID. */
    dev->psid = FUNC(init)();
    FUNC(reset)(dev->psid);

    /* Set up our I/O handler. */
    io_sethandler(dev->base, 32,
		  ssi_read,NULL,NULL, ssi_write,NULL,NULL, (priv_t)dev);

    sound_add_handler(get_buffer, dev);

    return(dev);
}


static const device_config_t ssi2001_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x280,
	{
		{
			"280H", 0x280
		},
		{
			"2A0H", 0x2a0
		},
		{
			"2C0H", 0x2c0
		},
		{
			"2E0H", 0x2e0
		},
		{
			NULL
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
			NULL
		}
	}
    },
    {
	NULL
    }
};


const device_t ssi2001_device = {
    "Innovation SSI-2001",
    DEVICE_ISA,
    0,
    NULL,
    ssi_init, ssi_close, NULL,
    NULL, NULL, NULL, NULL,
    ssi2001_config
};


/* Prepare for use, load DLL if needed. */
void
resid_init(void)
{
#if USE_RESID == 2
    wchar_t temp[512];
    const char *fn = PATH_RESID_DLL;
#endif

    /* If already loaded, good! */
    if (sid_handle != NULL)
	return;

#if USE_RESID == 2
    /* Try loading the DLL. */
    sid_handle = dynld_module(fn, resid_imports);
    if (sid_handle == NULL) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOLIB), "reSID", fn);
	ui_msgbox(MBX_ERROR, temp);
	ERRLOG("SOUND: unable to load '%s'; module disabled!\n", fn);
	return;
    } else
	INFO("SOUND: module '%s' loaded, version %s.\n",
		fn, FUNC(version)());
#else
    sid_handle = (void *)1;	/* just to indicate always therse */
#endif
}
