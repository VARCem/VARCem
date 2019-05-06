/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the Intel 8284 Clock Generator functionality.
 *
 * Version:	@(#)clk.c	1.0.1	2019/05/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../timer.h"
#include "clk.h"


float	cpuclock;
float	bus_timing;

double	PITCONST;
float	CGACONST;
float	MDACONST;
float	VGACONST1,
	VGACONST2;
float	RTCCONST;


/* Set default CPU/crystal clock and xt_cpu_multi. */
void
clk_setup(uint32_t freq)
{
    uint32_t speed;

    if (cpu_get_type() >= CPU_286) {
	/* For 286 and up, this is easy. */
	cpuclock = (float)freq;
	PITCONST = cpuclock / 1193182.0;
	CGACONST = (float) (cpuclock  / (19687503.0 / 11.0));
	xt_cpu_multi = 1;
    } else {
	/* Not so much for XT-class systems. */
	cpuclock = 14318184.0;
       	PITCONST = 12.0;
        CGACONST = 8.0;
	xt_cpu_multi = 3;

	/* Get selected CPU's (max) clock rate. */
	speed = cpu_get_speed();

	switch (speed) {
		case 7159092:	/* 7.16 MHz */
			if (cpu_get_flags() & CPU_ALTERNATE_XTAL) {
				cpuclock = 28636368.0;
				xt_cpu_multi = 4;
			} else
				xt_cpu_multi = 2;
			break;

		case 8000000:	/* 8 MHz */
		case 9545456:	/* 9.54 MHz */
		case 10000000:	/* 10 MHz */
		case 12000000:	/* 12 MHz */
		case 16000000:	/* 16 MHz */
			cpuclock = ((float)speed * xt_cpu_multi);
			break;

		default:
			if (cpu_get_flags() & CPU_ALTERNATE_XTAL) {
				cpuclock = 28636368.0;
				xt_cpu_multi = 6;
			}
			break;
	}

	if (cpuclock == 28636368.0) {
        	PITCONST = 24.0;
	        CGACONST = 16.0;
	} else if (cpuclock != 14318184.0) {
		PITCONST = cpuclock / 1193182.0;
		CGACONST = (float) (cpuclock / (19687503.0 / 11.0));
	}
   }

    xt_cpu_multi <<= TIMER_SHIFT;

    MDACONST = (float) (cpuclock / 2032125.0);
    VGACONST1 = (float) (cpuclock / 25175000.0);
    VGACONST2 = (float) (cpuclock / 28322000.0);
    RTCCONST = (float) (cpuclock / 32768.0);

    TIMER_USEC = (int64_t)((cpuclock / 1000000.0f) * (float)(1 << TIMER_SHIFT));

    bus_timing = (float) (cpuclock / (double)cpu_busspeed);

    INFO("CLK: cpu=%.2f xt=%d PIT=%.2f RTC=%.2f CGA=%.2f MDA=%.2f TMR=%" PRIu64 "\n",
	cpuclock, xt_cpu_multi, (float)PITCONST, RTCCONST, CGACONST, MDACONST,
	TIMER_USEC);

    device_speed_changed();
}
