/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the SCSI CD-ROM module.
 *
 * Version:	@(#)scsi_cdrom.h	1.0.3	2018/10/19
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
#ifndef EMU_SCSI_CDROM_H
#define EMU_SCSI_CDROM_H


#define CDROM_TIME (5LL * 100LL * (1LL << TIMER_SHIFT))


#ifdef EMU_SCSI_DEVICE_H
typedef struct {
    mode_sense_pages_t ms_pages_saved;

    cdrom_t *drv;

    uint8_t *buffer,
	    atapi_cdb[16],
	    current_cdb[16],
	    sense[256];

    uint8_t status, phase,
	    error, id,
	    features, pad0,
	    pad1, pad2;

    uint16_t request_length, max_transfer_len;

    int requested_blocks, packet_status,
	total_length, do_page_save,
	unit_attention;

    uint32_t sector_pos, sector_len,
	     packet_len, pos;

    int64_t callback;

    int media_status, data_pos,
	request_pos, total_read,
	old_len;

    uint8_t previous_command,
	    pad3, pad4, pad5;
} scsi_cdrom_t;
#endif

#define scsi_cdrom_sense_error	dev->sense[0]
#define scsi_cdrom_sense_key	dev->sense[2]
#define scsi_cdrom_asc		dev->sense[12]
#define scsi_cdrom_ascq		dev->sense[13]


//extern void	scsi_cdrom_reset(void *p);


#endif	/*EMU_SCSI_CDROM_H*/
