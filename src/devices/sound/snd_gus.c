/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Gravis UltraSound sound device.
 *
 * Version:	@(#)snd_gus.c	1.0.18	2020/06/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/dma.h"
#include "../system/nmi.h"
#include "../system/pic.h"
#include "sound.h"
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
#include "snd_cs423x.h"
#include <math.h>
#endif


enum {
    MIDI_INT_RECEIVE = 0x01,
    MIDI_INT_TRANSMIT = 0x02,
    MIDI_INT_MASTER = 0x80
};

enum {
    MIDI_CTRL_TRANSMIT_MASK = 0x60,
    MIDI_CTRL_TRANSMIT = 0x20,
    MIDI_CTRL_RECEIVE = 0x80
};

enum {
    GUS_INT_MIDI_TRANSMIT = 0x01,
    GUS_INT_MIDI_RECEIVE = 0x02
};

enum {
    GUS_TIMER_CTRL_AUTO = 0x01
};


typedef struct {
    int		reset;

    int		global;
    uint32_t	addr,
		dmaaddr;
    int		voice;
    uint32_t	start[32],
		end[32],
		cur[32];
    uint32_t	startx[32],
		endx[32],
		curx[32];
    int		rstart[32],
		rend[32];
    int		rcur[32];
    uint16_t	freq[32];
    uint16_t	rfreq[32];
    uint8_t	ctrl[32];
    uint8_t	rctrl[32];
    int		curvol[32];
    int		pan_l[32],
		pan_r[32];
    int		t1on,
		t2on;
    uint8_t	tctrl;
    uint16_t	t1,
		t2,
		t1l,
		t2l;
    uint8_t	irqstatus,
		irqstatus2;
    uint8_t	adcommand;
    int		waveirqs[32],
		rampirqs[32];
    int		voices;
    uint8_t	dmactrl;

    int32_t	out_l,
		out_r;

    tmrval_t	samp_timer,
		samp_latch;

    uint8_t	*ram;
    uint32_t gus_end_ram;

    int		pos;
    int16_t	buffer[2][SOUNDBUFLEN];

    int		irqnext;

    tmrval_t	timer_1,
		timer_2;

    int		irq,
		dma,
		irq_midi;
    uint16_t base;
    int		latch_enable;

    uint8_t	sb_2xa,
		sb_2xc,
		sb_2xe;
    uint8_t	sb_ctrl;
    int		sb_nmi;

    uint8_t	reg_ctrl;

    uint8_t	ad_status,
		ad_data;
    uint8_t	ad_timer_ctrl;

    uint8_t	midi_ctrl,
		midi_status;
    uint8_t	midi_data;
    int		midi_loopback;

    uint8_t	gp1,
		gp2;
    uint16_t	gp1_addr,
		gp2_addr;

    uint8_t	usrr;

    uint8_t	max_ctrl;

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
    cs423x_t cs423x;
#endif    
} gus_t;


static const int gus_irqs[8] = { -1, 2, 5, 3, 7, 11, 12, 15 };
static const int gus_irqs_midi[8] = { -1, 2, 5, 3, 7, 11, 12, 15 };
static const int gus_dmas[8] = { -1, 1, 3, 5, 6, 7, -1, -1 };
static const int gusfreqs[] = {
    44100, 41160, 38587, 36317, 34300, 32494, 30870, 29400,
    28063, 26843, 25725, 24696, 23746, 22866, 22050, 21289,
    20580, 19916, 19293
};
static double	vol16bit[4096];


static void
poll_irqs(gus_t *dev)
{
    int c;

    dev->irqstatus &= ~0x60;

    for (c = 0; c < 32; c++) {
	if (dev->waveirqs[c]) {
		dev->irqstatus2 = 0x60 | c;
		if (dev->rampirqs[c])
			dev->irqstatus2 |= 0x80;
		dev->irqstatus |= 0x20;
		if (dev->irq != -1)
			picint(1 << dev->irq);
		return;
	}

	if (dev->rampirqs[c]) {
		dev->irqstatus2 = 0xa0 | c;
		dev->irqstatus |= 0x40;
		if (dev->irq != -1)
			picint(1 << dev->irq);
		return;
	}
    }

    dev->irqstatus2 = 0xe0;

    if (!dev->irqstatus && dev->irq != -1)
	picintc(1 << dev->irq);
}


static void
midi_update_int_status(gus_t *dev)
{
    dev->midi_status &= ~MIDI_INT_MASTER;

    if ((dev->midi_ctrl & MIDI_CTRL_TRANSMIT_MASK) == MIDI_CTRL_TRANSMIT && (dev->midi_status & MIDI_INT_TRANSMIT)) {
	dev->midi_status |= MIDI_INT_MASTER;
	dev->irqstatus |= GUS_INT_MIDI_TRANSMIT;
    } else
	dev->irqstatus &= ~GUS_INT_MIDI_TRANSMIT;

    if ((dev->midi_ctrl & MIDI_CTRL_RECEIVE) && (dev->midi_status & MIDI_INT_RECEIVE)) {
	dev->midi_status |= MIDI_INT_MASTER;
	dev->irqstatus |= GUS_INT_MIDI_RECEIVE;
    } else
	dev->irqstatus &= ~GUS_INT_MIDI_RECEIVE;

    if ((dev->midi_status & MIDI_INT_MASTER) && (dev->irq_midi != -1))
	picint(1 << dev->irq_midi);
}


static void
gus_write(uint16_t addr, uint8_t val, priv_t priv)
{
    gus_t *dev = (gus_t *)priv;
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
    uint16_t csioport;
#endif
    int c, d, old;
    uint16_t port;

	if ((addr == 0x388) || (addr == 0x389))
		port = addr;
	else
		port = addr & 0xf0f; /* Bit masking GUS dynamic IO*/

    if (dev->latch_enable && port != 0x20b)
		dev->latch_enable = 0;

    switch (port) {
	case 0x300: /*MIDI control*/
		old = dev->midi_ctrl;
		dev->midi_ctrl = val;

		if ((val & 3) == 3)
			dev->midi_status = 0;
		else if ((old & 3) == 3)
			dev->midi_status |= MIDI_INT_TRANSMIT;
		midi_update_int_status(dev);
		break;

	case 0x301: /*MIDI data*/
		if (dev->midi_loopback) {
			dev->midi_status |= MIDI_INT_RECEIVE;
			dev->midi_data = val;
		}
		else
			dev->midi_status |= MIDI_INT_TRANSMIT;
		break;

	case 0x302: /*Voice select*/
		dev->voice = val & 31;
		break;

	case 0x303: /*Global select*/
		dev->global = val;
		break;

	case 0x304: /*Global low*/
		switch (dev->global) {
			case 0: /*Voice control*/
				dev->ctrl[dev->voice] = val;
				break;

			case 1: /*Frequency control*/
				dev->freq[dev->voice] = (dev->freq[dev->voice] & 0xFF00) | val;
				break;

			case 2: /*Start addr high*/
				dev->startx[dev->voice] = (dev->startx[dev->voice] & 0xF807F) | (val << 7);
				dev->start[dev->voice] = (dev->start[dev->voice] & 0x1F00FFFF) | (val << 16);
				break;

			case 3: /*Start addr low*/
				dev->start[dev->voice] = (dev->start[dev->voice] & 0x1FFFFF00) | val;
				break;

			case 4: /*End addr high*/
				dev->endx[dev->voice] = (dev->endx[dev->voice] & 0xF807F) | (val << 7);
				dev->end[dev->voice] = (dev->end[dev->voice] & 0x1F00FFFF) | (val << 16);
				break;

			case 5: /*End addr low*/
				dev->end[dev->voice] = (dev->end[dev->voice] & 0x1FFFFF00) | val;
				break;

			case 0x6: /*Ramp frequency*/
				dev->rfreq[dev->voice] = (int)((double)((val & 63) * 512) / (double)(1 << (3 * (val >> 6))));
				break;

			case 0x9: /*Current volume*/
				dev->curvol[dev->voice] = dev->rcur[dev->voice] = (dev->rcur[dev->voice] & ~(0xff << 6)) | (val << 6);
				break;

			case 0xA: /*Current addr high*/
				dev->cur[dev->voice] = (dev->cur[dev->voice] & 0x1F00FFFF) | (val << 16);
				dev->curx[dev->voice] = (dev->curx[dev->voice] & 0xF807F00) | ((val << 7) << 8);
				break;
	
			case 0xB: /*Current addr low*/
				dev->cur[dev->voice] = (dev->cur[dev->voice] & 0x1FFFFF00) | val;
				break;

			case 0x42: /*DMA address low*/
				dev->dmaaddr = (dev->dmaaddr & 0xFF000) | (val << 4);
				break;

			case 0x43: /*Address low*/
				dev->addr = (dev->addr & 0xFFF00) | val;
				break;

			case 0x45: /*Timer control*/
				dev->tctrl = val;
				break;
		}
		break;

	case 0x305: /*Global high*/
			switch (dev->global) {
			case 0: /*Voice control*/
				dev->ctrl[dev->voice] = val & 0x7f;

				old = dev->waveirqs[dev->voice];
				dev->waveirqs[dev->voice] = ((val & 0xa0) == 0xa0) ? 1 : 0;
				if (dev->waveirqs[dev->voice] != old)
					poll_irqs(dev);
				break;

			case 1: /*Frequency control*/
				dev->freq[dev->voice] = (dev->freq[dev->voice] & 0xFF) | (val << 8);
				break;

			case 2: /*Start addr high*/
				dev->startx[dev->voice] = (dev->startx[dev->voice] & 0x07FFF) | (val << 15);
				dev->start[dev->voice] = (dev->start[dev->voice] & 0x00FFFFFF) | ((val & 0x1F) << 24);
				break;

			case 3: /*Start addr low*/
				dev->startx[dev->voice] = (dev->startx[dev->voice] & 0xFFF80) | (val & 0x7F);
				dev->start[dev->voice] = (dev->start[dev->voice] & 0x1FFF00FF) | (val << 8);
				break;

			case 4: /*End addr high*/
				dev->endx[dev->voice] = (dev->endx[dev->voice] & 0x07FFF) | (val << 15);
				dev->end[dev->voice] = (dev->end[dev->voice] & 0x00FFFFFF) | ((val & 0x1F) << 24);
				break;

			case 5: /*End addr low*/
				dev->endx[dev->voice] = (dev->endx[dev->voice] & 0xFFF80) | (val & 0x7F);
				dev->end[dev->voice] = (dev->end[dev->voice] & 0x1FFF00FF) | (val << 8);
				break;

			case 0x6: /*Ramp frequency*/
				dev->rfreq[dev->voice] = (int)((double)((val & 63) * (1 << 10)) / (double)(1 << (3 * (val >> 6))));
				break;

			case 0x7: /*Ramp start*/
				dev->rstart[dev->voice] = val << 14;
				break;

			case 0x8: /*Ramp end*/
				dev->rend[dev->voice] = val << 14;
				break;

			case 0x9: /*Current volume*/
				dev->curvol[dev->voice] = dev->rcur[dev->voice] = (dev->rcur[dev->voice] & ~(0xff << 14)) | (val << 14);
				break;

			case 0xA: /*Current addr high*/
				dev->cur[dev->voice] = (dev->cur[dev->voice] & 0x00FFFFFF) | ((val & 0x1F) << 24);
				dev->curx[dev->voice] = (dev->curx[dev->voice] & 0x07FFF00) | ((val << 15) << 8);
				break;

			case 0xB: /*Current addr low*/
				dev->cur[dev->voice] = (dev->cur[dev->voice] & 0x1FFF00FF) | (val << 8);
				dev->curx[dev->voice] = (dev->curx[dev->voice] & 0xFFF8000) | ((val & 0x7F) << 8);
				break;

			case 0xC: /*Pan*/
				dev->pan_l[dev->voice] = 15 - (val & 0xf);
				dev->pan_r[dev->voice] = (val & 0xf);
				break;

			case 0xD: /*Ramp control*/
				old = dev->rampirqs[dev->voice];
				dev->rctrl[dev->voice] = val & 0x7F;
				dev->rampirqs[dev->voice] = ((val & 0xa0) == 0xa0) ? 1 : 0;
				if (dev->rampirqs[dev->voice] != old)
					poll_irqs(dev);
				break;

			case 0xE:
				dev->voices = (val & 63) + 1;
				if (dev->voices > 32) dev->voices = 32;
				if (dev->voices < 14) dev->voices = 14;
				dev->global = val;
				if (dev->voices < 14)
					dev->samp_latch = (int)(TIMER_USEC * (1000000.0 / 44100.0));
				else
					dev->samp_latch = (int)(TIMER_USEC * (1000000.0 / gusfreqs[dev->voices - 14]));
				break;

			case 0x41: /*DMA*/
				if (val & 1 && dev->dma != -1) {
					if (val & 2) {
						c = 0;
						while (c < 65536) {
							int dma_result;
	
							if (val & 0x04) {
								uint32_t gus_addr = (dev->dmaaddr & 0xc0000) | ((dev->dmaaddr & 0x1ffff) << 1);
								d = dev->ram[gus_addr] | (dev->ram[gus_addr + 1] << 8);
								if (val & 0x80)
									d ^= 0x8080;
								dma_result = dma_channel_write(dev->dma, d);
								if (dma_result == DMA_NODATA)
									break;
							} else {
								d = dev->ram[dev->dmaaddr];
								if (val & 0x80)
									d ^= 0x80;
								dma_result = dma_channel_write(dev->dma, d);
								if (dma_result == DMA_NODATA)
									break;
							}
							dev->dmaaddr++;
							dev->dmaaddr &= 0xFFFFF;
							c++;
							if (dma_result & DMA_OVER)
								break;
						}
						dev->dmactrl = val & ~0x40;
						if (val & 0x20)
							dev->irqnext = 1;
					} else {
						c = 0;
						while (c < 65536) {
							d = dma_channel_read(dev->dma);
							if (d == DMA_NODATA)
								break;
							if (val & 0x04) {
								uint32_t gus_addr = (dev->dmaaddr & 0xc0000) | ((dev->dmaaddr & 0x1ffff) << 1);
								if (val & 0x80)
									d ^= 0x8080;
								dev->ram[gus_addr] = d & 0xff;
								dev->ram[gus_addr + 1] = (d >> 8) & 0xff;
							} else {
								if (val & 0x80)
									d ^= 0x80;
								dev->ram[dev->dmaaddr] = d;
							}
							dev->dmaaddr++;
							dev->dmaaddr &= 0xFFFFF;
							c++;
							if (d & DMA_OVER)
								break;
						}
						dev->dmactrl = val & ~0x40;
						if (val & 0x20)
							dev->irqnext = 1;
					}
				}
				break;

			case 0x42: /*DMA address low*/
				dev->dmaaddr = (dev->dmaaddr & 0xFF0) | (val << 12);
				break;

			case 0x43: /*Address low*/
				dev->addr = (dev->addr & 0xF00FF) | (val << 8);
				break;

			case 0x44: /*Address high*/
				dev->addr = (dev->addr & 0xFFFF) | ((val << 16) & 0xF0000);
				break;

			case 0x45: /*Timer control*/
				if (!(val & 4)) dev->irqstatus &= ~4;
				if (!(val & 8)) dev->irqstatus &= ~8;
				if (!(val & 0x20)) {
					dev->ad_status &= ~0x18;
					nmi = 0;
				}
				if (!(val & 0x02)) {
					dev->ad_status &= ~0x01;
					nmi = 0;
				}
				dev->tctrl = val;
				dev->sb_ctrl = val;
				break;

			case 0x46: /*Timer 1*/
				dev->t1 = dev->t1l = val;
				dev->t1on = 1;
				break;

			case 0x47: /*Timer 2*/
				dev->t2 = dev->t2l = val;
				dev->t2on = 1;
				break;

			case 0x4c: /*Reset*/
				dev->reset = val;
				break;
		}
		break;

	case 0x307: /*DRAM access*/
		dev->addr &= 0xfffff;
		if (dev->addr < dev->gus_end_ram)
			dev->ram[dev->addr] = val;
		break;

	case 0x208:
	case 0x388:
		dev->adcommand = val;
		break;

	case 0x389:
		if ((dev->tctrl & GUS_TIMER_CTRL_AUTO) || dev->adcommand != 4) {
			dev->ad_data = val;
			dev->ad_status |= 0x01;
			if (dev->sb_ctrl & 0x02) {
				if (dev->sb_nmi)
					nmi = 1;
				else if (dev->irq != -1)
					picint(1 << dev->irq);
			}
		}
		else if (!(dev->tctrl & GUS_TIMER_CTRL_AUTO) && dev->adcommand == 4) {
			if (val & 0x80) {
				dev->ad_status &= ~0x60;
			}
			else {
				dev->ad_timer_ctrl = val;

				if (val & 0x01)
					dev->t1on = 1;
				else
					dev->t1 = dev->t1l;

				if (val & 0x02)
					dev->t2on = 1;
				else
					dev->t2 = dev->t2l;
			}
		}
		break;

	case 0x200:
		dev->midi_loopback = val & 0x20;
		dev->latch_enable = (val & 0x40) ? 2 : 1;
		break;

	case 0x20b:
		switch (dev->reg_ctrl & 0x07) {
			case 0:
				if (dev->latch_enable == 1)
					dev->dma = gus_dmas[val & 7];
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
				cs423x_setdma(&dev->cs423x, dev->dma);
#endif					
				if (dev->latch_enable == 2) {
					dev->irq = gus_irqs[val & 7];

					if (val & 0x40) {
						if (dev->irq == -1)
							dev->irq = dev->irq_midi = gus_irqs[(val >> 3) & 7];
						else
							dev->irq_midi = dev->irq;
					} else
						dev->irq_midi = gus_irqs_midi[(val >> 3) & 7];
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)		
					cs423x_setirq(&dev->cs423x, dev->irq);
#endif
					dev->sb_nmi = val & 0x80;
				}
				dev->latch_enable = 0;
				break;

			case 1:
				dev->gp1 = val;
				break;

			case 2:
				dev->gp2 = val;
				break;

			case 3:
				dev->gp1_addr = val;
				break;

			case 4:
				dev->gp2_addr = val;
				break;

			case 5:
				dev->usrr = 0;
				break;

			case 6:
				break;
		}
		break;

	case 0x206:
		dev->ad_status |= 0x08;
		if (dev->sb_ctrl & 0x20) {
			if (dev->sb_nmi)
				nmi = 1;
			else if (dev->irq != -1)
				picint(1 << dev->irq);
		}
		break;

	case 0x20a:
		dev->sb_2xa = val;
		break;

	case 0x20c:
		dev->ad_status |= 0x10;
		if (dev->sb_ctrl & 0x20) {
			if (dev->sb_nmi)
				nmi = 1;
			else if (dev->irq != -1)
				picint(1 << dev->irq);
		}
		/*FALLTHOUGH*/

	case 0x20d:
		dev->sb_2xc = val;
		break;

	case 0x20e:
		dev->sb_2xe = val;
		break;

	case 0x20f:
		dev->reg_ctrl = val;
		break;

	case 0x306: case 0x706:
		if (dev->dma >= 4)
			val |= 0x30;
		dev->max_ctrl = (val >> 6) & 1;
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
		if (val & 0x40) {
			if ((val & 0xF) != ((addr >> 4) & 0xF)) { /* Fix me : why is DOS application attempting to relocate the CODEC ? */
				csioport = 0x30c | ((addr >> 4) & 0xf);
				io_removehandler(csioport, 4,
						 cs423x_read,NULL,NULL,
						 cs423x_write,NULL,NULL,&dev->cs423x);
				csioport = 0x30c | ((val & 0xf) << 4);
				io_sethandler(csioport, 4,
					      cs423x_read,NULL,NULL,
					      cs423x_write,NULL,NULL, &dev->cs423x);
			}
		}
#endif
		break;
	}
}


