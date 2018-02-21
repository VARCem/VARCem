/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of SCSI fixed and removable disks.
 *
 * Version:	@(#)scsi_disk.h	1.0.1	2018/02/14
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
	/* Stuff for SCSI hard disks. */
	uint8_t cdb[16];
	uint8_t current_cdb[16];
	uint8_t max_cdb_len;
	int requested_blocks;
	int max_blocks_at_once;
	uint16_t request_length;
	int block_total;
	int all_blocks_total;
	uint32_t packet_len;
	int packet_status;
	uint8_t status;
	uint8_t phase;
	uint32_t pos;
	int callback;
	int total_read;
	int unit_attention;
	uint8_t sense[256];
	uint8_t previous_command;
	uint8_t error;
	uint32_t sector_pos;
	uint32_t sector_len;
	uint32_t seek_pos;
	int data_pos;
	int old_len;
	int request_pos;
	uint8_t hd_cdb[16];

	uint64_t current_page_code;
	int current_page_len;

	int current_page_pos;

	int mode_select_phase;

	int total_length;
	int written_length;

	int do_page_save;
	int block_descriptor_len;

	uint8_t *temp_buffer;
} scsi_hard_disk_t;


extern scsi_hard_disk_t shdc[HDD_NUM];
extern FILE		*shdf[HDD_NUM];


extern void	scsi_disk_insert(uint8_t id);
extern void	scsi_loadhd(int scsi_id, int scsi_lun, int id);
extern void	scsi_reloadhd(int id);
extern void	scsi_unloadhd(int scsi_id, int scsi_lun, int id);

extern int	scsi_hd_read_capacity(uint8_t id, uint8_t *cdb,
				      uint8_t *buffer, uint32_t *len);


#endif	/*SCSI_DISK_H*/
