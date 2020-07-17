/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the NukedOPL3 driver.
 *
 * Version:	@(#)snd_opl_nuked.h	1.0.5	2020/07/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#ifndef SOUND_OPL_NUKED_H
# define SOUND_OPL_NUKED_H


extern priv_t	nuked_init(uint32_t sample_rate);
extern void	nuked_close(priv_t);

extern uint16_t	nuked_write_addr(priv_t, uint16_t port, uint8_t val);
extern void	nuked_write_reg(priv_t, uint16_t reg, uint8_t v);
extern void	nuked_write_reg_buffered(priv_t, uint16_t reg, uint8_t v);

extern void	nuked_generate(priv_t, int32_t *buf);
extern void	nuked_generate_resampled(priv_t, int32_t *buf);
extern void	nuked_generate_stream(priv_t, int32_t *sndptr, uint32_t num);


#endif	/*SOUND_OPL_NUKED_H*/
