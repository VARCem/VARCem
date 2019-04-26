/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the XT-style keyboard.
 *
 * Version:	@(#)keyboard_xt.c	1.0.17	2019/04/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#define dbglog kbd_log
#include "../../emu.h"
#include "../../io.h"
#include "../../timer.h"
#include "../../device.h"
#include "../system/pic.h"
#include "../system/pit.h"
#include "../system/ppi.h"
#include "../floppy/fdd.h"
#include "../sound/sound.h"
#include "../sound/snd_speaker.h"
#include "../video/video.h"
#include "../../plat.h"
#ifdef USE_CASSETTE
# include <cassette.h>
#endif
#include "keyboard.h"


#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01


enum {
  KBC_PC = 0,		/* IBM PC, 1981 */
  KBC_PC82,		/* IBM PC, 1982 */
  KBC_XT,		/* IBM PC/XT, 1982 */
  KBC_XT86,		/* IBM PC/XT, 1986, and most clones */
  KBC_GENERIC,		/* Generic XT (no FDD count or memory size) */
  KBC_TANDY,		/* Tandy XT */
  KBC_LASER		/* VTech LaserXT */
};


typedef struct {
    uint8_t	type;

    uint8_t	pa,
		pb,
		pd;

    int8_t	want_irq;  
    int8_t	blocked;
    uint8_t	key_waiting;

    uint8_t	(*read_func)(void *);
    void	*func_priv;
} xtkbd_t;


/* PC/XT keyboard has no escape scancodes, and no scancodes beyond 53. */
const scancode scancode_xt[512] = {
    {	{ 0    }, { 0    }      },	/* 000 */
    {	{ 0x01 }, { 0x81 }	},
    {	{ 0x02 }, { 0x82 }	},
    {	{ 0x03 }, { 0x83 }	},
    {	{ 0x04 }, { 0x84 }	},	/* 004 */
    {	{ 0x05 }, { 0x85 }	},
    {	{ 0x06 }, { 0x86 }	},
    {	{ 0x07 }, { 0x87 }	},
    {	{ 0x08 }, { 0x88 }	},	/* 008 */
    {	{ 0x09 }, { 0x89 }	},
    {	{ 0x0a }, { 0x8a }	},
    {	{ 0x0b }, { 0x8b }	},
    {	{ 0x0c }, { 0x8c }	},	/* 00c */
    {	{ 0x0d }, { 0x8d }	},
    {	{ 0x0e }, { 0x8e }	},
    {	{ 0x0f }, { 0x8f }	},
    {	{ 0x10 }, { 0x90 }	},	/* 010 */
    {	{ 0x11 }, { 0x91 }	},
    {	{ 0x12 }, { 0x92 }	},
    {	{ 0x13 }, { 0x93 }	},
    {	{ 0x14 }, { 0x94 }	},	/* 014 */
    {	{ 0x15 }, { 0x95 }	},
    {	{ 0x16 }, { 0x96 }	},
    {	{ 0x17 }, { 0x97 }	},
    {	{ 0x18 }, { 0x98 }	},	/* 018 */
    {	{ 0x19 }, { 0x99 }	},
    {	{ 0x1a }, { 0x9a }	},
    {	{ 0x1b }, { 0x9b }	},
    {	{ 0x1c }, { 0x9c }	},	/* 01c */
    {	{ 0x1d }, { 0x9d }	},
    {	{ 0x1e }, { 0x9e }	},
    {	{ 0x1f }, { 0x9f }	},
    {	{ 0x20 }, { 0xa0 }	},	/* 020 */
    {	{ 0x21 }, { 0xa1 }	},
    {	{ 0x22 }, { 0xa2 }	},
    {	{ 0x23 }, { 0xa3 }	},
    {	{ 0x24 }, { 0xa4 }	},	/* 024 */
    {	{ 0x25 }, { 0xa5 }	},
    {	{ 0x26 }, { 0xa6 }	},
    {	{ 0x27 }, { 0xa7 }	},
    {	{ 0x28 }, { 0xa8 }	},	/* 028 */
    {	{ 0x29 }, { 0xa9 }	},
    {	{ 0x2a }, { 0xaa }	},
    {	{ 0x2b }, { 0xab }	},
    {	{ 0x2c }, { 0xac }	},	/* 02c */
    {	{ 0x2d }, { 0xad }	},
    {	{ 0x2e }, { 0xae }	},
    {	{ 0x2f }, { 0xaf }	},
    {	{ 0x30 }, { 0xb0 }	},	/* 030 */
    {	{ 0x31 }, { 0xb1 }	},
    {	{ 0x32 }, { 0xb2 }	},
    {	{ 0x33 }, { 0xb3 }	},
    {	{ 0x34 }, { 0xb4 }	},	/* 034 */
    {	{ 0x35 }, { 0xb5 }	},
    {	{ 0x36 }, { 0xb6 }	},
    {	{ 0x37 }, { 0xb7 }	},
    {	{ 0x38 }, { 0xb8 }	},	/* 038 */
    {	{ 0x39 }, { 0xb9 }	},
    {	{ 0x3a }, { 0xba }	},
    {	{ 0x3b }, { 0xbb }	},
    {	{ 0x3c }, { 0xbc }	},	/* 03c */
    {	{ 0x3d }, { 0xbd }	},
    {	{ 0x3e }, { 0xbe }	},
    {	{ 0x3f }, { 0xbf }	},
    {	{ 0x40 }, { 0xc0 }	},	/* 040 */
    {	{ 0x41 }, { 0xc1 }	},
    {	{ 0x42 }, { 0xc2 }	},
    {	{ 0x43 }, { 0xc3 }	},
    {	{ 0x44 }, { 0xc4 }	},	/* 044 */
    {	{ 0x45 }, { 0xc5 }	},
    {	{ 0x46 }, { 0xc6 }	},
    {	{ 0x47 }, { 0xc7 }	},
    {	{ 0x48 }, { 0xc8 }	},	/* 048 */
    {	{ 0x49 }, { 0xc9 }	},
    {	{ 0x4a }, { 0xca }	},
    {	{ 0x4b }, { 0xcb }	},
    {	{ 0x4c }, { 0xcc }	},	/* 04c */
    {	{ 0x4d }, { 0xcd }	},
    {	{ 0x4e }, { 0xce }	},
    {	{ 0x4f }, { 0xcf }	},
    {	{ 0x50 }, { 0xd0 }	},	/* 050 */
    {	{ 0x51 }, { 0xd1 }	},
    {	{ 0x52 }, { 0xd2 }	},
    {	{ 0x53 }, { 0xd3 }	},
    {	{ 0    }, { 0    }	},	/* 054 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 058 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 05c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 060 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 064 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 068 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 06c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 070 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 074 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 078 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 07c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 080 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 084 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 088 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 08c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 090 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 094 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 098 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 09c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0a0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0a4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0a8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0ac */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0b0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0b4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0b8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0bc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0c0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0c4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0c8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0cc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0d0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0d4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0d8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0dc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0e0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0e4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0e8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0ec */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0f0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0f4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0f8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 0fc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 100 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 104 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 108 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 10c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 110 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 114 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 118 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x1c }, { 0x9c }	},	/* 11c */
    {	{ 0x1d }, { 0x9d }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 120 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 124 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 128 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 12c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 130 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 134 */
    {	{ 0x35 }, { 0xb5 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x37 }, { 0xb7 }	},
    {	{ 0x38 }, { 0xb8 }	},	/* 138 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 13c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 140 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 144 */
    {	{ 0    }, { 0    }	},
    {	{ 0x46 }, { 0xc6 }	},
    {	{ 0x47 }, { 0xc7 }	},
    {	{ 0x48 }, { 0xc8 }	},	/* 148 */
    {	{ 0x49 }, { 0xc9 }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x4b }, { 0xcb }	},
    {	{ 0    }, { 0    }	},	/* 14c */
    {	{ 0x4d }, { 0xcd }	},
    {	{ 0    }, { 0    }	},
    {	{ 0x4f }, { 0xcf }	},
    {	{ 0x50 }, { 0xd0 }	},	/* 150 */
    {	{ 0x51 }, { 0xd1 }	},
    {	{ 0x52 }, { 0xd2 }	},
    {	{ 0x53 }, { 0xd3 }	},
    {	{ 0    }, { 0    }	},	/* 154 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 158 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 15c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 160 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 164 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 168 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 16c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 170 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 174 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 178 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 17c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 180 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 184 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 188 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 18c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 190 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 194 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 198 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 19c */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1a0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1a4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1a8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1ac */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1b0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1b4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1b8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1bc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1c0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1c4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1c8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1cc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1d0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1d4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1d8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1dc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1e0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1e4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1e8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1ec */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1f0 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1f4 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1f8 */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},	/* 1fc */
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	},
    {	{ 0    }, { 0    }	}
};


