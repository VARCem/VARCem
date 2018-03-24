/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the LPT-based DSS sound device.
 *
 * Version:	@(#)snd_lpt_dss.c	1.0.2	2018/03/15
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
#include "../timer.h"
#include "../lpt.h"
#include "sound.h"
#include "filters.h"
#include "snd_lpt_dss.h"


typedef struct dss_t
{
        uint8_t fifo[16];
        int read_idx, write_idx;
        
        uint8_t dac_val;
        
        int64_t time;
        
        int16_t buffer[SOUNDBUFLEN];
        int pos;
} dss_t;

static void dss_update(dss_t *dss)
{
        for (; dss->pos < sound_pos_global; dss->pos++)
                dss->buffer[dss->pos] = (int8_t)(dss->dac_val ^ 0x80) * 0x40;
}


static void dss_write_data(uint8_t val, void *p)
{
        dss_t *dss = (dss_t *)p;

        timer_clock();

        if ((dss->write_idx - dss->read_idx) < 16)
        {
                dss->fifo[dss->write_idx & 15] = val;
                dss->write_idx++;
        }
}

static void dss_write_ctrl(uint8_t val, void *p)
{
}

static uint8_t dss_read_status(void *p)
{
        dss_t *dss = (dss_t *)p;

        if ((dss->write_idx - dss->read_idx) >= 16)
                return 0x40;
        return 0;
}


static void dss_get_buffer(int32_t *buffer, int len, void *p)
{
        dss_t *dss = (dss_t *)p;
        int c;
        
        dss_update(dss);
        
        for (c = 0; c < len*2; c += 2)
        {
                int16_t val = (int16_t)dss_iir((float)dss->buffer[c >> 1]);
                
                buffer[c] += val;
                buffer[c+1] += val;
        }

        dss->pos = 0;
}

static void dss_callback(void *p)
{
        dss_t *dss = (dss_t *)p;

        dss_update(dss);

        if ((dss->write_idx - dss->read_idx) > 0)
        {
                dss->dac_val = dss->fifo[dss->read_idx & 15];
                dss->read_idx++;
        }
        
        dss->time += (int64_t) (TIMER_USEC * (1000000.0 / 7000.0));
}

static void *dss_init(void)
{
        dss_t *dss = malloc(sizeof(dss_t));
        memset(dss, 0, sizeof(dss_t));

        sound_add_handler(dss_get_buffer, dss);
        timer_add(dss_callback, &dss->time, TIMER_ALWAYS_ENABLED, dss);
                
        return dss;
}
static void dss_close(void *p)
{
        dss_t *dss = (dss_t *)p;
        
        free(dss);
}

const lpt_device_t dss_device =
{
        "Disney Sound Source",
        dss_init,
        dss_close,
        dss_write_data,
        dss_write_ctrl,
        dss_read_status
};
