/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implemantation of LPT-based sound devices.
 *
 * Version:	@(#)snd_lpt_dac.c	1.0.13	2021/03/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../system/clk.h"
#include "../ports/parallel_dev.h"
#include "sound.h"
#include "snd_filters.h"
#include "snd_lpt_dac.h"


typedef struct {
    uint8_t	dac_val_l,
		dac_val_r;

    int8_t	is_stereo;
    int8_t	channel;

    int16_t	buffer[2][SOUNDBUFLEN];
    int		pos;

    const char	*name;
} lpt_dac_t;


static void
dac_update(lpt_dac_t *dev)
{
    for (; dev->pos < sound_pos_global; dev->pos++) {
	dev->buffer[0][dev->pos] = (int8_t)(dev->dac_val_l ^ 0x80) * 0x40;
	dev->buffer[1][dev->pos] = (int8_t)(dev->dac_val_r ^ 0x80) * 0x40;
    }
}


static void
write_data(uint8_t val, priv_t priv)
{
    lpt_dac_t *dev = (lpt_dac_t *)priv;

    timer_clock();

    if (dev->is_stereo) {
	if (dev->channel)
		dev->dac_val_r = val;
	else
		dev->dac_val_l = val;
    } else {
	dev->dac_val_l = dev->dac_val_r = val;
    }

    dac_update(dev);
}


static void
write_ctrl(uint8_t val, priv_t priv)
{
    lpt_dac_t *dev = (lpt_dac_t *)priv;

    if (dev->is_stereo)
	dev->channel = val & 0x01;
}


static uint8_t
read_status(priv_t priv)
{
    return(0x00);
}


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    lpt_dac_t *dev = (lpt_dac_t *)priv;
    int c;

    dac_update(dev);

    for (c = 0; c < len; c++) {
	buffer[c*2]     += (int32_t)dac_iir(0, dev->buffer[0][c]);
	buffer[c*2 + 1] += (int32_t)dac_iir(1, dev->buffer[1][c]);
    }

    dev->pos = 0;
}


static priv_t
dac_init(const lpt_device_t *info)
{
    lpt_dac_t *dev;

    INFO("SOUND: LPT device '%s' [%d] initializing!\n", info->name, info->type);

    dev = (lpt_dac_t *)mem_alloc(sizeof(lpt_dac_t));
    memset(dev, 0x00, sizeof(lpt_dac_t));
    dev->name = info->name;

    switch(info->type) {
	case 1:
		dev->is_stereo = 1;
		break;
    }

    sound_add_handler(get_buffer, (priv_t)dev);

    return((priv_t)dev);
}


static void
dac_close(priv_t priv)
{
    lpt_dac_t *dev = (lpt_dac_t *)priv;

    INFO("SOUND: LPT device '%s' closed!\n", dev->name);

    free(dev);
}


const lpt_device_t lpt_dac_device = {
    "LPT DAC / Covox Speech Thing",
    0,
    dac_init,
    dac_close,
    write_data,
    write_ctrl,
    read_status
};

const lpt_device_t lpt_dac_stereo_device = {
    "Stereo LPT DAC",
    1,
    dac_init,
    dac_close,
    write_data,
    write_ctrl,
    read_status
};
