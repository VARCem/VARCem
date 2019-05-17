/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the NCR 5380 series of SCSI Host Adapters
 *		made by NCR. These controllers were designed for
 *		the ISA bus.
 *
 * Version:	@(#)scsi_ncr5380.c	1.0.4	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#ifndef SCSI_NCR5380_H
# define SCSI_NCR5380_H


extern const device_t scsi_ncr53c80_onboard_device;

extern const device_t scsi_lcs6821n_device;
extern const device_t scsi_rt1000b_device;
extern const device_t scsi_t130b_device;
extern const device_t scsi_scsiat_device;


extern void	scsi_ncr5380_set_info(priv_t priv, int base, int irq);


#endif	/*SCSI_NCR5380_H*/
