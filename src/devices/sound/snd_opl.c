/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Interface to the actual OPL emulator.
 *
 * Version:	@(#)snd_opl.c	1.0.2	2018/05/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../timer.h"
#include "sound.h"
#include "snd_opl.h"
#include "snd_dbopl.h"


uint8_t opl2_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        opl2_update2(opl);
        return opl_read(0, a);
}
void opl2_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl2_update2(opl);
        opl_write(0, a, v);
        opl_write(1, a, v);
}

uint8_t opl2_l_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        opl2_update2(opl);
        return opl_read(0, a);
}
void opl2_l_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl2_update2(opl);
        opl_write(0, a, v);
}

uint8_t opl2_r_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        opl2_update2(opl);
        return opl_read(1, a);
}
void opl2_r_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl2_update2(opl);
        opl_write(1, a, v);
}

uint8_t opl3_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        opl3_update2(opl);
        return opl_read(0, a);
}
void opl3_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;
        
        opl3_update2(opl);
        opl_write(0, a, v);
}


void opl2_update2(opl_t *opl)
{
        if (opl->pos < sound_pos_global)
        {
                opl2_update(0, &opl->buffer[opl->pos*2], sound_pos_global - opl->pos);
                opl2_update(1, &opl->buffer[opl->pos*2 + 1], sound_pos_global - opl->pos);
                for (; opl->pos < sound_pos_global; opl->pos++)
                {
                        opl->filtbuf[0] = opl->buffer[opl->pos*2]   = (opl->buffer[opl->pos*2]   / 2);
                        opl->filtbuf[1] = opl->buffer[opl->pos*2+1] = (opl->buffer[opl->pos*2+1] / 2);
                }
        }
}

void opl3_update2(opl_t *opl)
{
        if (opl->pos < sound_pos_global)
        {
                opl3_update(0, &opl->buffer[opl->pos*2], sound_pos_global - opl->pos);
                for (; opl->pos < sound_pos_global; opl->pos++)
                {
                        opl->filtbuf[0] = opl->buffer[opl->pos*2]   = (opl->buffer[opl->pos*2]   / 2);
                        opl->filtbuf[1] = opl->buffer[opl->pos*2+1] = (opl->buffer[opl->pos*2+1] / 2);
                }
        }
}

void ym3812_timer_set_0(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;
        
        opl->timers[0][timer] = period * TIMER_USEC * 20LL;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}
void ym3812_timer_set_1(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[1][timer] = period * TIMER_USEC * 20LL;
        if (!opl->timers[1][timer]) opl->timers[1][timer] = 1;
        opl->timers_enable[1][timer] = period ? 1 : 0;
}

void ymf262_timer_set(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[0][timer] = period * TIMER_USEC * 20LL;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}

static void opl_timer_callback00(void *p)
{
        opl_t *opl = (opl_t *)p;
        
        opl->timers_enable[0][0] = 0;
        opl_timer_over(0, 0);
}
static void opl_timer_callback01(void *p)
{
        opl_t *opl = (opl_t *)p;
        
        opl->timers_enable[0][1] = 0;
        opl_timer_over(0, 1);
}
static void opl_timer_callback10(void *p)
{
        opl_t *opl = (opl_t *)p;
        
        opl->timers_enable[1][0] = 0;
        opl_timer_over(1, 0);
}
static void opl_timer_callback11(void *p)
{
        opl_t *opl = (opl_t *)p;
        
        opl->timers_enable[1][1] = 0;
        opl_timer_over(1, 1);
}
        
void opl2_init(opl_t *opl)
{
        opl_init(ym3812_timer_set_0, opl, 0, 0);
        opl_init(ym3812_timer_set_1, opl, 1, 0);
        timer_add(opl_timer_callback00, &opl->timers[0][0], &opl->timers_enable[0][0], (void *)opl);
        timer_add(opl_timer_callback01, &opl->timers[0][1], &opl->timers_enable[0][1], (void *)opl);
        timer_add(opl_timer_callback10, &opl->timers[1][0], &opl->timers_enable[1][0], (void *)opl);
        timer_add(opl_timer_callback11, &opl->timers[1][1], &opl->timers_enable[1][1], (void *)opl);
}

void opl3_init(opl_t *opl)
{
        opl_init(ymf262_timer_set, opl, 0, 1);
        timer_add(opl_timer_callback00, &opl->timers[0][0], &opl->timers_enable[0][0], (void *)opl);
        timer_add(opl_timer_callback01, &opl->timers[0][1], &opl->timers_enable[0][1], (void *)opl);
}