static uint8_t
gus_read(uint16_t addr, priv_t priv)
{
    gus_t *dev = (gus_t *)priv;
    uint8_t val = 0xff;
    uint16_t port;

    if ((addr == 0x388) || (addr == 0x389))
	port = addr;
    else
	port = addr & 0xf0f; /* Bit masking GUS dynamic IO*/

    switch (port) {
	case 0x300: /*MIDI status*/
		val = dev->midi_status;
		break;

	case 0x301: /*MIDI data*/
		val = dev->midi_data;
		dev->midi_status &= ~MIDI_INT_RECEIVE;
		midi_update_int_status(dev);
		break;

	case 0x200:
		val = 0x00;
		break;

	case 0x206: /*IRQ status*/
		val = dev->irqstatus & ~0x10;
		if (dev->ad_status & 0x19)
			val |= 0x10;
		break;

	case 0x20F:
		if (dev->max_ctrl)
			val = 0x02;
		else
			val = 0x00;
		break;

	case 0x302: /*GF1 Page Register*/
		val = dev->voice;
		break;

	case 0x303: /*GF1 Global Register Select*/
		val = dev->global;
		break;

	case 0x304: /*Global low*/
		switch (dev->global) {
		case 0x82: /*Start addr high*/
			val = dev->start[dev->voice] >> 16;
			break;

		case 0x83: /*Start addr low*/
			val = dev->start[dev->voice] & 0xFF;
			break;

		case 0x89: /*Current volume*/
			val = dev->rcur[dev->voice] >> 6;
			break;

		case 0x8A: /*Current addr high*/
			val = dev->cur[dev->voice] >> 16;
			break;

		case 0x8B: /*Current addr low*/
			val = dev->cur[dev->voice] & 0xFF;
			break;

		case 0x8F: /*IRQ status*/
			val = dev->irqstatus2;
			dev->rampirqs[dev->irqstatus2 & 0x1F] = 0;
			dev->waveirqs[dev->irqstatus2 & 0x1F] = 0;
			poll_irqs(dev);
			break;

		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			val = 0xff;
			break;
		}
		break;

	case 0x305: /*Global high*/
		switch (dev->global) {
		case 0x80: /*Voice control*/
			val = dev->ctrl[dev->voice] | (dev->waveirqs[dev->voice] ? 0x80 : 0);
			break;

		case 0x82: /*Start addr high*/
			val = dev->start[dev->voice] >> 24;
			break;

		case 0x83: /*Start addr low*/
			val = dev->start[dev->voice] >> 8;
			break;

		case 0x89: /*Current volume*/
			val = dev->rcur[dev->voice] >> 14;
			break;

		case 0x8A: /*Current addr high*/
			val = dev->cur[dev->voice] >> 24;
			break;

		case 0x8B: /*Current addr low*/
			val = dev->cur[dev->voice] >> 8;
			break;

		case 0x8C: /*Pan*/
			val = dev->pan_r[dev->voice];
			break;

		case 0x8D:
			val = dev->rctrl[dev->voice] | (dev->rampirqs[dev->voice] ? 0x80 : 0);
			break;

		case 0x8F: /*IRQ status*/
			val = dev->irqstatus2;
			dev->rampirqs[dev->irqstatus2 & 0x1F] = 0;
			dev->waveirqs[dev->irqstatus2 & 0x1F] = 0;
			poll_irqs(dev);
			break;

		case 0x41: /*DMA control*/
			val = dev->dmactrl | ((dev->irqstatus & 0x80) ? 0x40 : 0);
			dev->irqstatus &= ~0x80;
			break;

		case 0x45: /*Timer control*/
			val = dev->tctrl;
			break;

		case 0x49: /*Sampling control*/
			val = 0x00;
			break;

		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			val = 0xff;
			break;
		}
		break;

	case 0x306: case 0x706:
		if (dev->max_ctrl)
			val = 0x0a; /* GUS MAX */
		else
			val = 0xff; /*Pre 3.7 - no mixer*/
		break;

	case 0x307: /*DRAM access*/
		dev->addr &= 0xFFFFF;
		if (dev->addr < dev->gus_end_ram)
			val = dev->ram[dev->addr];
		else
			val = 0;
		break;

	case 0x309:
		val = 0x00;
		break;

	case 0x20B:
		switch (dev->reg_ctrl & 0x07) {
		case 1:
			val = dev->gp1;
			break;

		case 2:
			val = dev->gp2;
			break;

		case 3:
			val = (uint8_t)(dev->gp1_addr & 0xff);
			break;

		case 4:
			val = (uint8_t)(dev->gp2_addr & 0xff);
			break;
		}
		break;

	case 0x20C:
		val = dev->sb_2xc;
		if (dev->reg_ctrl & 0x20)
			dev->sb_2xc &= 0x80;
		break;

	case 0x20E:
		val = dev->sb_2xe;
		break;

	case 0x208: /* Timer Control Register */
	case 0x388:
		if (dev->tctrl & GUS_TIMER_CTRL_AUTO)
			val = dev->sb_2xa;
		else {
			val = dev->ad_status & ~(dev->ad_timer_ctrl & 0x60);
			if (val & 0x60)
				val |= 0x80;
		}
		break;

	case 0x209: /* Timer Data */
		dev->ad_status &= ~0x01;
		nmi = 0;
		/*FALLTHROUGH?*/

	case 0x389:/* Adlib port 389 */
		val = dev->ad_data;
		break;

	case 0x20A:
		val = dev->adcommand;
		break;

    }

    return(val);
}


