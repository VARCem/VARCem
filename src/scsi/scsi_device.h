/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the generic SCSI device command handler.
 *
 * Version:	@(#)scsi_device.h	1.0.2	2018/03/08
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#ifndef SCSI_DEVICE_H
# define SCSI_DEVICE_H


typedef struct {
    int		state;
    int		new_state;
    int		clear_req;
    uint32_t	bus_in, bus_out;
    int		dev_id;

    int		command_pos;
    uint8_t	command[20];
    int		data_pos;

    int		change_state_delay;
    int		new_req_delay;	
} scsi_bus_t;


extern uint8_t	*scsi_device_sense(uint8_t id, uint8_t lun);
extern void	scsi_device_type_data(uint8_t id, uint8_t lun,
				      uint8_t *type, uint8_t *rmb);
extern int64_t	scsi_device_get_callback(uint8_t scsi_id, uint8_t scsi_lun);
extern void	scsi_device_request_sense(uint8_t scsi_id, uint8_t scsi_lun,
					  uint8_t *buffer,
					  uint8_t alloc_length);
extern int	scsi_device_read_capacity(uint8_t id, uint8_t lun,
					  uint8_t *cdb, uint8_t *buffer,
					  uint32_t *len);
extern int	scsi_device_present(uint8_t id, uint8_t lun);
extern int	scsi_device_valid(uint8_t id, uint8_t lun);
extern int	scsi_device_cdb_length(uint8_t id, uint8_t lun);
extern void	scsi_device_command(uint8_t id, uint8_t lun, int cdb_len,
				    uint8_t *cdb);
extern void	scsi_device_command_phase0(uint8_t scsi_id, uint8_t scsi_lun,
					   int cdb_len, uint8_t *cdb);
extern void	scsi_device_command_phase1(uint8_t scsi_id, uint8_t scsi_lun);
extern int32_t	*scsi_device_get_buf_len(uint8_t scsi_id, uint8_t scsi_lun);

extern int scsi_bus_update(scsi_bus_t *bus, int bus_assert);
extern int scsi_bus_read(scsi_bus_t *bus);
extern int scsi_bus_match(scsi_bus_t *bus, int bus_assert);


#endif	/*SCSI_DEVICE_H*/
