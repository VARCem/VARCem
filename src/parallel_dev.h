/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the parallel port-attached devices.
 *
 * Version:	@(#)parallel_dev.h	1.0.1	2018/04/07
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
#ifndef EMU_PARALLEL_DEV_H
# define EMU_PARALLE_DEVL_H


typedef struct _lpt_device_ {
    const char	*name;
    int		type;
    void	*(*init)(const struct _lpt_device_ *);
    void	(*close)(void *priv);
    void	(*write_data)(uint8_t val, void *priv);
    void	(*write_ctrl)(uint8_t val, void *priv);
    uint8_t	(*read_status)(void *priv);
} lpt_device_t;


extern const char *parallel_device_get_name(int id);
extern const char *parallel_device_get_internal_name(int id);
extern const lpt_device_t *parallel_device_get_device(int id);
extern int	parallel_device_get_from_internal_name(char *s);
extern void	parallel_devices_init(void);
extern void	parallel_devices_close(void);


#endif	/*EMU_PARALLEL_DEV_H*/
