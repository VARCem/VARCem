/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the devices that attach to the Game Port.
 *
 * Version:	@(#)game_dev.h	1.0.1	2018/04/26
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018 Fred N. van Kempen.
 *		Copyright 2008-2017 Sarah Walker.
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
#ifndef EMU_GAME_DEV_H
# define EMU_GAME_DEV_H


#define AXIS_NOT_PRESENT	-99999


typedef struct {
    const char	*name;

    void	*(*init)(void);
    void	(*close)(void *priv);
    uint8_t	(*read)(void *priv);
    void	(*write)(void *priv);
    int		(*read_axis)(void *priv, int axis);
    void	(*a0_over)(void *priv);

    int		axis_count,
		button_count,
		pov_count;
    int		max_joysticks;

    const char	*axis_names[8];
    const char	*button_names[32];
    const char	*pov_names[4];
} gamedev_t;


#ifdef __cplusplus
extern "C" {
#endif

extern const gamedev_t	*gamedev_get_device(int js);
extern const char	*gamedev_get_name(int js);
extern int		gamedev_get_max_joysticks(int js);
extern int		gamedev_get_axis_count(int js);
extern int		gamedev_get_button_count(int js);
extern int		gamedev_get_pov_count(int js);
extern const char	*gamedev_get_axis_name(int js, int id);
extern const char	*gamedev_get_button_name(int js, int id);
extern const char	*gamedev_get_pov_name(int js, int id);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_GAME_DEV_H*/
