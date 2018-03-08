/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Direct3D 9 rendererer and screenshots taking.
 *
 * Version:	@(#)win_d3d.h	1.0.3	2018/03/07
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
#ifndef WIN_D3D_H
# define WIN_D3D_H


#ifdef __cplusplus
extern "C" {
#endif

extern int	d3d_init(HWND h);
extern int	d3d_init_fs(HWND h);
extern void	d3d_close(void);
extern void	d3d_reset(void);
extern void	d3d_reset_fs(void);
extern int	d3d_pause(void);
extern void	d3d_resize(int x, int y);
extern void	d3d_take_screenshot(wchar_t *fn);

#ifdef __cplusplus
}
#endif


#endif	/*WIN_D3D_H*/
