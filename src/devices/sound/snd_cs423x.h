/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Crystal CS423x driver.
 *
 * Version:	@(#)snd_cs423x.h	1.0.0	2018/11/27
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
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

typedef struct cs423x_t
{
	int index;
	uint8_t regs[32];
	uint8_t status;

	int trd;
	int mce;
	int ia4;
	int mode2;
	int initb;

	int count;

	int16_t out_l, out_r;

	int64_t enable;

	int irq, dma;

	int64_t freq;

	int64_t timer_count, timer_latch;

	int16_t buffer[SOUNDBUFLEN * 2];
	int pos;
} cs423x_t;

void cs423x_setirq(cs423x_t *cs423x, int irq);
void cs423x_setdma(cs423x_t *cs423x, int dma);

uint8_t cs423x_read(uint16_t addr, void *p);
void cs423x_write(uint16_t addr, uint8_t val, void *p);

void cs423x_update(cs423x_t *cs423x);
void cs423x_speed_changed(cs423x_t *cs423x);

void cs423x_init(cs423x_t *cs423x);
