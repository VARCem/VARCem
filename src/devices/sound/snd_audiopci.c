/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the AudioPCI sound device.
 *
 * Version:	@(#)snd_audiopci.c	1.0.21	2020/02/03
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../cpu/cpu.h"		/* for the debugging stuff */
#include "../../io.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/nmi.h"
#include "../system/pci.h"
#include "midi.h"
#include "snd_mpu401.h"
#include "sound.h"


#define N 16

#define ES1371_NCoef 91

static float low_fir_es1371_coef[ES1371_NCoef];

typedef struct {

    mpu_t mpu;
    
    uint8_t pci_command, pci_serr;

    uint32_t base_addr;

    uint8_t int_line;

    uint16_t pmcsr;

    uint32_t int_ctrl;
    uint32_t int_status;

    uint32_t legacy_ctrl;

    int mem_page;

    uint32_t si_cr;

    uint32_t sr_cir;
    uint16_t sr_ram[128];

    uint8_t uart_ctrl;
    uint8_t uart_status;

    uint16_t codec_regs[64];
    uint32_t codec_ctrl;

    struct {
	uint32_t addr, addr_latch;
	uint16_t count, size;

	uint16_t samp_ct;
	int curr_samp_ct;

	tmrval_t time, latch;

	uint32_t vf, ac;

	int16_t buffer_l[64], buffer_r[64];
	int buffer_pos, buffer_pos_end;

	int filtered_l[32], filtered_r[32];
	int f_pos;

	int16_t out_l, out_r;

	int32_t vol_l, vol_r;
    } dac[2], adc;

    tmrval_t dac_latch, dac_time;

    int master_vol_l, master_vol_r;

    int card;

    int pos;
    int16_t buffer[SOUNDBUFLEN * 2];
} es1371_t;


#define LEGACY_SB_ADDR			(1<<29)
#define LEGACY_SSCAPE_ADDR_SHIFT	27
#define LEGACY_CODEC_ADDR_SHIFT		25
#define LEGACY_FORCE_IRQ		(1<<24)
#define LEGACY_CAPTURE_SLAVE_DMA	(1<<23)
#define LEGACY_CAPTURE_SLAVE_PIC	(1<<22)
#define LEGACY_CAPTURE_MASTER_DMA	(1<<21)
#define LEGACY_CAPTURE_MASTER_PIC	(1<<20)
#define LEGACY_CAPTURE_ADLIB		(1<<19)
#define LEGACY_CAPTURE_SB		(1<<18)
#define LEGACY_CAPTURE_CODEC		(1<<17)
#define LEGACY_CAPTURE_SSCAPE		(1<<16)
#define LEGACY_EVENT_SSCAPE		(0<<8)
#define LEGACY_EVENT_CODEC		(1<<8)
#define LEGACY_EVENT_SB			(2<<8)
#define LEGACY_EVENT_ADLIB		(3<<8)
#define LEGACY_EVENT_MASTER_PIC		(4<<8)
#define LEGACY_EVENT_MASTER_DMA		(5<<8)
#define LEGACY_EVENT_SLAVE_PIC		(6<<8)
#define LEGACY_EVENT_SLAVE_DMA		(7<<8)
#define LEGACY_EVENT_MASK		(7<<8)
#define LEGACY_EVENT_ADDR_SHIFT		3
#define LEGACY_EVENT_ADDR_MASK		(0x1f<<3)
#define LEGACY_EVENT_TYPE_RW		(1<<2)
#define LEGACY_INT			(1<<0)

#define SRC_RAM_WE			(1<<24)

#define CODEC_READ			(1<<23)
#define CODEC_READY			(1<<31)

#define INT_DAC1_EN			(1<<6)
#define INT_DAC2_EN			(1<<5)
#define INT_UART_EN			(1<<3)

#define SI_P2_INTR_EN			(1<<9)
#define SI_P1_INTR_EN			(1<<8)

#define INT_STATUS_INTR			(1<<31)
#define INT_STATUS_UART			(1<<3)
#define INT_STATUS_DAC1			(1<<2)
#define INT_STATUS_DAC2			(1<<1)

#define UART_CTRL_RXINTEN		(1<<7)
#define UART_CTRL_TXINTEN		(1<<5)

#define UART_STATUS_RXINT		(1<<7)
#define UART_STATUS_TXINT		(1<<2)
#define UART_STATUS_TXRDY		(1<<1)
#define UART_STATUS_RXRDY		(1<<0)

#define FORMAT_MONO_8			0
#define FORMAT_STEREO_8			1
#define FORMAT_MONO_16			2
#define FORMAT_STEREO_16		3

static const int32_t codec_attn[]= {
      25,   32,    41,    51,    65,    82,   103,   130,
     164,  206,   260,   327,   412,   519,   653,   822,
    1036, 1304,  1641,  2067,  2602,  3276,  4125,  5192,
    6537, 8230, 10362, 13044, 16422, 20674, 26027, 32767
};


static void es1371_fetch(es1371_t *dev, int dac_nr);
static void update_legacy(es1371_t *dev);


static void
es1371_update_irqs(es1371_t *dev)
{
    int irq = 0;
	
    if ((dev->int_status & INT_STATUS_DAC1) && (dev->si_cr & SI_P1_INTR_EN))
	irq = 1;
    if ((dev->int_status & INT_STATUS_DAC2) && (dev->si_cr & SI_P2_INTR_EN))
	irq = 1;
	
    /* MIDI input is unsupported for now */
    if ((dev->int_status & INT_STATUS_UART) && (dev->uart_status & UART_STATUS_TXINT))
	irq = 1;

    if (irq)
	dev->int_status |= INT_STATUS_INTR;
    else
	dev->int_status &= ~INT_STATUS_INTR;

    if (dev->legacy_ctrl & LEGACY_FORCE_IRQ)
	irq = 1;
	
    if (irq) {
	pci_set_irq(dev->card, PCI_INTA);
	DBGLOG(1, "Raise IRQ\n");
    } else {
	pci_clear_irq(dev->card, PCI_INTA);
	DBGLOG(1, "Drop IRQ\n");
    }
}


static uint8_t
es1371_inb(uint16_t port, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
    uint8_t ret = 0;
	
    switch (port & 0x3f) {
	case 0x00:
		ret = dev->int_ctrl & 0xff;
		break;

	case 0x01:
		ret = (dev->int_ctrl >> 8) & 0xff;
		break;

	case 0x02:
		ret = (dev->int_ctrl >> 16) & 0xff;
		break;

	case 0x03:
		ret = (dev->int_ctrl >> 24) & 0xff;
		break;

	case 0x04:
		ret = dev->int_status & 0xff;
		break;

	case 0x05:
		ret = (dev->int_status >> 8) & 0xff;
		break;

	case 0x06:
		ret = (dev->int_status >> 16) & 0xff;
		break;

	case 0x07:
		ret = (dev->int_status >> 24) & 0xff;
		break;

	case 0x09:
		ret = dev->uart_status & 0xc7;
		break;
		
	case 0x0c:
		ret = dev->mem_page;
		break;
		
	case 0x1a:
		ret = dev->legacy_ctrl >> 16;
		break;

	case 0x1b:
		ret = dev->legacy_ctrl >> 24;
		break;
		
	case 0x20:
		ret = dev->si_cr & 0xff;
		break;

	case 0x21:
		ret = dev->si_cr >> 8;
		break;

	case 0x22:
		ret = (dev->si_cr >> 16) | 0x80;
		break;

	case 0x23:		
		ret = 0xff;
		break;
		
	default:
		DEBUG("Bad es1371_inb: port=%04x\n", port);
    }

    DBGLOG(2, "es1371_inb: port=%04x ret=%02x\n", port, ret);

    return ret;
}


static uint16_t
es1371_inw(uint16_t port, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
    uint16_t ret = 0;
	
    switch (port & 0x3e) {
	case 0x00:
		ret = dev->int_ctrl & 0xffff;
		break;

	case 0x02:
		ret = (dev->int_ctrl >> 16) & 0xffff;
		break;

	case 0x18:
		ret = dev->legacy_ctrl & 0xffff;
		DEBUG("Read legacy ctrl %04x\n", ret);
		break;

	case 0x26:
		ret = dev->dac[0].curr_samp_ct;
		break;

	case 0x2a:
		ret = dev->dac[1].curr_samp_ct;
		break;
		
	case 0x36:
		switch (dev->mem_page) {
			case 0xc:
				ret = dev->dac[0].count;
				break;
			
			default:
				DEBUG("Bad es1371_inw: mem_page=%x port=%04x\n", dev->mem_page, port);
		}
		break;

	case 0x3e:
		switch (dev->mem_page) {
			case 0xc:
				ret = dev->dac[1].count;
				break;
			
			default:
				DEBUG("Bad es1371_inw: mem_page=%x port=%04x\n", dev->mem_page, port);
		}
		break;

	default:
		DEBUG("Bad es1371_inw: port=%04x\n", port);
    }

    DBGLOG(2, "es1371_inw: port=%04x ret=%04x %04x:%08x\n", port, ret, CS,cpu_state.pc);
    return ret;
}


static uint32_t
es1371_inl(uint16_t port, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
    uint32_t ret = 0;

    switch (port & 0x3c) {
	case 0x00:
		ret = dev->int_ctrl;
		break;

	case 0x04:
		ret = dev->int_status;
		break;
		
	case 0x10:
		ret = dev->sr_cir & ~0xffff;
		ret |= dev->sr_ram[dev->sr_cir >> 25];
		break;
		
	case 0x14:
		ret = dev->codec_ctrl & 0x00ff0000;
		ret |= dev->codec_regs[(dev->codec_ctrl >> 16) & 0x3f];
		ret |= CODEC_READY;
		break;

	case 0x30:
		switch (dev->mem_page) {
			case 0xe: case 0xf:
			DEBUG("ES1371 0x30 read UART FIFO: val=%x \n", ret & 0xff);
				break;
		}
		break;
		
	case 0x34:
		switch (dev->mem_page) {
			case 0xc:
				ret = dev->dac[0].size | (dev->dac[0].count << 16);
				break;
			
			case 0xd:
				ret = dev->adc.size | (dev->adc.count << 16);
				break;
				
			case 0xe: case 0xf:
			DEBUG("ES1371 0x34 read UART FIFO: val=%x \n", ret & 0xff);
				break;

			default:
				DEBUG("Bad es1371_inl: mem_page=%x port=%04x\n", dev->mem_page, port);
		}
		break;

	case 0x38:
		switch (dev->mem_page) {
			case 0xe: case 0xf:
			DEBUG("ES1371 0x38 read UART FIFO: val=%x \n", ret & 0xff);
				break;
		}
		break;
		
	case 0x3c:
		switch (dev->mem_page) {
			case 0xc:
				ret = dev->dac[1].size | (dev->dac[1].count << 16);
				break;
				
			case 0xe: case 0xf:
			DEBUG("ES1371 0x3c read UART FIFO: val=%x \n", ret & 0xff);
				break;
						
			default:
				DEBUG("Bad es1371_inl: mem_page=%x port=%04x\n", dev->mem_page, port);
		}
		break;

	default:
		DEBUG("Bad es1371_inl: port=%04x\n", port);
    }

    DBGLOG(2, "es1371_inl: port=%04x ret=%08x  %08x\n", port, ret, cpu_state.pc);

    return ret;
}


static void
es1371_outb(uint16_t port, uint8_t val, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    DBGLOG(2, "es1371_outb: port=%04x val=%02x %04x:%08x\n", port, val, cs, cpu_state.pc);

    switch (port & 0x3f) {
	case 0x00:
		if (!(dev->int_ctrl & INT_DAC1_EN) && (val & INT_DAC1_EN)) {
			dev->dac[0].addr = dev->dac[0].addr_latch;
			dev->dac[0].buffer_pos = 0;
			dev->dac[0].buffer_pos_end = 0;
			es1371_fetch(dev, 0);
		}
		if (!(dev->int_ctrl & INT_DAC2_EN) && (val & INT_DAC2_EN)) {
			dev->dac[1].addr = dev->dac[1].addr_latch;
			dev->dac[1].buffer_pos = 0;
			dev->dac[1].buffer_pos_end = 0;
			es1371_fetch(dev, 1);
		}
		dev->int_ctrl = (dev->int_ctrl & 0xffffff00) | val;
		break;

	case 0x01:
		dev->int_ctrl = (dev->int_ctrl & 0xffff00ff) | (val << 8);
		break;

	case 0x02:
		dev->int_ctrl = (dev->int_ctrl & 0xff00ffff) | (val << 16);
		break;

	case 0x03:
		dev->int_ctrl = (dev->int_ctrl & 0x00ffffff) | (val << 24);
		break;
		
	case 0x08:
		midi_write(val);
		break;
	
	case 0x09:
		dev->uart_ctrl = val & 0xe3;
		break;
		
	case 0x0c:
		dev->mem_page = val & 0xf;
		break;

	case 0x18:
		dev->legacy_ctrl |= LEGACY_INT;
		nmi = 0;
		break;

	case 0x1a:
		dev->legacy_ctrl = (dev->legacy_ctrl & 0xff00ffff) | (val << 16);
		update_legacy(dev);
		break;

	case 0x1b:
		dev->legacy_ctrl = (dev->legacy_ctrl & 0x00ffffff) | (val << 24);
		es1371_update_irqs(dev);
		update_legacy(dev);
		break;
		
	case 0x20:
		dev->si_cr = (dev->si_cr & 0xffff00) | val;
		break;

	case 0x21:
		dev->si_cr = (dev->si_cr & 0xff00ff) | (val << 8);
		if (!(dev->si_cr & SI_P1_INTR_EN))
			dev->int_status &= ~INT_STATUS_DAC1;
		if (!(dev->si_cr & SI_P2_INTR_EN))
			dev->int_status &= ~INT_STATUS_DAC2;
		es1371_update_irqs(dev);
		break;

	case 0x22:
		dev->si_cr = (dev->si_cr & 0x00ffff) | (val << 16);
		break;
		
	default:
		DEBUG("Bad es1371_outb: port=%04x val=%02x\n", port, val);
    }
}


static void
es1371_outw(uint16_t port, uint16_t val, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    DBGLOG(2, "es1371_outw: port=%04x val=%04x\n", port, val);

    switch (port & 0x3f) {
	case 0x0c:
		dev->mem_page = val & 0xf;
		break;

	case 0x24:
		dev->dac[0].samp_ct = val;
		break;

	case 0x28:
		dev->dac[1].samp_ct = val;
		break;
		
	default:
		DEBUG("Bad es1371_outw: port=%04x val=%04x\n", port, val);
    }
}


static void
es1371_outl(uint16_t port, uint32_t val, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    DBGLOG(2, "es1371_outl: port=%04x val=%08x %04x:%08x\n", port, val, CS, cpu_state.pc);

    switch (port & 0x3f) {
	case 0x04:
		break;
		
	case 0x0c:
		dev->mem_page = val & 0xf;
		break;

	case 0x10:
		dev->sr_cir = val;
		if (dev->sr_cir & SRC_RAM_WE) {
			DBGLOG(1, "Write SR RAM %02x %04x\n", dev->sr_cir >> 25, val & 0xffff);
			dev->sr_ram[dev->sr_cir >> 25] = val & 0xffff;
			switch (dev->sr_cir >> 25) {
				case 0x71:
					dev->dac[0].vf = (dev->dac[0].vf & ~0x1f8000) | ((val & 0xfc00) << 5);
					dev->dac[0].ac = (dev->dac[0].ac & ~0x7f8000) | ((val & 0x00ff) << 15);
                                	dev->dac[0].f_pos = 0;
					break;

				case 0x72:
					dev->dac[0].ac = (dev->dac[0].ac & ~0x7fff) | (val & 0x7fff);
					break;				

				case 0x73:
					dev->dac[0].vf = (dev->dac[0].vf & ~0x7fff) | (val & 0x7fff);
					break;

				case 0x75:
					dev->dac[1].vf = (dev->dac[1].vf & ~0x1f8000) | ((val & 0xfc00) << 5);
					dev->dac[1].ac = (dev->dac[1].ac & ~0x7f8000) | ((val & 0x00ff) << 15);
                                	dev->dac[1].f_pos = 0;
					break;

				case 0x76:
					dev->dac[1].ac = (dev->dac[1].ac & ~0x7fff) | (val & 0x7fff);
					break;				

				case 0x77:
					dev->dac[1].vf = (dev->dac[1].vf & ~0x7fff) | (val & 0x7fff);
					break;
				
				case 0x7c:
					dev->dac[0].vol_l = (int32_t)(int16_t)(val & 0xffff);
					break;

				case 0x7d:
					dev->dac[0].vol_r = (int32_t)(int16_t)(val & 0xffff);
					break;

				case 0x7e:
					dev->dac[1].vol_l = (int32_t)(int16_t)(val & 0xffff);
					break;

				case 0x7f:
					dev->dac[1].vol_r = (int32_t)(int16_t)(val & 0xffff);
					break;
			}
		}
		break;

	case 0x14:
		dev->codec_ctrl = val;
		if (!(val & CODEC_READ)) {
			DBGLOG(1, "Write codec %02x %04x\n", (val >> 16) & 0x3f, val & 0xffff);
			dev->codec_regs[(val >> 16) & 0x3f] = val & 0xffff;
			switch ((val >> 16) & 0x3f) {
				case 0x02: /*Master volume*/
					if (val & 0x8000)
						dev->master_vol_l = dev->master_vol_r = 0;
					else {
						if (val & 0x2000)
							dev->master_vol_l = codec_attn[0];
						else
							dev->master_vol_l = codec_attn[0x1f - ((val >> 8) & 0x1f)];
						if (val & 0x20)
							dev->master_vol_r = codec_attn[0];
						else					
							dev->master_vol_r = codec_attn[0x1f - (val & 0x1f)];
					}
					break;

				case 0x12: /*CD volume*/
					if (val & 0x8000)
						sound_cd_set_volume(0, 0);
					else
						sound_cd_set_volume(codec_attn[0x1f - ((val >> 8) & 0x1f)] * 2, codec_attn[0x1f - (val & 0x1f)] * 2);
					break;
			}       
		}
		break;
		
	case 0x24:
		dev->dac[0].samp_ct = val & 0xffff;
		break;

	case 0x28:
		dev->dac[1].samp_ct = val & 0xffff;
		break;

	case 0x30:
		switch (dev->mem_page) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
			case 0x8: case 0x9: case 0xa: case 0xb:
				break;
			
			case 0xc:
				dev->dac[0].addr_latch = val;
				DBGLOG(1, "DAC1 addr %08x\n", val);
				break;
				
			case 0xe: case 0xf:
			DEBUG("ES1371 0x30 write UART FIFO: val=%x \n", val & 0xff);
				break;
			
			default:
				DEBUG("Bad es1371_outl: mem_page=%x port=%04x val=%08x\n", dev->mem_page, port, val);
		}
		break;

	case 0x34:
		switch (dev->mem_page) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
			case 0x8: case 0x9: case 0xa: case 0xb:
				break;

			case 0xc:
				dev->dac[0].size = val & 0xffff;
				dev->dac[0].count = val >> 16;
				break;
			
			case 0xd:
				dev->adc.size = val & 0xffff;
				dev->adc.count = val >> 16;
				break;
				
			case 0xe: case 0xf:
			DEBUG("ES1371 0x34 write UART FIFO: val=%x \n", val & 0xff);
				break;

			default:
				DEBUG("Bad es1371_outl: mem_page=%x port=%04x val=%08x\n", dev->mem_page, port, val);
		}
		break;

	case 0x38:
		switch (dev->mem_page) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
			case 0x8: case 0x9: case 0xa: case 0xb:
				break;

			case 0xc:
				dev->dac[1].addr_latch = val;
				break;
			
			case 0xd:
				break;
		
			case 0xe: case 0xf:
			DEBUG("ES1371 0x38 write UART FIFO: val=%x \n", val & 0xff);
				break;

			default:
				DEBUG("Bad es1371_outl: mem_page=%x port=%04x val=%08x\n", dev->mem_page, port, val);
		}
		break;

	case 0x3c:
		switch (dev->mem_page) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
			case 0x8: case 0x9: case 0xa: case 0xb:
				break;

			case 0xc:
				dev->dac[1].size = val & 0xffff;
				dev->dac[1].count = val >> 16;
				break;
			
			case 0xe: case 0xf:
			DEBUG("ES1371 0x3c write UART FIFO: val=%x \n", val & 0xff);
				break;

			default:
				DEBUG("Bad es1371_outl: mem_page=%x port=%04x val=%08x\n", dev->mem_page, port, val);
		}
		break;

	default:
		DEBUG("Bad es1371_outl: port=%04x val=%08x\n", port, val);
    }
}


