/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the CD-ROM image file handlers.
 *
 * Version:	@(#)cdrom_image.h	1.0.6	2018/10/14
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		RichardG, <richardg867@gmail.com>
 *		bit
 *
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
#ifndef CDROM_IMAGE_H
# define CDROM_IMAGE_H


#ifdef __cplusplus
extern "C" {
#endif

extern int	cdrom_image_do_log;
extern const cdrom_ops_t cdrom_image_ops;


extern void	cdrom_image_log(int level, const char *fmt, ...);

extern int	image_open(cdrom_t *dev, wchar_t *fn);
extern void	image_close(cdrom_t *dev);
extern void	image_reset(cdrom_t *dev);

extern int	image_audio_callback(cdrom_t *dev, int16_t *output, int len);
extern void	image_audio_stop(cdrom_t *dev);

#ifdef __cplusplus
}
#endif


#endif	/*CDROM_IMAGE_H*/
