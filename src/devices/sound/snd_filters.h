/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the various audio filters.
 *
 * Version:	@(#)snd_filters.h	1.0.1	2020/07/14
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
#ifndef SOUND_FILTERS_H
# define SOUND_FILTERS_H


#define NCoef 2


/* fc=350Hz */
static inline float
low_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.00049713569693400649f,
	0.00099427139386801299f,
	0.00049713569693400649f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.93522955470669530000f,
	0.93726236021404663000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];

    return(y[i][0]);
}


/* fc=350Hz */
static inline float
low_cut_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.96839970114733542000f,
	-1.93679940229467080000f,
	0.96839970114733542000f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.93522955471202770000f,
	0.93726236021916731000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];

    return(y[i][0]);
}


/* fc=3.5kHz */
static inline float
high_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.72248704753064896000f,
	-1.44497409506129790000f,
	0.72248704753064896000f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.36640781670578510000f,
	0.52352474706139873000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];

    return(y[i][0]);
}


/* fc=3.5kHz */
static inline float
high_cut_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.03927726802250377400f,
	0.07855453604500754700f,
	0.03927726802250377400f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.36640781666419950000f,
	0.52352474703279628000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];

    return(y[i][0]);
}


#undef NCoef
#define NCoef 2


/* fc=3.2kHz */
static inline float
sb_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.03356837051492005100f,
	0.06713674102984010200f,
	0.03356837051492005100f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.41898265221812010000f,
	0.55326988968868285000f
    };
#if 0
    static const float ACoef[NCoef+1] = {
	0.17529642630084405000f,
	0.17529642630084405000f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-0.64940759319751051000f
    };
#endif
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];

    return(y[i][0]);
}


#undef NCoef
#define NCoef 2


/* fc=150Hz */
static inline float
adgold_highpass_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.98657437157334349000f,
	-1.97314874314668700000f,
	0.98657437157334349000f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.97223372919758360000f,
	0.97261396931534050000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];
    
    return(y[i][0]);
}


/* fc=150Hz */
static inline float
adgold_lowpass_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.00009159473951071446f,
	0.00018318947902142891f,
	0.00009159473951071446f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.97223372919526560000f,
	0.97261396931306277000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];
    
    return(y[i][0]);
}


/* fc=56Hz */
static inline float
adgold_pseudo_stereo_iir(float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.00001409030866231767f,
	0.00002818061732463533f,
	0.00001409030866231767f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.98733021473466760000f,
	0.98738361004063568000f
    };
    static float y[NCoef+1];			// output samples
    static float x[NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[n] = x[n-1];
	y[n] = y[n-1];
    }

    /* Calculate the new output. */
    x[0] = NewSample;
    y[0] = ACoef[0] * x[0];
    for (n = 1; n <= NCoef; n++)
	y[0] += (ACoef[n] * x[n] - BCoef[n] * y[n]);
    
    return(y[0]);
}


/* fc=3.2kHz - probably incorrect */
static inline float
dss_iir(float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.03356837051492005100f,
	0.06713674102984010200f,
	0.03356837051492005100f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-1.41898265221812010000f,
	0.55326988968868285000f
    };
    static float y[NCoef+1];			// output samples
    static float x[NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[n] = x[n-1];
	y[n] = y[n-1];
    }

    /* Calculate the new output. */
    x[0] = NewSample;
    y[0] = ACoef[0] * x[0];
    for (n=1; n <= NCoef; n++)
	y[0] += ACoef[n] * x[n] - BCoef[n] * y[n];

    return(y[0]);
}


#undef NCoef
#define NCoef 1
/* Basic high pass to remove DC bias. fc=10Hz. */
static inline float
dac_iir(int i, float NewSample)
{
    static const float ACoef[NCoef+1] = {
	0.99901119820285345000f,
	-0.99901119820285345000f
    };
    static const float BCoef[NCoef+1] = {
	1.00000000000000000000f,
	-0.99869185905052738000f
    };
    static float y[2][NCoef+1];			// output samples
    static float x[2][NCoef+1];			// input samples
    int n;

    /* Shift the old samples. */
    for (n = NCoef; n > 0; n--) {
	x[i][n] = x[i][n-1];
	y[i][n] = y[i][n-1];
    }

    /* Calculate the new output. */
    x[i][0] = NewSample;
    y[i][0] = ACoef[0] * x[i][0];
    for (n = 1; n <= NCoef; n++)
	y[i][0] += ACoef[n] * x[i][n] - BCoef[n] * y[i][n];
    
    return(y[i][0]);
}


#define SB16_NCoef 51

extern float low_fir_sb16_coef[SB16_NCoef];

static inline float
low_fir_sb16(int i, float NewSample)
{
    static float x[2][SB16_NCoef+1];		//input samples
    static int pos = 0;
    float out = 0.0;
    int n;

    /* Calculate the new output. */
    x[i][pos] = NewSample;

    for (n = 0; n < ((SB16_NCoef+1)-pos) && n < SB16_NCoef; n++)
	out += low_fir_sb16_coef[n] * x[i][n+pos];
    for (; n < SB16_NCoef; n++)
	out += low_fir_sb16_coef[n] * x[i][(n+pos) - (SB16_NCoef+1)];

    if (i == 1) {
	pos++;
	if (pos > SB16_NCoef)
		pos = 0;
    }

    return(out);
}


#endif	/*SOUND_FILTERS*/