static void
poll_timer_1(priv_t priv)
{
    gus_t *dev = (gus_t *)priv;

    dev->timer_1 += (TIMER_USEC * 80LL);

    if (dev->t1on) {
	dev->t1++;
	if (dev->t1 > 0xFF) {
		dev->t1 = dev->t1l;
		dev->ad_status |= 0x40;
		if (dev->tctrl & 4) {
			if (dev->irq != -1)
				picint(1 << dev->irq);
			dev->ad_status |= 0x04;
			dev->irqstatus |= 0x04;
		}
	}
    }

    if (dev->irqnext) {
	dev->irqnext = 0;
	dev->irqstatus |= 0x80;
	if (dev->irq != -1)
		picint(1 << dev->irq);
    }

    midi_update_int_status(dev);
}


static void
poll_timer_2(priv_t priv)
{
    gus_t *dev = (gus_t *)priv;

    dev->timer_2 += (TIMER_USEC * 320LL);

    if (dev->t2on) {
	dev->t2++;
	if (dev->t2 > 0xFF) {
		dev->t2 = dev->t2l;
		dev->ad_status |= 0x20;
		if (dev->tctrl & 8) {
			if (dev->irq != -1)
				picint(1 << dev->irq);
			dev->ad_status |= 0x02;
			dev->irqstatus |= 0x08;
		}
	}
    }

    if (dev->irqnext) {
	dev->irqnext = 0;
	dev->irqstatus |= 0x80;
	if (dev->irq != -1)
		picint(1 << dev->irq);
    }
}


