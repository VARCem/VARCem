/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the OPL interface.
 *
 * Version:	@(#)snd_opl.h	1.0.1	2018/02/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#ifndef SOUND_OPL_H
# define SOUND_OPL_H


typedef struct opl_t
{
        int chip_nr[2];
        
        int64_t timers[2][2];
        int64_t timers_enable[2][2];

        int16_t filtbuf[2];

        int16_t buffer[SOUNDBUFLEN * 2];
        int     pos;
} opl_t;

uint8_t opl2_read(uint16_t a, void *priv);
void opl2_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl2_l_read(uint16_t a, void *priv);
void opl2_l_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl2_r_read(uint16_t a, void *priv);
void opl2_r_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl3_read(uint16_t a, void *priv);
void opl3_write(uint16_t a, uint8_t v, void *priv);

void opl2_poll(opl_t *opl, int16_t *bufl, int16_t *bufr);
void opl3_poll(opl_t *opl, int16_t *bufl, int16_t *bufr);

void opl2_init(opl_t *opl);
void opl3_init(opl_t *opl);

void opl2_update2(opl_t *opl);
void opl3_update2(opl_t *opl);


#endif	/*SOUND_OPL_H*/
