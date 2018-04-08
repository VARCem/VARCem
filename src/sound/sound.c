/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Sound emulation core.
 *
 * Version:	@(#)sound.c	1.0.7	2018/04/08
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../emu.h"
#include "../device.h"
#include "../timer.h"
#include "../cdrom/cdrom.h"
#include "../plat.h"
#include "sound.h"
#include "midi.h"
#ifdef USE_FLUIDSYNTH
# include "midi_fluidsynth.h"
#endif
#include "snd_dbopl.h"
#include "snd_mpu401.h"
#include "snd_opl.h"
#include "snd_sb.h"
#include "snd_sb_dsp.h"
#include "snd_speaker.h"
#include "snd_ssi2001.h"
#include "filters.h"


typedef struct {
    void (*get_buffer)(int32_t *buffer, int len, void *p);
    void *priv;
} sndhnd_t;


int		sound_pos_global = 0;
int		sound_gain = 0;
volatile int	soundon = 1;


//static int	card_last = 0;
static sndhnd_t	handlers[8];
static sndhnd_t	process_handlers[8];
static int	handlers_num;
static int	process_handlers_num;
static int64_t	poll_time = 0LL,
		poll_latch;
static int32_t	*outbuffer;
static float	*outbuffer_ex;
static int16_t	*outbuffer_ex_int16;

static int16_t	cd_buffer[CDROM_NUM][CD_BUFLEN * 2];
static float	cd_out_buffer[CD_BUFLEN * 2];
static int16_t	cd_out_buffer_int16[CD_BUFLEN * 2];
static thread_t	*cd_thread_h;
static event_t	*cd_event;
static event_t	*cd_start_event;
static unsigned	cd_vol_l,
		cd_vol_r;
static int	cd_buf_update = CD_BUFLEN / SOUNDBUFLEN;
static int	cd_thread_enable = 0;
static volatile int cd_audioon = 0;


static void
cd_thread(void *param)
{
    float cd_buffer_temp[2] = {0.0, 0.0};
    float cd_buffer_temp2[2] = {0.0, 0.0};
    int32_t cd_buffer_temp4[2] = {0, 0};
    int32_t audio_vol_l;
    int32_t audio_vol_r;
    int channel_select[2];
    int c, d, i, has_audio;

    thread_set_event(cd_start_event);

    while (cd_audioon) {
	thread_wait_event(cd_event, -1);
	thread_reset_event(cd_event);

	if (!soundon || !cd_audioon) return;

	for (c = 0; c < CD_BUFLEN*2; c += 2) {
		if (sound_is_float) {
			cd_out_buffer[c] = 0.0;
			cd_out_buffer[c+1] = 0.0;
		} else {
			cd_out_buffer_int16[c] = 0;
			cd_out_buffer_int16[c+1] = 0;
		}
	}

	for (i = 0; i < CDROM_NUM; i++) {
		has_audio = 0;

		if (cdrom_drives[i].bus_type == CDROM_BUS_DISABLED) continue;

		if (cdrom_drives[i].handler->audio_callback) {
			cdrom_drives[i].handler->audio_callback(i, cd_buffer[i], CD_BUFLEN*2);
			has_audio = (cdrom_drives[i].bus_type && cdrom_drives[i].sound_on);
		} else
			continue;

		if (soundon && has_audio) {
			audio_vol_l = cdrom_mode_sense_get_volume(i, 0);
			audio_vol_r = cdrom_mode_sense_get_volume(i, 1);

			channel_select[0] = cdrom_mode_sense_get_channel(i, 0);
			channel_select[1] = cdrom_mode_sense_get_channel(i, 1);

			for (c = 0; c < CD_BUFLEN*2; c += 2) {
				/* First, transfer the CD audio data to the temporary buffer. */
				cd_buffer_temp[0] = (float)cd_buffer[i][c];
				cd_buffer_temp[1] = (float)cd_buffer[i][c+1];

				/* Then, adjust input from drive according to ATAPI/SCSI volume. */
				cd_buffer_temp[0] *= (float)audio_vol_l;
				cd_buffer_temp[0] /= 511.0;
				cd_buffer_temp[1] *= (float)audio_vol_r;
				cd_buffer_temp[1] /= 511.0;

				/*Apply ATAPI channel select*/
				cd_buffer_temp2[0] = cd_buffer_temp2[1] = 0.0;
				if (channel_select[0] & 1)
					cd_buffer_temp2[0] += cd_buffer_temp[0];
				if (channel_select[0] & 2)
					cd_buffer_temp2[1] += cd_buffer_temp[0];
				if (channel_select[1] & 1)
					cd_buffer_temp2[0] += cd_buffer_temp[1];
				if (channel_select[1] & 2)
					cd_buffer_temp2[1] += cd_buffer_temp[1];

				if (process_handlers_num) {
					cd_buffer_temp4[0] = (int32_t)cd_buffer_temp2[0];
					cd_buffer_temp4[1] = (int32_t)cd_buffer_temp2[1];

					for (d = 0; d < process_handlers_num; d++)
						process_handlers[d].get_buffer(cd_buffer_temp4, 1, process_handlers[d].priv);

					cd_buffer_temp2[0] = (float)cd_buffer_temp4[0];
					cd_buffer_temp2[1] = (float)cd_buffer_temp4[1];
				} else {
					/*Apply sound card CD volume*/
					cd_buffer_temp2[0] *= (float)cd_vol_l;
					cd_buffer_temp2[0] /= 65535.0;

					cd_buffer_temp2[1] *= (float)cd_vol_r;
					cd_buffer_temp2[1] /= 65535.0;
				}

				if (sound_is_float) {
					cd_out_buffer[c] += (float)(cd_buffer_temp2[0] / 32768.0);
					cd_out_buffer[c+1] += (float)(cd_buffer_temp2[1] / 32768.0);
				} else {
					if (cd_buffer_temp2[0] > 32767)
						cd_buffer_temp2[0] = 32767;
					if (cd_buffer_temp2[0] < -32768)
						cd_buffer_temp2[0] = -32768;
					if (cd_buffer_temp2[1] > 32767)
						cd_buffer_temp2[1] = 32767;
					if (cd_buffer_temp2[1] < -32768)
						cd_buffer_temp2[1] = -32768;

					cd_out_buffer_int16[c] += (int16_t)cd_buffer_temp2[0];
					cd_out_buffer_int16[c+1] += (int16_t)cd_buffer_temp2[1];
				}
			}
		}
	}

	if (sound_is_float)
		givealbuffer_cd(cd_out_buffer);
	else
		givealbuffer_cd(cd_out_buffer_int16);
    }
}


static void
cd_thread_end(void)
{
    if (! cd_audioon) return;

    cd_audioon = 0;

#if 0
    pclog("Waiting for CD Audio thread to terminate...\n");
#endif
    thread_set_event(cd_event);
    thread_wait(cd_thread_h, -1);
#if 0
    pclog("CD Audio thread terminated...\n");
#endif

    if (cd_event) {
	thread_destroy_event(cd_event);
	cd_event = NULL;
    }
    cd_thread_h = NULL;

    if (cd_start_event) {
	thread_destroy_event(cd_start_event);
	cd_start_event = NULL;
    }
}


static void
sound_poll(void *priv)
{
    poll_time += poll_latch;

    midi_poll();

    sound_pos_global++;
    if (sound_pos_global == SOUNDBUFLEN) {
	int c;

	memset(outbuffer, 0, SOUNDBUFLEN * 2 * sizeof(int32_t));

	for (c = 0; c < handlers_num; c++)
		handlers[c].get_buffer(outbuffer, SOUNDBUFLEN, handlers[c].priv);

	for (c = 0; c < SOUNDBUFLEN * 2; c++) {
		if (sound_is_float) {
			outbuffer_ex[c] = (float)((outbuffer[c]) / 32768.0);
		} else {
			if (outbuffer[c] > 32767)
				outbuffer[c] = 32767;
			if (outbuffer[c] < -32768)
				outbuffer[c] = -32768;

			outbuffer_ex_int16[c] = outbuffer[c];
		}
	}
	
	if (soundon) {
		if (sound_is_float)
			givealbuffer(outbuffer_ex);
		else
			givealbuffer(outbuffer_ex_int16);
	}
	
	if (cd_thread_enable) {
		cd_buf_update--;
		if (! cd_buf_update) {
			cd_buf_update = (48000 / SOUNDBUFLEN) / (CD_FREQ / CD_BUFLEN);
			thread_set_event(cd_event);
		}
	}

	sound_pos_global = 0;
    }
}


/* Reset the sound system. */
void
sound_reset(void)
{
    int i;

    pclog("SOUND: reset (current=%d)\n", sound_card_current);

    /* Kill the CD-Audio thread. */
    sound_cd_stop();

    /* Reset the sound module buffers. */
    if (outbuffer_ex != NULL)
	free(outbuffer_ex);
    if (outbuffer_ex_int16 != NULL)
	free(outbuffer_ex_int16);
    if (sound_is_float)
	outbuffer_ex = malloc(SOUNDBUFLEN * 2 * sizeof(float));
      else
	outbuffer_ex_int16 = malloc(SOUNDBUFLEN * 2 * sizeof(int16_t));

    /* Reset the MIDI devices. */
    midi_device_init();

    /* Reset OpenAL. */
    inital();

    timer_add(sound_poll, &poll_time, TIMER_ALWAYS_ENABLED, NULL);

    handlers_num = 0;

    process_handlers_num = 0;

    sound_cd_set_volume(65535, 65535);

    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom_drives[i].handler->audio_stop)
		cdrom_drives[i].handler->audio_stop(i);
    }

    /* Initialize the currently selected sound card. */
    snddev_reset();
