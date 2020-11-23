/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the hard disk image handler.
 *
 * Version:	@(#)hdd.h	1.0.15	2020/11/21
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2020 Fred N. van Kempen.
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
#ifndef EMU_HDD_H
# define EMU_HDD_H


#define HDD_NUM		30	/* total of 30 images supported */

#include "../../minivhd/minivhd.h"
#include "../../minivhd/minivhd_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hard Disk bus types. */
enum {
    HDD_BUS_DISABLED = 0,
    HDD_BUS_ST506,
    HDD_BUS_ESDI,
    HDD_BUS_IDE,
    HDD_BUS_SCSI,
    HDD_BUS_USB
};
#define HDD_BUS_MAX	(HDD_BUS_USB)	/* USB exclusive */


typedef struct {
    uint8_t	cookie[8];
    uint32_t	features;
    uint32_t	version;
    uint64_t	offset;
    uint32_t	timestamp;
    uint8_t	creator[4];
    uint32_t	creator_vers;
    uint8_t	creator_host_os[4];
    uint64_t	orig_size;
    uint64_t	curr_size;
    struct {
	uint16_t	cyl;
	uint8_t		heads;
	uint8_t		spt;
    } geom;
    uint32_t	type;
    uint32_t	checksum;
    uint8_t	uuid[16];
    uint8_t	saved_state;
    uint8_t	reserved[427];
    
} vhd_footer_t;

/* Define a hard disk table entry. */
typedef struct {
    uint16_t	cyls;
    uint8_t	head;
    uint8_t	sect;
} hddtab_t;

/* Define the virtual Hard Disk. */
typedef struct {
    int8_t	is_hdi;			/* image type (should rename) */
    int8_t	wp;			/* disk has been mounted READ-ONLY */
    int8_t	removable;		/* disk is removable type */
    uint8_t	bus;
    int		num;			/* global disk number */

    union {
	uint8_t	st506_channel;		/* bus channel ID's */
	uint8_t	esdi_channel;
	uint8_t	ide_channel;
	struct {
		int8_t	id;
		int8_t	lun;
	}	scsi;
    }		bus_id;

    uint32_t	base;

    uint32_t	spt,			/* physical geometry parameters */
		hpc;
    uint32_t	tracks;

    uint32_t	at_spt,			/* [Translation] parameters */
		at_hpc;

    void	*priv;

    wchar_t	fn[260];		/* name of current image file */
    wchar_t	prev_fn[260];		/* name of previous image file */
    
} hard_disk_t;


extern const hddtab_t 	hdd_table[128];
extern hard_disk_t      hdd[HDD_NUM];
extern int		hdd_do_log;


extern void	hdd_log(int level, const char *fmt, ...);
extern int	hdd_init(void);
extern int	hdd_count(int bus);
extern int	hdd_string_to_bus(const char *str);
extern const char *hdd_bus_to_string(int bus);
extern const wchar_t *hdd_bus_to_ids(int bus);
extern int	hdd_is_valid(int c);
extern void	hdd_active(int c, int active);

extern const char *vhd_type_to_string(int vhd_type);
extern const wchar_t *vhd_type_to_ids(int vhd_type);
extern const wchar_t *vhd_blksize_to_ids(int blk_size);

extern void	hdd_image_log(int level, const char *fmt, ...);
extern void	hdd_image_init(void);
extern int	hdd_image_load(int id);
extern void	hdd_image_seek(uint8_t id, uint32_t sector);
extern void	hdd_image_read(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer);
extern int	hdd_image_read_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer);
extern void	hdd_image_write(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer);
extern int	hdd_image_write_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer);
extern void	hdd_image_zero(uint8_t id, uint32_t sector, uint32_t count);
extern int	hdd_image_zero_ex(uint8_t id, uint32_t sector, uint32_t count);
extern uint32_t	hdd_image_get_last_sector(uint8_t id);
extern uint32_t	hdd_image_get_pos(uint8_t id);
extern uint8_t	hdd_image_get_type(uint8_t id);
extern void	hdd_image_specify(uint8_t id, int hpc, int spt);
extern void	hdd_image_unload(uint8_t id, int fn_preserve);
extern void	hdd_image_close(uint8_t id);
extern void	hdd_image_calc_chs(uint32_t *c, uint32_t *h, uint32_t *s, uint32_t size);
extern void	vhd_footer_from_bytes(vhd_footer_t *vhd, uint8_t *bytes);
extern void	vhd_footer_to_bytes(uint8_t *bytes, vhd_footer_t *vhd);
extern void	new_vhd_footer(vhd_footer_t **vhd);
extern void	generate_vhd_checksum(vhd_footer_t *vhd);

extern int	image_is_hdi(const wchar_t *s);
extern int	image_is_hdx(const wchar_t *s, int check_signature);
extern int	image_is_vhd(const wchar_t *s, int check_signature);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_HDD_H*/