static uint8_t	key_queue[16];
static int	key_queue_start,
		key_queue_end;


static void
kbd_poll(void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)priv;

    keyboard_delay += (1000LL * TIMER_USEC);

    if (!(dev->pb & 0x40) && (dev->type != KBC_TANDY))
	return;

    if (dev->want_irq) {
	dev->want_irq = 0;
	dev->pa = dev->key_waiting;
	dev->blocked = 1;
	picint(2);
    }

    if (key_queue_start != key_queue_end && !dev->blocked) {
	dev->key_waiting = key_queue[key_queue_start];
	key_queue_start = (key_queue_start + 1) & 0x0f;
	dev->want_irq = 1;
    }
}


static void
kbd_adddata(uint16_t val)
{
    key_queue[key_queue_end] = val & 0xff;
    key_queue_end = (key_queue_end + 1) & 0x0f;
}


void
kbd_adddata_process(uint16_t val, void (*adddata)(uint16_t val))
{
    uint8_t num_lock = 0, shift_states = 0;

    if (adddata == NULL) return;

    num_lock = !!(keyboard_get_state() & KBD_FLAG_NUM);
    shift_states = keyboard_get_shift() & STATE_SHIFT_MASK;

    switch(val) {
	case FAKE_LSHIFT_ON:
		if (num_lock) {
			if (! shift_states) {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				adddata(0x2a);
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				adddata(0xaa);
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				adddata(0xb6);
			}
		}
		break;

	case FAKE_LSHIFT_OFF:
		if (num_lock) {
			if (! shift_states) {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				adddata(0xaa);
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				adddata(0x2a);
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				adddata(0x36);
			}
		}
		break;

	default:
		adddata(val);
		break;
    }
}