//    card_last = sound_card_current;

    if (mpu401_standalone_enable)
	mpu401_device_add();

#if 0
    if (GAMEBLASTER)
	device_add(&cms_device);
#endif
}


/* Initialize the sound system (once.) */
void
sound_init(void)
{
    int drives = 0;
    int i;

    /* Initialize the OpenAL module. */
    initalmain(0,NULL);

#ifdef USE_FLUIDSYNTH
    /* Initialize the FluidSynth module. */
    fluidsynth_global_init();
#endif

    outbuffer_ex = NULL;
    outbuffer_ex_int16 = NULL;

    outbuffer = malloc(SOUNDBUFLEN * 2 * sizeof(int32_t));

    /* Set up the CD-AUDIO thread. */
    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom_drives[i].bus_type != CDROM_BUS_DISABLED)
		drives++;
    }	
    if (drives) {
	cd_audioon = 1;

	cd_start_event = thread_create_event();

	cd_event = thread_create_event();
	cd_thread_h = thread_create(cd_thread, NULL);

#if 0
	pclog("Waiting for CD start event...\n");
#endif
	thread_wait_event(cd_start_event, -1);
	thread_reset_event(cd_start_event);
#if 0
	pclog("Done!\n");
#endif
    } else
	cd_audioon = 0;

    cd_thread_enable = drives ? 1 : 0;
}


