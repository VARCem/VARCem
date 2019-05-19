/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		IBM CGA composite filter, borrowed from reenigne's DOSBox
 *		patch and ported to C.
 *
 *		New algorithm by reenigne.
 *		Works in all CGA modes/color settings and can simulate
 *		older and newer CGA revisions.
 *
 *		Reworked to have its data on the heap.
 *
 * Version:	@(#)vid_cga_comp.c	1.0.9	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Andrew Jenner (reenigne), <andrew@reenigne.org>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2015-2018 Andrew Jenner.
 *		Copyright 2016-2018 Miran Grca.
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
#include <math.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../mem.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_cga_comp.h"


static const double tau = 6.28318531; /* == 2*pi */

static const uint8_t chroma_multiplexer[256] = {
      2,  2,  2,  2, 114,174,  4,  3,   2,  1,133,135,   2,113,150,  4,
    133,  2,  1, 99, 151,152,  2,  1,   3,  2, 96,136, 151,152,151,152,
      2, 56, 62,  4, 111,250,118,  4,   0, 51,207,137,   1,171,209,  5,
    140, 50, 54,100, 133,202, 57,  4,   2, 50,153,149, 128,198,198,135,
     32,  1, 36, 81, 147,158,  1, 42,  33,  1,210,254,  34,109,169, 77,
    177,  2,  0,165, 189,154,  3, 44,  33,  0, 91,197, 178,142,144,192,
      4,  2, 61, 67, 117,151,112, 83,   4,  0,249,255,   3,107,249,117,
    147,  1, 50,162, 143,141, 52, 54,   3,  0,145,206, 124,123,192,193,
     72, 78,  2,  0, 159,208,  4,  0,  53, 58,164,159,  37,159,171,  1,
    248,117,  4, 98, 212,218,  5,  2,  54, 59, 93,121, 176,181,134,130,
      1, 61, 31,  0, 160,255, 34,  1,   1, 58,197,166,   0,177,194,  2,
    162,111, 34, 96, 205,253, 32,  1,   1, 57,123,125, 119,188,150,112,
     78,  4,  0, 75, 166,180, 20, 38,  78,  1,143,246,  42,113,156, 37,
    252,  4,  1,188, 175,129,  1, 37, 118,  4, 88,249, 202,150,145,200,
     61, 59, 60, 60, 228,252,117, 77,  60, 58,248,251,  81,212,254,107,
    198, 59, 58,169, 250,251, 81, 80, 100, 58,154,250, 251,252,252,252
};
static const double intensity[4] = {
    77.175381, 88.654656, 166.564623, 174.228438
};

#define NEW_CGA(c,i,r,g,b) (((c)/0.72)*0.29 + ((i)/0.28)*0.32 + ((r)/0.28)*0.1 + ((g)/0.28)*0.22 + ((b)/0.28)*0.07)

/* 2048x1536 is the maximum we can possibly support. */
#define SCALER_MAXWIDTH 2048


typedef struct {
    int		new_cga;

    int		video_sharpness;
    double	mode_brightness,
		mode_contrast,
		mode_hue,
		min_v, max_v;
    double	video_ri, video_rq, video_gi,
		video_gq, video_bi, video_bq;
    double	brightness,
		contrast,
		saturation,
		sharpness,
		hue_offset;

    int		temp[SCALER_MAXWIDTH + 10];
    int		atemp[SCALER_MAXWIDTH + 2];
    int		btemp[SCALER_MAXWIDTH + 2];

    int		table[1024];
} cga_comp_t;


static uint8_t
byte_clamp(int v)
{
    v >>= 13;

    return v < 0 ? 0 : (v > 255 ? 255 : v);
}


void
cga_comp_process(priv_t priv, uint8_t cgamode, uint8_t border,
		 uint32_t blocks/*, int8_t doublewidth*/, pel_t *pels)
{
    cga_comp_t *state = (cga_comp_t *)priv;
    uint32_t x2;
    pel_t *ptr;
    int x, w = blocks*4;
    int *o, *b2, *i, *ap, *bp;

#define COMPOSITE_CONVERT(I, Q) do { \
        i[1] = (i[1]<<3) - ap[1]; \
        a = ap[0]; \
        b = bp[0]; \
        c = i[0]+i[0]; \
        d = i[-1]+i[1]; \
        y = ((c+d)<<8) + state->video_sharpness*(c-d); \
        rr = (int)(y + state->video_ri*(I) + state->video_rq*(Q)); \
        gg = (int)(y + state->video_gi*(I) + state->video_gq*(Q)); \
        bb = (int)(y + state->video_bi*(I) + state->video_bq*(Q)); \
        ++i; \
        ++ap; \
        ++bp; \
        ptr[0].val = (byte_clamp(rr)<<16) | (byte_clamp(gg)<<8) | byte_clamp(bb); \
        ptr++; \
    } while (0)

#define OUT(v) do { *o = (v); ++o; } while (0)

    /* Simulate CGA composite output. */
    ptr = pels;
    o = state->temp;
    b2 = &state->table[border * 68];
    for (x = 0; x < 4; ++x)
	OUT(b2[(x+3)&3]);
    OUT(state->table[(border<<6) | ((ptr[0].pal & 0x0f)<<2) | 3]);

    for (x = 0; x < w-1; ++x) {
	OUT(state->table[((ptr[0].pal & 0x0f)<<6) | ((ptr[1].pal & 0x0f)<<2) | (x&3)]);
	ptr++;
    }
    OUT(state->table[((ptr[0].pal & 0x0f)<<6) | (border<<2) | 3]);

    for (x = 0; x < 5; ++x)
	OUT(b2[x&3]);

    ptr = pels;
    if ((cgamode & 4) != 0) {
	/* Decode. */
	i = state->temp + 5;
	for (x2 = 0; x2 < blocks*4; ++x2) {
		int c = (i[0]+i[0])<<3;
		int d = (i[-1]+i[1])<<3;
		int y = ((c+d)<<8) + state->video_sharpness*(c-d);
		++i;
		ptr[0].val = byte_clamp(y)*0x10101;
		ptr++;
	}
    } else {
	/* Store chroma. */
	i = state->temp + 4;
	ap = state->atemp + 1;
	bp = state->btemp + 1;
	for (x = -1; x < w + 1; ++x) {
		ap[x] = i[-4]-((i[-2]-i[0]+i[2])<<1)+i[4];
		bp[x] = (i[-3]-i[-1]+i[1]-i[3])<<1;
		++i;
	}

	/* Decode. */
	i = state->temp + 5;
	i[-1] = (i[-1]<<3) - ap[-1];
	i[0] = (i[0]<<3) - ap[0];
	for (x2 = 0; x2 < blocks; ++x2) {
		int y,a,b,c,d,rr,gg,bb;

		COMPOSITE_CONVERT(a, b);
		COMPOSITE_CONVERT(-b, a);
		COMPOSITE_CONVERT(-a, -b);
		COMPOSITE_CONVERT(b, -a);
	}
    }
}