static void
kbd_adddata_ex(uint16_t val)
{
    kbd_adddata_process(val, kbd_adddata);
}


static void
kbd_write(uint16_t port, uint8_t val, void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)priv;

    switch (port) {
	case 0x61:
		if (!(dev->pb & 0x40) && (val & 0x40)) {
			key_queue_start = key_queue_end = 0;
			dev->want_irq = 0;
			dev->blocked = 0;
			kbd_adddata(0xaa);
		}
		dev->pb = val;
		ppi.pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();

		/* Enable or disable the PC Speaker. */
		speaker_gated = val & 1;
		speaker_enable = val & 2;

#ifdef USE_CASSETTE
		if (dev->type <= KBC_PC82) {
			if (cassette_enabled)
				cassette_motor(! (val & 0x08));
		}
#endif

		if (speaker_enable) 
			speaker_was_enable = 1;
		pit_set_gate(&pit, 2, val & 1);

		if (val & 0x80) {
			dev->pa = 0;
			dev->blocked = 0;
			picintc(2);
		}
		break;

	case 0x63:
		/*
		 * The value written to port 63H is normally 99H,
		 * being the correct 'setup byte' for the original
		 * Intel 8255 PPI device.
		 *
		 * We ignore it here.
		 */
		if (val == 0x99)
			break;
		/*FALLTHROUGH*/

	default:
		ERRLOG("XTkbd: write(%04x, %02x) invalid\n", port, val);
		break;
    }
}


static uint8_t
kbd_read(uint16_t port, void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x60:
		if ((dev->type <= KBC_PC82) && (dev->pb & 0x80))
			ret = dev->pd;
		else if (((dev->type == KBC_XT) || (dev->type == KBC_XT86)) &&
			 (dev->pb & 0x80))
			ret = 0xff;	/* According to Ruud on the PCem forum,
					 * this is supposed to return 0xff on
					 * the XT.   FIXME: check!
					 */
		else
			ret = dev->pa;
		break;

	case 0x61:
		ret = dev->pb;
		break;

	case 0x62:
		if (dev->type == KBC_PC)
			ret = 0x00;
		else if (dev->type == KBC_PC82) {
			if (dev->pb & 0x04)
				ret = ((mem_size - 64) / 32) & 0x0f;
			else
				ret = ((mem_size - 64) / 32) >> 4;
		} else {
			if (dev->pb & 0x08)
				ret = dev->pd >> 4;
			else {
				if (dev->type == KBC_LASER)
					/* LaserXT = Always 512k RAM;
					 * LaserXT/3 = Bit 0: 1=512K, 0=256K
					 */
					ret = (mem_size == 512) ? 0x0d : 0x0c;
				else
					ret = dev->pd & 0x0f;
			}

			/* Tandy uses bit4 for the Serial EEPROM. */
			if (dev->type == KBC_TANDY)
				ret |= dev->read_func(dev->func_priv) ? 0x10 : 0x00;
		}

		/* Indicate the PC SPEAKER state. */
		ret |= (ppispeakon ? 0x20 : 0);

		/* Make the IBM PC BIOS happy (Cassette Interface.) */
		if (dev->type <= KBC_PC82)
			ret |= (ppispeakon ? 0x10 : 0);

		break;

	case 0x63:
		if ((dev->type == KBC_XT) || (dev->type == KBC_XT86) || (dev->type == KBC_GENERIC))
			ret = dev->pd;
		break;

	default:
		ERRLOG("XTkbd: read(%04x) invalid\n", port);
		break;
    }

    return(ret);
}