/* Close down the sound module. */
void
sound_close(void)
{
    /* Kill the CD-Audio thread if needed. */
    sound_cd_stop();

    /* Close down the MIDI module. */
    midi_close();

    /* Close the OpenAL interface. */
    closeal();
}


void
sound_cd_stop(void)
{
    int drives = 0;
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom_drives[i].bus_type != CDROM_BUS_DISABLED)
			drives++;
    }
    if (drives && !cd_thread_enable) {
	cd_audioon = 1;

	cd_start_event = thread_create_event();

	cd_event = thread_create_event();
	cd_thread_h = thread_create(cd_thread, NULL);

	thread_wait_event(cd_start_event, -1);
	thread_reset_event(cd_start_event);
    } else if (!drives && cd_thread_enable) {
	cd_thread_end();
    }

    cd_thread_enable = drives ? 1 : 0;
}


void
sound_cd_set_volume(unsigned int vol_l, unsigned int vol_r)
{
    cd_vol_l = vol_l;
    cd_vol_r = vol_r;
}


void
sound_add_handler(void (*get_buffer)(int32_t *buffer, int len, void *p), void *p)
{
    handlers[handlers_num].get_buffer = get_buffer;
    handlers[handlers_num].priv = p;
    handlers_num++;
}


void
sound_add_process_handler(void (*get_buffer)(int32_t *buffer, int len, void *p), void *p)
{
    process_handlers[process_handlers_num].get_buffer = get_buffer;
    process_handlers[process_handlers_num].priv = p;
    process_handlers_num++;
}


void
sound_speed_changed(void)
{
    poll_latch = (int64_t)((double)TIMER_USEC * (1000000.0 / 48000.0));
}