static void
capture_event(priv_t priv, int type, int rw, uint16_t port)
{
    es1371_t *dev = (es1371_t *)priv;

    dev->legacy_ctrl &= ~(LEGACY_EVENT_MASK | LEGACY_EVENT_ADDR_MASK);
    dev->legacy_ctrl |= type;

    if (rw)	
	dev->legacy_ctrl |= LEGACY_EVENT_TYPE_RW;
    else
	dev->legacy_ctrl &= ~LEGACY_EVENT_TYPE_RW;
    dev->legacy_ctrl |= ((port << LEGACY_EVENT_ADDR_SHIFT) & LEGACY_EVENT_ADDR_MASK);
    dev->legacy_ctrl &= ~LEGACY_INT;

    nmi = 1;

    DBGLOG(1, "Event! %s %04x\n", rw ? "write" : "read", port);
}


static void
cap_write_sscape(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SSCAPE, 1, port);
}
static void
cap_write_codec(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_CODEC, 1, port);
}
static void
cap_write_sb(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SB, 1, port);
}
static void
cap_write_adlib(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_ADLIB, 1, port);
}
static void
cap_write_master_pic(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_MASTER_PIC, 1, port);
}
static void
cap_write_master_dma(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_MASTER_DMA, 1, port);
}
static void
cap_write_slave_pic(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SLAVE_PIC, 1, port);
}
static void
cap_write_slave_dma(uint16_t port, uint8_t val, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SLAVE_DMA, 1, port);
}

