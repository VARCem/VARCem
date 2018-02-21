/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		System timer module.
 *
 * Version:	@(#)timer.c	1.0.1	2018/02/14
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
#include <wchar.h>
#include "emu.h"
#include "timer.h"


#define TIMERS_MAX 64


static struct
{
	int64_t present;
	void (*callback)(void *priv);
	void *priv;
	int64_t *enable;
	int64_t *count;
} timers[TIMERS_MAX];


int64_t TIMER_USEC;
int64_t timers_present = 0;
int64_t timer_one = 1;
	
int64_t timer_count = 0, timer_latch = 0;
int64_t timer_start = 0;


void timer_process(void)
{
	int64_t c;
	int64_t process = 0;
	/*Get actual elapsed time*/
	int64_t diff = timer_latch - timer_count;
	int64_t enable[TIMERS_MAX];

	timer_latch = 0;

        for (c = 0; c < timers_present; c++)
        {
		/* This is needed to avoid timer crashes on hard reset. */
		if ((timers[c].enable == NULL) || (timers[c].count == NULL))
		{
			continue;
		}
                enable[c] = *timers[c].enable;
                if (*timers[c].enable)
                {
                        *timers[c].count = *timers[c].count - diff;
                        if (*timers[c].count <= 0)
                                process = 1;
                }
        }
        
        if (!process)
                return;

        while (1)
        {
                int64_t lowest = 1, lowest_c;
                
                for (c = 0; c < timers_present; c++)
                {
                        if (enable[c])
                        {
                                if (*timers[c].count < lowest)
                                {
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


void timer_update_outstanding(void)
{
	int64_t c;
	timer_latch = 0x7fffffffffffffff;
	for (c = 0; c < timers_present; c++)
	{
		if (*timers[c].enable && *timers[c].count < timer_latch)
			timer_latch = *timers[c].count;
	}
	timer_count = timer_latch = (timer_latch + ((1 << TIMER_SHIFT) - 1));
}


void timer_reset(void)
{
	/* pclog("timer_reset\n"); */
	timers_present = 0;
	timer_latch = timer_count = 0;
}


int64_t timer_add(void (*callback)(void *priv), int64_t *count, int64_t *enable, void *priv)
{
	int64_t i = 0;

	if (timers_present < TIMERS_MAX)
	{
		if (timers_present != 0)
		{
			/* This is the sanity check - it goes through all present timers and makes sure we're not adding a timer that already exists. */
			for (i = 0; i < timers_present; i++)
			{
				if (timers[i].present && (timers[i].callback == callback) && (timers[i].priv == priv) && (timers[i].count == count) && (timers[i].enable == enable))
				{
					return 0;
				}
			}
		}

		timers[timers_present].present = 1;
		timers[timers_present].callback = callback;
		timers[timers_present].priv = priv;
		timers[timers_present].count = count;
		timers[timers_present].enable = enable;
		timers_present++;
		return timers_present - 1;
	}
	return -1;
}


void timer_set_callback(int64_t timer, void (*callback)(void *priv))
{
	timers[timer].callback = callback;
}
