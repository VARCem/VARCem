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
 *		The clock generator is responsible for providing clocks for
 *		all peripheral components on the board. Originally, it used
 *		a single crystal oscillator (the 14.3MHz one) from which all
 *		others were derived (the 1.8Mhz clock for the PIT, the 4.77M
 *		for the processor and bus, and a buffered 14.3M for the video
 *		cards) but later, a second oscillator was added, allowing for
 *		a 'turbo' mode for the processor, where the processor speed
 *		was higher than the bus and peripheral speeds.
 *
 *		On more modern systems, this functionality of course has been
 *		integrated into the chipsets.
 *
 * Version:	@(#)clk.c	1.0.2	2019/05/17
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
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../device.h"
#include "clk.h"


float	cpu_clock;		/* clock for the processor */
float	bus_timing;		/* clock divider for the bus clock */

double	PITCONST;		/* divider for the PIT (i8254) */

float	CGACONST;		/* divider for the CGA controller */
float	MDACONST;		/* divider for the MDA controller */
float	VGACONST1,		/* divider for the VGA controllers */
	VGACONST2;
float	RTCCONST;		/* divider for the RTC */


/* Set default CPU/crystal clock and xt_cpu_multi. */
void
clk_setup(uint32_t freq)
{
    uint32_t speed;

    if (cpu_get_type() >= CPU_286) {
	/* For 286 and up, this is easy. */
	cpu_clock = (float)freq;
	PITCONST = cpu_clock / 1193182.0;
	CGACONST = (float) (cpu_clock  / (19687503.0 / 11.0));
	xt_cpu_multi = 1;
    } else {
	/* Not so much for XT-class systems. */
	cpu_clock = 14318184.0;
       	PITCONST = 12.0;
        CGACONST = 8.0;
	xt_cpu_multi = 3;

	/* Get selected CPU's (max) clock rate. */
	speed = cpu_get_speed();

	switch (speed) {
		case 7159092:	/* 7.16 MHz */
			if (cpu_get_flags() & CPU_ALTERNATE_XTAL) {
				cpu_clock = 28636368.0;
				xt_cpu_multi = 4;
			} else
				xt_cpu_multi = 2;
			break;

		case 8000000:	/* 8 MHz */
		case 9545456:	/* 9.54 MHz */
		case 10000000:	/* 10 MHz */
		case 12000000:	/* 12 MHz */
		case 16000000:	/* 16 MHz */
			cpu_clock = ((float)speed * xt_cpu_multi);
			break;

		default:
			if (cpu_get_flags() & CPU_ALTERNATE_XTAL) {
				cpu_clock = 28636368.0;
				xt_cpu_multi = 6;
			}
			break;
	}

	if (cpu_clock == 28636368.0) {
        	PITCONST = 24.0;
	        CGACONST = 16.0;
	} else if (cpu_clock != 14318184.0) {
		PITCONST = cpu_clock / 1193182.0;
		CGACONST = (float) (cpu_clock / (19687503.0 / 11.0));
	}
   }

    xt_cpu_multi <<= TIMER_SHIFT;

    MDACONST = (float) (cpu_clock / 2032125.0);
    VGACONST1 = (float) (cpu_clock / 25175000.0);
    VGACONST2 = (float) (cpu_clock / 28322000.0);
    RTCCONST = (float) (cpu_clock / 32768.0);

    TIMER_USEC = (tmrval_t)((cpu_clock / 1000000.0f) * (float)(1 << TIMER_SHIFT));

    bus_timing = (float) (cpu_clock / (double)cpu_busspeed);

    INFO("CLK: cpu=%.2f xt=%d PIT=%.2f RTC=%.2f CGA=%.2f MDA=%.2f TMR=%" PRIu64 "\n",
	cpu_clock, xt_cpu_multi, (float)PITCONST, RTCCONST, CGACONST, MDACONST,
	TIMER_USEC);

    device_speed_changed();
}
