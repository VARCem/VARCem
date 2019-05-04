/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the DOSbox OPL emulator.
 *
 * Version:	@(#)snd_dbopl.h	1.0.4	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#ifndef SOUND_DBOPL_H
# define SOUND_DBOPL_H


#ifdef __cplusplus
extern "C" {
#endif

void	opl_init(void (*timer_callback)(void *param, int timer, int64_t period), void *timer_param, int nr, int is_opl3);
void	opl_write(int nr, uint16_t addr, uint8_t val);
uint8_t	opl_read(int nr, uint16_t addr);
void	opl_status_update(int nr);
void	opl_timer_over(int nr, int timer);
void	opl2_update(int nr, int16_t *buffer, int samples);
void	opl3_update(int nr, int16_t *buffer, int samples);

#ifdef __cplusplus
}
#endif


#endif	/*SOUND_DBOPL_H*/
