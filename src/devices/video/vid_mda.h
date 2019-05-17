/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the MDA driver.
 *
 * Version:	@(#)vid_mda.h	1.0.2	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef VIDEO_MDA_H
# define VIDEO_MDA_H


#define MDA_FONT_ROM_PATH	L"video/ibm/mda/mda.rom"


typedef struct {
    int		type;

    mem_map_t	mapping;

    uint8_t	crtc[32];
    int		crtcreg;

    uint8_t	ctrl, stat;

    int64_t	dispontime, dispofftime;
    int64_t	vidtime;

    int		firstline, lastline;

    int		linepos, displine;
    int		vc, sc;
    uint16_t	ma, maback;
    int		con, coff, cursoron;
    int		dispon, blink;
    int64_t	vsynctime;
    int		vadj;

    uint8_t	cols[256][2][2];

    uint8_t	*vram;
} mda_t;


#ifdef EMU_DEVICE_H
extern const device_config_t mda_config[];
#endif


extern void	mda_init(mda_t *);
extern void	mda_out(uint16_t port, uint8_t val, priv_t);
extern uint8_t	mda_in(uint16_t port, priv_t);
extern void	mda_write(uint32_t addr, uint8_t val, priv_t);
extern uint8_t	mda_read(uint32_t addr, priv_t);
extern void	mda_recalctimings(mda_t *);
extern void	mda_poll(priv_t);
extern void	mda_setcol(mda_t *, int chr, int blink, int fg, uint8_t ink);


#endif	/*VIDEO_MDA_H*/
