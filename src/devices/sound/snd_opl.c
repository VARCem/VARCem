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
 * TODO:	Finish re-working this into a device_t, which requires a
 *		poll-like function for "update" so the sound card can call
 *		that and get a buffer-full of sample data.
 *
 * Version:	@(#)snd_opl.c	1.0.8	2020/07/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "sound.h"
#include "snd_opl.h"
#include "snd_opl_nuked.h"


enum {
    STATUS_TIMER_1 = 0x40,
    STATUS_TIMER_2 = 0x20,
    STATUS_TIMER_ALL = 0x80
};

enum {
    CTRL_IRQ_RESET   = 0x80,
    CTRL_TIMER1_MASK = 0x40,
    CTRL_TIMER2_MASK = 0x20,
    CTRL_TIMER2_CTRL = 0x02,
    CTRL_TIMER1_CTRL = 0x01
};


static void
status_update(opl_t *dev)
{
    if (dev->status & (STATUS_TIMER_1 | STATUS_TIMER_2) & dev->status_mask)
	dev->status |= STATUS_TIMER_ALL;
    else
	dev->status &= ~STATUS_TIMER_ALL;
}


static void
timer_set(opl_t *dev, int timer, tmrval_t period)
{
    dev->timers[timer] = period * TIMER_USEC * 20LL;

    if (! dev->timers[timer])
	dev->timers[timer] = 1;

    dev->timers_enable[timer] = period ? 1 : 0;
}


static void
timer_over(opl_t *dev, int tmr)
{
    if (tmr) {
	dev->status |= STATUS_TIMER_2;
	timer_set(dev, 1, dev->timer[1] * 16);
    } else {
	dev->status |= STATUS_TIMER_1;
	timer_set(dev, 0, dev->timer[0] * 4);
    }

    status_update(dev);
}


static void
timer_1(priv_t priv)
{
    opl_t *dev = (opl_t *)priv;

    dev->timers_enable[0] = 0;

    timer_over(dev, 0);
}


static void
timer_2(priv_t priv)
{
    opl_t *dev = (opl_t *)priv;

    dev->timers_enable[1] = 0;

    timer_over(dev, 1);
}


static uint8_t
opl_read(opl_t *dev, uint16_t port)
{
    if (! (port & 1))
	return((dev->status & dev->status_mask) | (dev->is_opl3 ? 0x00 : 0x06));

    if (dev->is_opl3 && ((port & 0x03) == 0x03))
	return(0x00);

    return(dev->is_opl3 ? 0x00 : 0xff);
}


static void
opl_write(opl_t *dev, uint16_t port, uint8_t val)
{
    if (! (port & 1)) {
	dev->port = nuked_write_addr(dev->opl, port, val) & 0x01ff;

	if (! dev->is_opl3)
		dev->port &= 0x00ff;

	return;
    }

    nuked_write_reg_buffered(dev->opl, dev->port, val);

    switch (dev->port) {
	case 0x02:	// timer 1
		dev->timer[0] = 256 - val;
		break;

	case 0x03:	// timer 2
		dev->timer[1] = 256 - val;
		break;

	case 0x04:	// timer control
		if (val & CTRL_IRQ_RESET) {
			dev->status &= ~(STATUS_TIMER_1 | STATUS_TIMER_2);
			status_update(dev);
			return;
		}
		if ((val ^ dev->timer_ctrl) & CTRL_TIMER1_CTRL) {
			if (val & CTRL_TIMER1_CTRL)
				timer_set(dev, 0, dev->timer[0] * 4);
			else
				timer_set(dev, 0, 0);
		}
		if ((val ^ dev->timer_ctrl) & CTRL_TIMER2_CTRL) {
			if (val & CTRL_TIMER2_CTRL)
				timer_set(dev, 1, dev->timer[1] * 16);
			else
				timer_set(dev, 1, 0);
		}
		dev->status_mask = (~val & (CTRL_TIMER1_MASK | CTRL_TIMER2_MASK)) | 0x80;
		dev->timer_ctrl = val;
		break;
    }
}


void
opl_set_do_cycles(opl_t *dev, int8_t do_cycles)
{
    dev->do_cycles = do_cycles;
}


static void
opl_init(opl_t *dev, int is_opl3)
{
    memset(dev, 0x00, sizeof(opl_t));

    dev->is_opl3 = is_opl3;
    dev->do_cycles = 1;

    /* Create a NukedOPL object. */
    dev->opl = nuked_init(48000);

    timer_add(timer_1, dev, &dev->timers[0], &dev->timers_enable[0]);
    timer_add(timer_2, dev, &dev->timers[1], &dev->timers_enable[1]);
}


void
opl_close(opl_t *dev)
{
    /* Release the NukedOPL object. */
    if (dev->opl) {
	nuked_close(dev->opl);
	dev->opl = NULL;
    }
}


uint8_t
opl2_read(uint16_t port, priv_t priv)
{
    opl_t *dev = (opl_t *)priv;

    if (dev->do_cycles)
	cycles -= ISA_CYCLES(8);

    opl2_update(dev);

    return(opl_read(dev, port));
}


void
opl2_write(uint16_t port, uint8_t val, priv_t priv)
{
    opl_t *dev = (opl_t *)priv;

    opl2_update(dev);

    opl_write(dev, port, val);
}


void
opl2_init(opl_t *dev)
{
    opl_init(dev, 0);
}


void
opl2_update(opl_t *dev)
{
    if (dev->pos >= sound_pos_global)
	return;

    nuked_generate_stream(dev->opl,
			  &dev->buffer[dev->pos * 2],
			  sound_pos_global - dev->pos);

    for (; dev->pos < sound_pos_global; dev->pos++) {
	dev->buffer[dev->pos * 2] /= 2;
	dev->buffer[(dev->pos * 2) + 1] = dev->buffer[dev->pos * 2];
    }
}


uint8_t
opl3_read(uint16_t port, priv_t priv)
{
    opl_t *dev = (opl_t *)priv;

    if (dev->do_cycles)
	cycles -= ISA_CYCLES(8);

    opl3_update(dev);

    return(opl_read(dev, port));
}


void
opl3_write(uint16_t port, uint8_t val, priv_t priv)
{
    opl_t *dev = (opl_t *)priv;
	
    opl3_update(dev);

    opl_write(dev, port, val);
}


void
opl3_init(opl_t *dev)
{
    opl_init(dev, 1);
}


/* API to sound interface. */
void
opl3_update(opl_t *dev)
{
    if (dev->pos >= sound_pos_global)
	return;

    nuked_generate_stream(dev->opl,
			  &dev->buffer[dev->pos * 2],
			  sound_pos_global - dev->pos);

    for (; dev->pos < sound_pos_global; dev->pos++) {
	dev->buffer[dev->pos * 2] /= 2;
	dev->buffer[(dev->pos * 2) + 1] /= 2;
    }
}
