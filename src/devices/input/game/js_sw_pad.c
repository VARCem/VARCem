/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of a Side Winder GamePad.
 *
 * Notes:	- Write to 0x201 starts packet transfer (5*N or 15*N bits)
 *		- Currently alternates between Mode A and Mode B (is there
 *		  any way of actually controlling which is used?)
 *		- Windows 9x drivers require Mode B when more than 1 pad
 *		  connected
 *		- Packet preceeded by high data (currently 50us), and
 *		  followed by low data (currently 160us) - timings are
 *		  probably wrong, but good enough for everything I've tried
 *		- Analog inputs are only used to time ID packet request.
 *		  If A0 timing out is followed after ~64us by another 0x201
 *		  write then an ID packet is triggered
 *		- Sidewinder game pad ID is 'H0003'
 *		- ID is sent in Mode A (1 bit per clock), but data bit 2
 *		  must change during ID packet transfer, or Windows 9x
 *		  drivers won't use Mode B. I don't know if it oscillates,
 *		  mirrors the data transfer, or something else - the drivers
 *		  only check that it changes at least 10 times during the
 *		  transfer
 *		- Some DOS stuff will write to 0x201 while a packet is
 *		  being transferred. This seems to be ignored.
 *
 * Version:	@(#)js_sw_pad.c	1.0.9	2018/09/22
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include "../../../emu.h"
#include "../../../timer.h"
#include "../../ports/game_dev.h"
#include "joystick.h"


typedef struct {
    int64_t	poll_time;
    int64_t	poll_left;
    int64_t	poll_clock;
    uint64_t	poll_data;
    int64_t	poll_mode;

    int64_t	trigger_time;
    int64_t	data_mode;
} sw_data;


static void
timer_over(void *priv)
{
    sw_data *sw = (sw_data *)priv;
	
    while (sw->poll_time <= 0 && sw->poll_left) {
	sw->poll_clock = !sw->poll_clock;

	if (sw->poll_clock) {
		sw->poll_data >>= (sw->poll_mode ? 3 : 1);
		sw->poll_left--;
	}

	if (sw->poll_left == 1 && !sw->poll_clock)
		sw->poll_time += TIMER_USEC * 160LL;
	else if (sw->poll_left)
		sw->poll_time += TIMER_USEC * 5;
	else
		sw->poll_time = 0;
    }

    if (! sw->poll_left)
	sw->poll_time = 0;
}


static void
trigger_timer_over(void *priv)
{
    sw_data *sw = (sw_data *)priv;

    sw->trigger_time = 0;
}


static int
sw_parity(uint16_t data)
{
    int bits_set = 0;

    while (data) {
	bits_set++;
	data &= (data - 1);
    }
	
    return(bits_set & 1);
}


static void *
sw_init(void)
{
    sw_data *sw;

    INFO("SideWinder: initializing..\n");

    sw = (sw_data *)mem_alloc(sizeof(sw_data));
    memset(sw, 0x00, sizeof(sw_data));

    timer_add(timer_over, &sw->poll_time, &sw->poll_time, sw);
    timer_add(trigger_timer_over, &sw->trigger_time, &sw->trigger_time, sw);

    return(sw);
}


static void
sw_close(void *priv)
{
    sw_data *sw = (sw_data *)priv;

    INFO("SideWinder: closing.\n");

    free(sw);
}


static uint8_t
sw_read(void *priv)
{
    sw_data *sw = (sw_data *)priv;
    uint8_t temp = 0;

    DEBUG("SideWinder: read(): %d\n", !!JOYSTICK_PRESENT(0));

    if (! JOYSTICK_PRESENT(0)) return(0xff);

    if (sw->poll_time) {	
	if (sw->poll_clock)
		temp |= 0x10;

	if (sw->poll_mode)
		temp |= (sw->poll_data & 7) << 5;
	else {
		temp |= ((sw->poll_data & 1) << 5) | 0xc0;
		if (sw->poll_left > 31 && !(sw->poll_left & 1))
			temp &= ~0x80;
	}
    } else
	temp |= 0xf0;

    return(temp);
}


static void
sw_write(void *priv)
{
    sw_data *sw = (sw_data *)priv;
    int64_t time_since_last = sw->trigger_time / TIMER_USEC;
    uint16_t data;
    int b, c;

    DEBUG("SideWinder: write(): %d\n", !!JOYSTICK_PRESENT(0));

    if (! JOYSTICK_PRESENT(0)) return;

    timer_process();

    if (! sw->poll_left) {
	sw->poll_clock = 1;
	sw->poll_time = TIMER_USEC * 50;

	if (time_since_last > 9900 && time_since_last < 9940) {
		sw->poll_mode = 0;
		sw->poll_left = 49;
		sw->poll_data = 0x2400ULL | (0x1830ULL << 15) | (0x19b0ULL << 30);		
	} else {
		sw->poll_mode = sw->data_mode;
		sw->data_mode = !sw->data_mode;

		if (sw->poll_mode) {
			sw->poll_left = 1;
			sw->poll_data = 7;
		} else {
			sw->poll_left = 1;
			sw->poll_data = 1;
		}

		for (c = 0; c < 4; c++) {
			data = 0x3fff;

			if (! JOYSTICK_PRESENT(c)) break;

			if (joystick_state[c].axis[1] < -16383)
				data &= ~1;
			if (joystick_state[c].axis[1] > 16383)
				data &= ~2;
			if (joystick_state[c].axis[0] > 16383)
				data &= ~4;
			if (joystick_state[c].axis[0] < -16383)
				data &= ~8;

			for (b = 0; b < 10; b++) {
				if (joystick_state[c].button[b])
					data &= ~(1 << (b + 4));
			}

			if (sw_parity(data))
				data |= 0x4000;

			if (sw->poll_mode) {
				sw->poll_left += 5;
				sw->poll_data |= (data << (c*15 + 3));
			} else {
				sw->poll_left += 15;
				sw->poll_data |= (data << (c*15 + 1));
			}
		}
	}
    }
	
    sw->trigger_time = 0;
	
    timer_update_outstanding();
}


static int
sw_axis(void *priv, int axis)
{
    if (! JOYSTICK_PRESENT(0))
	return(AXIS_NOT_PRESENT);

    /* No analog support on Sidewinder game pad. */
    return(0);
}


static void
sw_over(void *priv)
{
    sw_data *sw = (sw_data *)priv;

    sw->trigger_time = TIMER_USEC * 10000;
}


const gamedev_t js_sw_pad = {
    "Microsoft SideWinder Pad",
    sw_init, sw_close,
    sw_read, sw_write,
    sw_axis, sw_over,
    2,
    10,
    0,
    4,
    {"X axis", "Y axis"},
    {"A", "B", "C", "X", "Y", "Z", "L", "R", "Start", "M"}
};
