/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Intel PIIX and PIIX3 Xcelerators.
 *
 * Version:	@(#)intel_piix.h	1.0.1	2018/02/14
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
#ifndef EMU_INTELPIIX_H
# define EMU_INTELPIIX_H


extern void	piix_init(int card);

extern void	piix3_init(int card);

extern void	piix4_init(int card);

extern uint8_t	piix_bus_master_read(uint16_t port, void *priv);
extern void	piix_bus_master_write(uint16_t port, uint8_t val, void *priv);

extern int	piix_bus_master_get_count(int channel);

extern int	piix_bus_master_dma_read(int channel, uint8_t *data, int transfer_length);
extern int	piix_bus_master_dma_write(int channel, uint8_t *data, int transfer_length);

extern void	piix_bus_master_set_irq(int channel);


#endif	/*EMU_INTELPIIX_H*/