void
cga_comp_update(priv_t priv, uint8_t cgamode)
{
    static const double ri = 0.9563;
    static const double rq = 0.6210;
    static const double gi = -0.2721;
    static const double gq = -0.6474;
    static const double bi = -1.1069;
    static const double bq = 1.7046;
    cga_comp_t *state = (cga_comp_t *)priv;
    double iq_adjust_i, iq_adjust_q;
    double i0, i3, mode_saturation;
    double q, a, s, r;
    double c, i, v;
    int x;

    if (state->new_cga) {
	i0 = intensity[0];
	i3 = intensity[3];
	state->min_v = NEW_CGA(chroma_multiplexer[0], i0, i0, i0, i0);
	state->max_v = NEW_CGA(chroma_multiplexer[255], i3, i3, i3, i3);
    } else {
	state->min_v = chroma_multiplexer[0] + intensity[0];
	state->max_v = chroma_multiplexer[255] + intensity[3];
    }
    state->mode_contrast = 256 / (state->max_v - state->min_v);
    state->mode_brightness = -state->min_v * state->mode_contrast;

    if ((cgamode & 3) == 1)
	state->mode_hue = 14;
    else
	state->mode_hue = 4;

    state->mode_contrast *= state->contrast * (state->new_cga ? 1.2 : 1)/100;  /* new CGA: 120% */
    state->mode_brightness += (state->new_cga ? state->brightness-10 : state->brightness)*5;     /* new CGA: -10 */
    mode_saturation = (state->new_cga ? 4.35 : 2.9)*state->saturation/100;  /* new CGA: 150% */

    for (x = 0; x < 1024; ++x) {
	int phase = x & 3;
	int right = (x >> 2) & 15;
	int left = (x >> 6) & 15;
	int rc = right;
	int lc = left;

	if ((cgamode & 4) != 0) {
		rc = (right & 8) | ((right & 7) != 0 ? 7 : 0);
		lc = (left & 8) | ((left & 7) != 0 ? 7 : 0);
	}
	c = chroma_multiplexer[((lc & 7) << 5) | ((rc & 7) << 2) | phase];
	i = intensity[(left >> 3) | ((right >> 2) & 2)];
	if (state->new_cga) {
		double d_r = intensity[((left >> 2) & 1) | ((right >> 1) & 2)];
		double d_g = intensity[((left >> 1) & 1) | (right & 2)];
		double d_b = intensity[(left & 1) | ((right << 1) & 2)];
		v = NEW_CGA(c, i, d_r, d_g, d_b);
	} else
		v = c + i;
	state->table[x] = (int) (v*state->mode_contrast + state->mode_brightness);
    }

    i = state->table[6*68] - state->table[6*68 + 2];
    q = state->table[6*68 + 1] - state->table[6*68 + 3];

    a = tau*(33 + 90 + state->hue_offset + state->mode_hue)/360.0;
    c = cos(a);
    s = sin(a);
    r = 256*mode_saturation/sqrt(i*i+q*q);

    iq_adjust_i = -(i*c + q*s)*r;
    iq_adjust_q = (q*c - i*s)*r;

    state->video_ri = (int) (ri * iq_adjust_i + rq * iq_adjust_q);
    state->video_rq = (int) (-ri * iq_adjust_q + rq * iq_adjust_i);
    state->video_gi = (int) (gi * iq_adjust_i + gq * iq_adjust_q);
    state->video_gq = (int) (-gi * iq_adjust_q + gq * iq_adjust_i);
    state->video_bi = (int) (bi * iq_adjust_i + bq * iq_adjust_q);
    state->video_bq = (int) (-bi * iq_adjust_q + bq * iq_adjust_i);
    state->video_sharpness = (int) ((state->sharpness * 256) / 100);
}


