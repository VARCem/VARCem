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
 * Version:	@(#)snd_lpt_dac.c	1.0.2	2018/03/15
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
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../machine/machine.h"
#include "../lpt.h"
#include "../timer.h"
#include "sound.h"
#include "filters.h"
#include "snd_lpt_dac.h"

typedef struct lpt_dac_t
{
        uint8_t dac_val_l, dac_val_r;
        
        int is_stereo;
        int channel;
        
        int16_t buffer[2][SOUNDBUFLEN];
        int pos;
} lpt_dac_t;

static void dac_update(lpt_dac_t *lpt_dac)
{
        for (; lpt_dac->pos < sound_pos_global; lpt_dac->pos++)
        {
                lpt_dac->buffer[0][lpt_dac->pos] = (int8_t)(lpt_dac->dac_val_l ^ 0x80) * 0x40;
                lpt_dac->buffer[1][lpt_dac->pos] = (int8_t)(lpt_dac->dac_val_r ^ 0x80) * 0x40;
        }
}


static void dac_write_data(uint8_t val, void *p)
{
        lpt_dac_t *lpt_dac = (lpt_dac_t *)p;
        
        timer_clock();

        if (lpt_dac->is_stereo)
        {
                if (lpt_dac->channel)
                        lpt_dac->dac_val_r = val;
                else
                        lpt_dac->dac_val_l = val;
        }
        else        
                lpt_dac->dac_val_l = lpt_dac->dac_val_r = val;
        dac_update(lpt_dac);
}

static void dac_write_ctrl(uint8_t val, void *p)
{
        lpt_dac_t *lpt_dac = (lpt_dac_t *)p;

        if (lpt_dac->is_stereo)
                lpt_dac->channel = val & 0x01;
}

static uint8_t dac_read_status(void *p)
{
        return 0;
}


static void dac_get_buffer(int32_t *buffer, int len, void *p)
{
        lpt_dac_t *lpt_dac = (lpt_dac_t *)p;
        int c;
        
        dac_update(lpt_dac);
        
        for (c = 0; c < len; c++)
        {
                buffer[c*2]     += dac_iir(0, lpt_dac->buffer[0][c]);
                buffer[c*2 + 1] += dac_iir(1, lpt_dac->buffer[1][c]);
        }
        lpt_dac->pos = 0;
}

static void *dac_init(void)
{
        lpt_dac_t *lpt_dac = malloc(sizeof(lpt_dac_t));
        memset(lpt_dac, 0, sizeof(lpt_dac_t));

        sound_add_handler(dac_get_buffer, lpt_dac);
                
        return lpt_dac;
}
static void *dac_stereo_init(void)
{
        lpt_dac_t *lpt_dac = dac_init();
        
        lpt_dac->is_stereo = 1;
                
        return lpt_dac;
}
static void dac_close(void *p)
{
        lpt_dac_t *lpt_dac = (lpt_dac_t *)p;
        
        free(lpt_dac);
}

const lpt_device_t lpt_dac_device =
{
        "LPT DAC / Covox Speech Thing",
        dac_init,
        dac_close,
        dac_write_data,
        dac_write_ctrl,
        dac_read_status
};
const lpt_device_t lpt_dac_stereo_device =
{
        "Stereo LPT DAC",
        dac_stereo_init,
        dac_close,
        dac_write_data,
        dac_write_ctrl,
        dac_read_status
};