static void
kbd_reset(void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)priv;

    dev->want_irq = 0;
    dev->blocked = 0;
    dev->pa = 0x00;
    dev->pb = 0x00;

    keyboard_scan = 1;

    key_queue_start = 0;
    key_queue_end = 0;
}


static void *
kbd_init(const device_t *info, UNUSED(void *parent))
{
    int i, fdd_count = 0;
    xtkbd_t *dev;

    dev = (xtkbd_t *)mem_alloc(sizeof(xtkbd_t));
    memset(dev, 0x00, sizeof(xtkbd_t));
    dev->type = info->local;

    /* See how many diskette drives we have. */
    for (i = 0; i < FDD_NUM; i++) {
	if (fdd_get_flags(i))
		fdd_count++;
    }

    if (dev->type <= KBC_XT86) {
	/*
	 * DIP switch readout: bit set = OFF, clear = ON.
	 *
	 * Switches 7, 8 - floppy drives.
	 */
	if (fdd_count == 0)
		dev->pd = 0x00;
	else
		dev->pd = ((fdd_count - 1) << 6);

	/* Switches 5, 6 - video. */
	i = video_type();
	if (i == VID_TYPE_MDA)
		dev->pd |= 0x30;	/* MDA/Herc */
#if 0
	else if (i == VID_TYPE_CGA40)
		dev->pd |= 0x10;	/* CGA, 40 columns */
#endif
	else if (i == VID_TYPE_CGA)
		dev->pd |= 0x20;	/* CGA, 80 colums */
	else if (i == VID_TYPE_SPEC)
		dev->pd |= 0x00;	/* EGA/VGA */

	/* Switches 3, 4 - memory size. */
	if (dev->type == KBC_XT86) {
		/* PC/XT (1986) and up. */
		switch (mem_size) {
			case 256:
				dev->pd |= 0x00;
				break;

			case 512:
				dev->pd |= 0x04;
				break;

			case 576:
				dev->pd |= 0x08;
				break;

			case 640:
			default:
				dev->pd |= 0x0c;
				break;
		}
	} else if (dev->type >= KBC_PC82) {
		/* PC (1982) and PC/XT (1982.) */
		switch (mem_size) {
			case 64:
				dev->pd |= 0x00;
				break;

			case 128:
				dev->pd |= 0x04;
				break;

			case 192:
				dev->pd |= 0x08;
				break;
			case 256:
			default:

				dev->pd |= 0x0c;
				break;
		}
	} else if (dev->type >= KBC_PC) {
		/* PC (1981.) */
		switch (mem_size) {
			case 16:
				dev->pd |= 0x00;
				break;

			case 32:
				dev->pd |= 0x04;
				break;

			case 48:
				dev->pd |= 0x08;
				break;

			case 64:
			default:
				dev->pd |= 0x0c;
				break;
		}
	}

	/* Switch 2 - return bit clear (switch ON) because no 8087 right now. */

	/* Switch 1 - diskette drives available? */
	if (fdd_count > 0)
		dev->pd |= 0x01;
    }

    keyboard_send = kbd_adddata_ex;
    kbd_reset(dev);

    io_sethandler(0x0060, 4,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);

    timer_add(kbd_poll, dev, &keyboard_delay, TIMER_ALWAYS_ENABLED);

    keyboard_set_table(scancode_xt);

    return(dev);
}


static void
kbd_close(void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)priv;

    /* Stop the timer. */
    keyboard_delay = 0;

    /* Disable scanning. */
    keyboard_scan = 0;

    keyboard_send = NULL;

    io_removehandler(0x0060, 4,
		     kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);

    free(dev);
}


const device_t keyboard_pc_device = {
    "IBM PC (1981) Keyboard",
    0,
    KBC_PC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_pc82_device = {
    "IBM PC (1982) Keyboard",
    0,
    KBC_PC82,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_xt_device = {
    "IBM PC/XT (1982) Keyboard",
    0,
    KBC_XT,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_xt86_device = {
    "IBM PC/XT (1986) Keyboard",
    0,
    KBC_XT86,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_generic_device = {
    "Generic XT Keyboard",
    0,
    KBC_GENERIC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_tandy_device = {
    "Tandy 1000 Keyboard",
    0,
    KBC_TANDY,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_laserxt3_device = {
    "VTech Laser XT3 Keyboard",
    0,
    KBC_LASER,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};


void
keyboard_xt_set_funcs(void *arg, uint8_t (*func)(void *), void *priv)
{
    xtkbd_t *dev = (xtkbd_t *)arg;

    dev->read_func = func;
    dev->func_priv = priv;
}