priv_t
cga_comp_init(int revision)
{
    cga_comp_t *state;

    state = (cga_comp_t *)mem_alloc(sizeof(cga_comp_t));
    memset(state, 0x00, sizeof(cga_comp_t));
    state->new_cga = revision;

    /* Making sure this gets reset after reset. */
    state->contrast = 100;
    state->saturation = 100;

    cga_comp_update(state, 0);

    return((priv_t)state);
}


void
cga_comp_close(priv_t priv)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    free(state);
}


#if 0	/*NOT_USED*/
void
IncreaseHue(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->hue_offset += 5.0;

    cga_comp_update(state, cgamode);
}


void
DecreaseHue(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->hue_offset -= 5.0;

    cga_comp_update(state, cgamode);
}


void
IncreaseSaturation(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->saturation += 5;

    cga_comp_update(state, cgamode);
}


void
DecreaseSaturation(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->saturation -= 5;

    cga_comp_update(state, cgamode);
}


void
IncreaseContrast(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->contrast += 5;

    cga_comp_update(state, cgamode);
}


void
DecreaseContrast(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->contrast -= 5;

    cga_comp_update(state, cgamode);
}


void
IncreaseBrightness(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->brightness += 5;

    cga_comp_update(state, cgamode);
}


void
DecreaseBrightness(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->brightness -= 5;

    cga_comp_update(state, cgamode);
}


void
IncreaseSharpness(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->sharpness += 10;

    cga_comp_update(state, cgamode);
}


void
DecreaseSharpness(priv_t priv, uint8_t cgamode)
{
    cga_comp_t *state = (cga_comp_t *)priv;

    state->sharpness -= 10;

    cga_comp_update(state, cgamode);
}
#endif	/*NOT_USED*/
