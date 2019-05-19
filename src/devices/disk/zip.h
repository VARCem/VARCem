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
 * Version:	@(#)zip.h	1.0.13	2019/05/17
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

#define BUF_SIZE		32768

#define ZIP_TIME		(5LL * 100LL * (1LL << TIMER_SHIFT))

#define ZIP_SECTORS		(96*2048)
#define ZIP_SECTORS_250		(489532)


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

    union {
	uint8_t ide_channel;
	struct {
		int8_t  id;
		int8_t  lun;
	}       scsi;
    }		bus_id;

    uint32_t	medium_size,
		base;

    void	*priv;

    FILE	*f;

    wchar_t	image_path[1024],
		prev_image_path[1024];
} zip_drive_t;

typedef struct {
    uint8_t	id,
		error, status,
		phase,
		features,
		is_dma,
		do_page_save,
		unit_attention;

    zip_drive_t	*drv;

    uint16_t	request_length,
		max_transfer_len;

    int		requested_blocks, packet_status,
		request_pos, old_len,
		total_length;

    uint32_t	sector_pos, sector_len,
		packet_len, pos,
		seek_pos;

    tmrval_t	callback;

    mode_sense_pages_t ms_pages_saved;

    uint8_t	*buffer,
		atapi_cdb[16],
		current_cdb[16],
		sense[256];
} zip_t;


extern zip_drive_t	zip_drives[ZIP_NUM];


#ifdef __cplusplus
extern "C" {
#endif

extern void     zip_disk_close(zip_t *dev);
extern void     zip_disk_reload(zip_t *dev);
extern void	zip_insert(zip_t *dev);

extern void	zip_global_init(void);
extern void	zip_hard_reset(void);
extern void	zip_reset_bus(int bus);

extern int	zip_string_to_bus(const char *str);
extern const char *zip_bus_to_string(int bus);

extern int	zip_load(zip_t *dev, const wchar_t *fn);
extern void	zip_close(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_ZIP_H*/
