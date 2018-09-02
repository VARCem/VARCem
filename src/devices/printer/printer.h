/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the printers module.
 *
 * Version:	@(#)printer.h	1.0.2	2018/09/02
 *
 * Authors:	Michael Drüing, <michael@drueing.de>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Based on code by Frederic Weymann (originally for DosBox)
 *
 *		Copyright 2018 Michael Drüing.
 *		Copyright 2018 Fred N. van Kempen.
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
#ifndef PRINTER_H
# define PRINTER_H


#define FONT_FILE_DOTMATRIX	L"dotmatrix.ttf"

#define FONT_FILE_ROMAN		L"roman.ttf"
#define FONT_FILE_SANSSERIF	L"sansserif.ttf"
#define FONT_FILE_COURIER	L"courier.ttf"
#define FONT_FILE_SCRIPT	L"script.ttf"
#define FONT_FILE_OCRA		L"ocra.ttf"
#define FONT_FILE_OCRB		L"ocra.ttf"


#ifdef EMU_PARALLEL_DEV_H
extern const lpt_device_t lpt_prt_text_device;
extern const lpt_device_t lpt_prt_escp_device;
#endif


extern const uint16_t	*select_codepage(uint16_t num);


#endif	/*PRINTER_H*/
