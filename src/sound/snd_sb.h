/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Sound Blaster driver.
 *
 * Version:	@(#)sound_sb.h	1.0.1	2018/02/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#ifndef SOUND_SNDB_H
# define SOUND_SNDB_H


#define SADLIB		1	/* No DSP */
#define SB1		2	/* DSP v1.05 */
#define SB15		3	/* DSP v2.00 */
#define SB2		4	/* DSP v2.01 - needed for high-speed DMA */
#define SBPRO		5	/* DSP v3.00 */
#define SBPRO2		6	/* DSP v3.02 + OPL3 */
#define SB16		7	/* DSP v4.05 + OPL3 */
#define SADGOLD		8	/* AdLib Gold */
#define SND_WSS		9	/* Windows Sound System */
#define SND_PAS16	10	/* Pro Audio Spectrum 16 */


extern device_t sb_1_device;
extern device_t sb_15_device;
extern device_t sb_mcv_device;
extern device_t sb_2_device;
extern device_t sb_pro_v1_device;
extern device_t sb_pro_v2_device;
extern device_t sb_pro_mcv_device;
extern device_t sb_16_device;
extern device_t sb_awe32_device;


#endif	/*SOUND_SNDB_H*/
