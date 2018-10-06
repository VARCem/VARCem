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
 * Version:	@(#)scsi_disk.h	1.0.3	2018/09/19
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
#ifndef SCSI_DISK_H
# define SCSI_DISK_H


typedef struct {
    mode_sense_pages_t ms_pages_saved;

    hard_disk_t *drv;

    /* Stuff for SCSI hard disks. */
    uint8_t status, phase,
	    error, id,
	    current_cdb[16],
	    sense[256];

    uint16_t request_length;

    int requested_blocks, block_total,
	packet_status, callback,
	block_descriptor_len,
	total_length, do_page_save;

    uint32_t sector_pos, sector_len,
	     packet_len;

    uint64_t current_page_code;

    uint8_t *temp_buffer;
} scsi_disk_t;


extern scsi_disk_t	*scsi_disk[HDD_NUM];
extern uint8_t		scsi_disks[16][8];


#ifdef USE_REMOVABLE_DISK
extern void	scsi_disk_insert(int id);
extern void	scsi_reloadhd(int id);
extern void	scsi_unloadhd(int scsi_id, int scsi_lun, int id);
#endif

extern void	scsi_disk_log(int level, const char *fmt, ...);
extern void	scsi_disk_global_init(void);
extern void	scsi_disk_hard_reset(void);
extern void	scsi_disk_close(void);
extern void	scsi_loadhd(int scsi_id, int scsi_lun, int id);

extern int	scsi_disk_read_capacity(scsi_disk_t *dev, uint8_t *cdb,
					uint8_t *buffer, uint32_t *len);
extern int	scsi_disk_err_stat_to_scsi(scsi_disk_t *dev);
extern int	scsi_disk_phase_to_scsi(scsi_disk_t *dev);
extern int	find_hdd_for_scsi_id(uint8_t scsi_id, uint8_t scsi_lun);
extern void	build_scsi_disk_map(void);
extern void	scsi_disk_reset(scsi_disk_t *dev);
extern void	scsi_disk_request_sense_for_scsi(scsi_disk_t *dev,
						 uint8_t *buffer,
						 uint8_t alloc_length);
extern void	scsi_disk_command(scsi_disk_t *dev, uint8_t *cdb);
extern void	scsi_disk_callback(scsi_disk_t *dev);


#endif	/*SCSI_DISK_H*/