static uint8_t
cap_read_sscape(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SSCAPE, 0, port);
    return 0xff;
}
static uint8_t
cap_read_codec(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_CODEC, 0, port);
    return 0xff;
}
static uint8_t
cap_read_sb(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SB, 0, port);
    return 0xff;
}
static uint8_t
cap_read_adlib(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_ADLIB, 0, port);
    return 0xff;
}
static uint8_t
cap_read_master_pic(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_MASTER_PIC, 0, port);
    return 0xff;
}
static uint8_t
cap_read_master_dma(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_MASTER_DMA, 0, port);
    return 0xff;
}
static uint8_t
cap_read_slave_pic(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SLAVE_PIC, 0, port);
    return 0xff;
}
static uint8_t
cap_read_slave_dma(uint16_t port, priv_t priv)
{
    capture_event(priv, LEGACY_EVENT_SLAVE_DMA, 0, port);
    return 0xff;
}


static void
update_legacy(es1371_t *dev)
{
    io_removehandler(0x0320, 8,
	cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev);
    io_removehandler(0x0330, 8,
	cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev);
    io_removehandler(0x0340, 8,
	cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev);
    io_removehandler(0x0350, 8,
	cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev);

    io_removehandler(0x5300, 0x0080,
	cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev);
    io_removehandler(0xe800, 0x0080,
	cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev);
    io_removehandler(0xf400, 0x0080,
	cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev);

    io_removehandler(0x0220, 0x0010,
	cap_read_sb,NULL,NULL, cap_write_sb,NULL,NULL, dev);
    io_removehandler(0x0240, 0x0010,
	cap_read_sb,NULL,NULL, cap_write_sb,NULL,NULL, dev);

    io_removehandler(0x0388, 0x0004,
	cap_read_adlib,NULL,NULL, cap_write_adlib,NULL,NULL, dev);

    io_removehandler(0x0020, 0x0002,
	cap_read_master_pic,NULL,NULL, cap_write_master_pic,NULL,NULL, dev);
    io_removehandler(0x0000, 0x0010,
	cap_read_master_dma,NULL,NULL, cap_write_master_dma,NULL,NULL, dev);
    io_removehandler(0x00a0, 0x0002,
	cap_read_slave_pic,NULL,NULL, cap_write_slave_pic,NULL,NULL, dev);
    io_removehandler(0x00c0, 0x0020,
	cap_read_slave_dma,NULL,NULL, cap_write_slave_dma,NULL,NULL, dev);

    if (dev->legacy_ctrl & LEGACY_CAPTURE_SSCAPE) {
	switch ((dev->legacy_ctrl >> LEGACY_SSCAPE_ADDR_SHIFT) & 3) {
		case 0: io_sethandler(0x0320, 0x0008, cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev); break;
		case 1: io_sethandler(0x0330, 0x0008, cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev); break;
		case 2: io_sethandler(0x0340, 0x0008, cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev); break;
		case 3: io_sethandler(0x0350, 0x0008, cap_read_sscape,NULL,NULL, cap_write_sscape,NULL,NULL, dev); break;
	}
    } if (dev->legacy_ctrl & LEGACY_CAPTURE_CODEC) {
	switch ((dev->legacy_ctrl >> LEGACY_CODEC_ADDR_SHIFT) & 3) {
		case 0: io_sethandler(0x5300, 0x0080, cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev); break;
		case 2: io_sethandler(0xe800, 0x0080, cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev); break;
		case 3: io_sethandler(0xf400, 0x0080, cap_read_codec,NULL,NULL, cap_write_codec,NULL,NULL, dev); break;
	}
    }

    if (dev->legacy_ctrl & LEGACY_CAPTURE_SB) {
	if (!(dev->legacy_ctrl & LEGACY_SB_ADDR))
		io_sethandler(0x0220, 0x0010, cap_read_sb,NULL,NULL, cap_write_sb,NULL,NULL, dev);
	else
		io_sethandler(0x0240, 0x0010, cap_read_sb,NULL,NULL, cap_write_sb,NULL,NULL, dev);
    }

    if (dev->legacy_ctrl & LEGACY_CAPTURE_ADLIB)	
	io_sethandler(0x0388, 0x0004, cap_read_adlib,NULL,NULL, cap_write_adlib,NULL,NULL, dev);
    if (dev->legacy_ctrl & LEGACY_CAPTURE_MASTER_PIC)
	io_sethandler(0x0020, 0x0002, cap_read_master_pic,NULL,NULL, cap_write_master_pic,NULL,NULL, dev);
    if (dev->legacy_ctrl & LEGACY_CAPTURE_MASTER_DMA)
	io_sethandler(0x0000, 0x0010, cap_read_master_dma,NULL,NULL, cap_write_master_dma,NULL,NULL, dev);
    if (dev->legacy_ctrl & LEGACY_CAPTURE_SLAVE_PIC)
	io_sethandler(0x00a0, 0x0002, cap_read_slave_pic,NULL,NULL, cap_write_slave_pic,NULL,NULL, dev);
    if (dev->legacy_ctrl & LEGACY_CAPTURE_SLAVE_DMA)
	io_sethandler(0x00c0, 0x0020, cap_read_slave_dma,NULL,NULL, cap_write_slave_dma,NULL,NULL, dev);
}


