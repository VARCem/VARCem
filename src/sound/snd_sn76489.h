/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the TI SN74689 PSG driver.
 *
 * Version:	@(#)snd_sn76489.h	1.0.1	2018/02/14
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
#ifndef SOUND_SN76489_H
# define SOUND_SN76489_H


enum
{
        SN76496,
        NCR8496,
        PSSJ
};

extern device_t sn76489_device;
extern device_t ncr8496_device;

extern int sn76489_mute;

typedef struct sn76489_t
{
        int stat[4];
        int latch[4], count[4];
        int freqlo[4], freqhi[4];
        int vol[4];
        uint32_t shift;
        uint8_t noise;
        int lasttone;
        uint8_t firstdat;
        int type;
        int extra_divide;
        
        int16_t buffer[SOUNDBUFLEN];
        int pos;
        
        double psgconst;
} sn76489_t;

void sn76489_init(sn76489_t *sn76489, uint16_t base, uint16_t size, int type, int freq);
void sn74689_set_extra_divide(sn76489_t *sn76489, int enable);


#endif	/*SOUND_SN76489_H*/
