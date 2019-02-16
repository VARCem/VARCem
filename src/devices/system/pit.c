/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the PIT (Programmable Interval Timer.)
 *
 *		B0 to 40, two writes to 43, then two reads
 *			- value does not change!
 *		B4 to 40, two writes to 43, then two reads
 *			- value _does_ change!
 *
 * Version:	@(#)pit.c	1.0.11	2019/02/15
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
#include "../../machines/machine.h"
#include "../../io.h"
#include "../../device.h"
#include "../../timer.h"
#include "../sound/sound.h"
#include "../sound/snd_speaker.h"
#ifdef USE_CASSETTE
# include <cassette.h>
#endif
#include "../video/video.h"
#include "nmi.h"
#include "dma.h"
#include "pic.h"
#include "pit.h"
#include "ppi.h"


PIT	pit,
	pit2;

float	cpuclock;
float	bus_timing;

double	PITCONST;
float	CGACONST;
float	MDACONST;
float	VGACONST1,
	VGACONST2;
float	RTCCONST;


static void
set_out(PIT *dev, int t, int out)
{
    dev->set_out_funcs[t](out, dev->out[t]);
    dev->out[t] = out;
}


static void
set_gate_no_timer(PIT *dev, int t, int gate)
{
    int64_t l = dev->l[t] ? dev->l[t] : 0x10000LL;

    if (dev->disabled[t]) {
	dev->gate[t] = gate;
	return;
    }

    switch (dev->m[t]) {
	case 0:		/* interrupt on terminal count */
	case 4:		/* software triggered strobe */
		dev->enabled[t] = gate;
		break;

	case 1:		/* hardware retriggerable one-shot */
	case 5:		/* hardware triggered strobe */
		if (gate && !dev->gate[t]) {
			dev->count[t] = (int)l;
                        dev->c[t] = (int64_t)((l << TIMER_SHIFT) * PITCONST);
			set_out(dev, t, 0);
			dev->thit[t] = 0;
			dev->enabled[t] = 1;
		}
		break;

	case 2:		/* rate generator */
		if (gate && !dev->gate[t]) {
			dev->count[t] = (int)(l - 1);
                        dev->c[t] = (int64_t)((l << TIMER_SHIFT) * PITCONST);
			set_out(dev, t, 1);
			dev->thit[t] = 0;
		}		
		dev->enabled[t] = gate;
		break;

	case 3:		/* square wave mode */
		if (gate && !dev->gate[t]) {
			dev->count[t] = (int)l;
			dev->c[t] = (int64_t)((((l+1)>>1)<<TIMER_SHIFT)*PITCONST);
			set_out(dev, t, 1);
			dev->thit[t] = 0;
		}
		dev->enabled[t] = gate;
		break;
    }

    dev->gate[t] = gate;
    dev->running[t] = dev->enabled[t] &&
		      dev->using_timer[t] && !dev->disabled[t];
}


static void
do_over(PIT *dev, int t)
{
    int64_t l = dev->l[t] ? dev->l[t] : 0x10000LL;

    if (dev->disabled[t]) {
	dev->count[t] += 0xffff;
	dev->c[t] += (int64_t)(((int64_t)0xffff << TIMER_SHIFT) * PITCONST);
	return;
    }

    switch (dev->m[t]) {
	case 0:		/* interrupt on terminal count */
	case 1:		/* hardware retriggerable one-shot */
		if (! dev->thit[t])
			set_out(dev, t, 1);
		dev->thit[t] = 1;
		dev->count[t] += 0xffff;
		dev->c[t] += (int64_t)(((int64_t)0xffff << TIMER_SHIFT) * PITCONST);
		break;

	case 2:		/* rate generator */
		dev->count[t] += (int)l;
		dev->c[t] += (int64_t)((l << TIMER_SHIFT) * PITCONST);
		set_out(dev, t, 0);
		set_out(dev, t, 1);
		break;

	case 3:		/* square wave mode */
		if (dev->out[t]) {
			set_out(dev, t, 0);
			dev->count[t] += (int)(l >> 1);
			dev->c[t] += (int64_t)(((l >> 1) << TIMER_SHIFT) * PITCONST);
		} else {
			set_out(dev, t, 1);
			dev->count[t] += (int)((l + 1) >> 1);
			dev->c[t] = (int64_t)((((l + 1) >> 1) << TIMER_SHIFT) * PITCONST);
		}
		break;

	case 4:		/* software triggered strobe */
		if (! dev->thit[t]) {
			set_out(dev, t, 0);
			set_out(dev, t, 1);
		}
		if (dev->newcount[t]) {
			dev->newcount[t] = 0;
			dev->count[t] += (int)l;
			dev->c[t] += (int64_t)((l << TIMER_SHIFT) * PITCONST);
		} else {
			dev->thit[t] = 1;
			dev->count[t] += 0xffff;
			dev->c[t] += (int64_t)(((int64_t)0xffff << TIMER_SHIFT) * PITCONST);
		}
		break;

	case 5:		/* hardware triggered strobe */
		if (! dev->thit[t]) {
			set_out(dev, t, 0);
			set_out(dev, t, 1);
		}
		dev->thit[t] = 1;
		dev->count[t] += 0xffff;
		dev->c[t] += (int64_t)(((int64_t)0xffff << TIMER_SHIFT) * PITCONST);
		break;
    }

    dev->running[t] = dev->enabled[t] &&
		      dev->using_timer[t] && !dev->disabled[t];
}


static void
pit_clock(PIT *dev, int t)
{
    if (dev->thit[t] || !dev->enabled[t]) return;

    if (dev->using_timer[t]) return;

    dev->count[t] -= (dev->m[t] == 3) ? 2 : 1;
    if (dev->count[t] == 0)
	do_over(dev, t);
}


static void
timer_over(void *priv)
{
    PIT_nr *pit_nr = (PIT_nr *)priv;
    PIT *dev = pit_nr->pit;
    int t = pit_nr->nr;

    do_over(dev, t);
}


static int
read_timer(PIT *dev, int t)
{
    int r;

    timer_clock();

    if (dev->using_timer[t] && !(dev->m[t] == 3 && !dev->gate[t])) {
	r = (int)((dev->c[t] + ((1 << TIMER_SHIFT) - 1)) / PITCONST) >> TIMER_SHIFT;

	if (dev->m[t] == 2)
		r++;
	if (r < 0)
		r = 0;
	if (r > 0x10000)
		r = 0x10000;
	if (dev->m[t] == 3)
		r <<= 1;

	return r;
    }

    if (dev->m[t] == 2)
	return (dev->count[t] + 1);

    return dev->count[t];
}


static void
load_timer(PIT *dev, int t)
{
    int l = dev->l[t] ? dev->l[t] : 0x10000;

    timer_clock();

    dev->newcount[t] = 0;
    dev->disabled[t] = 0;

    switch (dev->m[t]) {
	case 0:		/* interrupt on terminal count */
                dev->count[t] = l;
                dev->c[t] = (int64_t)((((int64_t) l) << TIMER_SHIFT) * PITCONST);
		set_out(dev, t, 0);
		dev->thit[t] = 0;
		dev->enabled[t] = dev->gate[t];
		break;

	case 1:		/* hardware retriggerable one-shot */
		dev->enabled[t] = 1;
		break;

	case 2:		/* rate generator */
		if (dev->initial[t]) {
                        dev->count[t] = l - 1;
                        dev->c[t] = (int64_t)(((((int64_t) l) - 1LL) << TIMER_SHIFT) * PITCONST);
			set_out(dev, t, 1);
			dev->thit[t] = 0;
		}
		dev->enabled[t] = dev->gate[t];
		break;

	case 3:		/* square wave mode */
		if (dev->initial[t]) {
                        dev->count[t] = l;
                        dev->c[t] = (int64_t)((((((int64_t) l) + 1LL) >> 1) << TIMER_SHIFT) * PITCONST);
			set_out(dev, t, 1);
			dev->thit[t] = 0;
		}
		dev->enabled[t] = dev->gate[t];
		break;

	case 4:		/* software triggered strobe */
		if (!dev->thit[t] && !dev->initial[t]) {
			dev->newcount[t] = 1;
		} else {
                        dev->count[t] = l;
                        dev->c[t] = (int64_t)((l << TIMER_SHIFT) * PITCONST);
			set_out(dev, t, 0);
			dev->thit[t] = 0;
		}
		dev->enabled[t] = dev->gate[t];
		break;

	case 5:		/* hardware triggered strobe */
		dev->enabled[t] = 1;
		break;
    }

    dev->initial[t] = 0;
    dev->running[t] = dev->enabled[t] &&
		      dev->using_timer[t] && !dev->disabled[t];

    timer_update_outstanding();
}


static void
pit_write(uint16_t addr, uint8_t val, void *priv)
{
    PIT *dev = (PIT *)priv;
    double sv = 0.0;
    int t;

    switch (addr & 3) {
	case 3:		/* control */
		if ((val & 0xc0) == 0xc0) {
			 if (! (val & 0x20)) {
				if (val & 2)
					dev->rl[0] = (uint32_t)read_timer(dev, 0);
				if (val & 4)
					dev->rl[1] = (uint32_t)read_timer(dev, 1);
				if (val & 8)
					dev->rl[2] = (uint32_t)read_timer(dev, 2);
			}
			if (! (val & 0x10)) {
				if (val & 2) {
					dev->read_status[0] = (dev->ctrls[0] & 0x3f) | (dev->out[0] ? 0x80 : 0);
					dev->do_read_status[0] = 1;
				}
				if (val & 4) {
					dev->read_status[1] = (dev->ctrls[1] & 0x3f) | (dev->out[1] ? 0x80 : 0);
					dev->do_read_status[1] = 1;
				}
				if (val & 8) {
					dev->read_status[2] = (dev->ctrls[2] & 0x3f) | (dev->out[2] ? 0x80 : 0);
					dev->do_read_status[2] = 1;
				}
			}
			return;
		}
		t = val >> 6;
		dev->ctrl = val;
		if ((val >> 7) == 3)
			return;

		if (! (dev->ctrl & 0x30)) {
			dev->rl[t] = (uint32_t)read_timer(dev, t);
			dev->ctrl |= 0x30;
			dev->rereadlatch[t] = 0;
			dev->rm[t] = 3;
			dev->latched[t] = 1;
		} else {
			dev->ctrls[t] = val;
			dev->rm[t] = dev->wm[t] = (dev->ctrl >> 4) & 3;
			dev->m[t] = (val >> 1) & 7;
			if (dev->m[t]>5)
				dev->m[t] &= 3;
			if (! (dev->rm[t])) {
				dev->rm[t] = 3;
				dev->rl[t] = (uint32_t)read_timer(dev, t);
			}
			dev->rereadlatch[t] = 1;
			dev->initial[t] = 1;
			if (dev->m[t])
				set_out(dev, t, 1);
			  else
				set_out(dev, t, 0);
			dev->disabled[t] = 1;
		}
		dev->wp = 0;
		dev->thit[t] = 0;
		break;

	case 0:		/* the actual timers */
	case 1:
	case 2:
		t = addr & 3;
		switch (dev->wm[t]) {
			case 0:
				dev->l[t] &= 0xff;
				dev->l[t] |= (val << 8);
				load_timer(dev, t);
				dev->wm[t] = 3;
				break;

			case 1:
				dev->l[t] = val;
				load_timer(dev, t);
				break;

			case 2:
				dev->l[t] = (val << 8);
				load_timer(dev, t);
				break;

			case 3:
				dev->l[t] &= 0xff00;
				dev->l[t] |= val;
				dev->wm[t] = 0;
#ifdef USE_CASSETTE
				if (t == 2) {
					cassette_write(dev->l[t]);
				}
#endif
				break;
		}

		/* PIT latches are in fractions of 60 ms, so convert to sample using the formula below. */
		sv = (((double) dev->l[2]) / 60.0) * 16384.0;
		speaker_val = ((int) sv) - 0x2000;
		if (speaker_val > 0x2000)
			speaker_val = 0x2000;
		break;
    }
}


static uint8_t
pit_read(uint16_t addr, void *priv)
{
    PIT *dev = (PIT *)priv;
    uint8_t temp = 0xff;
    int t;

    switch (addr & 3) {
	case 3:		/* control */
		temp = dev->ctrl;
		break;

	case 0:		/*the actual timers */
	case 1:
	case 2:
		t = addr & 3;
		if (dev->do_read_status[t]) {
			dev->do_read_status[t] = 0;
			temp = dev->read_status[t];
			break;
		}
		if (dev->rereadlatch[addr & 3] && !dev->latched[addr & 3]) {
			dev->rereadlatch[addr & 3] = 0;
			dev->rl[t] = read_timer(dev, t);
		}
		switch (dev->rm[addr & 3]) {
			case 0:
				temp = dev->rl[addr & 3] >> 8;
				dev->rm[addr & 3] = 3;
				dev->latched[addr & 3] = 0;
				dev->rereadlatch[addr & 3] = 1;
				break;

			case 1:
				temp = (dev->rl[addr & 3]) & 0xff;
				dev->latched[addr & 3] = 0;
				dev->rereadlatch[addr & 3] = 1;
				break;

			case 2:
				temp = (dev->rl[addr & 3]) >> 8;
				dev->latched[addr & 3] = 0;
				dev->rereadlatch[addr & 3] = 1;
				break;

			case 3:
				temp = (dev->rl[addr & 3]) & 0xFF;
				if (dev->m[addr & 3] & 0x80)
					dev->m[addr & 3] &= 7;
				  else
					dev->rm[addr & 3] = 0;
				break;
		}
		break;
    }

    return temp;
}


/* FIXME: should be removed. */
static void
null_timer(int new_out, int old_out)
{
}


/* FIXME: should be moved to machine.c (default for most machines..) */
static void
irq0_timer(int new_out, int old_out)
{
    if (new_out && !old_out)
	picint(1);

    if (! new_out)
	picintc(1);
}


/* FIXME: should be moved to snd_speaker.c */
static void
speaker_timer(int new_out, int old_out)
{
    int64_t l;

    speaker_update();

    l = pit.l[2] ? pit.l[2] : 0x10000LL;
    if (l < 25LL)
	speaker_on = 0;
      else
	speaker_on = new_out;

    ppispeakon = new_out;
}


void
pit_init(void)
{
    int i;

    pit_reset(&pit);

    for (i = 0; i < 3; i++) {
	pit.pit_nr[i].nr = i;
	pit.pit_nr[i].pit = &pit;

	pit.gate[i] = 1;
	pit.using_timer[i] = 1;

	timer_add(timer_over,
		  &pit.c[i], &pit.running[i], (void *)&pit.pit_nr[i]);
    }

    io_sethandler(0x0040, 4,
		  pit_read,NULL,NULL, pit_write,NULL,NULL, &pit);

    /* Timer0: the TOD clock. */
    pit_set_out_func(&pit, 0, irq0_timer);

    /* Timer1: unused. */
    pit_set_out_func(&pit, 1, null_timer);

    /* Timer2: speaker and cassette. */
    pit_set_out_func(&pit, 2, speaker_timer);
    pit.gate[2] = 0;
}


static void
ps2_irq0(int new_out, int old_out)
{
    if (new_out && !old_out) {
	picint(1);
	set_gate_no_timer(&pit2, 0, 1);
    }

    if (! new_out)
	picintc(1);

    if (!new_out && old_out)
	pit_clock(&pit2, 0);
}


static void
ps2_nmi(int new_out, int old_out)
{
    nmi = new_out;

    if (nmi)
	nmi_auto_clear = 1;
}


void
pit_ps2_init(void)
{
    pit_reset(&pit2);

    pit2.gate[0] = 0;
    pit2.using_timer[0] = 0;
    pit2.disabled[0] = 1;

    pit2.pit_nr[0].nr = 0;
    pit2.pit_nr[0].pit = &pit2;

    timer_add(timer_over,
	      &pit2.c[0], &pit2.running[0], (void *)&pit2.pit_nr[0]);

    io_sethandler(0x0044, 1,
		  pit_read,NULL,NULL, pit_write,NULL,NULL, &pit2);
    io_sethandler(0x0047, 1,
		  pit_read,NULL,NULL, pit_write,NULL,NULL, &pit2);

    pit_set_out_func(&pit, 0, ps2_irq0);
    pit_set_out_func(&pit2, 0, ps2_nmi);
}


void
pit_reset(PIT *dev)
{
    void (*old_set_out_funcs[3])(int new_out, int old_out);
    PIT_nr old_pit_nr[3];
    int i;

    memcpy(old_set_out_funcs, dev->set_out_funcs, 3 * sizeof(void *));
    memcpy(old_pit_nr, dev->pit_nr, 3 * sizeof(PIT_nr));
    memset(dev, 0, sizeof(PIT));
    memcpy(dev->set_out_funcs, old_set_out_funcs, 3 * sizeof(void *));
    memcpy(dev->pit_nr, old_pit_nr, 3 * sizeof(PIT_nr));

    for (i = 0; i < 3; i++) {
	dev->ctrls[i] = 0;
	dev->thit[i] = 1;

	dev->m[i] = 0;
	dev->gate[i] = 1;
	dev->l[i] = 0xffff;
	dev->c[i] = (int64_t)(0xffffLL * PITCONST);
	dev->using_timer[i] = 1;
    }

    /* Disable speaker gate. */
    dev->gate[2] = 0;
}


/* Set default CPU/crystal clock and xt_cpu_multi. */
void
pit_setclock(uint32_t freq)
{
    uint32_t speed;

    if (machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].cpu_type >= CPU_286) {
	/* For 286 and up, this is easy. */
	cpuclock = (double)freq;
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
	speed = machine_speed();

	switch (speed) {
		case 7159092:	/* 7.16 MHz */
			if (machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].cpu_flags & CPU_ALTERNATE_XTAL) {
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
			cpuclock = ((double)speed * xt_cpu_multi);
			break;

		default:
			if (machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].cpu_flags & CPU_ALTERNATE_XTAL) {
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

INFO("PIT: cpu=%.2f xt=%d PIT=%.2f RTC=%.2f CGA=%.2f MDA=%.2f TMR=%" PRIu64 "\n",
	cpuclock, xt_cpu_multi, (float)PITCONST, RTCCONST, CGACONST, MDACONST,
	TIMER_USEC);

    video_update_timing();

    device_speed_changed();
}


void
clearpit(void)
{
    pit.c[0] = (pit.l[0] << 2);
}


int
pit_get_timer_0(void)
{
    int r = (int)((pit.c[0] + ((1LL << TIMER_SHIFT) - 1)) / PITCONST) >> TIMER_SHIFT;

    if (pit.m[0] == 2)
	r++;
    if (r < 0)
	r = 0;
    if (r > 0x10000)
	r = 0x10000;
    if (pit.m[0] == 3)
	r <<= 1;

    return r;
}


void
pit_set_gate(PIT *dev, int t, int gate)
{
    if (dev->disabled[t]) {
	dev->gate[t] = gate;
	return;
    }

    timer_process();

    set_gate_no_timer(dev, t, gate);

    timer_update_outstanding();
}


static int64_t
read_timer_ex(PIT *pit, int t)
{
    int64_t r;

    timer_clock();

    if (pit->using_timer[t] && !(pit->m[t] == 3 && !pit->gate[t])) {
	r = (int)(pit->c[t] + ((1 << TIMER_SHIFT) - 1));
	if (pit->m[t] == 2)
		r += (int64_t)((1LL << TIMER_SHIFT) * PITCONST);
	if (r < 0)
		r = 0;
	if (r > ((0x10000LL << TIMER_SHIFT) * PITCONST))
		r = (int64_t)((0x10000LL << TIMER_SHIFT) * PITCONST);
	if (pit->m[t] == 3)
		r <<= 1;

	return r;
    }

    if (pit->m[t] == 2) {
	r = (int64_t) (((pit->count[t] + 1LL) << TIMER_SHIFT) * PITCONST);

	return r;
    }

    r = (int64_t) ((pit->count[t] << TIMER_SHIFT) * PITCONST);

    return r;
}


void
pit_set_using_timer(PIT *dev, int t, int using_timer)
{
    timer_process();

    if (dev->using_timer[t] && !using_timer)
	dev->count[t] = read_timer(dev, t);

    if (!dev->using_timer[t] && using_timer)
	dev->c[t] = read_timer_ex(dev, t);

    dev->using_timer[t] = using_timer;
    dev->running[t] = dev->enabled[t] &&
		      dev->using_timer[t] && !dev->disabled[t];

    timer_update_outstanding();
}


void
pit_set_out_func(PIT *dev, int t, void (*func)(int new_out, int old_out))
{
    dev->set_out_funcs[t] = func;
}


/* FIXME: should be moved to m_pcjr.c */
void
pit_irq0_timer_pcjr(int new_out, int old_out)
{
    if (new_out && !old_out) {
	picint(1);
	pit_clock(&pit, 1);
    }

    if (! new_out)
	picintc(1);
}


/* FIXME: should be moved to m_xt.c */
void
pit_refresh_timer_xt(int new_out, int old_out)
{
    if (new_out && !old_out)
	dma_channel_read(0);
}


/* FIXME: should be moved to m_at.c */
void
pit_refresh_timer_at(int new_out, int old_out)
{
    if (new_out && !old_out)
	ppi.pb ^= 0x10;
}
