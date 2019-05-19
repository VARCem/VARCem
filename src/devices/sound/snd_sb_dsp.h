/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the SoundBlaster DSP driver.
 *
 * Version:	@(#)snd_sb_dsp.h	1.0.3	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef SOUND_SNDB_DSP_H
# define SOUND_SNDB_DSP_H


typedef struct sb_dsp_t
{
        int sb_type;

        int sb_8_length,  sb_8_format,  sb_8_autoinit,  sb_8_pause,  sb_8_enable,  sb_8_autolen,  sb_8_output;
        int sb_8_dmanum;
        int sb_16_length, sb_16_format, sb_16_autoinit, sb_16_pause, sb_16_enable, sb_16_autolen, sb_16_output;
        int sb_16_dmanum;
        tmrval_t sb_pausetime;

        uint8_t sb_read_data[256];
        int sb_read_wp, sb_read_rp;
        int sb_speaker;
        int muted;

        int sb_data_stat;
	int uart_midi;
	int uart_irq;
	int onebyte_midi;

        int sb_irqnum;

        uint8_t sbe2;
        int sbe2count;

        uint8_t sb_data[8];

        int sb_freq;
        
        int16_t sbdat;
        int sbdat2;
        int16_t sbdatl, sbdatr;

        uint8_t sbref;
        int8_t sbstep;

        int sbdacpos;

        int sbleftright;

        int sbreset;
        uint8_t sbreaddat;
        uint8_t sb_command;
        uint8_t sb_test;
        tmrval_t sb_timei, sb_timeo;

        int sb_irq8, sb_irq16;

        uint8_t sb_asp_regs[256];
        
        tmrval_t sbenable, sb_enable_i;
        
        tmrval_t sbcount, sb_count_i;
        
        tmrval_t sblatcho, sblatchi;
        
        uint16_t sb_addr;
        
        int stereo;
        
        int asp_data_len;
        
        tmrval_t wb_time, wb_full;

	int busy_count;

        int record_pos_read;
        int record_pos_write;
        int16_t record_buffer[0xFFFF];
        int16_t buffer[SOUNDBUFLEN * 2];
        int pos;
} sb_dsp_t;

void sb_dsp_set_mpu(mpu_t *src_mpu);

void sb_dsp_init(sb_dsp_t *dsp, int type);
void sb_dsp_close(sb_dsp_t *dsp);

void sb_dsp_setirq(sb_dsp_t *dsp, int irq);
void sb_dsp_setdma8(sb_dsp_t *dsp, int dma);
void sb_dsp_setdma16(sb_dsp_t *dsp, int dma);
void sb_dsp_setaddr(sb_dsp_t *dsp, uint16_t addr);

void sb_dsp_speed_changed(sb_dsp_t *dsp);

void sb_dsp_poll(sb_dsp_t *dsp, int16_t *l, int16_t *r);

void sb_dsp_set_stereo(sb_dsp_t *dsp, int stereo);

void sb_dsp_update(sb_dsp_t *dsp);


#endif	/*SOUND_SNDB_DSP_H*/
