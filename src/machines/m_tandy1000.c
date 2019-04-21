/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of Tandy models 1000, 1000HX and 1000SL2.
 *
 * Version:	@(#)m_tandy1000.c	1.0.19	2019/04/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../timer.h"
#include "../nvr.h"
#include "../device.h"
#include "../devices/system/dma.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/nmi.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_sn76489.h"
#include "../plat.h"
#include "machine.h"
#include "m_tandy1000.h"


#define TANDY_EEP_SIZE	64

#define TANDY_RGB	0
#define TANDY_COMPOSITE	1


enum {
    EEPROM_IDLE = 0,
    EEPROM_GET_OPERATION,
    EEPROM_READ,
    EEPROM_WRITE
};


typedef struct {
    sn76489_t	sn76489;

    uint8_t	ctrl;
    uint8_t	wave;
    uint8_t	dac_val;
    uint16_t	freq;
    int		amplitude;

    int		irq;
    int64_t	timer_count;
    int64_t	enable;

    int		wave_pos;
    int		pulse_width;

    int		pos;
    int16_t	buffer[SOUNDBUFLEN];
} t1ksnd_t;

typedef struct {
    wchar_t	*fn;

    int8_t	state;
    uint8_t	count;
    uint8_t	addr;
    uint8_t	clk;

    uint16_t	data_out,
		data;

    uint16_t	store[TANDY_EEP_SIZE];
} t1keep_t;

typedef struct {
    int		type;
    int		display_type;

    uint32_t	base;

    mem_map_t	ram_mapping;
    mem_map_t	rom_mapping;		/* SL2 */

    uint8_t	*rom;			/* SL2 */
    uint8_t	ram_bank;
    uint8_t	rom_bank;		/* SL2 */
    int		rom_offset;		/* SL2 */
} tandy_t;


static const scancode scancode_tandy[512] = {
    { {-1},       {-1}       }, { {0x01, -1}, {0x81, -1} },
    { {0x02, -1}, {0x82, -1} }, { {0x03, -1}, {0x83, -1} },
    { {0x04, -1}, {0x84, -1} }, { {0x05, -1}, {0x85, -1} },
    { {0x06, -1}, {0x86, -1} }, { {0x07, -1}, {0x87, -1} },
    { {0x08, -1}, {0x88, -1} }, { {0x09, -1}, {0x89, -1} },
    { {0x0a, -1}, {0x8a, -1} }, { {0x0b, -1}, {0x8b, -1} },
    { {0x0c, -1}, {0x8c, -1} }, { {0x0d, -1}, {0x8d, -1} },
    { {0x0e, -1}, {0x8e, -1} }, { {0x0f, -1}, {0x8f, -1} },
    { {0x10, -1}, {0x90, -1} }, { {0x11, -1}, {0x91, -1} },
    { {0x12, -1}, {0x92, -1} }, { {0x13, -1}, {0x93, -1} },
    { {0x14, -1}, {0x94, -1} }, { {0x15, -1}, {0x95, -1} },
    { {0x16, -1}, {0x96, -1} }, { {0x17, -1}, {0x97, -1} },
    { {0x18, -1}, {0x98, -1} }, { {0x19, -1}, {0x99, -1} },
    { {0x1a, -1}, {0x9a, -1} }, { {0x1b, -1}, {0x9b, -1} },
    { {0x1c, -1}, {0x9c, -1} }, { {0x1d, -1}, {0x9d, -1} },
    { {0x1e, -1}, {0x9e, -1} }, { {0x1f, -1}, {0x9f, -1} },
    { {0x20, -1}, {0xa0, -1} }, { {0x21, -1}, {0xa1, -1} },
    { {0x22, -1}, {0xa2, -1} }, { {0x23, -1}, {0xa3, -1} },
    { {0x24, -1}, {0xa4, -1} }, { {0x25, -1}, {0xa5, -1} },
    { {0x26, -1}, {0xa6, -1} }, { {0x27, -1}, {0xa7, -1} },
    { {0x28, -1}, {0xa8, -1} }, { {0x29, -1}, {0xa9, -1} },
    { {0x2a, -1}, {0xaa, -1} }, { {0x2b, -1}, {0xab, -1} },
    { {0x2c, -1}, {0xac, -1} }, { {0x2d, -1}, {0xad, -1} },
    { {0x2e, -1}, {0xae, -1} }, { {0x2f, -1}, {0xaf, -1} },
    { {0x30, -1}, {0xb0, -1} }, { {0x31, -1}, {0xb1, -1} },
    { {0x32, -1}, {0xb2, -1} }, { {0x33, -1}, {0xb3, -1} },
    { {0x34, -1}, {0xb4, -1} }, { {0x35, -1}, {0xb5, -1} },
    { {0x36, -1}, {0xb6, -1} }, { {0x37, -1}, {0xb7, -1} },
    { {0x38, -1}, {0xb8, -1} }, { {0x39, -1}, {0xb9, -1} },
    { {0x3a, -1}, {0xba, -1} }, { {0x3b, -1}, {0xbb, -1} },
    { {0x3c, -1}, {0xbc, -1} }, { {0x3d, -1}, {0xbd, -1} },
    { {0x3e, -1}, {0xbe, -1} }, { {0x3f, -1}, {0xbf, -1} },
    { {0x40, -1}, {0xc0, -1} }, { {0x41, -1}, {0xc1, -1} },
    { {0x42, -1}, {0xc2, -1} }, { {0x43, -1}, {0xc3, -1} },
    { {0x44, -1}, {0xc4, -1} }, { {0x45, -1}, {0xc5, -1} },
    { {0x46, -1}, {0xc6, -1} }, { {0x47, -1}, {0xc7, -1} },
    { {0x48, -1}, {0xc8, -1} }, { {0x49, -1}, {0xc9, -1} },
    { {0x4a, -1}, {0xca, -1} }, { {0x4b, -1}, {0xcb, -1} },
    { {0x4c, -1}, {0xcc, -1} }, { {0x4d, -1}, {0xcd, -1} },
    { {0x4e, -1}, {0xce, -1} }, { {0x4f, -1}, {0xcf, -1} },
    { {0x50, -1}, {0xd0, -1} }, { {0x51, -1}, {0xd1, -1} },
    { {0x52, -1}, {0xd2, -1} }, { {0x56, -1}, {0xd6, -1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*054*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*058*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*05c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*060*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*064*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*068*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*06c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*070*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*074*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*078*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*07c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*080*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*084*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*088*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*08c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*090*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*094*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*098*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*09c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0a0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0a4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0a8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0ac*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0b0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0b4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0b8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0bc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0c0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0c4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0c8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0cc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0d0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0d4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0d8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0dc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0e0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0e4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0e8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0ec*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0f0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0f4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0f8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*0fc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*100*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*104*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*108*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*10c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*110*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*114*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*118*/
    { {0x57, -1}, {0xd7, -1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*11c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*120*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*124*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*128*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*12c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*130*/
    { {-1},	     {-1} }, { {0x35, -1}, {0xb5, -1} },
    { {-1},	     {-1} }, { {0x37, -1}, {0xb7, -1} },	/*134*/
    { {0x38, -1}, {0xb8, -1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*138*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*13c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*140*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {0x46, -1}, {0xc6, -1} }, { {0x47, -1}, {0xc7, -1} },	/*144*/
    { {0x48, -1}, {0xc8, -1} }, { {0x49, -1}, {0xc9, -1} },
    { {-1},	     {-1} }, { {0x4b, -1}, {0xcb, -1} },	/*148*/
    { {-1},	     {-1} }, { {0x4d, -1}, {0xcd, -1} },
    { {-1},	     {-1} }, { {0x4f, -1}, {0xcf, -1} },	/*14c*/
    { {0x50, -1}, {0xd0, -1} }, { {0x51, -1}, {0xd1, -1} },
    { {0x52, -1}, {0xd2, -1} }, { {0x53, -1}, {0xd3, -1} },	/*150*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*154*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*158*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*15c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*160*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*164*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*168*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*16c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*170*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*174*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*148*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*17c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*180*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*184*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*88*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*18c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*190*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*194*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*198*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*19c*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1a0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1a4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1a8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1ac*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1b0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1b4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1b8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1bc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1c0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1c4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1c8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1cc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1d0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1d4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1d8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1dc*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1e0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1e4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1e8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1ec*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1f0*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1f4*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} },	/*1f8*/
    { {-1},	     {-1} }, { {-1},	     {-1} },
    { {-1},	     {-1} }, { {-1},	     {-1} }	/*1fc*/
};


static void
snd_update_irq(t1ksnd_t *dev)
{
    if (dev->irq && (dev->ctrl & 0x10) && (dev->ctrl & 0x08))
	picint(1 << 7);
}


static void
snd_write(uint16_t port, uint8_t val, void *priv)
{
    t1ksnd_t *dev = (t1ksnd_t *)priv;

    switch (port & 3) {
	case 0:
		dev->ctrl = val;
		dev->enable = (val & 4) && (dev->ctrl & 3);
		sn74689_set_extra_divide(&dev->sn76489, val & 0x40);
		if (!(val & 8))
			dev->irq = 0;
		snd_update_irq(dev);
		break;

	case 1:
		switch (dev->ctrl & 3) {
			case 1: /*Sound channel*/
				dev->wave = val;
				dev->pulse_width = val & 7;
				break;

			case 3: /*Direct DAC*/
				dev->dac_val = val;
				break;
		}
		break;

	case 2:
		dev->freq = (dev->freq & 0xf00) | val;
		break;

	case 3:
		dev->freq = (dev->freq & 0x0ff) | ((val & 0xf) << 8);
		dev->amplitude = val >> 4;
		break;
    }       
}


static uint8_t
snd_read(uint16_t port, void *priv)
{
    t1ksnd_t *dev = (t1ksnd_t *)priv;
    uint8_t ret = 0xff;

    switch (port & 3) {
	case 0:
		ret = (dev->ctrl & ~0x88) | (dev->irq ? 8 : 0);
		break;

	case 1:
		switch (dev->ctrl & 3) {
			case 0: /*Joystick*/
				ret = 0x00;
				break;

			case 1: /*Sound channel*/
				ret = dev->wave;
				break;

			case 2: /*Successive approximation*/
				ret = 0x80;
				break;

			case 3: /*Direct DAC*/
				ret = dev->dac_val;
				break;
		}
		break;

	case 2:
		ret = dev->freq & 0xff;
		break;

	case 3:
		ret = (dev->freq >> 8) | (dev->amplitude << 4);
		break;

	default:
		break;
    }

    return(ret);
}


static void
snd_update(t1ksnd_t *dev)
{
    for (; dev->pos < sound_pos_global; dev->pos++)	
	dev->buffer[dev->pos] =
		(((int8_t)(dev->dac_val ^ 0x80) * 0x20) * dev->amplitude) / 15;
}


static void
snd_callback(void *priv)
{
    t1ksnd_t *dev = (t1ksnd_t *)priv;
    int data;

    snd_update(dev);

    if (dev->ctrl & 2) {
	if ((dev->ctrl & 3) == 3) {
		data = dma_channel_read(1);

		if (data != DMA_NODATA)
			dev->dac_val = data & 0xff;
	} else {
		data = dma_channel_write(1, 0x80);
	}

	if ((data & DMA_OVER) && data != DMA_NODATA) {
		if (dev->ctrl & 0x08) {
			dev->irq = 1;
			snd_update_irq(dev);
		}
	} 
    } else {
	switch (dev->wave & 0xc0) {
		case 0x00: /*Pulse*/
			dev->dac_val =
			  (dev->wave_pos > (dev->pulse_width << 1)) ? 0xff : 0;
			break;

		case 0x40: /*Ramp*/
			dev->dac_val = dev->wave_pos << 3;
			break;

		case 0x80: /*Triangle*/
			if (dev->wave_pos & 16)
				dev->dac_val = (dev->wave_pos ^ 31) << 4;
			else
				dev->dac_val = dev->wave_pos << 4;
			break;

		case 0xc0:
			dev->dac_val = 0x80;
			break;
	}

	dev->wave_pos = (dev->wave_pos + 1) & 31;
    }

    dev->timer_count += (int64_t)(TIMER_USEC * (1000000.0 / 3579545.0) * (double)(dev->freq ? dev->freq : 0x400));
}


static void
snd_get_buffer(int32_t *bufp, int len, void *priv)
{
    t1ksnd_t *dev = (t1ksnd_t *)priv;
    int c;

    snd_update(dev);

    for (c = 0; c < len * 2; c++)
	bufp[c] += dev->buffer[c >> 1];

    dev->pos = 0;
}


static void *
snd_init(const device_t *info, UNUSED(void *parent))
{
    t1ksnd_t *dev;

    dev = (t1ksnd_t *)mem_alloc(sizeof(t1ksnd_t));
    memset(dev, 0x00, sizeof(t1ksnd_t));

    sn76489_init(&dev->sn76489, 0x00c0, 0x0004, PSSJ, 3579545);

    io_sethandler(0x00c4, 4,
		  snd_read,NULL,NULL, snd_write,NULL,NULL, dev);

    timer_add(snd_callback, &dev->timer_count, &dev->enable, dev);

    sound_add_handler(snd_get_buffer, dev);

    return(dev);
}


static void
snd_close(void *priv)
{
    t1ksnd_t *dev = (t1ksnd_t *)priv;

    free(dev);	
}


static const device_t snd_device = {
    "Tandy PSSJ",
    0, 0,
    NULL,
    snd_init, snd_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static void
eep_write(uint16_t addr, uint8_t val, void *priv)
{
    t1keep_t *dev = (t1keep_t *)priv;

    if ((val & 4) && !dev->clk) switch (dev->state) {
	case EEPROM_IDLE:
		switch (dev->count) {
			case 0:
				if (val & 3)
					dev->count = 0;
				  else
					dev->count = 1;
				break;

			case 1:
				if ((val & 3) == 2)
					dev->count = 2;
				  else
					dev->count = 0;
				break;

			case 2:
				if ((val & 3) == 3)
					dev->state = EEPROM_GET_OPERATION;
				dev->count = 0;
				break;
		}
		break;

	case EEPROM_GET_OPERATION:
		dev->data = (dev->data << 1) | (val & 1);
		if (++dev->count == 8) {
			dev->count = 0;
			dev->addr = dev->data & 0x3f;
			switch (dev->data & 0xc0) {
				case 0x40:
					dev->state = EEPROM_WRITE;
					break;

				case 0x80:
					dev->state = EEPROM_READ;
					dev->data = dev->store[dev->addr];
					break;

				default:
					dev->state = EEPROM_IDLE;
					break;
			}
		}
		break;

	case EEPROM_READ:
		dev->data_out = dev->data & 0x8000;
		dev->data <<= 1;
		if (++dev->count == 16) {
			dev->count = 0;
			dev->state = EEPROM_IDLE;
		}
		break;

	case EEPROM_WRITE:
		dev->data = (dev->data << 1) | (val & 1);
		if (++dev->count == 16) {
			dev->count = 0;
			dev->state = EEPROM_IDLE;
			dev->store[dev->addr] = dev->data;
		}
		break;
    }

    dev->clk = val & 4;
}


static void *
eep_init(const device_t *info, UNUSED(void *parent))
{
    char temp[128];
    t1keep_t *dev;
    FILE *fp;
    int i;

    dev = (t1keep_t *)mem_alloc(sizeof(t1keep_t));
    memset(dev, 0x00, sizeof(t1keep_t));

    /* Set up the EEPROM's file name. */
    sprintf(temp, "%s.bin", machine_get_internal_name());
    i = (int)strlen(temp) + 1;
    dev->fn = (wchar_t *)mem_alloc(i * sizeof(wchar_t));
    mbstowcs(dev->fn, temp, i);

    fp = plat_fopen(nvr_path(dev->fn), L"rb");
    if (fp != NULL) {
	fread(dev->store, sizeof(dev->store), 1, fp);
	(void)fclose(fp);
    }

    io_sethandler(0x037c, 1, NULL,NULL,NULL, eep_write,NULL,NULL, dev);

    return(dev);
}


static void
eep_close(void *priv)
{
    t1keep_t *dev = (t1keep_t *)priv;
    FILE *fp;

    fp = plat_fopen(nvr_path(dev->fn), L"wb");
    if (fp != NULL) {
	(void)fwrite(dev->store, sizeof(dev->store), 1, fp);
	(void)fclose(fp);
    }

    free(dev->fn);

    free(dev);
}


/* Called by the keyboard driver to read the next bit. */
static uint8_t
eep_readbit(void *priv)
{
    t1keep_t *dev = (t1keep_t *)priv;

    return(dev->data_out ? 1 : 0);
}


static const device_t eep_device = {
    "Tandy 1000 EEPROM",
    0, 0, NULL,
    eep_init, eep_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static void
tandy_write(uint16_t addr, uint8_t val, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;

    switch (addr) {
	case 0x00a0:
		mem_map_set_addr(&dev->ram_mapping,
				 ((val >> 1) & 7) * 128 * 1024, 0x20000);
		dev->ram_bank = val;
		break;

	case 0xffe8:
		if ((val & 0x0e) == 0x0e)
			mem_map_disable(&dev->ram_mapping);
		  else
			mem_map_set_addr(&dev->ram_mapping,
					 ((val >> 1) & 7) * 128 * 1024, 0x20000);
// FIXME.. is this needed?
		//recalc_address(dev, 1);
		dev->ram_bank = val;
		break;

	case 0xffea:
		dev->rom_bank = val;
		dev->rom_offset = ((val ^ 4) & 7) * 0x10000;
		mem_map_set_exec(&dev->rom_mapping,
				 &dev->rom[dev->rom_offset]);
    }
}


static uint8_t
tandy_read(uint16_t addr, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0x00a0:
		ret = dev->ram_bank;
		break;

	case 0xffe8:
		ret = dev->ram_bank;
		break;

	case 0xffea:
		ret = (dev->rom_bank ^ 0x10);
		break;
    }

    return(ret);
}


static void
write_ram(uint32_t addr, uint8_t val, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;

    ram[dev->base + (addr & 0x1ffff)] = val;
}


static uint8_t
read_ram(uint32_t addr, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;

    return(ram[dev->base + (addr & 0x1ffff)]);
}


static uint8_t
read_rom(uint32_t addr, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;
    uint32_t addr2 = (addr & 0xffff) + dev->rom_offset;

    return(dev->rom[addr2]);
}


static uint16_t
read_romw(uint32_t addr, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;
    uint32_t addr2 = (addr & 0xffff) + dev->rom_offset;

    return(*(uint16_t *)&dev->rom[addr2]);
}


static uint32_t
read_roml(uint32_t addr, void *priv)
{
    tandy_t *dev = (tandy_t *)priv;

    return(*(uint32_t *)&dev->rom[addr]);
}


/*
 * The Tandy 1000 SL/2 has 512KB of ROM on the board.
 *
 * Part of this is the 64KB ROM BIOS, which loads at
 * the usual address (OK, that is F0000 ..), and the
 * rest is their "DOS in ROM" data.
 *
 * This DOS data is loaded at E0000, but only using a
 * single segment (64KB) at a time, through the bank
 * switch register at FFEA. Because of this, we have
 * set the bios.txt script to "no load", allowing us
 * to implement the loading here.
 */
static void
init_romdos(tandy_t *dev, romdef_t *roms)
{
    uint32_t tot = 1024UL * 512;
    int i;

    /* Allocate the space to store the (custom) ROM. */
    dev->rom = (uint8_t *)mem_alloc(tot);

    if (roms->mode == 1) {
	/* Interleaved mode. */
	i = rom_load_interleaved(roms->files[0].path, roms->files[1].path,
				 0x000000, tot, 0, dev->rom);
    } else {
	/* Linear mode. */
	i = rom_load_linear(roms->files[0].path,
			    0x000000, tot, 0, dev->rom);
    }

    if (! i) {
	ERRLOG("TANDY: unable to load BIOS for 1000 SL/2 !\n");
	free(dev->rom);
	dev->rom = NULL;
	return;
    }

    /* Set up a memory segment at E0000, 64KB in size. */
    mem_map_add(&dev->rom_mapping, 0xe0000, 0x10000,
		read_rom, read_romw, read_roml, NULL, NULL, NULL,
		dev->rom, MEM_MAPPING_EXTERNAL, dev);
}


static void *
tandy_init(const device_t *info, void *arg)
{
    romdef_t *roms = (romdef_t *)arg;
    void *eep = NULL, *kbd;
    tandy_t *dev;

    dev = (tandy_t *)mem_alloc(sizeof(tandy_t));
    memset(dev, 0x00, sizeof(tandy_t));
    dev->type = info->local;

    /* Add machine device. */
    device_add_ex(info, dev);

    dev->display_type = machine_get_config_int("display_type");

    machine_common_init();

    nmi_init();

    /*
     * Base 128K mapping is controlled via ports 0xA0 or
     * 0xFFE8 (SL2), so we remove it from the main mapping.
     */
    dev->base = (mem_size - 128) * 1024;
    mem_map_add(&dev->ram_mapping, 0x80000, 0x20000,
		read_ram,NULL,NULL, write_ram,NULL,NULL, NULL, 0, dev);
    mem_map_set_addr(&ram_low_mapping, 0, dev->base);

    /* Set up the video controller. */
    tandy1k_video_init(dev->type, dev->display_type, dev->base, roms->fontfn);

    kbd = device_add(&keyboard_tandy_device);
    keyboard_set_table(scancode_tandy);

    device_add(&fdc_xt_device);

    switch(dev->type) {
	case 0:		/* Tandy 1000 */
		io_sethandler(0x00a0, 1,
			      tandy_read,NULL,NULL,tandy_write,NULL,NULL,dev);
		device_add(&sn76489_device);
		break;

	case 1:		/* Tandy 1000HX */
		io_sethandler(0x00a0, 1,
			      tandy_read,NULL,NULL,tandy_write,NULL,NULL,dev);
		device_add(&ncr8496_device);
		eep = device_add(&eep_device);
		break;

	case 2:		/* Tandy 1000SL2 */
		init_romdos(dev, roms);
		io_sethandler(0xffe8, 8,
			      tandy_read,NULL,NULL,tandy_write,NULL,NULL,dev);
		device_add(&snd_device);
		eep = device_add(&eep_device);
		break;
    }

    /* Set the callback function for the keyboard controller. */
    keyboard_xt_set_funcs(kbd, eep_readbit, eep);

    return(dev);
}


static void
tandy_close(void *priv)
{
    tandy_t *dev = (tandy_t *)priv;

    if (dev->rom != NULL)
	free(dev->rom);

    free(dev);
}


static const device_config_t tandy_config[] = {
    {
	"display_type", "Display type", CONFIG_SELECTION, "", TANDY_RGB,
	{
		{
			"RGB", TANDY_RGB
		},
		{
			"Composite", TANDY_COMPOSITE
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


static const CPU cpus_tandy[] = {
    { "8088/4.77", CPU_8088, 4772728, 1, 0, 0, 0, 0, CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8088/7.16", CPU_8088, 7159092, 1, 0, 0, 0, 0, CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8088/9.54", CPU_8088, 9545456, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1 },
    { NULL }
};

static const machine_t tandy_1k_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_SOUND,
    MACHINE_VIDEO,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_tandy},{"NEC",cpus_nec}}
};

const device_t m_tandy_1k = {
    "Tandy 1000",
    DEVICE_ROOT,
    0,
    L"tandy/t1000",
    tandy_init, tandy_close, NULL,
    NULL, NULL, NULL,
    &tandy_1k_info,
    tandy_config
};


static const machine_t tandy_1k_hx_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_SOUND,
    MACHINE_VIDEO,
    256, 640, 128, 0, -1,
    {{"Intel",cpus_tandy},{"NEC",cpus_nec}}
};

const device_t m_tandy_1k_hx = {
    "Tandy 1000 HX",
    DEVICE_ROOT,
    1,
    L"tandy/t1000hx",
    tandy_init, tandy_close, NULL,
    NULL, NULL, NULL,
    &tandy_1k_hx_info,
    tandy_config
};


static const machine_t tandy_1k_sl2_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_SOUND,
    MACHINE_VIDEO,
    512, 640, 128, 0, -1,
    {{"Intel",cpus_8086},{"NEC",cpus_nec}}
};

const device_t m_tandy_1k_sl2 = {
    "Tandy 1000SL2",
    DEVICE_ROOT,
    2,
    L"tandy/t1000sl2",
    tandy_init, tandy_close, NULL,
    NULL, NULL, NULL,
    &tandy_1k_sl2_info,
    NULL
};
