/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		Emulation of BusLogic BT-542B ISA and BT-958D PCI SCSI
 *		controllers.
 *
 * Version:	@(#)scsi_buslogic.h	1.0.2	2020/06/01
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#ifndef SCSI_BUSLOGIC_H
# define SCSI_BUSLOGIC_H


extern const device_t buslogic_device;
extern const device_t buslogic_542b_device;
extern const device_t buslogic_545s_device;
extern const device_t buslogic_640a_device;
extern const device_t buslogic_445s_device;
extern const device_t buslogic_pci_device;

extern void	BuslogicDeviceReset(void *p);
  
  
#endif	/*SCSI_BUSLOGIC_H*/
