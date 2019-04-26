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
 * Version:	@(#)m_tandy1000.c	1.0.21	2019/04/26
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
    int64_t	timer_count,
		enable;

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
    {	{ 0    }, { 0    }	},
    {	{ 0x01 }, { 0x81 }	},
    {	{ 0x02 }, { 0x82 }	},
    {	{ 0x03 }, { 0x83 }	},
    {	{ 0x04 }, { 0x84 }	},
    {	{ 0x05 }, { 0x85 }	},
    {	{ 0x06 }, { 0x86 }	},
    {	{ 0x07 }, { 0x87 }	},
    {	{ 0x08 }, { 0x88 }	},
    {	{ 0x09 }, { 0x89 }	},
    {	{ 0x0a }, { 0x8a }	},
    {	{ 0x0b }, { 0x8b }	},
    {	{ 0x0c }, { 0x8c }	},
    {	{ 0x0d }, { 0x8d }	},
    {	{ 0x0e }, { 0x8e }	},
    {	{ 0x0f }, { 0x8f }	},
    {	{ 0x10 }, { 0x90 }	},
    {	{ 0x11 }, { 0x91 }	},
    {	{ 0x12 }, { 0x92 }	},
    {	{ 0x13 }, { 0x93 }	},
    {	{ 0x14 }, { 0x94 }	},
    {	{ 0x15 }, { 0x95 }	},
    {	{ 0x16 }, { 0x96 }	},
    {	{ 0x17 }, { 0x97 }	},
    {	{ 0x18 }, { 0x98 }	},
    {	{ 0x19 }, { 0x99 }	},
    {	{ 0x1a }, { 0x9a }	},
    {	{ 0x1b }, { 0x9b }	},
    {	{ 0x1c }, { 0x9c }	},
    {	{ 0x1d }, { 0x9d }	},
    {	{ 0x1e }, { 0x9e }	},
    {	{ 0x1f }, { 0x9f }	},
    {	{ 0x20 }, { 0xa0 }	},
    {	{ 0x21 }, { 0xa1 }	},
    {	{ 0x22 }, { 0xa2 }	},
    {	{ 0x23 }, { 0xa3 }	},
    {	{ 0x24 }, { 0xa4 }	},
    {	{ 0x25 }, { 0xa5 }	},
    {	{ 0x26 }, { 0xa6 }	},
    {	{ 0x27 }, { 0xa7 }	},
    {	{ 0x28 }, { 0xa8 }	},
    {	{ 0x29 }, { 0xa9 }	},
    {	{ 0x2a }, { 0xaa }	},
    {	{ 0x2b }, { 0xab }	},
    {	{ 0x2c }, { 0xac }	},
    {	{ 0x2d }, { 0xad }	},
    {	{ 0x2e }, { 0xae }	},
    {	{ 0x2f }, { 0xaf }	},
    {	{ 0x30 }, { 0xb0 }	},
    {	{ 0x31 }, { 0xb1 }	},
    {	{ 0x32 }, { 0xb2 }	},
    {	{ 0x33 }, { 0xb3 }	},
    {	{ 0x34 }, { 0xb4 }	},
    {	{ 0x35 }, { 0xb5 }	},
    {	{ 0x36 }, { 0xb6 }	},
    {	{ 0x37 }, { 0xb7 }	},
    {	{ 0x38 }, { 0xb8 }	},
    {	{ 0x39 }, { 0xb9 }	},
    {	{ 0x3a }, { 0xba }	},
    {	{ 0x3b }, { 0xbb }	},
    {	{ 0x3c }, { 0xbc }	},
    {	{ 0x3d }, { 0xbd }	},
    {	{ 0x3e }, { 0xbe }	},
    {	{ 0x3f }, { 0xbf }	},
    {	{ 0x40 }, { 0xc0 }	},
    {	{ 0x41 }, { 0xc1 }	},
    {	{ 0x42 }, { 0xc2 }	},
    {	{ 0x43 }, { 0xc3 }	},
    {	{ 0x44 }, { 0xc4 }	},
    {	{ 0x45 }, { 0xc5 }	},
    {	{ 0x46 }, { 0xc6 }	},
    {	{ 0x47 }, { 0xc7 }	},
    {	{ 0x48 }, { 0xc8 }	},
    {	{ 0x49 }, { 0xc9 }	},
    {	{ 0x4a }, { 0xca }	},
    {	{ 0x4b }, { 0xcb }	},
    {	{ 0x4c }, { 0xcc }	},
    {	{ 0x4d }, { 0xcd }	},
    {	{ 0x4e }, { 0xce }	},
    {	{ 0x4f }, { 0xcf }	},
    {	{ 0x50 }, { 0xd0 }	},
    {	{ 0x51 }, { 0xd1 }	},
    {	{ 0x52 }, { 0xd2 }	},
    {	{ 0x56 }, { 0xd6 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x57 }, { 0xd7 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x35 }, { 0xb5 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x37 }, { 0xb7 }	},
    {	{ 0x38 }, { 0xb8 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x46 }, { 0xc6 }	},
    {	{ 0x47 }, { 0xc7 }	},
    {	{ 0x48 }, { 0xc8 }	},
    {	{ 0x49 }, { 0xc9 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x4b }, { 0xcb }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x4d }, { 0xcd }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x4f }, { 0xcf }	},
    {	{ 0x50 }, { 0xd0 }	},
    {	{ 0x51 }, { 0xd1 }	},
    {	{ 0x52 }, { 0xd2 }	},
    {	{ 0x53 }, { 0xd3 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	}
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

    timer_add(snd_callback, dev, &dev->timer_count, &dev->enable);

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
