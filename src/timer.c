/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		System timer module.
 *
 * Version:	@(#)timer.c	1.0.5	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <wchar.h>
#include "emu.h"
#include "timer.h"


#define TIMERS_MAX 64


tmrval_t		TIMER_USEC;
tmrval_t		timer_one = 1;
tmrval_t		timer_start = 0;
tmrval_t		timer_count = 0;


static struct {
    int		present;

    tmrval_t	*count;
    tmrval_t	*enable;

    void	(*callback)(priv_t);
    priv_t	priv;
}		timers[TIMERS_MAX];
static int	present = 0;
static tmrval_t	latch = 0;


void
timer_process(void)
{
    tmrval_t diff = latch - timer_count;	/* get actual elapsed time */
    tmrval_t enable[TIMERS_MAX];
    int c, process = 0;

    latch = 0;

    for (c = 0; c < present; c++) {
	/* This is needed to avoid timer crashes on hard reset. */
	if ((timers[c].enable == NULL) || (timers[c].count == NULL))
		continue;

	enable[c] = *timers[c].enable;
	if (enable[c]) {
		*timers[c].count = *timers[c].count - diff;
		if (*timers[c].count <= (tmrval_t)0)
			process = 1;
	}
    }

    if (! process)
	return;

    for (;;) {
	tmrval_t lowest = 1LL;
	int lowest_c;

	for (c = 0; c < present; c++) {
		if (enable[c]) {
			if (*timers[c].count < lowest) {
				lowest = *timers[c].count;
				lowest_c = c;
			}
		}
	}

	if (lowest > 0)
		break;

	timers[lowest_c].callback(timers[lowest_c].priv);

	enable[lowest_c] = *timers[lowest_c].enable;
    }              
}


void
timer_update_outstanding(void)
{
    int c;

    latch = 0x7fffffffffffffff;

    for (c = 0; c < present; c++) {
	if (*timers[c].enable && *timers[c].count < latch)
		latch = *timers[c].count;
    }

    timer_count = latch = (latch + ((1 << TIMER_SHIFT) - 1));
}


void
timer_reset(void)
{
    present = 0;

    latch = timer_count = 0;
}


int
timer_add(void (*func)(priv_t), priv_t priv, tmrval_t *count, tmrval_t *enable)
{
    int i = 0;

    /* Can we allocate another one/ */
    if (present == TIMERS_MAX)
	return(-1);

    if (present != 0) {
	/*
	 * Sanity check:
	 * go through all present timers and make sure
	 * we're not adding a timer that already exists.
	 */
	for (i = 0; i < present; i++) {
		if (timers[i].present &&
		    (timers[i].callback == func) &&
		    (timers[i].priv == priv) &&
		    (timers[i].count == count) && (timers[i].enable == enable))
			return 0;
	}
    }

    timers[present].present = 1;
    timers[present].callback = func;
    timers[present].priv = priv;
    timers[present].count = count;
    timers[present].enable = enable;
    present++;

    return present - 1;
}
