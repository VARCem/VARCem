/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the FDI floppy stream image format
 *		interface to the FDI2RAW module.
 *
 * Version:	@(#)floppy_fdi.h	1.0.1	2018/02/14
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
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
#ifndef EMU_FLOPPY_FDI_H
# define EMU_FLOPPY_FDI_H


extern void fdi_load(int drive, wchar_t *fn);
extern void fdi_close(int drive);
extern void fdi_seek(int drive, int track);
extern void fdi_readsector(int drive, int sector, int track, int side, int density, int sector_size);
extern void fdi_writesector(int drive, int sector, int track, int side, int density, int sector_size);
extern void fdi_comparesector(int drive, int sector, int track, int side, int density, int sector_size);
extern void fdi_readaddress(int drive, int sector, int side, int density);
extern void fdi_format(int drive, int sector, int side, int density, uint8_t fill);
extern int fdi_hole(int drive);
extern double fdi_byteperiod(int drive);
extern void fdi_stop(void);
extern void fdi_poll(void);


#endif	/*EMU_FLOPPY_FDI_H*/
