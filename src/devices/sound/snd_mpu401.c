/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Roland MPU-401 emulation.
 *
 * Version:	@(#)snd_mpu401.c	1.0.15	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		DOSBox Team,
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *		Copyright 2008-2017 DOSBox Team.
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
#include <stdarg.h>
#include <wchar.h>
#define dbglog sound_midi_log
#include "../../emu.h"
#include "../../io.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/pic.h"
#include "../system/mca.h"
#include "sound.h"
#include "snd_mpu401.h"
#include "midi.h"


enum {
    STATUS_OUTPUT_NOT_READY = 0x40,
    STATUS_INPUT_NOT_READY  = 0x80
};


static int64_t	mpu401_event_callback = 0LL;
static int64_t	mpu401_eoi_callback = 0LL;
static int64_t	mpu401_reset_callback = 0LL;


static void	MPU401_WriteCommand(mpu_t *, uint8_t);
static void	MPU401_EOIHandlerDispatch(priv_t);


static void
QueueByte(mpu_t *mpu, uint8_t data)
{
    if (mpu->state.block_ack) {
	mpu->state.block_ack = 0;
	return;
    }
	
    if ((mpu->queue_used == 0) && (mpu->mode == M_INTELLIGENT)) {
	mpu->state.irq_pending = 1;
	picint(1 << mpu->irq);
    }
    if (mpu->queue_used < MPU401_QUEUE) {
	int pos = mpu->queue_used+mpu->queue_pos;

	if (mpu->queue_pos >= MPU401_QUEUE) 
		mpu->queue_pos -= MPU401_QUEUE;

	if (pos>=MPU401_QUEUE) 
		pos-=MPU401_QUEUE;

	mpu->queue_used++;
	mpu->queue[pos] = data;
    } else
	DEBUG("MPU401:Data queue full\n");
}


static void
ClrQueue(mpu_t *mpu)
{
    mpu->queue_used=0;
    mpu->queue_pos=0;
}


static void
MPU401_Reset(mpu_t *mpu)
{
    uint8_t i;

    if (mpu->mode == M_INTELLIGENT) {
	picintc(1 << mpu->irq);
	mpu->state.irq_pending = 0;
    }

    mpu->mode = M_INTELLIGENT;
    mpu->state.eoi_scheduled = 0;
    mpu->state.wsd = 0;
    mpu->state.wsm = 0;
    mpu->state.conductor = 0;
    mpu->state.cond_req = 0;
    mpu->state.cond_set = 0;
    mpu->state.playing = 0;
    mpu->state.run_irq = 0;
    mpu->state.cmask = 0xff;
    mpu->state.amask = mpu->state.tmask = 0;
    mpu->state.midi_mask = 0xffff;
    mpu->state.data_onoff = 0;
    mpu->state.command_byte = 0;
    mpu->state.block_ack = 0;
    mpu->clock.tempo = mpu->clock.old_tempo = 100;
    mpu->clock.timebase = mpu->clock.old_timebase = 120;
    mpu->clock.tempo_rel = mpu->clock.old_tempo_rel = 40;
    mpu->clock.tempo_grad = 0;
    mpu->clock.clock_to_host = 0;
    mpu->clock.cth_rate = 60;
    mpu->clock.cth_counter = 0;

    ClrQueue(mpu);

    mpu->state.req_mask = 0;
    mpu->condbuf.counter = 0;
    mpu->condbuf.type = T_OVERFLOW;

    for (i=0;i<8;i++) {
	mpu->playbuf[i].type = T_OVERFLOW;
	mpu->playbuf[i].counter = 0;
    }
}


static void
MPU401_ResetDone(priv_t priv)
{
    mpu_t *mpu = (mpu_t *)priv;

    DEBUG("MPU-401 reset callback\n");

    mpu401_reset_callback = 0LL;

    mpu->state.reset = 0;
    if (mpu->state.cmd_pending) {
	MPU401_WriteCommand(mpu, mpu->state.cmd_pending - 1);
	mpu->state.cmd_pending = 0;
    }
}


static void
MPU401_WriteCommand(mpu_t *mpu, uint8_t val)
{	
    uint8_t i, was_uart;

    if (mpu->state.reset)
	mpu->state.cmd_pending = val + 1;
    if ((val != 0x3f) && (val != 0xff) && !mpu->intelligent)
	return;

    if (val <= 0x2f) {
	switch (val & 3) { /* MIDI stop, start, continue */
		case 1:
			midi_write(0xfc);
			break;

		case 2:
			midi_write(0xfa);
			break;

		case 3:
			midi_write(0xfb);
			break;
	}

	if (val & 0x20)
		DEBUG("MPU-401:Unhandled Recording Command %x",(int)val);

	switch (val & 0xc) {
		case  0x4:	/* Stop */
			mpu->state.playing = 0;
			mpu401_event_callback = 0LL;

			for (i = 0xb0; i < 0xbf; i++) {
				/* All notes off */
				midi_write(i);
				midi_write(0x7b);
				midi_write(0);
			}
			break;

		case 0x8:	/* Play */
			DEBUG("MPU-401:Intelligent mode playback started");
			mpu->state.playing = 1;
			mpu401_event_callback = (int64_t) ((MPU401_TIMECONSTANT / (mpu->clock.tempo*mpu->clock.timebase)) * 1000LL * TIMER_USEC);
			ClrQueue(mpu);
			break;
	}
    } else if ((val >= 0xa0) && (val <= 0xa7)) {/* Request play counter */
	if (mpu->state.cmask & (1 << (val&7)))
		QueueByte(mpu, mpu->playbuf[val&7].counter);
    } else if ((val >= 0xd0) && (val <= 0xd7)) {	/* Send data */
	mpu->state.old_chan = mpu->state.channel;
	mpu->state.channel= val & 7;
	mpu->state.wsd = 1;
	mpu->state.wsm = 0;
	mpu->state.wsd_start = 1;
    } else switch (val) {
	case 0xdf:	/* Send system message */
		mpu->state.wsd = 0;
		mpu->state.wsm = 1;
		mpu->state.wsd_start = 1;
		break;

	case 0x8e:	/* Conductor */
		mpu->state.cond_set = 0;
		break;

	case 0x8f:
		mpu->state.cond_set = 1;
		break;

	case 0x94: /* Clock to host */
		mpu->clock.clock_to_host = 0;
		break;

	case 0x95:
		mpu->clock.clock_to_host = 1;
		break;

	case 0xc2: /* Internal timebase */
		mpu->clock.timebase = 48;
		break;

	case 0xc3:
		mpu->clock.timebase = 72;
		break;

	case 0xc4:
		mpu->clock.timebase = 96;
		break;

	case 0xc5:
		mpu->clock.timebase = 120;
		break;

	case 0xc6:
		mpu->clock.timebase = 144;
		break;

	case 0xc7:
		mpu->clock.timebase = 168;
		break;
	case 0xc8:
		mpu->clock.timebase = 192;
		break;

	/* Commands with data byte */
	case 0xe0: case 0xe1: case 0xe2: case 0xe4: case 0xe6: 
	case 0xe7: case 0xec: case 0xed: case 0xee: case 0xef:
		mpu->state.command_byte = val;
		break;

	/* Commands 0xa# returning data */
	case 0xab:	/* Request and clear recording counter */
		QueueByte(mpu, MSG_MPU_ACK);
		QueueByte(mpu, 0);
		return;

	case 0xac:	/* Request version */
		QueueByte(mpu, MSG_MPU_ACK);
		QueueByte(mpu, MPU401_VERSION);
		return;

	case 0xad:	/* Request revision */
		QueueByte(mpu, MSG_MPU_ACK);
		QueueByte(mpu, MPU401_REVISION);
		return;

	case 0xaf:	/* Request tempo */
		QueueByte(mpu, MSG_MPU_ACK);
		QueueByte(mpu, mpu->clock.tempo);
		return;

	case 0xb1:	/* Reset relative tempo */
		mpu->clock.old_tempo_rel = mpu->clock.tempo_rel;
		mpu->clock.tempo_rel = 40;
		break;

	case 0xb9:	/* Clear play map */
	case 0xb8:	/* Clear play counters */
		for (i = 0xb0; i < 0xbf; i++) {
			/* All notes off */
			midi_write(i);
			midi_write(0x7b);
			midi_write(0);
		}
		for (i = 0; i < 8; i++) {
			mpu->playbuf[i].counter = 0;
			mpu->playbuf[i].type = T_OVERFLOW;
		}
		mpu->condbuf.counter = 0;
		mpu->condbuf.type = T_OVERFLOW;
		if (! (mpu->state.conductor = mpu->state.cond_set))
					mpu->state.cond_req = 0;
		mpu->state.amask = mpu->state.tmask;
		mpu->state.req_mask = 0;
		mpu->state.irq_pending = 1;
		break;

	case 0xff:	/* Reset MPU-401 */
		DEBUG("MPU-401:Reset %X\n", val);
		mpu401_reset_callback = (int64_t) (MPU401_RESETBUSY * 33LL * TIMER_USEC);
		mpu->state.reset = 1;
		was_uart = (mpu->mode == M_UART);
		MPU401_Reset(mpu);
		if (was_uart)
			return;		/* do not send ack in UART mode */
		break;

	case 0x3f:	/* UART mode */
		DEBUG("MPU-401:Set UART mode %X\n", val);
		QueueByte(mpu, MSG_MPU_ACK);
		mpu->mode = M_UART;
		return;

	default:;
		DEBUG("MPU-401:Unhandled command %X", val);
    }

    QueueByte(mpu, MSG_MPU_ACK);
}


static void
MPU401_WriteData(mpu_t *mpu, uint8_t val)
{
    static int length, cnt, posd;

    if (mpu->mode == M_UART) {
	midi_write(val);
	return;
    }

    if (!mpu->intelligent) {
	mpu->state.command_byte = 0;
	return;
    }

    switch (mpu->state.command_byte) {	/* 0xe# command data */
	case 0x00:
		break;

	case 0xe0:	/* Set tempo */
		mpu->state.command_byte = 0;
		mpu->clock.tempo = val;
		return;

	case 0xe1:	/* Set relative tempo */
		mpu->state.command_byte = 0;
		mpu->clock.old_tempo_rel = mpu->clock.tempo_rel;
		mpu->clock.tempo_rel = val;
		return;

	case 0xe7:	/* Set internal clock to host interval */
		mpu->state.command_byte = 0;
		mpu->clock.cth_rate = val >> 2;
		return;

	case 0xec:	/* Set active track mask */
		mpu->state.command_byte = 0;
		mpu->state.tmask = val;
		return;

	case 0xed: /* Set play counter mask */
		mpu->state.command_byte = 0;
		mpu->state.cmask = val;
		return;

	case 0xee: /* Set 1-8 MIDI channel mask */
		mpu->state.command_byte = 0;
		mpu->state.midi_mask &= 0xff00;
		mpu->state.midi_mask |= val;
		return;

	case 0xef: /* Set 9-16 MIDI channel mask */
		mpu->state.command_byte = 0;
		mpu->state.midi_mask &= 0x00ff;
		mpu->state.midi_mask |= ((uint16_t)val) << 8;
		return;

	default:
		mpu->state.command_byte = 0;
		return;
    }

    if (mpu->state.wsd) {
	/* Directly send MIDI message */
	if (mpu->state.wsd_start) {
		mpu->state.wsd_start = 0;
		cnt = 0;
		switch (val & 0xf0) {
			case 0xc0:
			case 0xd0:
				mpu->playbuf[mpu->state.channel].value[0] = val;
				length = 2;
				break;

			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				mpu->playbuf[mpu->state.channel].value[0] = val;
				length = 3;
				break;

			case 0xf0:
				DEBUG("MPU-401:Illegal WSD byte\n");
				mpu->state.wsd = 0;
				mpu->state.channel = mpu->state.old_chan;
				return;

			default: /* MIDI with running status */
				cnt++;
				midi_write(mpu->playbuf[mpu->state.channel].value[0]);
		}
	}

	if (cnt < length) {
		midi_write(val);
		cnt++;
	}

	if (cnt == length) {
		mpu->state.wsd = 0;
		mpu->state.channel = mpu->state.old_chan;
	}

	return;
    }

    if (mpu->state.wsm) {	/* Directly send system message */
	if (val == MSG_EOX) {
		midi_write(MSG_EOX);
		mpu->state.wsm = 0;
		return;
	}

	if (mpu->state.wsd_start) {
		mpu->state.wsd_start = 0;
		cnt = 0;
		switch (val) {
			case 0xf2:
				length = 3;
				break;

			case 0xf3:
				length = 2;
				break;

			case 0xf6:
				length = 1;
				break;

			case 0xf0:
				length = 0;
				break;

			default:
				length = 0;
		}
	}

	if (!length || cnt < length) {
		midi_write(val);
		cnt++;
	}

	if (cnt == length)
		mpu->state.wsm = 0;

	return;
    }

    if (mpu->state.cond_req) {
	/* Command */
	switch (mpu->state.data_onoff) {
		case -1:
			return;

		case  0: /* Timing byte */
			mpu->condbuf.vlength = 0;
			if (val < 0xf0)
				mpu->state.data_onoff++;
			  else {
				mpu->state.data_onoff = -1;
				MPU401_EOIHandlerDispatch(mpu);
				return;
			}
			mpu->state.send_now = val ? 0 : 1;
			mpu->condbuf.counter = val;
			break;

		case  1: /* Command byte #1 */
			mpu->condbuf.type = T_COMMAND;
			if (val == 0xf8 || val == 0xf9)
				mpu->condbuf.type = T_OVERFLOW;
			mpu->condbuf.value[mpu->condbuf.vlength] = val;
			mpu->condbuf.vlength++;
			if ((val & 0xf0) != 0xe0)
				MPU401_EOIHandlerDispatch(mpu);
			  else
				mpu->state.data_onoff++;
			break;

		case  2:/* Command byte #2 */
			mpu->condbuf.value[mpu->condbuf.vlength] = val;
			mpu->condbuf.vlength++;
			MPU401_EOIHandlerDispatch(mpu);
			break;
	}
	return;
    }

    switch (mpu->state.data_onoff) {
	/* Data */
	case -1:
		return;

	case 0: /* Timing byte */
		if (val < 0xf0)
			mpu->state.data_onoff = 1;
		  else {
			mpu->state.data_onoff = -1;
			MPU401_EOIHandlerDispatch(mpu);
			return;
		}
		mpu->state.send_now = val ? 0 : 1;
		mpu->playbuf[mpu->state.channel].counter = val;
		break;

	case 1: /* MIDI */
		mpu->playbuf[mpu->state.channel].vlength++;
		posd = mpu->playbuf[mpu->state.channel].vlength;
		if (posd == 1) switch (val & 0xf0) {
			case 0xf0: /* System message or mark */
				if (val > 0xf7)
					mpu->playbuf[mpu->state.channel].type = T_MARK;
				  else
					mpu->playbuf[mpu->state.channel].type = T_MIDI_SYS;
				mpu->playbuf[mpu->state.channel].sys_val = val;
				length = 1;
				break;

			case 0xc0: /* MIDI Message */
			case 0xd0:
				mpu->playbuf[mpu->state.channel].type = T_MIDI_NORM;
				length = mpu->playbuf[mpu->state.channel].length = 2;
				break;

			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				mpu->playbuf[mpu->state.channel].type = T_MIDI_NORM;
				length = mpu->playbuf[mpu->state.channel].length = 3;
				break;

			default: /* MIDI data with running status */
				posd++;
				mpu->playbuf[mpu->state.channel].vlength++;
				mpu->playbuf[mpu->state.channel].type = T_MIDI_NORM;
				length = mpu->playbuf[mpu->state.channel].length;
				break;
		}

		if (! (posd == 1 && val >= 0xf0))
			mpu->playbuf[mpu->state.channel].value[posd - 1] = val;
		if (posd == length)
			MPU401_EOIHandlerDispatch(mpu);
    }
}


static void
MPU401_IntelligentOut(mpu_t *mpu, uint8_t chan)
{
    uint8_t val;
    uint8_t i;

    switch (mpu->playbuf[chan].type) {
	case T_OVERFLOW:
		break;

	case T_MARK:
		val = mpu->playbuf[chan].sys_val;
		if (val == 0xfc) {
			midi_write(val);
			mpu->state.amask &= ~(1 << chan);
			mpu->state.req_mask &= ~(1 << chan);
		}
		break;

	case T_MIDI_NORM:
		for (i = 0; i < mpu->playbuf[chan].vlength; i++)
			midi_write(mpu->playbuf[chan].value[i]);
		break;

	default:
		break;
    }
}


static void
UpdateTrack(mpu_t *mpu, uint8_t chan)
{
    MPU401_IntelligentOut(mpu, chan);

    if (mpu->state.amask & (1 << chan)) {
	mpu->playbuf[chan].vlength = 0;
	mpu->playbuf[chan].type = T_OVERFLOW;
	mpu->playbuf[chan].counter = 0xf0;
	mpu->state.req_mask |= (1 << chan);
    } else {
	if (mpu->state.amask == 0 && !mpu->state.conductor)
				mpu->state.req_mask |= (1 << 12);
    }
}


static void
UpdateConductor(mpu_t *mpu)
{
    if (mpu->condbuf.value[0] == 0xfc) {
	mpu->condbuf.value[0] = 0;
	mpu->state.conductor = 0;
	mpu->state.req_mask &= ~(1 << 9);
	if (mpu->state.amask == 0)
		mpu->state.req_mask |= (1 << 12);
	return;
    }

    mpu->condbuf.vlength = 0;
    mpu->condbuf.counter = 0xf0;
    mpu->state.req_mask |= (1 << 9);
}


/* Updates counters and requests new data on "End of Input". */
static void
MPU401_EOIHandler(void *priv)
{
    mpu_t *mpu = (mpu_t *)priv;
    uint8_t i;

    DEBUG("MPU-401 end of input callback\n");

    mpu401_eoi_callback = 0LL;
    mpu->state.eoi_scheduled = 0;
    if (mpu->state.send_now) {
	mpu->state.send_now = 0;
	if (mpu->state.cond_req)
		UpdateConductor(mpu);
	  else
		UpdateTrack(mpu, mpu->state.channel);
    }

    mpu->state.irq_pending = 0;

    if (!mpu->state.playing || !mpu->state.req_mask) return;

    i = 0;
    do {
	if (mpu->state.req_mask & (1 << i)) {
		QueueByte(mpu, 0xf0 + i);
		mpu->state.req_mask &= ~(1 << i);
		break;
	}
    } while ((i++) < 16);
}


static void
MPU401_EOIHandlerDispatch(priv_t priv)
{
    mpu_t *mpu = (mpu_t *)priv;

    if (mpu->state.send_now) {
	mpu->state.eoi_scheduled = 1;
	mpu401_eoi_callback = 60LL * TIMER_USEC; /* Possible a bit longer */
    } else if (! mpu->state.eoi_scheduled) 
	MPU401_EOIHandler(mpu);
}


static void
imf_write(uint16_t addr, uint8_t val, priv_t priv)
{
    DBGLOG(1, "IMF:Wr %4X,%X\n", addr, val);
}


uint8_t
MPU401_ReadData(mpu_t *mpu)
{
    uint8_t ret;

    ret = MSG_MPU_ACK;
    if (mpu->queue_used) {
	if (mpu->queue_pos >= MPU401_QUEUE)
		mpu->queue_pos -= MPU401_QUEUE;
	ret = mpu->queue[mpu->queue_pos];
	mpu->queue_pos++;
	mpu->queue_used--;
    }

    if (mpu->mode == M_UART) {
	if (mpu->state.irq_pending) {
		picintc(1 << mpu->irq);
		mpu->state.irq_pending = 0;
	}

	return ret;
    }

    if (mpu->queue_used == 0) {
	picintc(1 << mpu->irq);
	mpu->state.irq_pending = 0;
    }

    if (ret >= 0xf0 && ret <= 0xf7) {
	/* MIDI data request */
	mpu->state.channel = ret & 7;
	mpu->state.data_onoff = 0;
	mpu->state.cond_req = 0;
    }

    if (ret == MSG_MPU_COMMAND_REQ) {
	mpu->state.data_onoff = 0;
	mpu->state.cond_req = 1;
	if (mpu->condbuf.type != T_OVERFLOW) {
		mpu->state.block_ack = 1;
		MPU401_WriteCommand(mpu, mpu->condbuf.value[0]);
		if (mpu->state.command_byte)
			MPU401_WriteData(mpu, mpu->condbuf.value[1]);
	}
	mpu->condbuf.type = T_OVERFLOW;
    }

    if (ret == MSG_MPU_END || ret == MSG_MPU_CLOCK || ret == MSG_MPU_ACK) {
	mpu->state.data_onoff = -1;
	MPU401_EOIHandlerDispatch(mpu);
    }

    return(ret);
}


static void
mpu401_write(uint16_t addr, uint8_t val, priv_t priv)
{
    mpu_t *mpu = (mpu_t *)priv;

    DBGLOG(2, "MPU401 Write Port %04X, val %x\n", addr, val);

    switch (addr & 1) {
	case 0: /*Data*/
		MPU401_WriteData(mpu, val);
		DBGLOG(1, "Write Data (0x330) %X\n", val);
		break;

	case 1: /*Command*/
		MPU401_WriteCommand(mpu, val);
		DBGLOG(1, "Write Command (0x331) %x\n", val);
		break;
    }
}


static uint8_t
mpu401_read(uint16_t addr, priv_t priv)
{
    mpu_t *mpu = (mpu_t *)priv;
    uint8_t ret = 0;

    switch (addr & 1) {	
	case 0:	/* Read Data */
		ret = MPU401_ReadData(mpu);
		DBGLOG(1, "Read Data (0x330) %X\n", ret);
		break;

	case 1: /* Read Status */
		if (mpu->state.cmd_pending)
			ret = STATUS_OUTPUT_NOT_READY;
		if (! mpu->queue_used)
			ret = STATUS_INPUT_NOT_READY;
		ret |= 0x3f;
		DBGLOG(1, "Read Status (0x331) %x\n", ret);
		break;
    }

    DBGLOG(2, "MPU401 Read Port %04X, ret %x\n", addr, ret);

    return(ret);
}


static void
MPU401_Event(priv_t priv) 
{
    mpu_t *mpu = (mpu_t *)priv;
    int new_time;
    uint8_t i;

    DBGLOG(1, "MPU-401 event callback\n");

    if (mpu->mode == M_UART) {
	mpu401_event_callback = 0LL;
	return;
    }

    if (mpu->state.irq_pending)
	goto next_event;

    for (i = 0; i < 8; i++) { /* Decrease counters */
	if (mpu->state.amask & (1 << i)) {
		mpu->playbuf[i].counter--;
		if (mpu->playbuf[i].counter <= 0)
			UpdateTrack(mpu, i);
	}
    }

    if (mpu->state.conductor) {
	mpu->condbuf.counter--;
	if (mpu->condbuf.counter <= 0)
		UpdateConductor(mpu);
    }

    if (mpu->clock.clock_to_host) {
	mpu->clock.cth_counter++;
	if (mpu->clock.cth_counter >= mpu->clock.cth_rate) {
		mpu->clock.cth_counter = 0;
		mpu->state.req_mask |= (1 << 13);
	}
    }

    if (!mpu->state.irq_pending && mpu->state.req_mask)
				MPU401_EOIHandler(mpu);

next_event:
    /* mpu401_event_callback = 0LL; */
    new_time = ((mpu->clock.tempo * mpu->clock.timebase * mpu->clock.tempo_rel) / 0x40);
    if (new_time == 0)
	mpu401_event_callback = 0LL;
      else
	mpu401_event_callback += (int64_t) ((MPU401_TIMECONSTANT / new_time) * 1000LL * TIMER_USEC);
}


void
mpu401_init(mpu_t *mpu, uint16_t addr, int irq, int mode)
{
    mpu->status = STATUS_INPUT_NOT_READY;
    mpu->irq = irq;
    mpu->queue_used = 0;
    mpu->queue_pos = 0;
    mpu->mode = M_UART;

    /* Explanation:
     *
     * MPU-401 starting in intelligent mode = Full MPU-401 intelligent
     *	       mode capability;
     * MPU-401 starting in UART mode = Reduced MPU-401 intelligent mode
     *	       capability seen on the Sound Blaster 16/AWE32, only
     * 	       supporting commands 3F (set UART mode) and FF (reset).
     */
    mpu->intelligent = (mode == M_INTELLIGENT) ? 1 : 0;
    INFO("MPU-401: starting as %s (mode is %s)\n",
	 mpu->intelligent ? "INTELLIGENT" : "UART",
	 (mode == M_INTELLIGENT) ? "INTELLIGENT" : "UART");

    mpu401_event_callback = 0LL;
    mpu401_eoi_callback = 0LL;
    mpu401_reset_callback = 0LL;

    if (addr)
	io_sethandler(addr, 2,
		      mpu401_read, NULL, NULL, mpu401_write, NULL, NULL, mpu);
    io_sethandler(0x2A20, 16,
		  NULL, NULL, NULL, imf_write, NULL, NULL, mpu);

    timer_add(MPU401_Event, (priv_t)mpu,
	      &mpu401_event_callback, &mpu401_event_callback);
    timer_add(MPU401_EOIHandler, (priv_t)mpu,
	      &mpu401_eoi_callback, &mpu401_eoi_callback);
    timer_add(MPU401_ResetDone, (priv_t)mpu,
	      &mpu401_reset_callback, &mpu401_reset_callback);

    MPU401_Reset(mpu);
}


void
mpu401_device_add(void)
{
    if (MCA)
	device_add(&mpu401_mca_device);
      else
	device_add(&mpu401_device);
}


static uint8_t
mpu401_mca_read(int port, priv_t priv)
{
    mpu_t *dev = (mpu_t *)priv;

    return dev->pos_regs[port & 7];
}


static void
mpu401_mca_write(int port, uint8_t val, priv_t priv)
{
    mpu_t *dev = (mpu_t *)priv;
    uint16_t addr;

    if (port < 0x102) return;

    addr = (dev->pos_regs[2] & 2) ? 0x0330 : 0x1330;
    port &= 7;
    dev->pos_regs[port] = val;

    if (port == 2) {
	io_removehandler(addr, 2,
			 mpu401_read,NULL,NULL, mpu401_write,NULL,NULL, dev);

	addr = (dev->pos_regs[2] & 2) ? 0x0330 : 0x1330;

	io_removehandler(addr, 2,
			 mpu401_read,NULL,NULL, mpu401_write,NULL,NULL, dev);
    }
}


static priv_t
mpu401_standalone_init(const device_t *info, UNUSED(void *parent))
{
    mpu_t *dev;
    uint16_t base;
    int irq;

    dev = (mpu_t *)mem_alloc(sizeof(mpu_t));
    memset(dev, 0x00, sizeof(mpu_t));
 
    if (info->flags & DEVICE_MCA) {
	dev->pos_regs[0] = 0x0F;
	dev->pos_regs[1] = 0x6C;
	mca_add(mpu401_mca_read, mpu401_mca_write, dev);

	base = 0;	/* Tell mpu401_init() that this is the MCA variant. */
	irq = 2;	/* According to @6c0f.adf, the IRQ is fixed to 2. */
    } else {
	base = device_get_config_hex16("base");
	irq = device_get_config_int("irq");
    }

    mpu401_init(dev, base, irq, M_INTELLIGENT);

    return((priv_t)dev);
}


static void
mpu401_standalone_close(priv_t priv)
{
    mpu_t *dev = (mpu_t *)priv;

    free(dev);
}


static const device_config_t mpu401_standalone_config[] = {
    {
        "base", "MPU-401 Address", CONFIG_HEX16, "", 0x330,
        {
                {
                        "300H", 0x300
                },
                {
                        "330H", 0x330
                },
                {
                        NULL
                }
        }
    },
    {
        "irq", "MPU-401 IRQ", CONFIG_SELECTION, "", 9,
        {
                {
                        "IRQ 9", 9
                },
                {
                        "IRQ 3", 3
                },
                {
                        "IRQ 4", 4
                },
                {
                        "IRQ 5", 5
                },
                {
                        "IRQ 7", 7
                },
                {
                        "IRQ 10", 10
                },
                {
                        NULL
                }
        }
    },
    {
        NULL
    }
};

const device_t mpu401_device = {
    "Roland MPU-IPC-T",
    DEVICE_ISA,
    0,
    NULL,
    mpu401_standalone_init, mpu401_standalone_close, NULL,
    NULL, NULL, NULL, NULL,
    mpu401_standalone_config
};

const device_t mpu401_mca_device = {
    "MPU-401 MCA (Standalone)",
    DEVICE_MCA,
    0,
    NULL,
    mpu401_standalone_init, mpu401_standalone_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