static void
gus_update(gus_t *dev)
{
    for (; dev->pos < sound_pos_global; dev->pos++) {
	if (dev->out_l < -32768)
		dev->buffer[0][dev->pos] = -32768;
	else if (dev->out_l > 32767)
		dev->buffer[0][dev->pos] = 32767;
	else
		dev->buffer[0][dev->pos] = dev->out_l;
	if (dev->out_r < -32768)
		dev->buffer[1][dev->pos] = -32768;
	else if (dev->out_r > 32767)
		dev->buffer[1][dev->pos] = 32767;
	else
		dev->buffer[1][dev->pos] = dev->out_r;
    }
}


static void
poll_wave(priv_t priv)
{
    gus_t *dev = (gus_t *)priv;
    uint32_t addr;
    int d;
    int16_t v;
    int32_t vl;
    int update_irqs = 0;

    gus_update(dev);

    dev->samp_timer += dev->samp_latch;

    dev->out_l = dev->out_r = 0;

    if ((dev->reset & 3) != 3)
	return;

    for (d = 0; d < 32; d++) {
	if (!(dev->ctrl[d] & 3)) {
		if (dev->ctrl[d] & 4) {
			addr = dev->cur[d] >> 9;
			addr = (addr & 0xC0000) | ((addr << 1) & 0x3FFFE);

			if (!(dev->freq[d] >> 10)) {	/*Interpolate*/
				vl = (int16_t)(int8_t)((dev->ram[(addr + 1) & 0xFFFFF] ^ 0x80) - 0x80) * (511 - (dev->cur[d] & 511));
				vl += (int16_t)(int8_t)((dev->ram[(addr + 3) & 0xFFFFF] ^ 0x80) - 0x80) * (dev->cur[d] & 511);
				v = vl >> 9;
			} else
				v = (int16_t)(int8_t)((dev->ram[(addr + 1) & 0xFFFFF] ^ 0x80) - 0x80);
		} else {
			if (!(dev->freq[d] >> 10)) {	/*Interpolate*/
				vl = ((int8_t)((dev->ram[(dev->cur[d] >> 9) & 0xFFFFF] ^ 0x80) - 0x80)) * (511 - (dev->cur[d] & 511));
				vl += ((int8_t)((dev->ram[((dev->cur[d] >> 9) + 1) & 0xFFFFF] ^ 0x80) - 0x80)) * (dev->cur[d] & 511);
				v = vl >> 9;
			} else
				v = (int16_t)(int8_t)((dev->ram[(dev->cur[d] >> 9) & 0xFFFFF] ^ 0x80) - 0x80);
		}

		if ((dev->rcur[d] >> 14) > 4095)
			v = (int16_t)((float)v * 24.0 * vol16bit[4095]);
		else
			v = (int16_t)((float)v * 24.0 * vol16bit[(dev->rcur[d] >> 10) & 4095]);

		dev->out_l += (v * dev->pan_l[d]) / 7;
		dev->out_r += (v * dev->pan_r[d]) / 7;

		if (dev->ctrl[d] & 0x40) {
			dev->cur[d] -= (dev->freq[d] >> 1);
			if (dev->cur[d] <= dev->start[d]) {
				int diff = dev->start[d] - dev->cur[d];

				if (dev->ctrl[d] & 8) {
					if (dev->ctrl[d] & 0x10)
						dev->ctrl[d] ^= 0x40;
					dev->cur[d] = (dev->ctrl[d] & 0x40) ? (dev->end[d] - diff) : (dev->start[d] + diff);
				} else if (!(dev->rctrl[d] & 4)) {
					dev->ctrl[d] |= 1;
					dev->cur[d] = (dev->ctrl[d] & 0x40) ? dev->end[d] : dev->start[d];
				}

				if ((dev->ctrl[d] & 0x20) && !dev->waveirqs[d]) {
					dev->waveirqs[d] = 1;
					update_irqs = 1;
				}
			}
		} else {
			dev->cur[d] += (dev->freq[d] >> 1);

			if (dev->cur[d] >= dev->end[d]) {
				int diff = dev->cur[d] - dev->end[d];

				if (dev->ctrl[d] & 8) {
					if (dev->ctrl[d] & 0x10)
						dev->ctrl[d] ^= 0x40;
					dev->cur[d] = (dev->ctrl[d] & 0x40) ? (dev->end[d] - diff) : (dev->start[d] + diff);
				} else if (!(dev->rctrl[d] & 4)) {
					dev->ctrl[d] |= 1;
					dev->cur[d] = (dev->ctrl[d] & 0x40) ? dev->end[d] : dev->start[d];
				}

				if ((dev->ctrl[d] & 0x20) && !dev->waveirqs[d]) {
					dev->waveirqs[d] = 1;
					update_irqs = 1;
				}
			}
		}
	}

	if (!(dev->rctrl[d] & 3)) {
		if (dev->rctrl[d] & 0x40) {
			dev->rcur[d] -= dev->rfreq[d];
			if (dev->rcur[d] <= dev->rstart[d]) {
				int diff = dev->rstart[d] - dev->rcur[d];

				if (!(dev->rctrl[d] & 8)) {
					dev->rctrl[d] |= 1;
					dev->rcur[d] = (dev->rctrl[d] & 0x40) ? dev->rstart[d] : dev->rend[d];
				} else {
					if (dev->rctrl[d] & 0x10)
						dev->rctrl[d] ^= 0x40;
					dev->rcur[d] = (dev->rctrl[d] & 0x40) ? (dev->rend[d] - diff) : (dev->rstart[d] + diff);
				}

				if ((dev->rctrl[d] & 0x20) && !dev->rampirqs[d]) {
					dev->rampirqs[d] = 1;
					update_irqs = 1;
				}
			}
		} else {
			dev->rcur[d] += dev->rfreq[d];
			if (dev->rcur[d] >= dev->rend[d]) {
				int diff = dev->rcur[d] - dev->rend[d];

				if (!(dev->rctrl[d] & 8)) {
					dev->rctrl[d] |= 1;
					dev->rcur[d] = (dev->rctrl[d] & 0x40) ? dev->rstart[d] : dev->rend[d];
				} else {
					if (dev->rctrl[d] & 0x10)
						dev->rctrl[d] ^= 0x40;
					dev->rcur[d] = (dev->rctrl[d] & 0x40) ? (dev->rend[d] - diff) : (dev->rstart[d] + diff);
				}

				if ((dev->rctrl[d] & 0x20) && !dev->rampirqs[d]) {
					dev->rampirqs[d] = 1;
					update_irqs = 1;
				}
			}
		}
	}
    }

    if (update_irqs)
	poll_irqs(dev);
}


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    gus_t *dev = (gus_t *)priv;
    int c;

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)  
    if (dev->max_ctrl)
	cs423x_update(&dev->cs423x);
