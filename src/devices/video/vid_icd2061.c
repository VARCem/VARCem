/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ICD2061 clock generator emulation.
 *
 *		Also emulates the ICS9161 which is the same as the ICD2016,
 *		but without the need for tuning (which is irrelevant in
 *		emulation anyway).
 *
 * Version:	@(#)vid_icd2061.c	1.0.4	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../../device.h"
#include "video.h"
#include "vid_icd2061.h"


void
icd2061_write(icd2061_t *dev, int val)
{
    int /*od, */nd, oc, nc;
    int a/*, i*/, qa, q, pa, p, m, ps;

#if 0
    od = (dev->state & 2) >> 1;	/* Old data. */
#endif
    nd = (val & 2) >> 1;		/* Old data. */
    oc = dev->state & 1;		/* Old clock. */
    nc = val & 1;			/* New clock. */

    dev->state = val;

    if (nc && !oc) {			/* Low-to-high transition of CLK. */
	if (!dev->unlocked) {
		if (nd) {			/* DATA high. */
			dev->count++;
			/* DEBUG("Low-to-high transition of CLK with DATA high, %i total\n", dev->count); */
		} else {			/* DATA low. */
			if (dev->count >= 5) {
				dev->unlocked = 1;
				dev->bit_count = dev->data = 0;
				/* DEBUG("ICD2061 unlocked\n"); */
			} else {
				dev->count = 0;
				/* DEBUG("ICD2061 locked\n"); */
			}
		}
	} else if (nc) {
		dev->data |= (nd << dev->bit_count);
		dev->bit_count++;

		if (dev->bit_count == 26) {
			dev->data >>= 1;
			/* DEBUG("26 bits received, data = %08X\n", dev->data); */
	
			a = ((dev->data >> 22) & 0x07);	/* A  */
			/* DEBUG("A = %01X\n", a); */

			if (a < 3) {
#if 0
				i = ((dev->data >> 18) & 0x0f);	/* I  */
#endif
				pa = ((dev->data >> 11) & 0x7f);	/* P' (ICD2061) / N' (ICS9161) */
				m = ((dev->data >> 8) & 0x07);	/* M  (ICD2061) / R  (ICS9161) */
				qa = ((dev->data >> 1) & 0x7f);	/* Q' (ICD2061) / M' (ICS9161) */

				p = pa + 3;				/* P  (ICD2061) / N  (ICS9161) */
				m = 1 << m;
				q = qa + 2;				/* Q  (ICD2061) / M  (ICS9161) */
				ps = (dev->ctrl & (1 << a)) ? 4 : 2;/* Prescale */

				dev->freq[a] = ((float)(p * ps) / (float)(q * m)) * 14318184.0f;

				/* DEBUG("P = %02X, M = %01X, Q = %02X, freq[%i] = %f\n", p, m, q, a, dev->freq[a]); */
			} else if (a == 6) {
				dev->ctrl = ((dev->data >> 13) & 0xff);
				/* DEBUG("ctrl = %02X\n", dev->ctrl); */
			}
			dev->count = dev->bit_count = dev->data = 0;
			dev->unlocked = 0;
			/* DEBUG("ICD2061 locked\n"); */
		}
	}
    }
}


float
icd2061_getclock(int clock, void *priv)
{
    icd2061_t *dev = (icd2061_t *)priv;

    if (clock > 2)
	clock = 2;

    return dev->freq[clock];
}


static void *
icd2061_init(const device_t *info)
{
    icd2061_t *dev = (icd2061_t *)mem_alloc(sizeof(icd2061_t));
    memset(dev, 0x00, sizeof(icd2061_t));

    dev->freq[0] = 25175000.0;
    dev->freq[1] = 28322000.0;
    dev->freq[2] = 28322000.0;

    return(dev);
}


static void
icd2061_close(void *priv)
{
    icd2061_t *dev = (icd2061_t *)priv;

    if (dev != NULL)
	free(dev);
}


const device_t icd2061_device = {
    "ICD2061 Clock Generator",
    0,
    0,
    icd2061_init, icd2061_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t ics9161_device = {
    "ICS9161 Clock Generator",
    0,
    0,
    icd2061_init, icd2061_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
