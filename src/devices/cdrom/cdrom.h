/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the CDROM module..
 *
 * Version:	@(#)cdrom.h	1.0.17	2019/05/17
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
#ifndef EMU_CDROM_H
#define EMU_CDROM_H


#define CDROM_NUM		4

#define CDROM_SPEED_DEFAULT	8


#define CD_STATUS_EMPTY		0
#define CD_STATUS_DATA_ONLY	1
#define CD_STATUS_PLAYING	2
#define CD_STATUS_PAUSED	3
#define CD_STATUS_STOPPED	4

#define BUF_SIZE		32768

#define CDROM_EMPTY		(dev->host_drive == 0)
#define CDROM_IMAGE		200


/*
 * The addresses sent from the guest are absolute, ie. a LBA of 0
 * corresponds to an MSF of 00:00:00, or else the counter displayed
 * by the guest is wrong:
 *
 * there is a seeming 2 seconds in which audio plays but counter
 * does not move, while a data track before audio jumps to 2 seconds
 * before the actual start of the audio while audio still plays.
 *
 * With an absolute conversion, the counter is fine.
 */
#define MSFtoLBA(m,s,f)	((((m*60)+s)*75)+f)


#ifdef __cplusplus
extern "C" {
#endif

enum {
    CDROM_BUS_DISABLED = 0,
    CDROM_BUS_ATAPI,
    CDROM_BUS_SCSI,
    CDROM_BUS_USB
};
#define CDROM_BUS_MAX	(CDROM_BUS_USB)	/* USB exclusive */


/* To shut up the GCC compilers. */
struct cdrom;


/* Define the various CD-ROM drive operations (ops). */
typedef struct {
    int		(*ready)(struct cdrom *dev);
    void	(*notify_change)(struct cdrom *dev, int media_present);
    void	(*medium_lock)(struct cdrom *dev, int locked);
    int		(*media_type_id)(struct cdrom *dev);

    uint32_t	(*size)(struct cdrom *dev);
    int		(*status)(struct cdrom *dev);
    void	(*stop)(struct cdrom *dev);
    void	(*close)(struct cdrom *dev);

    void	(*eject)(struct cdrom *dev);
    void	(*load)(struct cdrom *dev);

    int		(*readtoc)(struct cdrom *dev, uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single);
    int		(*readtoc_session)(struct cdrom *dev, uint8_t *b, int msf, int maxlen);
    int		(*readtoc_raw)(struct cdrom *dev, uint8_t *b, int maxlen);

    uint8_t	(*audio_play)(struct cdrom *dev, uint32_t pos, uint32_t len, int ismsf);
    void	(*audio_stop)(struct cdrom *dev);
    void	(*audio_pause)(struct cdrom *dev);
    void	(*audio_resume)(struct cdrom *dev);
    int		(*audio_callback)(struct cdrom *dev, int16_t *output, int len);

    uint8_t	(*getcurrentsubchannel)(struct cdrom *dev, uint8_t *b, int msf);

    int		(*readsector_raw)(struct cdrom *dev, uint8_t *buffer, int sector, int ismsf, int cdrom_sector_type, int cdrom_sector_flags, int *len);
} cdrom_ops_t;

/* Define the generic CD-ROM drive. */
typedef struct cdrom {
    int		id;

    uint8_t	speed_idx,		/* default speed [index] */
		cur_speed;		/* currently set speed [index] */
    int8_t	bus_type,		/* 0 = ATAPI, 1 = SCSI */
		bus_mode,		/* Bit 0 = PIO suported;
					 * Bit 1 = DMA supportd. */
		sound_on,
		can_lock,		/* device can be locked */
		is_locked;		/* device is currently locked */

    union {
	uint8_t ide_channel;		/* IDE drive: channel (0/1) */
	struct {			/* SCSI drive: ID and LUN */
		int8_t  id;
		int8_t  lun;
	} scsi;
    }		bus_id;

    FILE	*img_fp;
    int		img_type;
    int   img_is_iso;
    int		host_drive, prev_host_drive;

    int		cd_status, prev_status,
		cd_buflen, cd_state;

    uint32_t	seek_pos, seek_diff,
	     	cd_end,
		cdrom_capacity;

    const cdrom_ops_t *ops;			/* device ops */

    void	*local;				/* local data for handler */

    void	(*reset)(struct cdrom *);

    void	*priv;
    void	(*insert)(void *priv);
    void	(*close)(void *priv);
    uint32_t	(*get_volume)(void *priv, int channel);
    uint32_t	(*get_channel)(void *priv, int channel);

    wchar_t	image_path[1024],
		*prev_image_path;

    int16_t	cd_buffer[BUF_SIZE];
} cdrom_t;

typedef struct {
    int		speed;
    double	seek1;
    double	seek2;
} cdrom_speed_t;


extern const cdrom_speed_t	cdrom_speeds[];

extern cdrom_t		cdrom[CDROM_NUM];
#ifdef USE_HOST_CDROM
extern uint8_t		cdrom_host_drive_available[26];
extern uint8_t		cdrom_host_drive_available_num;
#endif

extern void		cdrom_global_init(void);
extern void		cdrom_global_reset(void);
extern void		cdrom_hard_reset(void);
extern void		cdrom_close(void);
extern void		cdrom_reset_bus(int bus);

extern int		cdrom_string_to_bus(const char *str);
extern const char	*cdrom_bus_to_string(int bus);

extern void		cdrom_log(int level, const char *fmt, ...);
extern int		cdrom_speed_idx(int realspeed);

extern int		cdrom_lba_to_msf_accurate(int lba);
extern double		cdrom_seek_time(cdrom_t *dev);
extern void		cdrom_seek(cdrom_t *dev, uint32_t pos);
extern int		cdrom_playing_completed(cdrom_t *dev);

extern void		cdrom_insert(uint8_t id);
extern void		cdrom_eject(uint8_t id);
extern void		cdrom_reload(uint8_t id);

extern void		cdrom_notify(const char *drive, int present);

extern int		cdrom_image_open(cdrom_t *dev, const wchar_t *fn);

#ifdef USE_HOST_CDROM
extern void		cdrom_host_init(void);
extern int		cdrom_host_open(cdrom_t *dev, char d);
#endif

extern void		scsi_cdrom_drive_reset(int c);
extern void		scsi_cdrom_update_cdb(uint8_t *cdb, int lba_pos,
					      int number_of_blocks);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_CDROM_H*/
