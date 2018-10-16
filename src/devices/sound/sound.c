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
 * Version:	@(#)sound.c	1.0.13	2018/10/14
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
#define dbglog sound_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../cdrom/cdrom.h"
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
#include "filters.h"


typedef struct {
    void (*get_buffer)(int32_t *buffer, int len, void *p);
    void *priv;
} sndhnd_t;


#ifdef ENABLE_SOUND_LOG
int		sound_do_log = ENABLE_SOUND_LOG;
#endif
int		sound_pos_global = 0;
volatile int	soundon = 1;


static sndhnd_t	handlers[8];
static int	handlers_num;
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
    int32_t audio_vol_l;
    int32_t audio_vol_r;
    int ch_sel[2];
    int c, i, r, has_audio;

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

		if ((cdrom[i].bus_type == CDROM_BUS_DISABLED) || !cdrom[i].ops) continue;

		if (cdrom[i].ops->audio_callback) {
			r = cdrom[i].ops->audio_callback(&cdrom[i], cd_buffer[i], CD_BUFLEN*2);
			has_audio = (cdrom[i].bus_type && cdrom[i].sound_on/* && r*/);
		} else
			continue;

		if (soundon && has_audio) {
			if (cdrom[i].get_volume) {
				audio_vol_l = cdrom[i].get_volume(cdrom[i].p, 0);
				audio_vol_r = cdrom[i].get_volume(cdrom[i].p, 1);
			} else {
				audio_vol_l = 255;
				audio_vol_r = 255;
			}

			if (cdrom[i].get_channel) {
				ch_sel[0] = cdrom[i].get_channel(cdrom[i].p, 0);
				ch_sel[1] = cdrom[i].get_channel(cdrom[i].p, 1);
			} else {
				ch_sel[0] = 1;
				ch_sel[1] = 2;
			}

			if (! r) {
				for (c = 0; c < CD_BUFLEN*2; c += 2) {
					if (sound_is_float) {
						cd_out_buffer[c] += 0.0;
						cd_out_buffer[c+1] += 0.0;
					} else {
						cd_out_buffer_int16[c] += 0;
						cd_out_buffer_int16[c+1] += 0;
					}
				}
				continue;
			}

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
				if (ch_sel[0] & 1)
					cd_buffer_temp2[0] += cd_buffer_temp[0];
				if (ch_sel[0] & 2)
					cd_buffer_temp2[1] += cd_buffer_temp[0];
				if (ch_sel[1] & 1)
					cd_buffer_temp2[0] += cd_buffer_temp[1];
				if (ch_sel[1] & 2)
					cd_buffer_temp2[1] += cd_buffer_temp[1];

				/*Apply sound card CD volume*/
				cd_buffer_temp2[0] *= (float)cd_vol_l;
				cd_buffer_temp2[0] /= 65535.0;

				cd_buffer_temp2[1] *= (float)cd_vol_r;
				cd_buffer_temp2[1] /= 65535.0;

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
		openal_buffer_cd(cd_out_buffer);
	else
		openal_buffer_cd(cd_out_buffer_int16);
    }
}


static void
cd_thread_end(void)
{
    if (! cd_audioon) return;

    cd_audioon = 0;

    DEBUG("Waiting for CD Audio thread to terminate...\n");

    thread_set_event(cd_event);
    thread_wait(cd_thread_h, -1);

    DEBUG("CD Audio thread terminated...\n");

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
			openal_buffer(outbuffer_ex);
		else
			openal_buffer(outbuffer_ex_int16);
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


void
sound_log(int level, const char *fmt, ...)
{
#ifdef ENABLE_SOUND_LOG
    va_list ap;

    if (sound_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


/* Reset the sound system. */
void
sound_reset(void)
{
    int i;

    INFO("SOUND: reset (current=%i)\n", sound_card);

    /* Kill the CD-Audio thread. */
    sound_cd_stop();

    /* Reset the sound module buffers. */
    if (outbuffer_ex != NULL)
	free(outbuffer_ex);
    if (outbuffer_ex_int16 != NULL)
	free(outbuffer_ex_int16);
    if (sound_is_float)
	outbuffer_ex = (float *)mem_alloc(SOUNDBUFLEN * 2 * sizeof(float));
      else
	outbuffer_ex_int16 = (int16_t *)mem_alloc(SOUNDBUFLEN * 2 * sizeof(int16_t));

    /* Reset the sound module data handlers. */
    handlers_num = 0;

    /* Reset the MIDI devices. */
    midi_device_init();

    /* Reset OpenAL. */
    openal_reset();

    timer_add(sound_poll, &poll_time, TIMER_ALWAYS_ENABLED, NULL);

    sound_cd_set_volume(65535, 65535);

    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom[i].ops && cdrom[i].ops->audio_stop)
	       	cdrom[i].ops->audio_stop(&cdrom[i]);
    }

    /* Initialize the PC Speaker device. */
    speaker_reset();

    /* Initialize the currently selected sound card. */
    sound_card_reset();

    /* Enable the standlone MPU401 if needed. */
    if (mpu401_standalone_enable)
	mpu401_device_add();
}


/* Initialize the sound system (once.) */
void
sound_init(void)
{
    int drives, i;

    /* Initialize the OpenAL module. */
    openal_init();

#ifdef USE_FLUIDSYNTH
    /* Initialize the FluidSynth module. */
    fluidsynth_global_init();
#endif

    handlers_num = 0;

    outbuffer_ex = NULL;
    outbuffer_ex_int16 = NULL;

    outbuffer = (int32_t *)mem_alloc(SOUNDBUFLEN * 2 * sizeof(int32_t));

    /* Set up the CD-AUDIO thread. */
    drives = 0;
    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom[i].bus_type != CDROM_BUS_DISABLED)
		drives++;
    }	

    if (drives) {
	cd_audioon = 1;

	cd_start_event = thread_create_event();

	cd_event = thread_create_event();
	cd_thread_h = thread_create(cd_thread, NULL);

	DEBUG("Waiting for CD start event...\n");

	thread_wait_event(cd_start_event, -1);
	thread_reset_event(cd_start_event);

	DEBUG("Done!\n");
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
    openal_close();
}


void
sound_cd_stop(void)
{
    int drives = 0;
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom[i].ops && cdrom[i].ops->audio_stop)
	       	cdrom[i].ops->audio_stop(&cdrom[i]);
	if (cdrom[i].bus_type != CDROM_BUS_DISABLED)
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
sound_speed_changed(void)
{
    poll_latch = (int64_t)((double)TIMER_USEC * (1000000.0 / 48000.0));
}
