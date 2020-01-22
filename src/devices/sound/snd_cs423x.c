/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Cirrus Logic Crystal 423x sound devices.
 *
 * Version:	@(#)snd_cs423x.c	1.0.5	2020/01/21
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2018,2019 Altheos.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../system/dma.h"
#include "../system/pic.h"
#include "sound.h"
#include "snd_cs423x.h"


#define CS4231	  		0x80
#define CS4231A	  		0xa0
#define CS4232	  		0xa2
#define CS4232A	  		0xb2
#define CS4236	  		0x83
#define CS4236B	  		0x03


static int cs423x_vols[64];


void
cs423x_setirq(cs423x_t *dev, int irq_ch)
{
    dev->irq = irq_ch;
}


void
cs423x_setdma(cs423x_t *dev, int dma_ch)
{
    dev->dma = dma_ch;
}


uint8_t
cs423x_read(uint16_t addr, priv_t priv)
{
    cs423x_t *dev = (cs423x_t *)priv;
    uint8_t ret = 0xff;

    if (dev->initb)
	return 0x80;

    switch (addr & 3) {
	case 0: /*Index*/
		if (dev->mode2)
			ret = dev->indx | dev->trd | dev->mce | dev->ia4 | dev->initb;
		else
			ret = dev->indx | dev->trd | dev->mce | dev->initb;
		break;

	case 1:
		ret = dev->regs[dev->indx];
		if (dev->indx == 0x0b) {
			ret ^= 0x20;
			dev->regs[dev->indx] = ret;
		}
		break;

	case 2:
		ret = dev->status;
		break;

	case 3: // Todo Capture I/O read
		break;
    }

    return ret;
}


void
cs423x_write(uint16_t addr, uint8_t val, priv_t priv)
{
    cs423x_t *dev = (cs423x_t *)priv;
    double freq;

    switch (addr & 3) {
	case 0: /*Index*/
		dev->indx = val & (dev->mode2 ? 0x1f : 0x0f);
		dev->trd = val & 0x20;
		dev->mce = val & 0x40;
		dev->ia4 = val & 0x10;
		dev->initb = val & 0x80;
		break;

	case 1:
		switch (dev->indx) {
			case 8:
				freq = (double)((val & 1) ? 16934400LL : 24576000LL);
				switch ((val >> 1) & 7) {
					case 0: freq /= 3072; break;
					case 1: freq /= 1536; break;
					case 2: freq /= 896;  break;
					case 3: freq /= 768;  break;
					case 4: freq /= 448;  break;
					case 5: freq /= 384;  break;
					case 6: freq /= 512;  break;
					case 7: freq /= 2560; break;
				}
				dev->freq = (tmrval_t)freq;
				dev->timer_latch = (tmrval_t)((double)TIMER_USEC * (1000000.0 / (double)dev->freq));
				break;

			case 9:
				dev->enable = ((val & 0x41) == 0x01);
				if (! dev->enable)
					dev->out_l = dev->out_r = 0;
				break;

			case 11:
				break;

			case 12:
				val |= 0x8a;
				dev->mode2 = (val >> 6) & 1;
				break;

			case 14:
				dev->count = dev->regs[15] | (val << 8);
				break;

			case 24:
				if (! (val & 0x70))
					dev->status &= 0xfe;
				break;

			case 25:
				break;
		}
		dev->regs[dev->indx] = val;
		break;

	case 2:
		dev->status &= 0xfe;
		break;

	case 3: // Todo Playback I/O Write
		break;
    }
}


void
cs423x_update(cs423x_t *dev)
{
    for (; dev->pos < sound_pos_global; dev->pos++) {
	dev->buffer[dev->pos * 2] = dev->out_l;
	dev->buffer[dev->pos * 2 + 1] = dev->out_r;
    }
}


static void
cs423x_poll(priv_t priv)
{
    cs423x_t *dev = (cs423x_t *)priv;
    int32_t temp;

    if (dev->timer_latch)
	dev->timer_count += dev->timer_latch;
    else
	dev->timer_count = TIMER_USEC;

    cs423x_update(dev);

    if (! dev->enable) {
	dev->out_l = dev->out_r = 0;
	return;
    }

    if (! (dev->mode2)) switch (dev->regs[8] & 0x70) {
	case 0x00: /*Mono, 8-bit PCM*/
		dev->out_l = dev->out_r = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		break;

	case 0x10: /*Stereo, 8-bit PCM*/
		dev->out_l = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		dev->out_r = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		break;

	case 0x40: /*Mono, 16-bit PCM*/
		temp = dma_channel_read(dev->dma);
		dev->out_l = dev->out_r = (dma_channel_read(dev->dma) << 8) | temp;
		break;

	case 0x50: /*Stereo, 16-bit PCM*/
		temp = dma_channel_read(dev->dma);
		dev->out_l = (dma_channel_read(dev->dma) << 8) | temp;
		temp = dma_channel_read(dev->dma);
		dev->out_r = (dma_channel_read(dev->dma) << 8) | temp;
		break;
    } else switch (dev->regs[8] & 0xf0) {
	case 0x00: /*Mono, 8-bit PCM*/
		dev->out_l = dev->out_r = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		break;

	case 0x10: /*Stereo, 8-bit PCM*/
		dev->out_l = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		dev->out_r = (dma_channel_read(dev->dma) ^ 0x80) * 256;
		break;

	case 0x40: /*Mono, 16-bit PCM*/
	case 0xc0: /*Mono, 16-bit PCM Big-Endian should not happen on x86*/
		temp = dma_channel_read(dev->dma);
		dev->out_l = dev->out_r = (dma_channel_read(dev->dma) << 8) | temp;
		break;

	case 0x50: /*Stereo, 16-bit PCM*/
	case 0xd0: /*Stereo, 16-bit PCM Big-Endian. Should not happen on x86*/
		temp = dma_channel_read(dev->dma);
		dev->out_l = (dma_channel_read(dev->dma) << 8) | temp;
		temp = dma_channel_read(dev->dma);
		dev->out_r = (dma_channel_read(dev->dma) << 8) | temp;
		break;

	case 0xa0: /*TODO Mono, ADPCM, 4-bit*/
	case 0xb0: /*TODO Stereo, ADPCM, 4-bit*/
		break;
    }

    if (dev->regs[6] & 0x80) // Mute Left-Channel
	dev->out_l = 0;
    else
	dev->out_l = (dev->out_l * cs423x_vols[dev->regs[6] & 0x3f]) >> 16;

    if (dev->regs[7] & 0x80) // Mute Right-Channel
	dev->out_r = 0;
    else
	dev->out_r = (dev->out_r * cs423x_vols[dev->regs[7] & 0x3f]) >> 16;

    if (dev->count < 0) {
	dev->count = dev->regs[15] | (dev->regs[14] << 8);
	if (! (dev->status & 0x01)) {
		dev->status |= 0x01;
		if (dev->regs[10] & 2)
			picint(1 << dev->irq);
	}
    }

    dev->count--;
}


void
cs423x_init(cs423x_t *dev)
{
    double attenuation;
    int c;

    dev->enable = 0;

    dev->status = 0xcc;
    dev->indx = dev->trd = 0;
    dev->mce = 0x40;
    dev->initb = 0;
    dev->mode2 = 0;

    dev->regs[0] = dev->regs[1] = 0;
    dev->regs[2] = dev->regs[3] = 0x88;
    dev->regs[4] = dev->regs[5] = 0x88;
    dev->regs[6] = dev->regs[7] = 0x80;
    dev->regs[8] = 0;
    dev->regs[9] = 0x08;
    dev->regs[10] = dev->regs[11] = 0;
    dev->regs[12] = 0x8a;
    dev->regs[13] = 0;
    dev->regs[14] = dev->regs[15] = 0;
    dev->regs[16] = dev->regs[17] = 0;
    dev->regs[18] = dev->regs[19] = 0x88;
    dev->regs[22] = 0x80;
    dev->regs[24] = 0;
    dev->regs[25] = CS4231; //CS4231 for GUS MAX
    dev->regs[26] = 0x80;
    dev->regs[29] = 0x80;

    dev->out_l = 0;
    dev->out_r = 0;

    for (c = 0; c < 64; c++) {	// DAC and Loopback attenuation
	attenuation = 0.0;
	if (c & 0x01) attenuation -= 1.5;
	if (c & 0x02) attenuation -= 3.0;
	if (c & 0x04) attenuation -= 6.0;
	if (c & 0x08) attenuation -= 12.0;
	if (c & 0x10) attenuation -= 24.0;
	if (c & 0x20) attenuation -= 48.0;

	attenuation = pow(10, attenuation / 10);
	cs423x_vols[c] = (int)(attenuation * 65536);
    }

    timer_add(cs423x_poll, dev, &dev->timer_count, &dev->enable);
}


void
cs423x_speed_changed(cs423x_t *dev)
{
    dev->timer_latch = (tmrval_t)((double)TIMER_USEC * (1000000.0 / (double)dev->freq));
}
