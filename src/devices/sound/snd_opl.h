/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the OPL interface.
 *
 * Version:	@(#)snd_opl.h	1.0.3	2020/07/15
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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


typedef void	(*tmrfunc)(priv_t, int timer, tmrval_t period);

/* Define an OPLx chip. */
typedef struct {
#ifdef SOUND_OPL_NUKED_H
    nuked_t	*opl;
#else
    void	*opl;
#endif
    int8_t	is_opl3, do_cycles;

    uint16_t	port;
    uint8_t	status;
    uint8_t	status_mask;
    uint8_t	timer_ctrl;
    uint16_t	timer[2];

    tmrval_t	timers[2];
    tmrval_t	timers_enable[2];

    int		pos;
#if 0
    int32_t	filtbuf;
#endif
    int32_t	buffer[SOUNDBUFLEN * 2];
} opl_t;


extern void	opl_set_do_cycles(opl_t *dev, int8_t do_cycles);

extern uint8_t	opl2_read(uint16_t port, priv_t);
extern void	opl2_write(uint16_t port, uint8_t val, priv_t);
extern void	opl2_init(opl_t *);
extern void	opl2_update(opl_t *);

extern uint8_t	opl3_read(uint16_t port, priv_t);
extern void	opl3_write(uint16_t port, uint8_t val, priv_t);
extern void	opl3_init(opl_t *);
extern void	opl3_update(opl_t *);


#endif	/*SOUND_OPL_H*/
