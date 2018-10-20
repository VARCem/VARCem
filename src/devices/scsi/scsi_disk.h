/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of SCSI fixed and removable disks.
 *
 * Version:	@(#)scsi_disk.h	1.0.5	2018/10/19
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
#ifndef EMU_SCSI_DISK_H
# define EMU_SCSI_DISK_H


typedef struct {
    mode_sense_pages_t ms_pages_saved;

    hard_disk_t *drv;

    uint8_t *temp_buffer,
	    pad[16],	/* This is atapi_cdb in ATAPI-supporting devices,
			   and pad in SCSI-only devices. */
	    current_cdb[16],
	    sense[256];

    uint8_t status, phase,
	    error, id,
	    pad0, pad1,
	    pad2, pad3;

    uint16_t request_length, pad4;

    int requested_blocks, packet_status,
	total_length, do_page_save,
	unit_attention;

    uint32_t sector_pos, sector_len,
	     packet_len, pos;

    int64_t callback;
} scsi_disk_t;


extern scsi_disk_t	*scsi_disk[HDD_NUM];


#ifdef USE_REMOVABLE_DISK
extern void	scsi_disk_insert(int id);
extern void	scsi_reloadhd(int id);
extern void	scsi_unloadhd(int scsi_id, int scsi_lun, int id);
#endif

extern void	scsi_disk_global_init(void);
extern void	scsi_disk_hard_reset(void);
extern void	scsi_disk_close(void);


#endif	/*EMU_SCSI_DISK_H*/
