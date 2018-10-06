/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Iomega ZIP drive with SCSI(-like)
 *		commands, for both ATAPI and SCSI usage.
 *
 * Version:	@(#)zip.h	1.0.7	2018/09/09
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018 Miran Grca.
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
#ifndef EMU_ZIP_H
#define EMU_ZIP_H


#define ZIP_NUM			4

#define ZIP_PHASE_IDLE		0x00
#define ZIP_PHASE_COMMAND	0x01
#define ZIP_PHASE_COMPLETE	0x02
#define ZIP_PHASE_DATA_IN	0x03
#define ZIP_PHASE_DATA_IN_DMA	0x04
#define ZIP_PHASE_DATA_OUT	0x05
#define ZIP_PHASE_DATA_OUT_DMA	0x06
#define ZIP_PHASE_ERROR		0x80

#define BUF_SIZE		32768

#define ZIP_TIME		(5LL * 100LL * (1LL << TIMER_SHIFT))

#define ZIP_SECTORS		(96*2048)
#define ZIP_250_SECTORS		(489532)


enum {
    ZIP_BUS_DISABLED = 0,
    ZIP_BUS_ATAPI,
    ZIP_BUS_SCSI,
    ZIP_BUS_USB
};
#define ZIP_BUS_MAX	(ZIP_BUS_USB)	/* USB exclusive */


typedef struct {
    int8_t	bus_type,		/* 0 = ATAPI, 1 = SCSI */
		bus_mode,		/* Bit 0 = PIO suported;
					   Bit 1 = DMA supportd. */
		is_250,
		read_only,
		ui_writeprot;

    uint8_t	ide_channel;
    uint8_t	scsi_device_id,
		scsi_device_lun;

    wchar_t	image_path[1024],
		prev_image_path[1024];

    uint32_t	medium_size,
		base;
    FILE	*f;
} zip_drive_t;

typedef struct {
    mode_sense_pages_t ms_pages_saved;

    zip_drive_t	*drv;

    uint8_t	id,
		previous_command,
		error, features,
		status, phase,
		*buffer,
		atapi_cdb[16],
		current_cdb[16],
		sense[256];

    uint16_t	request_length,
		max_transfer_len;

    int		toctimes, media_status,
		is_dma, requested_blocks,
		current_page_len, current_page_pos,
		total_length, written_length,
		mode_select_phase, do_page_save,
		data_pos,
		packet_status, unit_attention,
		cdb_len_setting, cdb_len,
		request_pos, total_read,
		block_total, all_blocks_total,
		old_len, block_descriptor_len,
		init_length;

    uint32_t	sector_pos, sector_len,
		packet_len, pos,
		seek_pos;

    uint64_t	current_page_code;
    int64_t	callback;
} zip_t;


extern zip_t		*zip[ZIP_NUM];
extern zip_drive_t	zip_drives[ZIP_NUM];
extern uint8_t		atapi_zip_drives[8];
extern uint8_t		scsi_zip_drives[16][8];

/*FIXME: These should be removed, it makes the code unclear. --FvK */
#define zip_sense_error	dev->sense[0]
#define zip_sense_key	dev->sense[2]
#define zip_asc		dev->sense[12]
#define zip_ascq	dev->sense[13]


#ifdef __cplusplus
extern "C" {
#endif

extern int	zip_ZIP_PHASE_to_scsi(zip_t *dev);
extern int	zip_atapi_phase_to_scsi(zip_t *dev);
extern void	zip_command(zip_t *dev, uint8_t *cdb);
extern void	zip_phase_callback(zip_t *dev);
extern void     zip_disk_close(zip_t *dev);
extern void     zip_disk_reload(zip_t *dev);
extern void	zip_reset(zip_t *dev);
extern void	zip_set_signature(zip_t *dev);
extern void	zip_request_sense_for_scsi(zip_t *dev, uint8_t *buffer, uint8_t alloc_length);
extern void	zip_update_cdb(uint8_t *cdb, int lba_pos, int number_of_blocks);
extern void	zip_insert(zip_t *dev);
extern int	find_zip_for_scsi_id(uint8_t scsi_id, uint8_t scsi_lun);
extern int	zip_read_capacity(zip_t *dev, uint8_t *cdb, uint8_t *buffer, uint32_t *len);

extern uint32_t	zip_read(uint8_t channel, int length);
extern void	zip_write(uint8_t channel, uint32_t val, int length);

extern void	zip_global_init(void);
extern void	zip_hard_reset(void);
extern void	zip_build_atapi_map(void);
extern void	zip_build_scsi_map(void);
extern int	zip_string_to_bus(const char *str);
extern const char *zip_bus_to_string(int bus);

extern void	zip_close(void);
extern int	zip_load(zip_t *dev, const wchar_t *fn);
extern void	zip_disk_close(zip_t *dev);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_ZIP_H*/