#endif	
    gus_update(dev);

    for (c = 0; c < len * 2; c++) {
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)    
	if (dev->max_ctrl)
		buffer[c] += (int32_t)(dev->cs423x.buffer[c] / 2);
#endif		
	buffer[c] += (int32_t)dev->buffer[c & 1][c >> 1];
    }

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)    
    if (dev->max_ctrl)
	dev->cs423x.pos = 0;
#endif	

    dev->pos = 0;
}


static priv_t
gus_init(const device_t *info, UNUSED(void *parent))
{
    gus_t *dev;
    double out = 1.0;
    int c;
    uint8_t gus_ram = device_get_config_int("gus_ram");

    dev = (gus_t *)mem_alloc(sizeof(gus_t));
    memset(dev, 0x00, sizeof(gus_t));

    dev->gus_end_ram = 1 << (18 + gus_ram);
    dev->ram = (uint8_t *)mem_alloc(dev->gus_end_ram);
    memset(dev->ram, 0x00, (dev->gus_end_ram));

    for (c = 0; c < 32; c++) {
	dev->ctrl[c] = 1;
	dev->rctrl[c] = 1;
	dev->rfreq[c] = 63 * 512;
    }

    for (c = 4095; c >= 0; c--) {
	vol16bit[c] = out;
	out /= 1.002709201;		/* 0.0235 dB Steps */
    }

    //   DEBUG("GUS: top volume %f %f %f %f\n",vol16bit[4095],vol16bit[3800],vol16bit[3000],vol16bit[2048]);

    dev->voices = 14;

    dev->samp_timer = dev->samp_latch = (tmrval_t)(TIMER_USEC * (1000000.0 / 44100.0));

    dev->t1l = dev->t2l = 0xff;

    dev->base = device_get_config_hex16("base");

    switch(info->local) {
	case 0:		/* Standard GUS */
		io_sethandler(dev->base, 16,
			      gus_read,NULL,NULL, gus_write,NULL,NULL, dev);
		io_sethandler(0x0100+dev->base, 16,
			      gus_read,NULL,NULL, gus_write,NULL,NULL, dev);
		io_sethandler(0x0506+dev->base, 1,
			      gus_read,NULL,NULL, gus_write,NULL,NULL, dev);
		io_sethandler(0x0388, 2,
			      gus_read,NULL,NULL, gus_write,NULL,NULL, dev);

		break;

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
	case 1:		/* GUS MAX */
		cs423x_init(&dev->cs423x);
		cs423x_setirq(&dev->cs423x, 5); /*Default irq and dma from GUS SDK*/
		cs423x_setdma(&dev->cs423x, 3);

		io_sethandler(dev->base, 16,
			      gus_read,NULL,NULL, gus_write,NULL, NULL, dev);
		io_sethandler(0x0100+dev->base, 9,
			      gus_read,NULL,NULL, gus_write,NULL, NULL, dev);
		io_sethandler(0x0506+dev->base, 1,
			      gus_read,NULL,NULL, gus_write,NULL, NULL, dev);
		io_sethandler(0x0388, 2,
			      gus_read,NULL,NULL, gus_write,NULL, NULL, dev);
		io_sethandler(0x10C+dev->base, 4,
			      cs423x_read,NULL,NULL, cs423x_write,NULL,NULL, &dev->cs423x);

		break;
#endif
    }

    timer_add(poll_wave, (priv_t)dev, &dev->samp_timer, TIMER_ALWAYS_ENABLED);
    timer_add(poll_timer_1, (priv_t)dev, &dev->timer_1, TIMER_ALWAYS_ENABLED);
    timer_add(poll_timer_2, (priv_t)dev, &dev->timer_2, TIMER_ALWAYS_ENABLED);

    sound_add_handler(get_buffer, (priv_t)dev);

    return((priv_t)dev);
}


