/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Windows Sound System sound device.
 *
 * Version:	@(#)snd_wss.c	1.0.11	2020/01/31
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2018 TheCollector1995.
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
#include <math.h>  
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/mca.h"
#include "../system/dma.h"
#include "../system/pic.h"
#include "sound.h"
#include "snd_ad1848.h"
#include "snd_opl.h"


/*Win95 only uses IRQ7-9, others may be wrong*/
static const int	wss_dmas[4] = { 0, 0, 1, 3 };
static const int	wss_irqs[8] = { 5, 7, 9, 10, 11, 12, 14, 15 };


typedef struct {
    const char	*name;
    int		board;

    uint8_t	config;

    ad1848_t	ad1848;

    opl_t	opl;
    int		opl_enabled;

    uint8_t	pos_regs[8];
} wss_t;


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    wss_t *dev = (wss_t *)priv;
    int c;

    opl3_update2(&dev->opl);

    ad1848_update(&dev->ad1848);

    for (c = 0; c < len * 2; c++) {
	buffer[c] += dev->opl.buffer[c];
	buffer[c] += (dev->ad1848.buffer[c] / 2);
    }

    dev->opl.pos = 0;
    dev->ad1848.pos = 0;
}


static uint8_t
wss_read(uint16_t addr, priv_t priv)
{
    wss_t *dev = (wss_t *)priv;
    uint8_t ret;

    ret = 4 | (dev->config & 0x40);

    return(ret);
}


static void
wss_write(uint16_t addr, uint8_t val, priv_t priv)
{
    wss_t *dev = (wss_t *)priv;

    dev->config = val;

    ad1848_setdma(&dev->ad1848, wss_dmas[val & 3]);
    ad1848_setirq(&dev->ad1848, wss_irqs[(val >> 3) & 7]);
}


static uint8_t
ncr_audio_mca_read(int port, priv_t priv)
{
    wss_t *dev = (wss_t *)priv;

    return(dev->pos_regs[port & 0x07]);
}


static void
ncr_audio_mca_write(int port, uint8_t val, priv_t priv)
{
    wss_t *dev = (wss_t *)priv;
    uint16_t ports[] = { 0x0530, 0x0E80, 0x0F40, 0x0604 };
    uint16_t addr;

    if (port < 0x102) return;	

    dev->opl_enabled = (dev->pos_regs[2] & 0x20) ? 1 : 0;
    addr = ports[(dev->pos_regs[2] & 0x18) >> 3];

    io_removehandler(0x0388, 4,
		     opl3_read,NULL,NULL, opl3_write,NULL,NULL, &dev->opl);
    io_removehandler(addr, 4,
		     wss_read,NULL,NULL, wss_write,NULL,NULL, dev);
    io_removehandler(addr+4, 4,
		     ad1848_read,NULL,NULL, ad1848_write,NULL,NULL, &dev->ad1848);
    INFO("%s: OPL=%i, I/O=%04XH\n", dev->name, dev->opl_enabled, addr);

    dev->pos_regs[port & 7] = val;
    if (dev->pos_regs[2] & 1) {
	addr = ports[(dev->pos_regs[2] & 0x18) >> 3];

	if (dev->opl_enabled)
		io_sethandler(0x0388, 4,
			      opl3_read,NULL,NULL, opl3_write,NULL,NULL, &dev->opl);	
	io_sethandler(addr, 4,
		      wss_read,NULL,NULL, wss_write,NULL,NULL, dev);
	io_sethandler(addr+4, 4,
		      ad1848_read,NULL,NULL, ad1848_write,NULL,NULL, &dev->ad1848);
    }
}


static void
wss_close(priv_t priv)
{
    wss_t *dev = (wss_t *)priv;

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    wss_t *dev = (wss_t *)priv;

    ad1848_speed_changed(&dev->ad1848);
}


static priv_t
wss_init(const device_t *info, UNUSED(void *parent))
{
    wss_t *dev;

    dev = (wss_t *)mem_alloc(sizeof(wss_t));
    memset(dev, 0x00, sizeof(wss_t));
    dev->name = info->name;
    dev->board = info->local;
    uint16_t addr = device_get_config_hex16("base");
    dev->opl_enabled = device_get_config_int("opl");
    
    switch(info->local) {
	case 0:		/* standard ISA controller */
		break;

	case 1:		/* NCR Business Audio MCA */
		dev->pos_regs[0] = 0x16;
		dev->pos_regs[1] = 0x51;		
		mca_add(ncr_audio_mca_read, ncr_audio_mca_write, (priv_t)dev);
		break;
    }

    ad1848_init(&dev->ad1848);
    ad1848_setirq(&dev->ad1848, 7);
    ad1848_setdma(&dev->ad1848, 3);

    if (dev->opl_enabled) {
	opl3_init(&dev->opl);
	io_sethandler(0x0388, 4,
			opl3_read,NULL,NULL, opl3_write,NULL,NULL, &dev->opl);
    }
    
    io_sethandler(addr, 4,
		  wss_read,NULL,NULL, wss_write,NULL,NULL, dev);
    io_sethandler(addr + 4, 4,
		  ad1848_read,NULL,NULL, ad1848_write,NULL,NULL, &dev->ad1848);

    sound_add_handler(get_buffer, (priv_t)dev);

    return((priv_t)dev);
}

static const 
device_config_t wss_config[] =
{
        {
                "base", "Address", CONFIG_HEX16, "", 0x530,
                {
                        {
                                "0x530", 0x530
                        },
                        {
                                "0x604", 0x604
                        },
                        {
                                "0xe80", 0xe80
                        },
                        {
                                "0xf40", 0xf40
                        },
                }
        },
	{
		"opl", "Enable OPL", CONFIG_BINARY, "", 1
	},
        {
                "", "", -1
        }
};

const device_t wss_device = {
    "Windows Sound System",
    DEVICE_ISA | DEVICE_AT,
    0,
    NULL,
    wss_init, wss_close, NULL,
    NULL,
    speed_changed,
    NULL, NULL,
    wss_config
};

const device_t ncr_business_audio_device = {
    "NCR Business Audio",
    DEVICE_MCA,
    1,
    NULL,
    wss_init, wss_close, NULL,
    NULL,
    speed_changed,
    NULL, NULL,
    NULL
};
