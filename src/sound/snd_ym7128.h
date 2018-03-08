/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the YM7128 driver.
 *
 * Version:	@(#)snd_ym7128.h	1.0.1	2018/02/14
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
#ifndef SOUND_YM7128_H
# define SOUND_YM7128_H


typedef struct ym7128_t
{
        int a0, sci;
        uint8_t dat;
        
        int reg_sel;
        uint8_t regs[32];
        
        int gl[8], gr[8];
        int vm, vc, vl, vr;
        int c0, c1;
        int t[9];
        
        int16_t filter_dat;
        int16_t prev_l, prev_r;
        
        int16_t delay_buffer[2400];
        int delay_pos;
        
        int16_t last_samp;
} ym7128_t;

void ym7128_init(ym7128_t *ym7128);
void ym7128_write(ym7128_t *ym7128, uint8_t val);
void ym7128_apply(ym7128_t *ym7128, int16_t *buffer, int len);


#endif	/*SOUND_YM7128_H*/
