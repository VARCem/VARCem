/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Interface to the ReSid library.
 *
 * Version:	@(#)snd_resid.cpp	1.0.4	2018/05/06
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
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "resid-fp/sid.h"
#include "../../plat.h"
#include "sound.h"
#include "snd_resid.h"


typedef struct {
    /* resid sid implementation */
    SIDFP	*sid;

    int16_t	last_sample;
} psid_t;


#define CLOCK_DELTA(n) (int)(((14318180.0 * n) / 16.0) / 48000.0)
static void
fillbuf2(int& count, int16_t *buf, int len, psid_t *dev)
{
    int c;

    c = dev->sid->clock(count, buf, len, 1);
    if (!c)
	*buf = dev->last_sample;

    dev->last_sample = *buf;
}


void *
sid_init(void)
{
    sampling_method method = SAMPLE_INTERPOLATE;
    float cycles_per_sec = 14318180.0 / 16.0;
    psid_t *dev;

    dev = new psid_t;
    dev->sid = new SIDFP;

    dev->sid->set_chip_model(MOS8580FP);

    dev->sid->set_voice_nonlinearity(1.0f);
    dev->sid->get_filter().set_distortion_properties(0.f, 0.f, 0.f);
    dev->sid->get_filter().set_type4_properties(6.55f, 20.0f);

    dev->sid->enable_filter(true);
    dev->sid->enable_external_filter(true);

    sid_reset(dev);

    if (! dev->sid->set_sampling_parameters((float)cycles_per_sec,
					    method,
					    (float)48000,
					    0.9*48000.0/2.0)) {
	snddev_log("ReSID set_sampling_parameters() failed!\n");
    }

    dev->sid->set_chip_model(MOS6581FP);
    dev->sid->set_voice_nonlinearity(0.96f);
    dev->sid->get_filter().set_distortion_properties(3.7e-3f,
						     2048.f,
						     1.2e-4f);

    dev->sid->input(0);
    dev->sid->get_filter().set_type3_properties(1.33e6f,
						2.2e9f,
						1.0056f,
						7e3f);

    return(dev);
}


void
sid_close(void *priv)
{
    psid_t *dev = (psid_t *)priv;

    delete dev->sid;
    delete dev;
}


void
sid_reset(void *priv)
{
    psid_t *dev = (psid_t *)priv;
    int c;
	
    dev->sid->reset();

    for (c = 0; c < 32; c++)
	dev->sid->write(c, 0x00);
}


uint8_t
sid_read(uint16_t addr, void *priv)
{
    psid_t *dev = (psid_t *)priv;

    return(dev->sid->read(addr & 0x1f));
}


void
sid_write(uint16_t addr, uint8_t val, void *priv)
{
    psid_t *dev = (psid_t *)priv;

    dev->sid->write(addr & 0x1f, val);
}


void
sid_fillbuf(int16_t *buf, int len, void *priv)
{
    psid_t *dev = (psid_t *)priv;
    int x = CLOCK_DELTA(len);

    fillbuf2(x, buf, len, dev);
}
