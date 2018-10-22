/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the SVGA renderers.
 *
 * Version:	@(#)vid_svga_render.h	1.0.3	2018/10/22
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
#ifndef VIDEO_SVGA_RENDER_H
# define VIDEO_SVGA_RENDER_H


extern int	firstline_draw, lastline_draw;
extern int	displine;
extern int	sc;
extern uint32_t	ma, ca;
extern int	con, cursoron, cgablink;
extern int	scrollcache;
extern uint8_t	edatlookup[4][4];


extern void	svga_render_blank(svga_t *svga);
extern void	svga_render_text_40(svga_t *svga);
extern void	svga_render_text_80(svga_t *svga);
extern void	svga_render_text_80_ksc5601(svga_t *svga);

extern void	svga_render_2bpp_lowres(svga_t *svga);
extern void	svga_render_2bpp_highres(svga_t *svga);
extern void	svga_render_4bpp_lowres(svga_t *svga);
extern void	svga_render_4bpp_highres(svga_t *svga);
extern void	svga_render_8bpp_lowres(svga_t *svga);
extern void	svga_render_8bpp_highres(svga_t *svga);
extern void	svga_render_15bpp_lowres(svga_t *svga);
extern void	svga_render_15bpp_highres(svga_t *svga);
extern void	svga_render_16bpp_lowres(svga_t *svga);
extern void	svga_render_16bpp_highres(svga_t *svga);
extern void	svga_render_24bpp_lowres(svga_t *svga);
extern void	svga_render_24bpp_highres(svga_t *svga);
extern void	svga_render_32bpp_lowres(svga_t *svga);
extern void	svga_render_32bpp_highres(svga_t *svga);
extern void	svga_render_ABGR8888_lowres(svga_t *svga);
extern void	svga_render_ABGR8888_highres(svga_t *svga);
extern void	svga_render_RGBA8888_lowres(svga_t *svga);
extern void	svga_render_RGBA8888_highres(svga_t *svga);
extern void	svga_render_mixed_highres(svga_t *svga);

extern void	(*svga_render)(svga_t *svga);


#endif	/*VIDEO_SVGA_RENDER_H*/