static void
gus_close(priv_t priv)
{
    gus_t *dev = (gus_t *)priv;

    if (dev->ram != NULL)
	free(dev->ram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    gus_t *dev = (gus_t *)priv;

    if (dev->voices < 14)
	dev->samp_latch = (int)(TIMER_USEC * (1000000.0 / 44100.0));
    else
	dev->samp_latch = (int)(TIMER_USEC * (1000000.0 / gusfreqs[dev->voices - 14]));

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
    if (dev->max_ctrl)
	cs423x_speed_changed(&dev->cs423x);
#endif
}

static const device_config_t gus_config[] = {
	{
		"base", "Address", CONFIG_HEX16, "", 0x220,
		{
			{
				"210H", 0x210
			},
			{
				"220H", 0x220
			},
			{
				"230H", 0x230
			},
			{
				"240H", 0x240
			},
			{
				"250H", 0x250
			},
			{
				"260H", 0x260
			},
		},
	},
    {
		 "gus_ram", "Onboard RAM", CONFIG_SELECTION, "", 0,
		{
			{
				"256 KB", 0
			},
			{
				"512 KB", 1
			},
			{
				"1 MB", 2
			},
			{
				NULL
			}
		}
	 },
	{
			"", "", -1
	}
};

const device_t gus_device = {
    "Gravis UltraSound",
    DEVICE_ISA,
    0,
    NULL,
    gus_init, gus_close, NULL,
    NULL,
    speed_changed, NULL, NULL,
    gus_config
};

#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
static const device_config_t gus_max_config[] = {
	{
		"base", "Address", CONFIG_HEX16, "", 0x220,
		{
			{
				"210H", 0x210
			},
			{
				"220H", 0x220
			},
			{
				"230H", 0x230
			},
			{
				"240H", 0x240
			},
			{
				"250H", 0x250
			},
			{
				"260H", 0x260
			},
		},
	},
	{
		"gus_ram", "Onboard RAM", CONFIG_SELECTION, "", 1,
		{
			{
				"512 KB", 1
			},
			{
				"1 MB", 2
			},
			{
				NULL
			}
		}
	},
	{
		"", "", -1
	}
};

const device_t gusmax_device = {
    "Gravis UltraSound MAX",
    DEVICE_ISA,
    1,
    NULL,
    gus_init, gus_close, NULL,
    NULL,
    speed_changed, NULL, NULL,
    gus_max_config
};
#endif