static void
es1371_fetch(es1371_t *dev, int dac_nr)
{
    int format = dac_nr ? ((dev->si_cr >> 2) & 3) : (dev->si_cr & 3);
    int pos = dev->dac[dac_nr].buffer_pos & 63;
    int c;

    DBGLOG(2, "Fetch format=%i %08x %08x  %08x %08x  %08x\n", format, dev->dac[dac_nr].count, dev->dac[dac_nr].size,  dev->dac[dac_nr].curr_samp_ct,dev->dac[dac_nr].samp_ct, dev->dac[dac_nr].addr);

    switch (format) {
	case FORMAT_MONO_8:
		for (c = 0; c < 32; c += 4) {
			dev->dac[dac_nr].buffer_l[(pos+c)   & 63] = dev->dac[dac_nr].buffer_r[(pos+c)   & 63] = (mem_readb_phys(dev->dac[dac_nr].addr) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_l[(pos+c+1) & 63] = dev->dac[dac_nr].buffer_r[(pos+c+1) & 63] = (mem_readb_phys(dev->dac[dac_nr].addr+1) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_l[(pos+c+2) & 63] = dev->dac[dac_nr].buffer_r[(pos+c+2) & 63] = (mem_readb_phys(dev->dac[dac_nr].addr+2) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_l[(pos+c+3) & 63] = dev->dac[dac_nr].buffer_r[(pos+c+3) & 63] = (mem_readb_phys(dev->dac[dac_nr].addr+3) ^ 0x80) << 8;
			dev->dac[dac_nr].addr += 4;
	
			dev->dac[dac_nr].buffer_pos_end += 4;
			dev->dac[dac_nr].count++;
			if (dev->dac[dac_nr].count > dev->dac[dac_nr].size) {
				dev->dac[dac_nr].count = 0;
				dev->dac[dac_nr].addr = dev->dac[dac_nr].addr_latch;
				break;
			}
		}
		break;

	case FORMAT_STEREO_8:
		for (c = 0; c < 16; c += 2) {
			dev->dac[dac_nr].buffer_l[(pos+c)   & 63] = (mem_readb_phys(dev->dac[dac_nr].addr) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_r[(pos+c)   & 63] = (mem_readb_phys(dev->dac[dac_nr].addr + 1) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_l[(pos+c+1) & 63] = (mem_readb_phys(dev->dac[dac_nr].addr + 2) ^ 0x80) << 8;
			dev->dac[dac_nr].buffer_r[(pos+c+1) & 63] = (mem_readb_phys(dev->dac[dac_nr].addr + 3) ^ 0x80) << 8;
			dev->dac[dac_nr].addr += 4;
	
			dev->dac[dac_nr].buffer_pos_end += 2;
			dev->dac[dac_nr].count++;
			if (dev->dac[dac_nr].count > dev->dac[dac_nr].size) {
				dev->dac[dac_nr].count = 0;
				dev->dac[dac_nr].addr = dev->dac[dac_nr].addr_latch;
				break;
			}
		}
		break;

	case FORMAT_MONO_16:
		for (c = 0; c < 16; c += 2) {
			dev->dac[dac_nr].buffer_l[(pos+c)   & 63] = dev->dac[dac_nr].buffer_r[(pos+c)   & 63] = mem_readw_phys(dev->dac[dac_nr].addr);
			dev->dac[dac_nr].buffer_l[(pos+c+1) & 63] = dev->dac[dac_nr].buffer_r[(pos+c+1) & 63] = mem_readw_phys(dev->dac[dac_nr].addr + 2);
			dev->dac[dac_nr].addr += 4;

			dev->dac[dac_nr].buffer_pos_end += 2;
			dev->dac[dac_nr].count++;
			if (dev->dac[dac_nr].count > dev->dac[dac_nr].size) {
				dev->dac[dac_nr].count = 0;
				dev->dac[dac_nr].addr = dev->dac[dac_nr].addr_latch;
				break;
			}
		}
		break;

	case FORMAT_STEREO_16:
		for (c = 0; c < 4; c++) {
			dev->dac[dac_nr].buffer_l[(pos+c) & 63] = mem_readw_phys(dev->dac[dac_nr].addr);
			dev->dac[dac_nr].buffer_r[(pos+c) & 63] = mem_readw_phys(dev->dac[dac_nr].addr + 2);
//			DBGLOG(1, "Fetch %02x %08x  %04x %04x\n", (pos+c) & 63, dev->dac[dac_nr].addr, dev->dac[dac_nr].buffer_l[(pos+c) & 63], dev->dac[dac_nr].buffer_r[(pos+c) & 63]);
			dev->dac[dac_nr].addr += 4;
	
			dev->dac[dac_nr].buffer_pos_end++;
			dev->dac[dac_nr].count++;
			if (dev->dac[dac_nr].count > dev->dac[dac_nr].size) {
				dev->dac[dac_nr].count = 0;
				dev->dac[dac_nr].addr = dev->dac[dac_nr].addr_latch;
				break;
			}
		}
		break;
    }
}


static __inline float
low_fir_es1371(int dac_nr, int i, float NewSample)
{
    static float x[2][2][128]; //input samples
    static int x_pos[2] = {0, 0};
    float out = 0.0;
    int read_pos;
    int n_coef;
    int pos = x_pos[dac_nr];

    x[dac_nr][i][pos] = NewSample;

    /*Since only 1/16th of input samples are non-zero, only filter those that
      are valid.*/
    read_pos = (pos + 15) & (127 & ~15);
    n_coef = (16 - pos) & 15;

    while (n_coef < ES1371_NCoef) {
	out += low_fir_es1371_coef[n_coef] * x[dac_nr][i][read_pos];
	read_pos = (read_pos + 16) & (127 & ~15);
	n_coef += 16;
    }

    if (i == 1) {
       	x_pos[dac_nr] = (x_pos[dac_nr] + 1) & 127;
       	if (x_pos[dac_nr] > 127)
       		x_pos[dac_nr] = 0;
    }

    return out;
}


static void
es1371_next_sample_filtered(es1371_t *dev, int dac_nr, int out_idx)
{
    int out_l, out_r;
    int c;
        
    if ((dev->dac[dac_nr].buffer_pos - dev->dac[dac_nr].buffer_pos_end) >= 0) {
	es1371_fetch(dev, dac_nr);
    }

    out_l = dev->dac[dac_nr].buffer_l[dev->dac[dac_nr].buffer_pos & 63];
    out_r = dev->dac[dac_nr].buffer_r[dev->dac[dac_nr].buffer_pos & 63];
        
    dev->dac[dac_nr].filtered_l[out_idx] = (int)low_fir_es1371(dac_nr, 0, (float)out_l);
    dev->dac[dac_nr].filtered_r[out_idx] = (int)low_fir_es1371(dac_nr, 1, (float)out_r);
    for (c = 1; c < 16; c++) {
	dev->dac[dac_nr].filtered_l[out_idx+c] = (int)low_fir_es1371(dac_nr, 0, 0);
	dev->dac[dac_nr].filtered_r[out_idx+c] = (int)low_fir_es1371(dac_nr, 1, 0);
    }
        
//  DBGLOG(1, "Use %02x %04x %04x\n", dev->dac[dac_nr].buffer_pos & 63, dev->dac[dac_nr].out_l, dev->dac[dac_nr].out_r);
	
    dev->dac[dac_nr].buffer_pos++;
//  DBGLOG(1, "Next sample %08x %08x  %08x\n", dev->dac[dac_nr].buffer_pos, dev->dac[dac_nr].buffer_pos_end, dev->dac[dac_nr].curr_samp_ct);
}


static void
es1371_update(es1371_t *dev)
{
    int32_t l, r;

    l = (dev->dac[0].out_l * dev->dac[0].vol_l) >> 12;
    l += ((dev->dac[1].out_l * dev->dac[1].vol_l) >> 12);
    r = (dev->dac[0].out_r * dev->dac[0].vol_r) >> 12;
    r += ((dev->dac[1].out_r * dev->dac[1].vol_r) >> 12);

    l >>= 1;
    r >>= 1;

    l = (l * dev->master_vol_l) >> 15;
    r = (r * dev->master_vol_r) >> 15;
      
    if (l < -32768)
	l = -32768;
    else if (l > 32767)
	l = 32767;
    if (r < -32768)
	r = -32768;
    else if (r > 32767)
	r = 32767;

    for (; dev->pos < sound_pos_global; dev->pos++) {
	dev->buffer[dev->pos*2]     = l;
	dev->buffer[dev->pos*2 + 1] = r;
    }
}


static void
es1371_poll(priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    dev->dac[1].time += dev->dac[1].latch;

    es1371_update(dev);

#if 0 /* Todo : MIDI Input */
    if (dev->int_ctrl & INT_UART_EN) { 
	if (dev->uart_ctrl & UART_CTRL_RXINTEN) { 
		if (dev->uart_ctrl & UART_CTRL_TXINTEN)
			dev->int_status |= INT_STATUS_UART;
		else
			dev->int_status &= ~INT_STATUS_UART;
		}
		else if (!(dev->uart_ctrl & UART_CTRL_RXINTEN) && ((dev->uart_ctrl & UART_CTRL_TXINTEN))) {
			dev->int_status |= INT_STATUS_UART;
		}
		
		if (dev->uart_ctrl & UART_CTRL_RXINTEN) {
			if (dev->uart_ctrl & UART_CTRL_TXINTEN) 
				dev->uart_status |= (UART_STATUS_TXINT | UART_STATUS_TXRDY);
			else
				dev->uart_status &= ~(UART_STATUS_TXINT | UART_STATUS_TXRDY);
		} else
			dev->uart_status |= (UART_STATUS_TXINT | UART_STATUS_TXRDY);
		
		es1371_update_irqs(dev);	
	}
#endif
					
    if (dev->int_ctrl & INT_DAC1_EN) {
	int frac = dev->dac[0].ac & 0x7fff;
	int idx = dev->dac[0].ac >> 15;
	int samp1_l = dev->dac[0].filtered_l[idx];
	int samp1_r = dev->dac[0].filtered_r[idx];
	int samp2_l = dev->dac[0].filtered_l[(idx + 1) & 31];
	int samp2_r = dev->dac[0].filtered_r[(idx + 1) & 31];

	dev->dac[0].out_l = ((samp1_l * (0x8000 - frac)) + (samp2_l * frac)) >> 15;
	dev->dac[0].out_r = ((samp1_r * (0x8000 - frac)) + (samp2_r * frac)) >> 15;
//	DBGLOG(1, "1Samp %i %i  %08x\n", dev->dac[0].curr_samp_ct, dev->dac[0].samp_ct, dev->dac[0].ac);
	dev->dac[0].ac += dev->dac[0].vf;
	dev->dac[0].ac &= ((32 << 15) - 1);

	if ((dev->dac[0].ac >> (15+4)) != dev->dac[0].f_pos) {
		es1371_next_sample_filtered(dev, 0, dev->dac[0].f_pos ? 16 : 0);
		dev->dac[0].f_pos = (dev->dac[0].f_pos + 1) & 1;

		dev->dac[0].curr_samp_ct--;
		if (dev->dac[0].curr_samp_ct < 0) {
//			DBGLOG(1, "DAC1 IRQ\n");
			dev->int_status |= INT_STATUS_DAC1;
			es1371_update_irqs(dev);
			dev->dac[0].curr_samp_ct = dev->dac[0].samp_ct;
		}
	}
    }

    if (dev->int_ctrl & INT_DAC2_EN) {
	int frac = dev->dac[1].ac & 0x7fff;
	int idx = dev->dac[1].ac >> 15;
	int samp1_l = dev->dac[1].filtered_l[idx];
	int samp1_r = dev->dac[1].filtered_r[idx];
	int samp2_l = dev->dac[1].filtered_l[(idx + 1) & 31];
	int samp2_r = dev->dac[1].filtered_r[(idx + 1) & 31];

	dev->dac[1].out_l = ((samp1_l * (0x8000 - frac)) + (samp2_l * frac)) >> 15;
	dev->dac[1].out_r = ((samp1_r * (0x8000 - frac)) + (samp2_r * frac)) >> 15;
//	DBGLOG(1,"2Samp %i %i  %08x\n", dev->dac[1].curr_samp_ct, dev->dac[1].samp_ct, dev->dac[1].ac);
	dev->dac[1].ac += dev->dac[1].vf;
	dev->dac[1].ac &= ((32 << 15) - 1);
	if ((dev->dac[1].ac >> (15+4)) != dev->dac[1].f_pos) {
		es1371_next_sample_filtered(dev, 1, dev->dac[1].f_pos ? 16 : 0);
		dev->dac[1].f_pos = (dev->dac[1].f_pos + 1) & 1;

		dev->dac[1].curr_samp_ct--;
		if (dev->dac[1].curr_samp_ct < 0) {
//			dev->dac[1].curr_samp_ct = 0;
//			DBGLOG(1, "DAC2 IRQ\n");
			dev->int_status |= INT_STATUS_DAC2;
			es1371_update_irqs(dev);
			dev->dac[1].curr_samp_ct = dev->dac[1].samp_ct;
		}
	}
    }
}


static void
es1371_get_buffer(int32_t *buffer, int len, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
    int c;

    es1371_update(dev);

    for (c = 0; c < len * 2; c++)
	buffer[c] += (dev->buffer[c] / 2);
	
    dev->pos = 0;
}


static __inline
double sinc(double x)
{
    return sin(M_PI * x) / (M_PI * x);
}


static void
generate_es1371_filter(void)
{
    /*Cutoff frequency = 1 / 32*/
    float fC = 1.0 / 32.0;
    float gain;
    int n;

    for (n = 0; n < ES1371_NCoef; n++) {
	/*Blackman window*/
	double w = 0.42 - (0.5 * cos((2.0*n*M_PI)/(double)(ES1371_NCoef-1))) + (0.08 * cos((4.0*n*M_PI)/(double)(ES1371_NCoef-1)));

	/*Sinc filter*/
	double h = sinc(2.0 * fC * ((double)n - ((double)(ES1371_NCoef-1) / 2.0)));

	/*Create windowed-sinc filter*/
	low_fir_es1371_coef[n] = (float)(w * h);
    }

    low_fir_es1371_coef[(ES1371_NCoef - 1) / 2] = 1.0;

    gain = 0.0;
    for (n = 0; n < ES1371_NCoef; n++)
	gain += low_fir_es1371_coef[n] / (float)N;

    gain /= (float)0.95;

    /*Normalise filter, to produce unity gain*/
    for (n = 0; n < ES1371_NCoef; n++)
	low_fir_es1371_coef[n] /= gain;
}        


static uint8_t
es1371_pci_read(int func, int addr, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    if (func)
	return 0;

    DBGLOG(2, "ES1371 PCI read %08X PC=%08x\n", addr, cpu_state.pc);

    switch (addr) {
	case 0x00: return 0x74; /*Ensoniq*/
	case 0x01: return 0x12;

	case 0x02: return 0x71; /*ES1371*/
	case 0x03: return 0x13;

	case 0x04: return dev->pci_command;
	case 0x05: return dev->pci_serr;

	case 0x06: return 0x10; /*Supports ACPI*/
	case 0x07: return 0x00;

	case 0x08: return 2; /*Revision ID*/
	case 0x09: return 0x00; /*Multimedia audio device*/
	case 0x0a: return 0x01;
	case 0x0b: return 0x04;
		
	case 0x10: return 0x01 | (dev->base_addr & 0xc0); /*memBaseAddr*/
	case 0x11: return dev->base_addr >> 8;
	case 0x12: return dev->base_addr >> 16;
	case 0x13: return dev->base_addr >> 24;

	case 0x2c: return 0x74; /*Subsystem vendor ID*/
	case 0x2d: return 0x12;
	case 0x2e: return 0x71;
	case 0x2f: return 0x13;

	case 0x34: return 0xdc; /*Capabilites pointer*/

	case 0x3c: return dev->int_line;
	case 0x3d: return 0x01; /*INTA*/

	case 0x3e: return 0xc; /*Minimum grant*/
	case 0x3f: return 0x80; /*Maximum latency*/

	case 0xdc: return 0x01; /*Capabilities identifier*/
	case 0xdd: return 0x00; /*Next item pointer*/
	case 0xde: return 0x31; /*Power management capabilities*/
	case 0xdf: return 0x6c;

	case 0xe0: return dev->pmcsr & 0xff;
	case 0xe1: return dev->pmcsr >> 8;
    }

    return 0;
}


static void
es1371_pci_write(int func, int addr, uint8_t val, priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;

    if (func)
	return;

    DBGLOG(2, "ES1371 PCI write %04X %02X PC=%08x\n", addr, val, cpu_state.pc);

    switch (addr) {
	case 0x04:
		if (dev->pci_command & PCI_COMMAND_IO)
			io_removehandler(dev->base_addr, 0x0040,
					 es1371_inb, es1371_inw, es1371_inl,
					 es1371_outb, es1371_outw, es1371_outl,
					 dev);
		dev->pci_command = val & 0x05;
		if (dev->pci_command & PCI_COMMAND_IO)
			io_sethandler(dev->base_addr, 0x0040,
				      es1371_inb, es1371_inw, es1371_inl,
				      es1371_outb, es1371_outw, es1371_outl,
				      dev);
		break;

	case 0x05:
		dev->pci_serr = val & 1;
		break;

	case 0x10:
		if (dev->pci_command & PCI_COMMAND_IO)
			io_removehandler(dev->base_addr, 0x0040,
					 es1371_inb, es1371_inw, es1371_inl,
					 es1371_outb, es1371_outw, es1371_outl,
					 dev);
		dev->base_addr = (dev->base_addr & 0xffffff00) | (val & 0xc0);
		if (dev->pci_command & PCI_COMMAND_IO)
			io_sethandler(dev->base_addr, 0x0040,
				      es1371_inb, es1371_inw, es1371_inl,
				      es1371_outb, es1371_outw, es1371_outl,
				      dev);
		break;

	case 0x11:
		if (dev->pci_command & PCI_COMMAND_IO)
			io_removehandler(dev->base_addr, 0x0040,
					 es1371_inb, es1371_inw, es1371_inl,
					 es1371_outb, es1371_outw, es1371_outl,
					 dev);
		dev->base_addr = (dev->base_addr & 0xffff00c0) | (val << 8);
		if (dev->pci_command & PCI_COMMAND_IO)
			io_sethandler(dev->base_addr, 0x0040,
				      es1371_inb, es1371_inw, es1371_inl,
				      es1371_outb, es1371_outw, es1371_outl,
				      dev);
		break;

	case 0x12:
		dev->base_addr = (dev->base_addr & 0xff00ffc0) | (val << 16);
		break;

	case 0x13:
		dev->base_addr = (dev->base_addr & 0x00ffffc0) | (val << 24);
		break;

	case 0x3c:
		dev->int_line = val;
		break;

	case 0xe0:
		dev->pmcsr = (dev->pmcsr & 0xff00) | (val & 0x03);
		break;

	case 0xe1:
		dev->pmcsr = (dev->pmcsr & 0x00ff) | ((val & 0x01) << 8);
		break;
    }

    DBGLOG(2, "dev->base_addr %08x\n", dev->base_addr);
}


static priv_t
es1371_init(const device_t *info, UNUSED(void *parent))
{
    es1371_t *dev = (es1371_t *)mem_alloc(sizeof(es1371_t));
    memset(dev, 0, sizeof(es1371_t));

    sound_add_handler(es1371_get_buffer, dev);

    dev->card = pci_add_card(PCI_ADD_NORMAL,
			     es1371_pci_read, es1371_pci_write, dev);

    timer_add(es1371_poll, dev, &dev->dac[1].time, TIMER_ALWAYS_ENABLED);

    generate_es1371_filter();
		
    return (priv_t)dev;
}


static void
es1371_close(priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
	
    free(dev);
}


static void
es1371_speed_changed(priv_t priv)
{
    es1371_t *dev = (es1371_t *)priv;
	
    dev->dac[1].latch = (int)((double)TIMER_USEC * (1000000.0 / 48000.0));
}
 

const device_t es1371_device = {
    "Ensoniq AudioPCI (ES1371)",
    DEVICE_PCI,
    0,
    NULL,
    es1371_init, es1371_close, NULL,
    NULL,
    es1371_speed_changed,
    NULL,
    NULL,
    NULL
};
 

const device_t sbpci128_device = {
    "Sound Blaster 128",
    DEVICE_PCI,
    1,
    NULL,
    es1371_init, es1371_close, NULL,
    NULL,
    es1371_speed_changed,
    NULL,
    NULL,
    NULL
};
