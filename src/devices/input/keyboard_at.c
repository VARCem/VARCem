/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Intel 8042 (AT keyboard controller) emulation.
 *
 * **NOTE**:	Several changes to disable Mode1 for now, as this breaks 
 *		 the TSX32 operating system. More cleanups needed..
 *
 * **NOTE**	The input functions for the Acer KBC chip (used by V30) is
 *		 not OK yet. One of the commands sent is confusuing it, and
 *		 it either will not process ctrl-alt-esc, or it will not do
 *		 ANY input.
 *
 * Version:	@(#)keyboard_at.c	1.0.29	2019/05/20
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
#define dbglog kbd_log
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../timer.h"
#include "../../device.h"
#include "../system/pic.h"
#include "../system/pit.h"
#include "../system/ppi.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../sound/sound.h"
#include "../sound/snd_speaker.h"
#include "../video/video.h"
#include "../../plat.h"
#include "keyboard.h"

//FIXME: get rid of this!
#include "../../machines/m_tosh3100e.h"


#define USE_SET1		0
#define USE_IGNORE		0


#define STAT_PARITY		0x80
#define STAT_RTIMEOUT		0x40
#define STAT_TTIMEOUT		0x20
#define STAT_MFULL		0x20
#define STAT_UNLOCKED		0x10
#define STAT_CD			0x08
#define STAT_SYSFLAG		0x04
#define STAT_IFULL		0x02
#define STAT_OFULL		0x01

#define PS2_REFRESH_TIME	(16LL * TIMER_USEC)

#define CCB_UNUSED		0x80
#define CCB_TRANSLATE		0x40
#define CCB_PCMODE		0x20
#define CCB_ENABLEKBD		0x10
#define CCB_IGNORELOCK		0x08
#define CCB_SYSTEM		0x04
#define CCB_ENABLEMINT		0x02
#define CCB_ENABLEKINT		0x01

#define CCB_MASK		0x68
#define MODE_MASK		0x6c

#define KBC_TYPE_ISA		0x00		/* AT ISA-based chips */
#define KBC_TYPE_PS2_NOREF	0x01		/* PS2 type, no refresh */
#define KBC_TYPE_PS2_1		0x02		/* PS2 on PS/2, type 1 */
#define KBC_TYPE_PS2_2		0x03		/* PS2 on PS/2, type 2 */
#define KBC_TYPE_MASK		0x03
#define KBC_TYPE(x)		((x)->flags & KBC_TYPE_MASK)

#define KBC_VEN_GENERIC		0x00
#define KBC_VEN_AMI		0x04
#define KBC_VEN_IBM_MCA		0x08
#define KBC_VEN_QUADTEL		0x0c
#define KBC_VEN_TOSHIBA		0x10
#define KBC_VEN_XI8088		0x14
#define KBC_VEN_IBM_PS1		0x18
#define KBC_VEN_ACER		0x1c
#define KBC_VEN_MASK		0x1c
#define KBC_VENDOR(x)		((x)->flags & KBC_VEN_MASK)


typedef struct atkbd {
    int		initialized;
    int		want60,
		wantirq,
		wantirq12;
    uint8_t	command;
    uint8_t	status;
    uint8_t	mem[0x100];
    uint8_t	out;
    int		out_new, out_delayed;
    uint8_t	secr_phase;
    uint8_t	mem_addr;

    uint8_t	input_port,
		output_port;

    uint8_t	old_output_port;

    uint8_t	key_command;
    int		key_wantdata;

    int		last_irq;

    uint8_t	last_scan_code;

    int		dtrans;
    int		first_write;

    int64_t	refresh_time;
    int		refresh;

    uint32_t	flags;
    uint8_t	output_locked;

    int64_t	pulse_cb;
    uint8_t	ami_stat;

    uint8_t	(*write60_ven)(struct atkbd *, uint8_t val);
    uint8_t	(*write64_ven)(struct atkbd *, uint8_t val);

    /* Custom machine-dependent keyboard stuff. */
    uint8_t	(*read_func)(priv_t priv);
    void	(*write_func)(priv_t priv, uint8_t val);
    priv_t	func_priv;
} atkbd_t;


/* bit 0 = repeat, bit 1 = makes break code? */
uint8_t		keyboard_set3_flags[512];
uint8_t		keyboard_set3_all_repeat;
uint8_t		keyboard_set3_all_break;

/* Bits 0 - 1 = scan code set, bit 6 = translate or not. */
uint8_t		keyboard_mode = 0x42;

int		mouse_queue_start = 0,
		mouse_queue_end = 0;


static uint8_t	key_ctrl_queue[16];
static int	key_ctrl_queue_start = 0,
		key_ctrl_queue_end = 0;
static uint8_t	key_queue[16];
static int	key_queue_start = 0,
		key_queue_end = 0;
static uint8_t	mouse_queue[16];
static void	(*mouse_write)(uint8_t val, priv_t priv) = NULL;
static priv_t	mouse_p = NULL;
static uint8_t	sc_or = 0;
static atkbd_t	*SavedKbd = NULL;		// FIXME: remove!!! --FvK


/* Non-translated to translated scan codes. */
static const uint8_t nont_to_t[256] = {
  0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58,
  0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
  0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a,
  0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
  0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c,
  0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
  0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e,
  0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
  0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60,
  0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
  0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e,
  0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
  0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b,
  0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
  0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45,
  0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54,
  0x80, 0x81, 0x82, 0x41, 0x54, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
  0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
  0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#if USE_SET1
static const scancode_t scancode_set1[512] = {
    {	{ 0              },	{ 0              }	},	/* 000 */
    {	{ 0x01           },	{ 0x81           }	},
    {	{ 0x02           },	{ 0x82           }	},
    {	{ 0x03           },	{ 0x83           }	},
    {	{ 0x04           },	{ 0x84           }	},	/* 004 */
    {	{ 0x05           },	{ 0x85           }	},
    {	{ 0x06           },	{ 0x86           }	},
    {	{ 0x07           },	{ 0x87           }	},
    {	{ 0x08           },	{ 0x88           }	},	/* 008 */
    {	{ 0x09           },	{ 0x89           }	},
    {	{ 0x0a           },	{ 0x8a           }	},
    {	{ 0x0b           },	{ 0x8b           }	},
    {	{ 0x0c           },	{ 0x8c           }	},	/* 00c */
    {	{ 0x0d           },	{ 0x8d           }	},
    {	{ 0x0e           },	{ 0x8e           }	},
    {	{ 0x0f           },	{ 0x8f           }	},
    {	{ 0x10           },	{ 0x90           }	},	/* 010 */
    {	{ 0x11           },	{ 0x91           }	},
    {	{ 0x12           },	{ 0x92           }	},
    {	{ 0x13           },	{ 0x93           }	},
    {	{ 0x14           },	{ 0x94           }	},	/* 014 */
    {	{ 0x15           },	{ 0x95           }	},
    {	{ 0x16           },	{ 0x96           }	},
    {	{ 0x17           },	{ 0x97           }	},
    {	{ 0x18           },	{ 0x98           }	},	/* 018 */
    {	{ 0x19           },	{ 0x99           }	},
    {	{ 0x1a           },	{ 0x9a           }	},
    {	{ 0x1b           },	{ 0x9b           }	},
    {	{ 0x1c           },	{ 0x9c           }	},	/* 01c */
    {	{ 0x1d           },	{ 0x9d           }	},
    {	{ 0x1e           },	{ 0x9e           }	},
    {	{ 0x1f           },	{ 0x9f           }	},
    {	{ 0x20           },	{ 0xa0           }	},	/* 020 */
    {	{ 0x21           },	{ 0xa1           }	},
    {	{ 0x22           },	{ 0xa2           }	},
    {	{ 0x23           },	{ 0xa3           }	},
    {	{ 0x24           },	{ 0xa4           }	},	/* 024 */
    {	{ 0x25           },	{ 0xa5           }	},
    {	{ 0x26           },	{ 0xa6           }	},
    {	{ 0x27           },	{ 0xa7           }	},
    {	{ 0x28           },	{ 0xa8           }	},	/* 028 */
    {	{ 0x29           },	{ 0xa9           }	},
    {	{ 0x2a           },	{ 0xaa           }	},
    {	{ 0x2b           },	{ 0xab           }	},
    {	{ 0x2c           },	{ 0xac           }	},	/* 02c */
    {	{ 0x2d           },	{ 0xad           }	},
    {	{ 0x2e           },	{ 0xae           }	},
    {	{ 0x2f           },	{ 0xaf           }	},
    {	{ 0x30           },	{ 0xb0           }	},	/* 030 */
    {	{ 0x31           },	{ 0xb1           }	},
    {	{ 0x32           },	{ 0xb2           }	},
    {	{ 0x33           },	{ 0xb3           }	},
    {	{ 0x34           },	{ 0xb4           }	},	/* 034 */
    {	{ 0x35           },	{ 0xb5           }	},
    {	{ 0x36           },	{ 0xb6           }	},
    {	{ 0x37           },	{ 0xb7           }	},
    {	{ 0x38           },	{ 0xb8           }	},	/* 038 */
    {	{ 0x39           },	{ 0xb9           }	},
    {	{ 0x3a           },	{ 0xba           }	},
    {	{ 0x3b           },	{ 0xbb           }	},
    {	{ 0x3c           },	{ 0xbc           }	},	/* 03c */
    {	{ 0x3d           },	{ 0xbd           }	},
    {	{ 0x3e           },	{ 0xbe           }	},
    {	{ 0x3f           },	{ 0xbf           }	},
    {	{ 0x40           },	{ 0xc0           }	},	/* 040 */
    {	{ 0x41           },	{ 0xc1           }	},
    {	{ 0x42           },	{ 0xc2           }	},
    {	{ 0x43           },	{ 0xc3           }	},
    {	{ 0x44           },	{ 0xc4           }	},	/* 044 */
    {	{ 0x45           },	{ 0xc5           }	},
    {	{ 0x46           },	{ 0xc6           }	},
    {	{ 0x47           },	{ 0xc7           }	},
    {	{ 0x48           },	{ 0xc8           }	},	/* 048 */
    {	{ 0x49           },	{ 0xc9           }	},
    {	{ 0x4a           },	{ 0xca           }	},
    {	{ 0x4b           },	{ 0xcb           }	},
    {	{ 0x4c           },	{ 0xcc           }	},	/* 04c */
    {	{ 0x4d           },	{ 0xcd           }	},
    {	{ 0x4e           },	{ 0xce           }	},
    {	{ 0x4f           },	{ 0xcf           }	},
    {	{ 0x50           },	{ 0xd0           }	},	/* 050 */
    {	{ 0x51           },	{ 0xd1           }	},
    {	{ 0x52           },	{ 0xd2           }	},
    {	{ 0x53           },	{ 0xd3           }	},
    {	{ 0x54           },	{ 0xd4           }	},	/* 054 */
    {	{ 0x55           },	{ 0xd5           }	},
    {	{ 0x56           },	{ 0xd6           }	},
    {	{ 0x57           },	{ 0xd7           }	},
    {	{ 0x58           },	{ 0xd8           }	},	/* 058 */
    {	{ 0x59           },	{ 0xd9           }	},
    {	{ 0x5a           },	{ 0xda           }	},
    {	{ 0x5b           },	{ 0xdb           }	},
    {	{ 0x5c           },	{ 0xdc           }	},	/* 05c */
    {	{ 0x5d           },	{ 0xdd           }	},
    {	{ 0x5e           },	{ 0xde           }	},
    {	{ 0x5f           },	{ 0xdf           }	},
    {	{ 0x60           },	{ 0xe0           }	},	/* 060 */
    {	{ 0x61           },	{ 0xe1           }	},
    {	{ 0x62           },	{ 0xe2           }	},
    {	{ 0x63           },	{ 0xe3           }	},
    {	{ 0x64           },	{ 0xe4           }	},	/* 064 */
    {	{ 0x65           },	{ 0xe5           }	},
    {	{ 0x66           },	{ 0xe6           }	},
    {	{ 0x67           },	{ 0xe7           }	},
    {	{ 0x68           },	{ 0xe8           }	},	/* 068 */
    {	{ 0x69           },	{ 0xe9           }	},
    {	{ 0x6a           },	{ 0xea           }	},
    {	{ 0x6b           },	{ 0xeb           }	},
    {	{ 0x6c           },	{ 0xec           }	},	/* 06c */
    {	{ 0x6d           },	{ 0xed           }	},
    {	{ 0x6e           },	{ 0xee           }	},
    {	{ 0x6f           },	{ 0xef           }	},
    {	{ 0x70           },	{ 0xf0           }	},	/* 070 */
    {	{ 0x71           },	{ 0xf1           }	},
    {	{ 0x72           },	{ 0xf2           }	},
    {	{ 0x73           },	{ 0xf3           }	},
    {	{ 0x74           },	{ 0xf4           }	},	/* 074 */
    {	{ 0x75           },	{ 0xf5           }	},
    {	{ 0x76           },	{ 0xf6           }	},
    {	{ 0x77           },	{ 0xf7           }	},
    {	{ 0x78           },	{ 0xf8           }	},	/* 078 */
    {	{ 0x79           },	{ 0xf9           }	},
    {	{ 0x7a           },	{ 0xfa           }	},
    {	{ 0x7b           },	{ 0xfb           }	},
    {	{ 0x7c           },	{ 0xfc           }	},	/* 07c */
    {	{ 0x7d           },	{ 0xfd           }	},
    {	{ 0x7e           },	{ 0xfe           }	},
    {	{ 0x7f           },	{ 0xff           }	},
    {	{ 0x80           },	{ 0              }	},	/* 080 */
    {	{ 0x81           },	{ 0              }	},
    {	{ 0x82           },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 084 */
    {	{ 0x85           },	{ 0              }	},
    {	{ 0x86           },	{ 0              }	},
    {	{ 0x87           },	{ 0              }	},
    {	{ 0x88           },	{ 0              }	},	/* 088 */
    {	{ 0x89           },	{ 0              }	},
    {	{ 0x8a           },	{ 0              }	},
    {	{ 0x8b           },	{ 0              }	},
    {	{ 0x8c           },	{ 0              }	},	/* 08c */
    {	{ 0x8d           },	{ 0              }	},
    {	{ 0x8e           },	{ 0              }	},
    {	{ 0x8f           },	{ 0              }	},
    {	{ 0x90           },	{ 0              }	},	/* 090 */
    {	{ 0x91           },	{ 0              }	},
    {	{ 0x92           },	{ 0              }	},
    {	{ 0x93           },	{ 0              }	},
    {	{ 0x94           },	{ 0              }	},	/* 094 */
    {	{ 0x95           },	{ 0              }	},
    {	{ 0x96           },	{ 0              }	},
    {	{ 0x97           },	{ 0              }	},
    {	{ 0x98           },	{ 0              }	},	/* 098 */
    {	{ 0x99           },	{ 0              }	},
    {	{ 0x9a           },	{ 0              }	},
    {	{ 0x9b           },	{ 0              }	},
    {	{ 0x9c           },	{ 0              }	},	/* 09c */
    {	{ 0x9d           },	{ 0              }	},
    {	{ 0x9e           },	{ 0              }	},
    {	{ 0x9f           },	{ 0              }	},
    {	{ 0xa0           },	{ 0              }	},	/* 0a0 */
    {	{ 0xa1           },	{ 0              }	},
    {	{ 0xa2           },	{ 0              }	},
    {	{ 0xa3           },	{ 0              }	},
    {	{ 0xa4           },	{ 0              }	},	/* 0a4 */
    {	{ 0xa5           },	{ 0              }	},
    {	{ 0xa6           },	{ 0              }	},
    {	{ 0xa7           },	{ 0              }	},
    {	{ 0xa8           },	{ 0              }	},	/* 0a8 */
    {	{ 0xa9           },	{ 0              }	},
    {	{ 0xaa           },	{ 0              }	},
    {	{ 0xab           },	{ 0              }	},
    {	{ 0xac           },	{ 0              }	},	/* 0ac */
    {	{ 0xad           },	{ 0              }	},
    {	{ 0xae           },	{ 0              }	},
    {	{ 0xaf           },	{ 0              }	},
    {	{ 0xb0           },	{ 0              }	},	/* 0b0 */
    {	{ 0xb1           },	{ 0              }	},
    {	{ 0xb2           },	{ 0              }	},
    {	{ 0xb3           },	{ 0              }	},
    {	{ 0xb4           },	{ 0              }	},	/* 0b4 */
    {	{ 0xb5           },	{ 0              }	},
    {	{ 0xb6           },	{ 0              }	},
    {	{ 0xb7           },	{ 0              }	},
    {	{ 0xb8           },	{ 0              }	},	/* 0b8 */
    {	{ 0xb9           },	{ 0              }	},
    {	{ 0xba           },	{ 0              }	},
    {	{ 0xbb           },	{ 0              }	},
    {	{ 0xbc           },	{ 0              }	},	/* 0bc */
    {	{ 0xbd           },	{ 0              }	},
    {	{ 0xbe           },	{ 0              }	},
    {	{ 0xbf           },	{ 0              }	},
    {	{ 0xc0           },	{ 0              }	},	/* 0c0 */
    {	{ 0xc1           },	{ 0              }	},
    {	{ 0xc2           },	{ 0              }	},
    {	{ 0xc3           },	{ 0              }	},
    {	{ 0xc4           },	{ 0              }	},	/* 0c4 */
    {	{ 0xc5           },	{ 0              }	},
    {	{ 0xc6           },	{ 0              }	},
    {	{ 0xc7           },	{ 0              }	},
    {	{ 0xc8           },	{ 0              }	},	/* 0c8 */
    {	{ 0xc9           },	{ 0              }	},
    {	{ 0xca           },	{ 0              }	},
    {	{ 0xcb           },	{ 0              }	},
    {	{ 0xcc           },	{ 0              }	},	/* 0cc */
    {	{ 0xcd           },	{ 0              }	},
    {	{ 0xce           },	{ 0              }	},
    {	{ 0xcf           },	{ 0              }	},
    {	{ 0xd0           },	{ 0              }	},	/* 0d0 */
    {	{ 0xd1           },	{ 0              }	},
    {	{ 0xd2           },	{ 0              }	},
    {	{ 0xd3           },	{ 0              }	},
    {	{ 0xd4           },	{ 0              }	},	/* 0d4 */
    {	{ 0xd5           },	{ 0              }	},
    {	{ 0xd6           },	{ 0              }	},
    {	{ 0xd7           },	{ 0              }	},
    {	{ 0xd8           },	{ 0              }	},	/* 0d8 */
    {	{ 0xd9           },	{ 0              }	},
    {	{ 0xda           },	{ 0              }	},
    {	{ 0xdb           },	{ 0              }	},
    {	{ 0xdc           },	{ 0              }	},	/* 0dc */
    {	{ 0xdd           },	{ 0              }	},
    {	{ 0xde           },	{ 0              }	},
    {	{ 0xdf           },	{ 0              }	},
    {	{ 0xe0           },	{ 0              }	},	/* 0e0 */
    {	{ 0xe1           },	{ 0              }	},
    {	{ 0xe2           },	{ 0              }	},
    {	{ 0xe3           },	{ 0              }	},
    {	{ 0xe4           },	{ 0              }	},	/* 0e4 */
    {	{ 0xe5           },	{ 0              }	},
    {	{ 0xe6           },	{ 0              }	},
    {	{ 0xe7           },	{ 0              }	},
    {	{ 0xe8           },	{ 0              }	},	/* 0e8 */
    {	{ 0xe9           },	{ 0              }	},
    {	{ 0xea           },	{ 0              }	},
    {	{ 0xeb           },	{ 0              }	},
    {	{ 0xec           },	{ 0              }	},	/* 0ec */
    {	{ 0xed           },	{ 0              }	},
    {	{ 0xee           },	{ 0              }	},
    {	{ 0xef           },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 0f0 */
    {	{ 0xf1           },	{ 0              }	},
    {	{ 0xf2           },	{ 0              }	},
    {	{ 0xf3           },	{ 0              }	},
    {	{ 0xf4           },	{ 0              }	},	/* 0f4 */
    {	{ 0xf5           },	{ 0              }	},
    {	{ 0xf6           },	{ 0              }	},
    {	{ 0xf7           },	{ 0              }	},
    {	{ 0xf8           },	{ 0              }	},	/* 0f8 */
    {	{ 0xf9           },	{ 0              }	},
    {	{ 0xfa           },	{ 0              }	},
    {	{ 0xfb           },	{ 0              }	},
    {	{ 0xfc           },	{ 0              }	},	/* 0fc */
    {	{ 0xfd           },	{ 0              }	},
    {	{ 0xfe           },	{ 0              }	},
    {	{ 0xff           },	{ 0              }	},

    {	{ 0xe1,0x1d      },	{ 0xe1,0x9d      }	},	/* 100 */
    {	{ 0xe0,0x01      },	{ 0xe0,0x81      }	},
    {	{ 0xe0,0x02      },	{ 0xe0,0x82      }	},
    {	{ 0xe0,0x03      },	{ 0xe0,0x83      }	},
    {	{ 0xe0,0x04      },	{ 0xe0,0x84      }	},	/* 104 */
    {	{ 0xe0,0x05      },	{ 0xe0,0x85      }	},
    {	{ 0xe0,0x06      },	{ 0xe0,0x86      }	},
    {	{ 0xe0,0x07      },	{ 0xe0,0x87      }	},
    {	{ 0xe0,0x08      },	{ 0xe0,0x88      }	},	/* 108 */
    {	{ 0xe0,0x09      },	{ 0xe0,0x89      }	},
    {	{ 0xe0,0x0a      },	{ 0xe0,0x8a      }	},
    {	{ 0xe0,0x0b      },	{ 0xe0,0x8b      }	},
    {	{ 0xe0,0x0c      },	{ 0xe0,0x8c      }	},	/* 10c */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x0e      },	{ 0xe0,0x8e      }	},
    {	{ 0xe0,0x0f      },	{ 0xe0,0x8f      }	},
    {	{ 0xe0,0x10      },	{ 0xe0,0x90      }	},	/* 110 */
    {	{ 0xe0,0x11      },	{ 0xe0,0x91      }	},
    {	{ 0xe0,0x12      },	{ 0xe0,0x92      }	},
    {	{ 0xe0,0x13      },	{ 0xe0,0x93      }	},
    {	{ 0xe0,0x14      },	{ 0xe0,0x94      }	},	/* 114 */
    {	{ 0xe0,0x15      },	{ 0xe0,0x95      }	},
    {	{ 0xe0,0x16      },	{ 0xe0,0x96      }	},
    {	{ 0xe0,0x17      },	{ 0xe0,0x97      }	},
    {	{ 0xe0,0x18      },	{ 0xe0,0x98      }	},	/* 118 */
    {	{ 0xe0,0x19      },	{ 0xe0,0x99      }	},
    {	{ 0xe0,0x1a      },	{ 0xe0,0x9a      }	},
    {	{ 0xe0,0x1b      },	{ 0xe0,0x9b      }	},
    {	{ 0xe0,0x1c      },	{ 0xe0,0x9c      }	},	/* 11c */
    {	{ 0xe0,0x1d      },	{ 0xe0,0x9d      }	},
    {	{ 0xe0,0x1e      },	{ 0xe0,0x9e      }	},
    {	{ 0xe0,0x1f      },	{ 0xe0,0x9f      }	},
    {	{ 0xe0,0x20      },	{ 0xe0,0xa0      }	},	/* 120 */
    {	{ 0xe0,0x21      },	{ 0xe0,0xa1      }	},
    {	{ 0xe0,0x22      },	{ 0xe0,0xa2      }	},
    {	{ 0xe0,0x23      },	{ 0xe0,0xa3      }	},
    {	{ 0xe0,0x24      },	{ 0xe0,0xa4      }	},	/* 124 */
    {	{ 0xe0,0x25      },	{ 0xe0,0xa5      }	},
    {	{ 0xe0,0x26      },	{ 0xe0,0xa6      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 128 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x2c      },	{ 0xe0,0xac      }	},	/* 12c */
    {	{ 0xe0,0x2d      },	{ 0xe0,0xad      }	},
    {	{ 0xe0,0x2e      },	{ 0xe0,0xae      }	},
    {	{ 0xe0,0x2f      },	{ 0xe0,0xaf      }	},
    {	{ 0xe0,0x30      },	{ 0xe0,0xb0      }	},	/* 130 */
    {	{ 0xe0,0x31      },	{ 0xe0,0xb1      }	},
    {	{ 0xe0,0x32      },	{ 0xe0,0xb2      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x34      },	{ 0xe0,0xb4      }	},	/* 134 */
    {	{ 0xe0,0x35      },	{ 0xe0,0xb5      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x37      },	{ 0xe0,0xb7      }	},
    {	{ 0xe0,0x38      },	{ 0xe0,0xb8      }	},	/* 138 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x3a      },	{ 0xe0,0xba      }	},
    {	{ 0xe0,0x3b      },	{ 0xe0,0xbb      }	},
    {	{ 0xe0,0x3c      },	{ 0xe0,0xbc      }	},	/* 13c */
    {	{ 0xe0,0x3d      },	{ 0xe0,0xbd      }	},
    {	{ 0xe0,0x3e      },	{ 0xe0,0xbe      }	},
    {	{ 0xe0,0x3f      },	{ 0xe0,0xbf      }	},
    {	{ 0xe0,0x40      },	{ 0xe0,0xc0      }	},	/* 140 */
    {	{ 0xe0,0x41      },	{ 0xe0,0xc1      }	},
    {	{ 0xe0,0x42      },	{ 0xe0,0xc2      }	},
    {	{ 0xe0,0x43      },	{ 0xe0,0xc3      }	},
    {	{ 0xe0,0x44      },	{ 0xe0,0xc4      }	},	/* 144 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x46      },	{ 0xe0,0xc6      }	},
    {	{ 0xe0,0x47      },	{ 0xe0,0xc7      }	},
    {	{ 0xe0,0x48      },	{ 0xe0,0xc8      }	},	/* 148 */
    {	{ 0xe0,0x49      },	{ 0xe0,0xc9      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x4b      },	{ 0xe0,0xcb      }	},
    {	{ 0xe0,0x4c      },	{ 0xe0,0xcc      }	},	/* 14c */
    {	{ 0xe0,0x4d      },	{ 0xe0,0xcd      }	},
    {	{ 0xe0,0x4e      },	{ 0xe0,0xce      }	},
    {	{ 0xe0,0x4f      },	{ 0xe0,0xcf      }	},
    {	{ 0xe0,0x50      },	{ 0xe0,0xd0      }	},	/* 150 */
    {	{ 0xe0,0x51      },	{ 0xe0,0xd1      }	},
    {	{ 0xe0,0x52      },	{ 0xe0,0xd2      }	},
    {	{ 0xe0,0x53      },	{ 0xe0,0xd3      }	},
    {	{ 0              },	{ 0              }	},	/* 154 */
    {	{ 0xe0,0x55      },	{ 0xe0,0xd5      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x57      },	{ 0xe0,0xd7      }	},
    {	{ 0xe0,0x58      },	{ 0xe0,0xd8      }	},	/* 158 */
    {	{ 0xe0,0x59      },	{ 0xe0,0xd9      }	},
    {	{ 0xe0,0x5a      },	{ 0xe0,0xaa      }	},
    {	{ 0xe0,0x5b      },	{ 0xe0,0xdb      }	},
    {	{ 0xe0,0x5c      },	{ 0xe0,0xdc      }	},	/* 15c */
    {	{ 0xe0,0x5d      },	{ 0xe0,0xdd      }	},
    {	{ 0xe0,0x5e      },	{ 0xe0,0xee      }	},
    {	{ 0xe0,0x5f      },	{ 0xe0,0xdf      }	},
    {	{ 0              },	{ 0              }	},	/* 160 */
    {	{ 0xe0,0x61      },	{ 0xe0,0xe1      }	},
    {	{ 0xe0,0x62      },	{ 0xe0,0xe2      }	},
    {	{ 0xe0,0x63      },	{ 0xe0,0xe3      }	},
    {	{ 0xe0,0x64      },	{ 0xe0,0xe4      }	},	/* 164 */
    {	{ 0xe0,0x65      },	{ 0xe0,0xe5      }	},
    {	{ 0xe0,0x66      },	{ 0xe0,0xe6      }	},
    {	{ 0xe0,0x67      },	{ 0xe0,0xe7      }	},
    {	{ 0xe0,0x68      },	{ 0xe0,0xe8      }	},	/* 168 */
    {	{ 0xe0,0x69      },	{ 0xe0,0xe9      }	},
    {	{ 0xe0,0x6a      },	{ 0xe0,0xea      }	},
    {	{ 0xe0,0x6b      },	{ 0xe0,0xeb      }	},
    {	{ 0xe0,0x6c      },	{ 0xe0,0xec      }	},	/* 16c */
    {	{ 0xe0,0x6d      },	{ 0xe0,0xed      }	},
    {	{ 0xe0,0x6e      },	{ 0xe0,0xee      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x70      },	{ 0xe0,0xf0      }	},	/* 170 */
    {	{ 0xe0,0x71      },	{ 0xe0,0xf1      }	},
    {	{ 0xe0,0x72      },	{ 0xe0,0xf2      }	},
    {	{ 0xe0,0x73      },	{ 0xe0,0xf3      }	},
    {	{ 0xe0,0x74      },	{ 0xe0,0xf4      }	},	/* 174 */
    {	{ 0xe0,0x75      },	{ 0xe0,0xf5      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x77      },	{ 0xe0,0xf7      }	},
    {	{ 0xe0,0x78      },	{ 0xe0,0xf8      }	},	/* 178 */
    {	{ 0xe0,0x79      },	{ 0xe0,0xf9      }	},
    {	{ 0xe0,0x7a      },	{ 0xe0,0xfa      }	},
    {	{ 0xe0,0x7b      },	{ 0xe0,0xfb      }	},
    {	{ 0xe0,0x7c      },	{ 0xe0,0xfc      }	},	/* 17c */
    {	{ 0xe0,0x7d      },	{ 0xe0,0xfd      }	},
    {	{ 0xe0,0x7e      },	{ 0xe0,0xfe      }	},
    {	{ 0xe0,0x7f      },	{ 0xe0,0xff      }	},
    {	{ 0              },	{ 0              }	},	/* 180 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 184 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 188 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 18c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 190 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 194 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 198 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 19c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ac */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1bc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1cc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d0 */
    {	{ 0xe0,0xe1      },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1dc */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xee      },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e0 */
    {	{ 0xe0,0xf1      },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ec */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xfe      },	{ 0              }	},
    {	{ 0xe0,0xff      },	{ 0              }	}
};
#endif

static const scancode_t scancode_set2[512] = {
    {	{ 0              },	{ 0              }	},	/* 000 */
    {	{ 0x76           },	{ 0xf0,0x76      }	},
    {	{ 0x16           },	{ 0xf0,0x16      }	},
    {	{ 0x1e           },	{ 0xf0,0x1e      }	},
    {	{ 0x26           },	{ 0xf0,0x26      }	},	/* 004 */
    {	{ 0x25           },	{ 0xf0,0x25      }	},
    {	{ 0x2e           },	{ 0xf0,0x2e      }	},
    {	{ 0x36           },	{ 0xf0,0x36      }	},
    {	{ 0x3d           },	{ 0xf0,0x3d      }	},	/* 008 */
    {	{ 0x3e           },	{ 0xf0,0x3e      }	},
    {	{ 0x46           },	{ 0xf0,0x46      }	},
    {	{ 0x45           },	{ 0xf0,0x45      }	},
    {	{ 0x4e           },	{ 0xf0,0x4e      }	},	/* 00c */
    {	{ 0x55           },	{ 0xf0,0x55      }	},
    {	{ 0x66           },	{ 0xf0,0x66      }	},
    {	{ 0x0d           },	{ 0xf0,0x0d      }	},
    {	{ 0x15           },	{ 0xf0,0x15      }	},	/* 010 */
    {	{ 0x1d           },	{ 0xf0,0x1d      }	},
    {	{ 0x24           },	{ 0xf0,0x24      }	},
    {	{ 0x2d           },	{ 0xf0,0x2d      }	},
    {	{ 0x2c           },	{ 0xf0,0x2c      }	},	/* 014 */
    {	{ 0x35           },	{ 0xf0,0x35      }	},
    {	{ 0x3c           },	{ 0xf0,0x3c      }	},
    {	{ 0x43           },	{ 0xf0,0x43      }	},
    {	{ 0x44           },	{ 0xf0,0x44      }	},	/* 018 */
    {	{ 0x4d           },	{ 0xf0,0x4d      }	},
    {	{ 0x54           },	{ 0xf0,0x54      }	},
    {	{ 0x5b           },	{ 0xf0,0x5b      }	},
    {	{ 0x5a           },	{ 0xf0,0x5a      }	},	/* 01c */
    {	{ 0x14           },	{ 0xf0,0x14      }	},
    {	{ 0x1c           },	{ 0xf0,0x1c      }	},
    {	{ 0x1b           },	{ 0xf0,0x1b      }	},
    {	{ 0x23           },	{ 0xf0,0x23      }	},	/* 020 */
    {	{ 0x2b           },	{ 0xf0,0x2b      }	},
    {	{ 0x34           },	{ 0xf0,0x34      }	},
    {	{ 0x33           },	{ 0xf0,0x33      }	},
    {	{ 0x3b           },	{ 0xf0,0x3b      }	},	/* 024 */
    {	{ 0x42           },	{ 0xf0,0x42      }	},
    {	{ 0x4b           },	{ 0xf0,0x4b      }	},
    {	{ 0x4c           },	{ 0xf0,0x4c      }	},
    {	{ 0x52           },	{ 0xf0,0x52      }	},	/* 028 */
    {	{ 0x0e           },	{ 0xf0,0x0e      }	},
    {	{ 0x12           },	{ 0xf0,0x12      }	},
    {	{ 0x5d           },	{ 0xf0,0x5d      }	},
    {	{ 0x1a           },	{ 0xf0,0x1a      }	},	/* 02c */
    {	{ 0x22           },	{ 0xf0,0x22      }	},
    {	{ 0x21           },	{ 0xf0,0x21      }	},
    {	{ 0x2a           },	{ 0xf0,0x2a      }	},
    {	{ 0x32           },	{ 0xf0,0x32      }	},	/* 030 */
    {	{ 0x31           },	{ 0xf0,0x31      }	},
    {	{ 0x3a           },	{ 0xf0,0x3a      }	},
    {	{ 0x41           },	{ 0xf0,0x41      }	},
    {	{ 0x49           },	{ 0xf0,0x49      }	},	/* 034 */
    {	{ 0x4a           },	{ 0xf0,0x4a      }	},
    {	{ 0x59           },	{ 0xf0,0x59      }	},
    {	{ 0x7c           },	{ 0xf0,0x7c      }	},
    {	{ 0x11           },	{ 0xf0,0x11      }	},	/* 038 */
    {	{ 0x29           },	{ 0xf0,0x29      }	},
    {	{ 0x58           },	{ 0xf0,0x58      }	},
    {	{ 0x05           },	{ 0xf0,0x05      }	},
    {	{ 0x06           },	{ 0xf0,0x06      }	},	/* 03c */
    {	{ 0x04           },	{ 0xf0,0x04      }	},
    {	{ 0x0c           },	{ 0xf0,0x0c      }	},
    {	{ 0x03           },	{ 0xf0,0x03      }	},
    {	{ 0x0b           },	{ 0xf0,0x0b      }	},	/* 040 */
    {	{ 0x83           },	{ 0xf0,0x83      }	},
    {	{ 0x0a           },	{ 0xf0,0x0a      }	},
    {	{ 0x01           },	{ 0xf0,0x01      }	},
    {	{ 0x09           },	{ 0xf0,0x09      }	},	/* 044 */
    {	{ 0x77           },	{ 0xf0,0x77      }	},
    {	{ 0x7e           },	{ 0xf0,0x7e      }	},
    {	{ 0x6c           },	{ 0xf0,0x6c      }	},
    {	{ 0x75           },	{ 0xf0,0x75      }	},	/* 048 */
    {	{ 0x7d           },	{ 0xf0,0x7d      }	},
    {	{ 0x7b           },	{ 0xf0,0x7b      }	},
    {	{ 0x6b           },	{ 0xf0,0x6b      }	},
    {	{ 0x73           },	{ 0xf0,0x73      }	},	/* 04c */
    {	{ 0x74           },	{ 0xf0,0x74      }	},
    {	{ 0x79           },	{ 0xf0,0x79      }	},
    {	{ 0x69           },	{ 0xf0,0x69      }	},
    {	{ 0x72           },	{ 0xf0,0x72      }	},	/* 050 */
    {	{ 0x7a           },	{ 0xf0,0x7a      }	},
    {	{ 0x70           },	{ 0xf0,0x70      }	},
    {	{ 0x71           },	{ 0xf0,0x71      }	},
    {	{ 0x84           },	{ 0xf0,0x84      }	},	/* 054 */
    {	{ 0x60           },	{ 0xf0,0x60      }	},
    {	{ 0x61           },	{ 0xf0,0x61      }	},
    {	{ 0x78           },	{ 0xf0,0x78      }	},
    {	{ 0x07           },	{ 0xf0,0x07      }	},	/* 058 */
    {	{ 0x0f           },	{ 0xf0,0x0f      }	},
    {	{ 0x17           },	{ 0xf0,0x17      }	},
    {	{ 0x1f           },	{ 0xf0,0x1f      }	},
    {	{ 0x27           },	{ 0xf0,0x27      }	},	/* 05c */
    {	{ 0x2f           },	{ 0xf0,0x2f      }	},
    {	{ 0x37           },	{ 0xf0,0x37      }	},
    {	{ 0x3f           },	{ 0xf0,0x3f      }	},
    {	{ 0x47           },	{ 0xf0,0x47      }	},	/* 060 */
    {	{ 0x4f           },	{ 0xf0,0x4f      }	},
    {	{ 0x56           },	{ 0xf0,0x56      }	},
    {	{ 0x5e           },	{ 0xf0,0x5e      }	},
    {	{ 0x08           },	{ 0xf0,0x08      }	},	/* 064 */
    {	{ 0x10           },	{ 0xf0,0x10      }	},
    {	{ 0x18           },	{ 0xf0,0x18      }	},
    {	{ 0x20           },	{ 0xf0,0x20      }	},
    {	{ 0x28           },	{ 0xf0,0x28      }	},	/* 068 */
    {	{ 0x30           },	{ 0xf0,0x30      }	},
    {	{ 0x38           },	{ 0xf0,0x38      }	},
    {	{ 0x40           },	{ 0xf0,0x40      }	},
    {	{ 0x48           },	{ 0xf0,0x48      }	},	/* 06c */
    {	{ 0x50           },	{ 0xf0,0x50      }	},
    {	{ 0x57           },	{ 0xf0,0x57      }	},
    {	{ 0x6f           },	{ 0xf0,0x6f      }	},
    {	{ 0x13           },	{ 0xf0,0x13      }	},	/* 070 */
    {	{ 0x19           },	{ 0xf0,0x19      }	},
    {	{ 0x39           },	{ 0xf0,0x39      }	},
    {	{ 0x51           },	{ 0xf0,0x51      }	},
    {	{ 0x53           },	{ 0xf0,0x53      }	},	/* 074 */
    {	{ 0x5c           },	{ 0xf0,0x5c      }	},
    {	{ 0x5f           },	{ 0xf0,0x5f      }	},
    {	{ 0x62           },	{ 0xf0,0x62      }	},
    {	{ 0x63           },	{ 0xf0,0x63      }	},	/* 078 */
    {	{ 0x64           },	{ 0xf0,0x64      }	},
    {	{ 0x65           },	{ 0xf0,0x65      }	},
    {	{ 0x67           },	{ 0xf0,0x67      }	},
    {	{ 0x68           },	{ 0xf0,0x68      }	},	/* 07c */
    {	{ 0x6a           },	{ 0xf0,0x6a      }	},
    {	{ 0x6d           },	{ 0xf0,0x6d      }	},
    {	{ 0x6e           },	{ 0xf0,0x6e      }	},
    {	{ 0x80           },	{ 0xf0,0x80      }	},	/* 080 */
    {	{ 0x81           },	{ 0xf0,0x81      }	},
    {	{ 0x82           },	{ 0xf0,0x82      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 084 */
    {	{ 0x85           },	{ 0xf0,0x54      }	},
    {	{ 0x86           },	{ 0xf0,0x86      }	},
    {	{ 0x87           },	{ 0xf0,0x87      }	},
    {	{ 0x88           },	{ 0xf0,0x88      }	},	/* 088 */
    {	{ 0x89           },	{ 0xf0,0x89      }	},
    {	{ 0x8a           },	{ 0xf0,0x8a      }	},
    {	{ 0x8b           },	{ 0xf0,0x8b      }	},
    {	{ 0x8c           },	{ 0xf0,0x8c      }	},	/* 08c */
    {	{ 0x8d           },	{ 0xf0,0x8d      }	},
    {	{ 0x8e           },	{ 0xf0,0x8e      }	},
    {	{ 0x8f           },	{ 0xf0,0x8f      }	},
    {	{ 0x90           },	{ 0xf0,0x90      }	},	/* 090 */
    {	{ 0x91           },	{ 0xf0,0x91      }	},
    {	{ 0x92           },	{ 0xf0,0x92      }	},
    {	{ 0x93           },	{ 0xf0,0x93      }	},
    {	{ 0x94           },	{ 0xf0,0x94      }	},	/* 094 */
    {	{ 0x95           },	{ 0xf0,0x95      }	},
    {	{ 0x96           },	{ 0xf0,0x96      }	},
    {	{ 0x97           },	{ 0xf0,0x97      }	},
    {	{ 0x98           },	{ 0xf0,0x98      }	},	/* 098 */
    {	{ 0x99           },	{ 0xf0,0x99      }	},
    {	{ 0x9a           },	{ 0xf0,0x9a      }	},
    {	{ 0x9b           },	{ 0xf0,0x9b      }	},
    {	{ 0x9c           },	{ 0xf0,0x9c      }	},	/* 09c */
    {	{ 0x9d           },	{ 0xf0,0x9d      }	},
    {	{ 0x9e           },	{ 0xf0,0x9e      }	},
    {	{ 0x9f           },	{ 0xf0,0x9f      }	},
    {	{ 0xa0           },	{ 0xf0,0xa0      }	},	/* 0a0 */
    {	{ 0xa1           },	{ 0xf0,0xa1      }	},
    {	{ 0xa2           },	{ 0xf0,0xa2      }	},
    {	{ 0xa3           },	{ 0xf0,0xa3      }	},
    {	{ 0xa4           },	{ 0xf0,0xa4      }	},	/* 0a4 */
    {	{ 0xa5           },	{ 0xf0,0xa5      }	},
    {	{ 0xa6           },	{ 0xf0,0xa6      }	},
    {	{ 0xa7           },	{ 0xf0,0xa7      }	},
    {	{ 0xa8           },	{ 0xf0,0xa8      }	},	/* 0a8 */
    {	{ 0xa9           },	{ 0xf0,0xa9      }	},
    {	{ 0xaa           },	{ 0xf0,0xaa      }	},
    {	{ 0xab           },	{ 0xf0,0xab      }	},
    {	{ 0xac           },	{ 0xf0,0xac      }	},	/* 0ac */
    {	{ 0xad           },	{ 0xf0,0xad      }	},
    {	{ 0xae           },	{ 0xf0,0xae      }	},
    {	{ 0xaf           },	{ 0xf0,0xaf      }	},
    {	{ 0xb0           },	{ 0xf0,0xb0      }	},	/* 0b0 */
    {	{ 0xb1           },	{ 0xf0,0xb1      }	},
    {	{ 0xb2           },	{ 0xf0,0xb2      }	},
    {	{ 0xb3           },	{ 0xf0,0xb3      }	},
    {	{ 0xb4           },	{ 0xf0,0xb4      }	},	/* 0b4 */
    {	{ 0xb5           },	{ 0xf0,0xb5      }	},
    {	{ 0xb6           },	{ 0xf0,0xb6      }	},
    {	{ 0xb7           },	{ 0xf0,0xb7      }	},
    {	{ 0xb8           },	{ 0xf0,0xb8      }	},	/* 0b8 */
    {	{ 0xb9           },	{ 0xf0,0xb9      }	},
    {	{ 0xba           },	{ 0xf0,0xba      }	},
    {	{ 0xbb           },	{ 0xf0,0xbb      }	},
    {	{ 0xbc           },	{ 0xf0,0xbc      }	},	/* 0bc */
    {	{ 0xbd           },	{ 0xf0,0xbd      }	},
    {	{ 0xbe           },	{ 0xf0,0xbe      }	},
    {	{ 0xbf           },	{ 0xf0,0xbf      }	},
    {	{ 0xc0           },	{ 0xf0,0xc0      }	},	/* 0c0 */
    {	{ 0xc1           },	{ 0xf0,0xc1      }	},
    {	{ 0xc2           },	{ 0xf0,0xc2      }	},
    {	{ 0xc3           },	{ 0xf0,0xc3      }	},
    {	{ 0xc4           },	{ 0xf0,0xc4      }	},	/* 0c4 */
    {	{ 0xc5           },	{ 0xf0,0xc5      }	},
    {	{ 0xc6           },	{ 0xf0,0xc6      }	},
    {	{ 0xc7           },	{ 0xf0,0xc7      }	},
    {	{ 0xc8           },	{ 0xf0,0xc8      }	},	/* 0c8 */
    {	{ 0xc9           },	{ 0xf0,0xc9      }	},
    {	{ 0xca           },	{ 0xf0,0xca      }	},
    {	{ 0xcb           },	{ 0xf0,0xcb      }	},
    {	{ 0xcc           },	{ 0xf0,0xcc      }	},	/* 0cc */
    {	{ 0xcd           },	{ 0xf0,0xcd      }	},
    {	{ 0xce           },	{ 0xf0,0xce      }	},
    {	{ 0xcf           },	{ 0xf0,0xcf      }	},
    {	{ 0xd0           },	{ 0xf0,0xd0      }	},	/* 0d0 */
    {	{ 0xd1           },	{ 0xf0,0xd0      }	},
    {	{ 0xd2           },	{ 0xf0,0xd2      }	},
    {	{ 0xd3           },	{ 0xf0,0xd3      }	},
    {	{ 0xd4           },	{ 0xf0,0xd4      }	},	/* 0d4 */
    {	{ 0xd5           },	{ 0xf0,0xd5      }	},
    {	{ 0xd6           },	{ 0xf0,0xd6      }	},
    {	{ 0xd7           },	{ 0xf0,0xd7      }	},
    {	{ 0xd8           },	{ 0xf0,0xd8      }	},	/* 0d8 */
    {	{ 0xd9           },	{ 0xf0,0xd9      }	},
    {	{ 0xda           },	{ 0xf0,0xda      }	},
    {	{ 0xdb           },	{ 0xf0,0xdb      }	},
    {	{ 0xdc           },	{ 0xf0,0xdc      }	},	/* 0dc */
    {	{ 0xdd           },	{ 0xf0,0xdd      }	},
    {	{ 0xde           },	{ 0xf0,0xde      }	},
    {	{ 0xdf           },	{ 0xf0,0xdf      }	},
    {	{ 0xe0           },	{ 0xf0,0xe0      }	},	/* 0e0 */
    {	{ 0xe1           },	{ 0xf0,0xe1      }	},
    {	{ 0xe2           },	{ 0xf0,0xe2      }	},
    {	{ 0xe3           },	{ 0xf0,0xe3      }	},
    {	{ 0xe4           },	{ 0xf0,0xe4      }	},	/* 0e4 */
    {	{ 0xe5           },	{ 0xf0,0xe5      }	},
    {	{ 0xe6           },	{ 0xf0,0xe6      }	},
    {	{ 0xe7           },	{ 0xf0,0xe7      }	},
    {	{ 0xe8           },	{ 0xf0,0xe8      }	},	/* 0e8 */
    {	{ 0xe9           },	{ 0xf0,0xe9      }	},
    {	{ 0xea           },	{ 0xf0,0xea      }	},
    {	{ 0xeb           },	{ 0xf0,0xeb      }	},
    {	{ 0xec           },	{ 0xf0,0xec      }	},	/* 0ec */
    {	{ 0xed           },	{ 0xf0,0xed      }	},
    {	{ 0xee           },	{ 0xf0,0xee      }	},
    {	{ 0xef           },	{ 0xf0,0xef      }	},
    {	{ 0              },	{ 0              }	},	/* 0f0 */
    {	{ 0xf1           },	{ 0xf0,0xf1      }	},
    {	{ 0xf2           },	{ 0xf0,0xf2      }	},
    {	{ 0xf3           },	{ 0xf0,0xf3      }	},
    {	{ 0xf4           },	{ 0xf0,0xf4      }	},	/* 0f4 */
    {	{ 0xf5           },	{ 0xf0,0xf5      }	},
    {	{ 0xf6           },	{ 0xf0,0xf6      }	},
    {	{ 0xf7           },	{ 0xf0,0xf7      }	},
    {	{ 0xf8           },	{ 0xf0,0xf8      }	},	/* 0f8 */
    {	{ 0xf9           },	{ 0xf0,0xf9      }	},
    {	{ 0xfa           },	{ 0xf0,0xfa      }	},
    {	{ 0xfb           },	{ 0xf0,0xfb      }	},
    {	{ 0xfc           },	{ 0xf0,0xfc      }	},	/* 0fc */
    {	{ 0xfd           },	{ 0xf0,0xfd      }	},
    {	{ 0xfe           },	{ 0xf0,0xfe      }	},
    {	{ 0xff           },	{ 0xf0,0xff      }	},

    {	{ 0xe1,0x14      },	{ 0xe1,0xf0,0x14 }	},	/* 100 */
    {	{ 0xe0,0x76      },	{ 0xe0,0xf0,0x76 }	},
    {	{ 0xe0,0x16      },	{ 0xe0,0xf0,0x16 }	},
    {	{ 0xe0,0x1e      },	{ 0xe0,0xf0,0x1e }	},
    {	{ 0xe0,0x26      },	{ 0xe0,0xf0,0x26 }	},	/* 104 */
    {	{ 0xe0,0x25      },	{ 0xe0,0xf0,0x25 }	},
    {	{ 0xe0,0x2e      },	{ 0xe0,0xf0,0x2e }	},
    {	{ 0xe0,0x36      },	{ 0xe0,0xf0,0x36 }	},
    {	{ 0xe0,0x3d      },	{ 0xe0,0xf0,0x3d }	},	/* 108 */
    {	{ 0xe0,0x3e      },	{ 0xe0,0xf0,0x3e }	},
    {	{ 0xe0,0x46      },	{ 0xe0,0xf0,0x46 }	},
    {	{ 0xe0,0x45      },	{ 0xe0,0xf0,0x45 }	},
    {	{ 0xe0,0x4e      },	{ 0xe0,0xf0,0x4e }	},	/* 10c */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x66      },	{ 0xe0,0xf0,0x66 }	},
    {	{ 0xe0,0x0d      },	{ 0xe0,0xf0,0x0d }	},
    {	{ 0xe0,0x15      },	{ 0xe0,0xf0,0x15 }	},	/* 110 */
    {	{ 0xe0,0x1d      },	{ 0xe0,0xf0,0x1d }	},
    {	{ 0xe0,0x24      },	{ 0xe0,0xf0,0x24 }	},
    {	{ 0xe0,0x2d      },	{ 0xe0,0xf0,0x2d }	},
    {	{ 0xe0,0x2c      },	{ 0xe0,0xf0,0x2c }	},	/* 114 */
    {	{ 0xe0,0x35      },	{ 0xe0,0xf0,0x35 }	},
    {	{ 0xe0,0x3c      },	{ 0xe0,0xf0,0x3c }	},
    {	{ 0xe0,0x43      },	{ 0xe0,0xf0,0x43 }	},
    {	{ 0xe0,0x44      },	{ 0xe0,0xf0,0x44 }	},	/* 118 */
    {	{ 0xe0,0x4d      },	{ 0xe0,0xf0,0x4d }	},
    {	{ 0xe0,0x54      },	{ 0xe0,0xf0,0x54 }	},
    {	{ 0xe0,0x5b      },	{ 0xe0,0xf0,0x5b }	},
    {	{ 0xe0,0x5a      },	{ 0xe0,0xf0,0x5a }	},	/* 11c */
    {	{ 0xe0,0x14      },	{ 0xe0,0xf0,0x14 }	},
    {	{ 0xe0,0x1c      },	{ 0xe0,0xf0,0x1c }	},
    {	{ 0xe0,0x1b      },	{ 0xe0,0xf0,0x1b }	},
    {	{ 0xe0,0x23      },	{ 0xe0,0xf0,0x23 }	},	/* 120 */
    {	{ 0xe0,0x2b      },	{ 0xe0,0xf0,0x2b }	},
    {	{ 0xe0,0x34      },	{ 0xe0,0xf0,0x34 }	},
    {	{ 0xe0,0x33      },	{ 0xe0,0xf0,0x33 }	},
    {	{ 0xe0,0x3b      },	{ 0xe0,0xf0,0x3b }	},	/* 124 */
    {	{ 0xe0,0x42      },	{ 0xe0,0xf0,0x42 }	},
    {	{ 0xe0,0x4b      },	{ 0xe0,0xf0,0x4b }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 128 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x1a      },	{ 0xe0,0xf0,0x1a }	},	/* 12c */
    {	{ 0xe0,0x22      },	{ 0xe0,0xf0,0x22 }	},
    {	{ 0xe0,0x21      },	{ 0xe0,0xf0,0x21 }	},
    {	{ 0xe0,0x2a      },	{ 0xe0,0xf0,0x2a }	},
    {	{ 0xe0,0x32      },	{ 0xe0,0xf0,0x32 }	},	/* 130 */
    {	{ 0xe0,0x31      },	{ 0xe0,0xf0,0x31 }	},
    {	{ 0xe0,0x3a      },	{ 0xe0,0xf0,0x3a }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x49      },	{ 0xe0,0xf0,0x49 }	},	/* 134 */
    {	{ 0xe0,0x4a      },	{ 0xe0,0xf0,0x4a }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x7c      },	{ 0xe0,0xf0,0x7c }	},
    {	{ 0xe0,0x11      },	{ 0xe0,0xf0,0x11 }	},	/* 138 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x58      },	{ 0xe0,0xf0,0x58 }	},
    {	{ 0xe0,0x05      },	{ 0xe0,0xf0,0x05 }	},
    {	{ 0xe0,0x06      },	{ 0xe0,0xf0,0x06 }	},	/* 13c */
    {	{ 0xe0,0x04      },	{ 0xe0,0xf0,0x04 }	},
    {	{ 0xe0,0x0c      },	{ 0xe0,0xf0,0x0c }	},
    {	{ 0xe0,0x03      },	{ 0xe0,0xf0,0x03 }	},
    {	{ 0xe0,0x0b      },	{ 0xe0,0xf0,0x0b }	},	/* 140 */
    {	{ 0xe0,0x02      },	{ 0xe0,0xf0,0x02 }	},
    {	{ 0xe0,0x0a      },	{ 0xe0,0xf0,0x0a }	},
    {	{ 0xe0,0x01      },	{ 0xe0,0xf0,0x01 }	},
    {	{ 0xe0,0x09      },	{ 0xe0,0xf0,0x09 }	},	/* 144 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x7e      },	{ 0xe0,0xf0,0x7e }	},
    {	{ 0xe0,0x6c      },	{ 0xe0,0xf0,0x6c }	},
    {	{ 0xe0,0x75      },	{ 0xe0,0xf0,0x75 }	},	/* 148 */
    {	{ 0xe0,0x7d      },	{ 0xe0,0xf0,0x7d }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x6b      },	{ 0xe0,0xf0,0x6b }	},
    {	{ 0xe0,0x73      },	{ 0xe0,0xf0,0x73 }	},	/* 14c */
    {	{ 0xe0,0x74      },	{ 0xe0,0xf0,0x74 }	},
    {	{ 0xe0,0x79      },	{ 0xe0,0xf0,0x79 }	},
    {	{ 0xe0,0x69      },	{ 0xe0,0xf0,0x69 }	},
    {	{ 0xe0,0x72      },	{ 0xe0,0xf0,0x72 }	},	/* 150 */
    {	{ 0xe0,0x7a      },	{ 0xe0,0xf0,0x7a }	},
    {	{ 0xe0,0x70      },	{ 0xe0,0xf0,0x70 }	},
    {	{ 0xe0,0x71      },	{ 0xe0,0xf0,0x71 }	},
    {	{ 0              },	{ 0              }	},	/* 154 */
    {	{ 0xe0,0x60      },	{ 0xe0,0xf0,0x60 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x78      },	{ 0xe0,0xf0,0x78 }	},
    {	{ 0xe0,0x07      },	{ 0xe0,0xf0,0x07 }	},	/* 158 */
    {	{ 0xe0,0x0f      },	{ 0xe0,0xf0,0x0f }	},
    {	{ 0xe0,0x17      },	{ 0xe0,0xf0,0x17 }	},
    {	{ 0xe0,0x1f      },	{ 0xe0,0xf0,0x1f }	},
    {	{ 0xe0,0x27      },	{ 0xe0,0xf0,0x27 }	},	/* 15c */
    {	{ 0xe0,0x2f      },	{ 0xe0,0xf0,0x2f }	},
    {	{ 0xe0,0x37      },	{ 0xe0,0xf0,0x37 }	},
    {	{ 0xe0,0x3f      },	{ 0xe0,0xf0,0x3f }	},
    {	{ 0              },	{ 0              }	},	/* 160 */
    {	{ 0xe0,0x4f      },	{ 0xe0,0xf0,0x4f }	},
    {	{ 0xe0,0x56      },	{ 0xe0,0xf0,0x56 }	},
    {	{ 0xe0,0x5e      },	{ 0xe0,0xf0,0x5e }	},
    {	{ 0xe0,0x08      },	{ 0xe0,0xf0,0x08 }	},	/* 164 */
    {	{ 0xe0,0x10      },	{ 0xe0,0xf0,0x10 }	},
    {	{ 0xe0,0x18      },	{ 0xe0,0xf0,0x18 }	},
    {	{ 0xe0,0x20      },	{ 0xe0,0xf0,0x20 }	},
    {	{ 0xe0,0x28      },	{ 0xe0,0xf0,0x28 }	},	/* 168 */
    {	{ 0xe0,0x30      },	{ 0xe0,0xf0,0x30 }	},
    {	{ 0xe0,0x38      },	{ 0xe0,0xf0,0x38 }	},
    {	{ 0xe0,0x40      },	{ 0xe0,0xf0,0x40 }	},
    {	{ 0xe0,0x48      },	{ 0xe0,0xf0,0x48 }	},	/* 16c */
    {	{ 0xe0,0x50      },	{ 0xe0,0xf0,0x50 }	},
    {	{ 0xe0,0x57      },	{ 0xe0,0xf0,0x57 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x13      },	{ 0xe0,0xf0,0x13 }	},	/* 170 */
    {	{ 0xe0,0x19      },	{ 0xe0,0xf0,0x19 }	},
    {	{ 0xe0,0x39      },	{ 0xe0,0xf0,0x39 }	},
    {	{ 0xe0,0x51      },	{ 0xe0,0xf0,0x51 }	},
    {	{ 0xe0,0x53      },	{ 0xe0,0xf0,0x53 }	},	/* 174 */
    {	{ 0xe0,0x5c      },	{ 0xe0,0xf0,0x5c }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x62      },	{ 0xe0,0xf0,0x62 }	},
    {	{ 0xe0,0x63      },	{ 0xe0,0xf0,0x63 }	},	/* 178 */
    {	{ 0xe0,0x64      },	{ 0xe0,0xf0,0x64 }	},
    {	{ 0xe0,0x65      },	{ 0xe0,0xf0,0x65 }	},
    {	{ 0xe0,0x67      },	{ 0xe0,0xf0,0x67 }	},
    {	{ 0xe0,0x68      },	{ 0xe0,0xf0,0x68 }	},	/* 17c */
    {	{ 0xe0,0x6a      },	{ 0xe0,0xf0,0x6a }	},
    {	{ 0xe0,0x6d      },	{ 0xe0,0xf0,0x6d }	},
    {	{ 0xe0,0x6e      },	{ 0xe0,0xf0,0x6e }	},
    {	{ 0              },	{ 0              }	},	/* 180 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 184 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 188 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 18c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 190 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 194 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 198 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 19c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ac */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1bc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1cc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d0 */
    {	{ 0xe0,0xe1      },	{ 0xe0,0xf0,0xe1 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1dc */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xee      },	{ 0xe0,0xf0,0xee }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e0 */
    {	{ 0xe0,0xf1      },	{ 0xe0,0xf0,0xf1 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ec */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xfe      },	{ 0xe0,0xf0,0xfe }	},
    {	{ 0xe0,0xff      },	{ 0xe0,0xf0,0xff }	}
};

static const scancode_t scancode_set3[512] = {
    {	{ 0              },	{ 0              }	},	/* 000 */
    {	{ 0x08           },	{ 0xf0,0x08      }	},
    {	{ 0x16           },	{ 0xf0,0x16      }	},
    {	{ 0x1e           },	{ 0xf0,0x1e      }	},
    {	{ 0x26           },	{ 0xf0,0x26      }	},	/* 004 */
    {	{ 0x25           },	{ 0xf0,0x25      }	},
    {	{ 0x2e           },	{ 0xf0,0x2e      }	},
    {	{ 0x36           },	{ 0xf0,0x36      }	},
    {	{ 0x3d           },	{ 0xf0,0x3d      }	},	/* 008 */
    {	{ 0x3e           },	{ 0xf0,0x3e      }	},
    {	{ 0x46           },	{ 0xf0,0x46      }	},
    {	{ 0x45           },	{ 0xf0,0x45      }	},
    {	{ 0x4e           },	{ 0xf0,0x4e      }	},	/* 00c */
    {	{ 0x55           },	{ 0xf0,0x55      }	},
    {	{ 0x66           },	{ 0xf0,0x66      }	},
    {	{ 0x0d           },	{ 0xf0,0x0d      }	},
    {	{ 0x15           },	{ 0xf0,0x15      }	},	/* 010 */
    {	{ 0x1d           },	{ 0xf0,0x1d      }	},
    {	{ 0x24           },	{ 0xf0,0x24      }	},
    {	{ 0x2d           },	{ 0xf0,0x2d      }	},
    {	{ 0x2c           },	{ 0xf0,0x2c      }	},	/* 014 */
    {	{ 0x35           },	{ 0xf0,0x35      }	},
    {	{ 0x3c           },	{ 0xf0,0x3c      }	},
    {	{ 0x43           },	{ 0xf0,0x43      }	},
    {	{ 0x44           },	{ 0xf0,0x44      }	},	/* 018 */
    {	{ 0x4d           },	{ 0xf0,0x4d      }	},
    {	{ 0x54           },	{ 0xf0,0x54      }	},
    {	{ 0x5b           },	{ 0xf0,0x5b      }	},
    {	{ 0x5a           },	{ 0xf0,0x5a      }	},	/* 01c */
    {	{ 0x11           },	{ 0xf0,0x11      }	},
    {	{ 0x1c           },	{ 0xf0,0x1c      }	},
    {	{ 0x1b           },	{ 0xf0,0x1b      }	},
    {	{ 0x23           },	{ 0xf0,0x23      }	},	/* 020 */
    {	{ 0x2b           },	{ 0xf0,0x2b      }	},
    {	{ 0x34           },	{ 0xf0,0x34      }	},
    {	{ 0x33           },	{ 0xf0,0x33      }	},
    {	{ 0x3b           },	{ 0xf0,0x3b      }	},	/* 024 */
    {	{ 0x42           },	{ 0xf0,0x42      }	},
    {	{ 0x4b           },	{ 0xf0,0x4b      }	},
    {	{ 0x4c           },	{ 0xf0,0x4c      }	},
    {	{ 0x52           },	{ 0xf0,0x52      }	},	/* 028 */
    {	{ 0x0e           },	{ 0xf0,0x0e      }	},
    {	{ 0x12           },	{ 0xf0,0x12      }	},
    {	{ 0x5c           },	{ 0xf0,0x5c      }	},
    {	{ 0x1a           },	{ 0xf0,0x1a      }	},	/* 02c */
    {	{ 0x22           },	{ 0xf0,0x22      }	},
    {	{ 0x21           },	{ 0xf0,0x21      }	},
    {	{ 0x2a           },	{ 0xf0,0x2a      }	},
    {	{ 0x32           },	{ 0xf0,0x32      }	},	/* 030 */
    {	{ 0x31           },	{ 0xf0,0x31      }	},
    {	{ 0x3a           },	{ 0xf0,0x3a      }	},
    {	{ 0x41           },	{ 0xf0,0x41      }	},
    {	{ 0x49           },	{ 0xf0,0x49      }	},	/* 034 */
    {	{ 0x4a           },	{ 0xf0,0x4a      }	},
    {	{ 0x59           },	{ 0xf0,0x59      }	},
    {	{ 0x7e           },	{ 0xf0,0x7e      }	},
    {	{ 0x19           },	{ 0xf0,0x19      }	},	/* 038 */
    {	{ 0x29           },	{ 0xf0,0x29      }	},
    {	{ 0x14           },	{ 0xf0,0x14      }	},
    {	{ 0x07           },	{ 0xf0,0x07      }	},
    {	{ 0x0f           },	{ 0xf0,0x0f      }	},	/* 03c */
    {	{ 0x17           },	{ 0xf0,0x17      }	},
    {	{ 0x1f           },	{ 0xf0,0x1f      }	},
    {	{ 0x27           },	{ 0xf0,0x27      }	},
    {	{ 0x2f           },	{ 0xf0,0x2f      }	},	/* 040 */
    {	{ 0x37           },	{ 0xf0,0x37      }	},
    {	{ 0x3f           },	{ 0xf0,0x3f      }	},
    {	{ 0x47           },	{ 0xf0,0x47      }	},
    {	{ 0x4f           },	{ 0xf0,0x4f      }	},	/* 044 */
    {	{ 0x76           },	{ 0xf0,0x76      }	},
    {	{ 0x5f           },	{ 0xf0,0x5f      }	},
    {	{ 0x6c           },	{ 0xf0,0x6c      }	},
    {	{ 0x75           },	{ 0xf0,0x75      }	},	/* 048 */
    {	{ 0x7d           },	{ 0xf0,0x7d      }	},
    {	{ 0x84           },	{ 0xf0,0x84      }	},
    {	{ 0x6b           },	{ 0xf0,0x6b      }	},
    {	{ 0x73           },	{ 0xf0,0x73      }	},	/* 04c */
    {	{ 0x74           },	{ 0xf0,0x74      }	},
    {	{ 0x7c           },	{ 0xf0,0x7c      }	},
    {	{ 0x69           },	{ 0xf0,0x69      }	},
    {	{ 0x72           },	{ 0xf0,0x72      }	},	/* 050 */
    {	{ 0x7a           },	{ 0xf0,0x7a      }	},
    {	{ 0x70           },	{ 0xf0,0x70      }	},
    {	{ 0x71           },	{ 0xf0,0x71      }	},
    {	{ 0x57           },	{ 0xf0,0x57      }	},	/* 054 */
    {	{ 0x60           },	{ 0xf0,0x60      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x56           },	{ 0xf0,0x56      }	},
    {	{ 0x5e           },	{ 0xf0,0x5e      }	},	/* 058 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 05c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 060 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 064 */
    {	{ 0x10           },	{ 0xf0,0x10      }	},
    {	{ 0x18           },	{ 0xf0,0x18      }	},
    {	{ 0x20           },	{ 0xf0,0x20      }	},
    {	{ 0x28           },	{ 0xf0,0x28      }	},	/* 068 */
    {	{ 0x30           },	{ 0xf0,0x30      }	},
    {	{ 0x38           },	{ 0xf0,0x38      }	},
    {	{ 0x40           },	{ 0xf0,0x40      }	},
    {	{ 0x48           },	{ 0xf0,0x48      }	},	/* 06c */
    {	{ 0x50           },	{ 0xf0,0x50      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x87           },	{ 0xf0,0x87      }	},	/* 070 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x51           },	{ 0xf0,0x51      }	},
    {	{ 0x53           },	{ 0xf0,0x53      }	},	/* 074 */
    {	{ 0x5c           },	{ 0xf0,0x5c      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x62           },	{ 0xf0,0x62      }	},
    {	{ 0x63           },	{ 0xf0,0x63      }	},	/* 078 */
    {	{ 0x86           },	{ 0xf0,0x86      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x85           },	{ 0xf0,0x85      }	},
    {	{ 0x68           },	{ 0xf0,0x68      }	},	/* 07c */
    {	{ 0x13           },	{ 0xf0,0x13      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x80           },	{ 0xf0,0x80      }	},	/* 080 */
    {	{ 0x81           },	{ 0xf0,0x81      }	},
    {	{ 0x82           },	{ 0xf0,0x82      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 084 */
    {	{ 0x85           },	{ 0xf0,0x54      }	},
    {	{ 0x86           },	{ 0xf0,0x86      }	},
    {	{ 0x87           },	{ 0xf0,0x87      }	},
    {	{ 0x88           },	{ 0xf0,0x88      }	},	/* 088 */
    {	{ 0x89           },	{ 0xf0,0x89      }	},
    {	{ 0x8a           },	{ 0xf0,0x8a      }	},
    {	{ 0x8b           },	{ 0xf0,0x8b      }	},
    {	{ 0              },	{ 0              }	},	/* 08c */
    {	{ 0              },	{ 0              }	},
    {	{ 0x8e           },	{ 0xf0,0x8e      }	},
    {	{ 0x8f           },	{ 0xf0,0x8f      }	},
    {	{ 0x90           },	{ 0xf0,0x90      }	},	/* 090 */
    {	{ 0x91           },	{ 0xf0,0x91      }	},
    {	{ 0x92           },	{ 0xf0,0x92      }	},
    {	{ 0x93           },	{ 0xf0,0x93      }	},
    {	{ 0x94           },	{ 0xf0,0x94      }	},	/* 094 */
    {	{ 0x95           },	{ 0xf0,0x95      }	},
    {	{ 0x96           },	{ 0xf0,0x96      }	},
    {	{ 0x97           },	{ 0xf0,0x97      }	},
    {	{ 0x98           },	{ 0xf0,0x98      }	},	/* 098 */
    {	{ 0x99           },	{ 0xf0,0x99      }	},
    {	{ 0x9a           },	{ 0xf0,0x9a      }	},
    {	{ 0x9b           },	{ 0xf0,0x9b      }	},
    {	{ 0x9c           },	{ 0xf0,0x9c      }	},	/* 09c */
    {	{ 0x9d           },	{ 0xf0,0x9d      }	},
    {	{ 0x9e           },	{ 0xf0,0x9e      }	},
    {	{ 0x9f           },	{ 0xf0,0x9f      }	},
    {	{ 0xa0           },	{ 0xf0,0xa0      }	},	/* 0a0 */
    {	{ 0xa1           },	{ 0xf0,0xa1      }	},
    {	{ 0xa2           },	{ 0xf0,0xa2      }	},
    {	{ 0xa3           },	{ 0xf0,0xa3      }	},
    {	{ 0xa4           },	{ 0xf0,0xa4      }	},	/* 0a4 */
    {	{ 0xa5           },	{ 0xf0,0xa5      }	},
    {	{ 0xa6           },	{ 0xf0,0xa6      }	},
    {	{ 0xa7           },	{ 0xf0,0xa7      }	},
    {	{ 0xa8           },	{ 0xf0,0xa8      }	},	/* 0a8 */
    {	{ 0xa9           },	{ 0xf0,0xa9      }	},
    {	{ 0xaa           },	{ 0xf0,0xaa      }	},
    {	{ 0xab           },	{ 0xf0,0xab      }	},
    {	{ 0xac           },	{ 0xf0,0xac      }	},	/* 0ac */
    {	{ 0xad           },	{ 0xf0,0xad      }	},
    {	{ 0xae           },	{ 0xf0,0xae      }	},
    {	{ 0xaf           },	{ 0xf0,0xaf      }	},
    {	{ 0xb0           },	{ 0xf0,0xb0      }	},	/* 0b0 */
    {	{ 0xb1           },	{ 0xf0,0xb1      }	},
    {	{ 0xb2           },	{ 0xf0,0xb2      }	},
    {	{ 0xb3           },	{ 0xf0,0xb3      }	},
    {	{ 0xb4           },	{ 0xf0,0xb4      }	},	/* 0b4 */
    {	{ 0xb5           },	{ 0xf0,0xb5      }	},
    {	{ 0xb6           },	{ 0xf0,0xb6      }	},
    {	{ 0xb7           },	{ 0xf0,0xb7      }	},
    {	{ 0xb8           },	{ 0xf0,0xb8      }	},	/* 0b8 */
    {	{ 0xb9           },	{ 0xf0,0xb9      }	},
    {	{ 0xba           },	{ 0xf0,0xba      }	},
    {	{ 0xbb           },	{ 0xf0,0xbb      }	},
    {	{ 0xbc           },	{ 0xf0,0xbc      }	},	/* 0bc */
    {	{ 0xbd           },	{ 0xf0,0xbd      }	},
    {	{ 0xbe           },	{ 0xf0,0xbe      }	},
    {	{ 0xbf           },	{ 0xf0,0xbf      }	},
    {	{ 0xc0           },	{ 0xf0,0xc0      }	},	/* 0c0 */
    {	{ 0xc1           },	{ 0xf0,0xc1      }	},
    {	{ 0xc2           },	{ 0xf0,0xc2      }	},
    {	{ 0xc3           },	{ 0xf0,0xc3      }	},
    {	{ 0xc4           },	{ 0xf0,0xc4      }	},	/* 0c4 */
    {	{ 0xc5           },	{ 0xf0,0xc5      }	},
    {	{ 0xc6           },	{ 0xf0,0xc6      }	},
    {	{ 0xc7           },	{ 0xf0,0xc7      }	},
    {	{ 0xc8           },	{ 0xf0,0xc8      }	},	/* 0c8 */
    {	{ 0xc9           },	{ 0xf0,0xc9      }	},
    {	{ 0xca           },	{ 0xf0,0xca      }	},
    {	{ 0xcb           },	{ 0xf0,0xcb      }	},
    {	{ 0xcc           },	{ 0xf0,0xcc      }	},	/* 0cc */
    {	{ 0xcd           },	{ 0xf0,0xcd      }	},
    {	{ 0xce           },	{ 0xf0,0xce      }	},
    {	{ 0xcf           },	{ 0xf0,0xcf      }	},
    {	{ 0xd0           },	{ 0xf0,0xd0      }	},	/* 0d0 */
    {	{ 0xd1           },	{ 0xf0,0xd0      }	},
    {	{ 0xd2           },	{ 0xf0,0xd2      }	},
    {	{ 0xd3           },	{ 0xf0,0xd3      }	},
    {	{ 0xd4           },	{ 0xf0,0xd4      }	},	/* 0d4 */
    {	{ 0xd5           },	{ 0xf0,0xd5      }	},
    {	{ 0xd6           },	{ 0xf0,0xd6      }	},
    {	{ 0xd7           },	{ 0xf0,0xd7      }	},
    {	{ 0xd8           },	{ 0xf0,0xd8      }	},	/* 0d8 */
    {	{ 0xd9           },	{ 0xf0,0xd9      }	},
    {	{ 0xda           },	{ 0xf0,0xda      }	},
    {	{ 0xdb           },	{ 0xf0,0xdb      }	},
    {	{ 0xdc           },	{ 0xf0,0xdc      }	},	/* 0dc */
    {	{ 0xdd           },	{ 0xf0,0xdd      }	},
    {	{ 0xde           },	{ 0xf0,0xde      }	},
    {	{ 0xdf           },	{ 0xf0,0xdf      }	},
    {	{ 0xe0           },	{ 0xf0,0xe0      }	},	/* 0e0 */
    {	{ 0xe1           },	{ 0xf0,0xe1      }	},
    {	{ 0xe2           },	{ 0xf0,0xe2      }	},
    {	{ 0xe3           },	{ 0xf0,0xe3      }	},
    {	{ 0xe4           },	{ 0xf0,0xe4      }	},	/* 0e4 */
    {	{ 0xe5           },	{ 0xf0,0xe5      }	},
    {	{ 0xe6           },	{ 0xf0,0xe6      }	},
    {	{ 0xe7           },	{ 0xf0,0xe7      }	},
    {	{ 0xe8           },	{ 0xf0,0xe8      }	},	/* 0e8 */
    {	{ 0xe9           },	{ 0xf0,0xe9      }	},
    {	{ 0xea           },	{ 0xf0,0xea      }	},
    {	{ 0xeb           },	{ 0xf0,0xeb      }	},
    {	{ 0xec           },	{ 0xf0,0xec      }	},	/* 0ec */
    {	{ 0xed           },	{ 0xf0,0xed      }	},
    {	{ 0xee           },	{ 0xf0,0xee      }	},
    {	{ 0xef           },	{ 0xf0,0xef      }	},
    {	{ 0              },	{ 0              }	},	/* 0f0 */
    {	{ 0xf1           },	{ 0xf0,0xf1      }	},
    {	{ 0xf2           },	{ 0xf0,0xf2      }	},
    {	{ 0xf3           },	{ 0xf0,0xf3      }	},
    {	{ 0xf4           },	{ 0xf0,0xf4      }	},	/* 0f4 */
    {	{ 0xf5           },	{ 0xf0,0xf5      }	},
    {	{ 0xf6           },	{ 0xf0,0xf6      }	},
    {	{ 0xf7           },	{ 0xf0,0xf7      }	},
    {	{ 0xf8           },	{ 0xf0,0xf8      }	},	/* 0f8 */
    {	{ 0xf9           },	{ 0xf0,0xf9      }	},
    {	{ 0xfa           },	{ 0xf0,0xfa      }	},
    {	{ 0xfb           },	{ 0xf0,0xfb      }	},
    {	{ 0xfc           },	{ 0xf0,0xfc      }	},	/* 0fc */
    {	{ 0xfd           },	{ 0xf0,0xfd      }	},
    {	{ 0xfe           },	{ 0xf0,0xfe      }	},
    {	{ 0xff           },	{ 0xf0,0xff      }	},

    {	{ 0x62           },	{ 0xf0,0x62      }	},	/* 100 */
    {	{ 0xe0,0x76      },	{ 0xe0,0xf0,0x76 }	},
    {	{ 0xe0,0x16      },	{ 0xe0,0xf0,0x16 }	},
    {	{ 0xe0,0x1e      },	{ 0xe0,0xf0,0x1e }	},
    {	{ 0xe0,0x26      },	{ 0xe0,0xf0,0x26 }	},	/* 104 */
    {	{ 0xe0,0x25      },	{ 0xe0,0xf0,0x25 }	},
    {	{ 0xe0,0x2e      },	{ 0xe0,0xf0,0x2e }	},
    {	{ 0xe0,0x36      },	{ 0xe0,0xf0,0x36 }	},
    {	{ 0xe0,0x3d      },	{ 0xe0,0xf0,0x3d }	},	/* 108 */
    {	{ 0xe0,0x3e      },	{ 0xe0,0xf0,0x3e }	},
    {	{ 0xe0,0x46      },	{ 0xe0,0xf0,0x46 }	},
    {	{ 0xe0,0x45      },	{ 0xe0,0xf0,0x45 }	},
    {	{ 0xe0,0x4e      },	{ 0xe0,0xf0,0x4e }	},	/* 10c */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x66      },	{ 0xe0,0xf0,0x66 }	},
    {	{ 0xe0,0x0d      },	{ 0xe0,0xf0,0x0d }	},
    {	{ 0xe0,0x15      },	{ 0xe0,0xf0,0x15 }	},	/* 110 */
    {	{ 0xe0,0x1d      },	{ 0xe0,0xf0,0x1d }	},
    {	{ 0xe0,0x24      },	{ 0xe0,0xf0,0x24 }	},
    {	{ 0xe0,0x2d      },	{ 0xe0,0xf0,0x2d }	},
    {	{ 0xe0,0x2c      },	{ 0xe0,0xf0,0x2c }	},	/* 114 */
    {	{ 0xe0,0x35      },	{ 0xe0,0xf0,0x35 }	},
    {	{ 0xe0,0x3c      },	{ 0xe0,0xf0,0x3c }	},
    {	{ 0xe0,0x43      },	{ 0xe0,0xf0,0x43 }	},
    {	{ 0xe0,0x44      },	{ 0xe0,0xf0,0x44 }	},	/* 118 */
    {	{ 0xe0,0x4d      },	{ 0xe0,0xf0,0x4d }	},
    {	{ 0xe0,0x54      },	{ 0xe0,0xf0,0x54 }	},
    {	{ 0xe0,0x5b      },	{ 0xe0,0xf0,0x5b }	},
    {	{ 0x79           },	{ 0xf0,0x79      }	},	/* 11c */
    {	{ 0x58           },	{ 0xf0,0x58      }	},
    {	{ 0xe0,0x1c      },	{ 0xe0,0xf0,0x1c }	},
    {	{ 0xe0,0x1b      },	{ 0xe0,0xf0,0x1b }	},
    {	{ 0xe0,0x23      },	{ 0xe0,0xf0,0x23 }	},	/* 120 */
    {	{ 0xe0,0x2b      },	{ 0xe0,0xf0,0x2b }	},
    {	{ 0xe0,0x34      },	{ 0xe0,0xf0,0x34 }	},
    {	{ 0xe0,0x33      },	{ 0xe0,0xf0,0x33 }	},
    {	{ 0xe0,0x3b      },	{ 0xe0,0xf0,0x3b }	},	/* 124 */
    {	{ 0xe0,0x42      },	{ 0xe0,0xf0,0x42 }	},
    {	{ 0xe0,0x4b      },	{ 0xe0,0xf0,0x4b }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 128 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x1a      },	{ 0xe0,0xf0,0x1a }	},	/* 12c */
    {	{ 0xe0,0x22      },	{ 0xe0,0xf0,0x22 }	},
    {	{ 0xe0,0x21      },	{ 0xe0,0xf0,0x21 }	},
    {	{ 0xe0,0x2a      },	{ 0xe0,0xf0,0x2a }	},
    {	{ 0xe0,0x32      },	{ 0xe0,0xf0,0x32 }	},	/* 130 */
    {	{ 0xe0,0x31      },	{ 0xe0,0xf0,0x31 }	},
    {	{ 0xe0,0x3a      },	{ 0xe0,0xf0,0x3a }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x49      },	{ 0xe0,0xf0,0x49 }	},	/* 134 */
    {	{ 0x77           },	{ 0xf0,0x77      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x57           },	{ 0xf0,0x57      }	},
    {	{ 0x39           },	{ 0xf0,0x39      }	},	/* 138 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x58      },	{ 0xe0,0xf0,0x58 }	},
    {	{ 0xe0,0x05      },	{ 0xe0,0xf0,0x05 }	},
    {	{ 0xe0,0x06      },	{ 0xe0,0xf0,0x06 }	},	/* 13c */
    {	{ 0xe0,0x04      },	{ 0xe0,0xf0,0x04 }	},
    {	{ 0xe0,0x0c      },	{ 0xe0,0xf0,0x0c }	},
    {	{ 0xe0,0x03      },	{ 0xe0,0xf0,0x03 }	},
    {	{ 0xe0,0x0b      },	{ 0xe0,0xf0,0x0b }	},	/* 140 */
    {	{ 0xe0,0x02      },	{ 0xe0,0xf0,0x02 }	},
    {	{ 0xe0,0x0a      },	{ 0xe0,0xf0,0x0a }	},
    {	{ 0xe0,0x01      },	{ 0xe0,0xf0,0x01 }	},
    {	{ 0xe0,0x09      },	{ 0xe0,0xf0,0x09 }	},	/* 144 */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x7e      },	{ 0xe0,0xf0,0x7e }	},
    {	{ 0x6e           },	{ 0xf0,0x6e      }	},
    {	{ 0x63           },	{ 0xf0,0x63      }	},	/* 148 */
    {	{ 0x6f           },	{ 0xf0,0x6f      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x61           },	{ 0xf0,0x61      }	},
    {	{ 0xe0,0x73      },	{ 0xe0,0xf0,0x73 }	},	/* 14c */
    {	{ 0x6a           },	{ 0xf0,0x6a      }	},
    {	{ 0xe0,0x79      },	{ 0xe0,0xf0,0x79 }	},
    {	{ 0x65           },	{ 0xf0,0x65      }	},
    {	{ 0x60           },	{ 0xf0,0x60      }	},	/* 150 */
    {	{ 0x6d           },	{ 0xf0,0x6d      }	},
    {	{ 0x67           },	{ 0xf0,0x67      }	},
    {	{ 0x64           },	{ 0xf0,0x64      }	},
    {	{ 0xd4           },	{ 0xf0,0xD4      }	},	/* 154 */
    {	{ 0xe0,0x60      },	{ 0xe0,0xf0,0x60 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x78      },	{ 0xe0,0xf0,0x78 }	},
    {	{ 0xe0,0x07      },	{ 0xe0,0xf0,0x07 }	},	/* 158 */
    {	{ 0xe0,0x0f      },	{ 0xe0,0xf0,0x0f }	},
    {	{ 0xe0,0x17      },	{ 0xe0,0xf0,0x17 }	},
    {	{ 0x8b           },	{ 0xf0,0x8b      }	},
    {	{ 0x8c           },	{ 0xf0,0x8c      }	},	/* 15c */
    {	{ 0x8d           },	{ 0xf0,0x8d      }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0x7f           },	{ 0xf0,0x7f      }	},
    {	{ 0              },	{ 0              }	},	/* 160 */
    {	{ 0xe0,0x4f      },	{ 0xe0,0xf0,0x4f }	},
    {	{ 0xe0,0x56      },	{ 0xe0,0xf0,0x56 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x08      },	{ 0xe0,0xf0,0x08 }	},	/* 164 */
    {	{ 0xe0,0x10      },	{ 0xe0,0xf0,0x10 }	},
    {	{ 0xe0,0x18      },	{ 0xe0,0xf0,0x18 }	},
    {	{ 0xe0,0x20      },	{ 0xe0,0xf0,0x20 }	},
    {	{ 0xe0,0x28      },	{ 0xe0,0xf0,0x28 }	},	/* 168 */
    {	{ 0xe0,0x30      },	{ 0xe0,0xf0,0x30 }	},
    {	{ 0xe0,0x38      },	{ 0xe0,0xf0,0x38 }	},
    {	{ 0xe0,0x40      },	{ 0xe0,0xf0,0x40 }	},
    {	{ 0xe0,0x48      },	{ 0xe0,0xf0,0x48 }	},	/* 16c */
    {	{ 0xe0,0x50      },	{ 0xe0,0xf0,0x50 }	},
    {	{ 0xe0,0x57      },	{ 0xe0,0xf0,0x57 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x13      },	{ 0xe0,0xf0,0x13 }	},	/* 170 */
    {	{ 0xe0,0x19      },	{ 0xe0,0xf0,0x19 }	},
    {	{ 0xe0,0x39      },	{ 0xe0,0xf0,0x39 }	},
    {	{ 0xe0,0x51      },	{ 0xe0,0xf0,0x51 }	},
    {	{ 0xe0,0x53      },	{ 0xe0,0xf0,0x53 }	},	/* 174 */
    {	{ 0xe0,0x5c      },	{ 0xe0,0xf0,0x5c }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0x62      },	{ 0xe0,0xf0,0x62 }	},
    {	{ 0xe0,0x63      },	{ 0xe0,0xf0,0x63 }	},	/* 178 */
    {	{ 0xe0,0x64      },	{ 0xe0,0xf0,0x64 }	},
    {	{ 0xe0,0x65      },	{ 0xe0,0xf0,0x65 }	},
    {	{ 0xe0,0x67      },	{ 0xe0,0xf0,0x67 }	},
    {	{ 0xe0,0x68      },	{ 0xe0,0xf0,0x68 }	},	/* 17c */
    {	{ 0xe0,0x6a      },	{ 0xe0,0xf0,0x6a }	},
    {	{ 0xe0,0x6d      },	{ 0xe0,0xf0,0x6d }	},
    {	{ 0xe0,0x6e      },	{ 0xe0,0xf0,0x6e }	},
    {	{ 0              },	{ 0              }	},	/* 180 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 184 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 188 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 18c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 190 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 194 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 198 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 19c */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1a8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ac */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1b8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1bc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c0 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1c8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1cc */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d0 */
    {	{ 0xe0,0xe1      },	{ 0xe0,0xf0,0xe1 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1d8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1dc */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xee      },	{ 0xe0,0xf0,0xee }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e0 */
    {	{ 0xe0,0xf1      },	{ 0xe0,0xf0,0xf1 }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e4 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1e8 */
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},
    {	{ 0              },	{ 0              }	},	/* 1ec */
    {	{ 0              },	{ 0              }	},
    {	{ 0xe0,0xfe      },	{ 0xe0,0xf0,0xfe }	},
    {	{ 0xe0,0xff      },	{ 0xe0,0xf0,0xff }	}
};


static void
set_scancode_map(atkbd_t *dev)
{
    switch (keyboard_mode & 3) {
#if USE_SET1
	case 1:
	default:
		keyboard_set_table(scancode_set1);
		break;
#else
	default:
#endif
	case 2:
		keyboard_set_table(scancode_set2);
		break;

	case 3:
		keyboard_set_table(scancode_set3);
		break;
    }

    if (keyboard_mode & 0x20)
#if USE_SET1
	keyboard_set_table(scancode_set1);
#else
	keyboard_set_table(scancode_set2);
#endif
}


static void
kbd_poll(priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;

    keyboard_delay += (1000LL * TIMER_USEC);

    if ((dev->out_new != -1) && !dev->last_irq) {
	dev->wantirq = 0;
	if (dev->out_new & 0x100) {
		DEBUG("ATkbd: want mouse data\n");
		if (dev->mem[0] & 0x02)
			picint(0x1000);
		dev->out = dev->out_new & 0xff;
		dev->out_new = -1;
		dev->status |=  STAT_OFULL;
		dev->status &= ~STAT_IFULL;
		dev->status |=  STAT_MFULL;
		dev->last_irq = 0x1000;
	} else {
		DEBUG("ATkbd: want keyboard data\n");
		if (dev->mem[0] & 0x01)
			picint(2);
		dev->out = dev->out_new & 0xff;
		dev->out_new = -1;
		dev->status |=  STAT_OFULL;
		dev->status &= ~STAT_IFULL;
		dev->status &= ~STAT_MFULL;
		dev->last_irq = 2;
	}
    }

    if (dev->out_new == -1 && !(dev->status & STAT_OFULL) && key_ctrl_queue_start != key_ctrl_queue_end) {
	dev->out_new = key_ctrl_queue[key_ctrl_queue_start] | 0x200;
	key_ctrl_queue_start = (key_ctrl_queue_start + 1) & 0xf;
    } else if (!(dev->status & STAT_OFULL) && dev->out_new == -1 && dev->out_delayed != -1) {
            dev->out_new = dev->out_delayed;
            dev->out_delayed = -1;
    } else if (!(dev->status & STAT_OFULL) && dev->out_new == -1 && !(dev->mem[0] & 0x10) && dev->out_delayed != -1) {
            dev->out_new = dev->out_delayed;
            dev->out_delayed = -1;
    } else if (!(dev->status & STAT_OFULL) && dev->out_new == -1/* && !(dev->mem[0] & 0x20)*/ &&
	    (mouse_queue_start != mouse_queue_end)) {
	dev->out_new = mouse_queue[mouse_queue_start] | 0x100;
	mouse_queue_start = (mouse_queue_start + 1) & 0xf;
    } else if (!(dev->status&STAT_OFULL) && dev->out_new == -1 &&
	       !(dev->mem[0]&0x10) && (key_queue_start != key_queue_end)) {
	dev->out_new = key_queue[key_queue_start];
	key_queue_start = (key_queue_start + 1) & 0xf;
    }
}


static void
add_data(atkbd_t *dev, uint8_t val)
{
    key_ctrl_queue[key_ctrl_queue_end] = val;
    key_ctrl_queue_end = (key_ctrl_queue_end + 1) & 0xf;

    if (! (dev->out_new & 0x300)) {
	dev->out_delayed = dev->out_new;
	dev->out_new = -1;
    }
}


static void
add_data_vals(atkbd_t *dev, uint8_t *val, uint8_t len)
{
    int xt_mode = (keyboard_mode & 0x20) &&
		  (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF);
    int translate = (keyboard_mode & 0x40);
    int i;
    uint8_t or = 0;
    uint8_t send;

    translate = translate || (keyboard_mode & 0x40) || xt_mode;
    translate = translate || (KBC_TYPE(dev) == KBC_TYPE_PS2_2);

    for (i = 0; i < len; i++) {
        if (translate) {
		if (val[i] == 0xf0) {
			or = 0x80;
			continue;
		}
		send = nont_to_t[val[i]] | or;
		if (or == 0x80)
			or = 0;
	} else
		send = val[i];
	DEBUG("%02x", send);
        add_data(dev, send);
	if (i < (len - 1)) DEBUG(" ");
    }

#ifdef _DEBUG
    if (translate) {
	DEBUG(" original: (");
    	for (i = 0; i < len; i++) {
		DEBUG("%02x", val[i]);
		if (i < (len - 1)) DEBUG(" ");
    	}
	DEBUG(")");
    }
    DEBUG("\n");
#endif
}


static void
add_data_kbd(uint16_t val)
{
    atkbd_t *dev = SavedKbd;
    int xt_mode = (keyboard_mode & 0x20) &&
		  (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF);
    int translate = (keyboard_mode & 0x40);
    uint8_t fake_shift[4];
    uint8_t num_lock = 0, shift_states = 0;

    translate = translate || (keyboard_mode & 0x40) || xt_mode;
    translate = translate || (KBC_TYPE(dev) == KBC_TYPE_PS2_2);

    num_lock = !!(keyboard_get_state() & KBD_FLAG_NUM);
    shift_states = keyboard_get_shift() & STATE_SHIFT_MASK;

    /* Allow for scan code translation. */
    if (translate && (val == 0xf0)) {
	DEBUG("ATkbd: translate is on, F0 prefix detected\n");
	sc_or = 0x80;
	return;
    }

    /* Skip break code if translated make code has bit 7 set. */
    if (translate && (sc_or == 0x80) && (val & 0x80)) {
	DEBUG("ATkbd: translate is on, skipping scan code: %02x (original: F0 %02x)\n", nont_to_t[val], val);
	sc_or = 0;
	return;
    }

    /* Test for T3100E 'Fn' key (Right Alt / Right Ctrl) */
    if ((dev != NULL) && (KBC_VENDOR(dev) == KBC_VEN_TOSHIBA) &&
	(keyboard_recv(0xb8) || keyboard_recv(0x9d))) switch (val) {
	case 0x4f: t3100e_notify_set(dev->func_priv, 0x01); break; /* End */
	case 0x50: t3100e_notify_set(dev->func_priv, 0x02); break; /* Down */
	case 0x51: t3100e_notify_set(dev->func_priv, 0x03); break; /* PgDn */
	case 0x52: t3100e_notify_set(dev->func_priv, 0x04); break; /* Ins */
	case 0x53: t3100e_notify_set(dev->func_priv, 0x05); break; /* Del */
	case 0x54: t3100e_notify_set(dev->func_priv, 0x06); break; /* SysRQ */
	case 0x45: t3100e_notify_set(dev->func_priv, 0x07); break; /* NumLock */
	case 0x46: t3100e_notify_set(dev->func_priv, 0x08); break; /* ScrLock */
	case 0x47: t3100e_notify_set(dev->func_priv, 0x09); break; /* Home */
	case 0x48: t3100e_notify_set(dev->func_priv, 0x0a); break; /* Up */
	case 0x49: t3100e_notify_set(dev->func_priv, 0x0b); break; /* PgUp */
	case 0x4A: t3100e_notify_set(dev->func_priv, 0x0c); break; /* Keypad -*/
	case 0x4B: t3100e_notify_set(dev->func_priv, 0x0d); break; /* Left */
	case 0x4C: t3100e_notify_set(dev->func_priv, 0x0e); break; /* KP 5 */
	case 0x4D: t3100e_notify_set(dev->func_priv, 0x0f); break; /* Right */
    }

    DEBUG("ATkbd: translate is %s, ", translate ? "on" : "off");
    switch(val) {
	case FAKE_LSHIFT_ON:
		DEBUG("fake left shift on, scan code: ");
		if (num_lock) {
			if (shift_states) {
				DEBUG("N/A (one or both shifts on)\n");
				break;
			} else {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x2a;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x12;
						add_data_vals(dev, fake_shift, 2);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xaa;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xf0; fake_shift[2] = 0x12;
						add_data_vals(dev, fake_shift, 3);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xb6;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xf0; fake_shift[2] = 0x59;
						add_data_vals(dev, fake_shift, 3);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
			if (! shift_states) {
				DEBUG("N/A (both shifts off)\n");
			}
		}
		break;

	case FAKE_LSHIFT_OFF:
		DEBUG("fake left shift on, scan code: ");
		if (num_lock) {
			if (shift_states) {
				DEBUG("N/A (one or both shifts on)\n");
				break;
			} else {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xaa;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0xf0; fake_shift[2] = 0x12;
						add_data_vals(dev, fake_shift, 3);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x2a;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x12;
						add_data_vals(dev, fake_shift, 2);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				switch(keyboard_mode & 0x02) {
					case 1:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x36;
						add_data_vals(dev, fake_shift, 2);
						break;

					case 2:
						fake_shift[0] = 0xe0; fake_shift[1] = 0x59;
						add_data_vals(dev, fake_shift, 2);
						break;

					default:
						DEBUG("N/A (scan code set %i)\n", keyboard_mode & 0x02);
						break;
				}
			}
			if (! shift_states) {
				DEBUG("N/A (both shifts off)\n");
			}
		}
		break;

	default:
#ifdef _DEBUG
		DEBUG("scan code: ");
		if (translate) {
			DEBUG("%02x (original: ", (nont_to_t[val] | sc_or));
			if (sc_or == 0x80)
				DEBUG("F0 ");
			DEBUG("%02x)\n", val);
		} else
			DEBUG("%02x\n", val);
#endif

		key_queue[key_queue_end] = (translate ? (nont_to_t[val] | sc_or) : val);
		key_queue_end = (key_queue_end + 1) & 0xf;
		break;
    }

    if (sc_or == 0x80)
	sc_or = 0;
}


static void
write_output(atkbd_t *dev, uint8_t val)
{
    DBGLOG(1, "ATkbd: write_output(%02x) [old=%02x]\n", val, dev->output_port);

    if ((dev->output_port ^ val) & 0x20) { /*IRQ 12*/
	if (val & 0x20)
		picint(1 << 12);
	else
		picintc(1 << 12);
    }
    if ((dev->output_port ^ val) & 0x10) { /*IRQ 1*/
	if (val & 0x10)
		picint(1 << 1);
	else
		picintc(1 << 1);
    }
    if ((dev->output_port ^ val) & 0x02) { /*A20 enable change*/
	mem_a20_key = val & 0x02;
	mem_a20_recalc();
	flushmmucache();
    }
    if ((dev->output_port ^ val) & 0x01) { /*Reset*/
	if (! (val & 0x01)) {
		cpu_reset(0);
		cpu_set_edx();
	}
    }
    dev->output_port = val;
}


static void
write_cmd(atkbd_t *dev, uint8_t val)
{
    DBGLOG(1, "ATkbd: write_cmd(%02x) [old=%02x]\n", val, dev->mem[0]);

    if ((val & 1) && (dev->status & STAT_OFULL))
	dev->wantirq = 1;
    if (!(val & 1) && dev->wantirq)
	dev->wantirq = 0;

    /* PS/2 type 2 keyboard controllers always force the XLAT bit to 0. */
    if (KBC_TYPE(dev) == KBC_TYPE_PS2_2) {
	val &= ~CCB_TRANSLATE;
	dev->mem[0] &= ~CCB_TRANSLATE;
    }

    /* Scan code translate ON/OFF. */
    keyboard_mode &= 0x93;
    keyboard_mode |= (val & MODE_MASK);

    keyboard_scan = !(val & 0x10);
    DEBUG("ATkbd: keyboard is now %s\n",  keyboard_scan ? "enabled" : "disabled");
    DEBUG("ATkbd: keyboard interrupt is now %s\n",  (val & 0x01) ? "enabled" : "disabled");

    /* ISA AT keyboard controllers use bit 5 for keyboard mode (1 = PC/XT, 2 = AT);
       PS/2 (and EISA/PCI) keyboard controllers use it as the PS/2 mouse enable switch. */
    if ((KBC_VENDOR(dev) == KBC_VEN_AMI) ||
        (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF)) {
	keyboard_mode &= ~CCB_PCMODE;

	mouse_scan = !(val & 0x20);
	DEBUG("ATkbd: mouse is now %s\n",  mouse_scan ? "enabled" : "disabled");

	DEBUG("ATkbd: mouse interrupt is now %s\n",  (val & 0x02) ? "enabled" : "disabled");
    }

    DBGLOG(1, "Command byte now: %02x (%02x)\n", dev->mem[0], val);
}


static void
pulse_output(atkbd_t *dev, uint8_t mask)
{
    if (mask != 0x0f) {
    	dev->old_output_port = dev->output_port & ~(0xf0 | mask);
    	write_output(dev, dev->output_port & (0xf0 | mask));
    	dev->pulse_cb = 6LL * TIMER_USEC;
    }
}


static void
pulse_poll(priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;

    write_output(dev, dev->output_port | dev->old_output_port);
    dev->pulse_cb = 0LL;
}


static void
set_enable_kbd(atkbd_t *dev, uint8_t enable)
{
    dev->mem[0] &= 0xef;
    dev->mem[0] |= (enable ? 0x00 : 0x10);

    if (enable)
	dev->status |= STAT_UNLOCKED;
      else
	dev->status &= ~STAT_UNLOCKED;

    keyboard_scan = enable;
}


static void
set_enable_mouse(atkbd_t *dev, uint8_t enable)
{
    dev->mem[0] &= 0xdf;
    dev->mem[0] |= (enable ? 0x00 : 0x20);

    mouse_scan = enable;
}


static uint8_t
write64_generic(atkbd_t *dev, uint8_t val)
{
    uint8_t current_drive;

    switch (val) {
	case 0xa4:	/* check if password installed */
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: check if password installed\n");
			add_data(dev, 0xf1);
			return 0;
		} else
			ERRLOG("ATkbd: bad command A4\n");
		break;

	case 0xa7:	/* disable mouse port */
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: disable mouse port\n");
			set_enable_mouse(dev, 0);
			return 0;
		} else
			ERRLOG("ATkbd: bad command A7\n");
		break;

	case 0xa8:	/* enable mouse port */
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: enable mouse port\n");
			set_enable_mouse(dev, 1);
			return 0;
		} else
			ERRLOG("ATkbd: bad command A8\n");
		break;

	case 0xa9:	/* test mouse port */
		DEBUG("ATkbd: test mouse port\n");
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
			if (mouse_write)
				add_data(dev, 0x00); /* no error */
			else
				add_data(dev, 0xff); /* no mouse */
			return 0;
		} else
			ERRLOG("ATkbd: bad command A9\n");
		break;

	case 0xaf:	/* read keyboard version */
		DEBUG("ATkbd: read keyboard version\n");
		add_data(dev, 0x00);
		return 0;

	case 0xc0:	/* read input port */
		DEBUG("ATkbd: read input port\n");
		if (KBC_VENDOR(dev) == KBC_VEN_IBM_PS1) {
			current_drive = fdc_get_current_drive();
			add_data(dev, dev->input_port | (fdd_is_525(current_drive) ? 0x40 : 0x00));
			dev->input_port = ((dev->input_port + 1) & 3) |
					   (dev->input_port & 0xfc) |
					   (fdd_is_525(current_drive) ? 0x40 : 0x00);
		} else {
			add_data(dev, dev->input_port);
			dev->input_port = ((dev->input_port + 1) & 3) |
					   (dev->input_port & 0xfc);
		}
		return 0;

	case 0xd3:	/* write mouse output buffer */
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: write mouse output buffer\n");
			dev->want60 = 1;
			return 0;
		}
		break;

	case 0xd4:	/* write to mouse */
#if 0
		if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF) {
#endif
			DEBUG("ATkbd: write to mouse\n");
			dev->want60 = 1;
			return 0;
#if 0
		}
		break;
#endif

	case 0xf0: case 0xf1: case 0xf2: case 0xf3:
	case 0xf4: case 0xf5: case 0xf6: case 0xf7:
	case 0xf8: case 0xf9: case 0xfa: case 0xfb:
	case 0xfc: case 0xfd: case 0xfe: case 0xff:
		DBGLOG(1, "ATkbd: pulse %01X\n", val & 0x0f);
		pulse_output(dev, val & 0x0f);
		return 0;
    }

    return 1;
}


static uint8_t
write60_acer(atkbd_t *dev, uint8_t val)
{
#if 0
    switch(dev->command) {
	case 0xc0:	/* sent by Acer V30 BIOS */
		return 0;
    }
#endif

    return 1;
}


static uint8_t
write64_acer(atkbd_t *dev, uint8_t val)
{
    INFO("ACER: write64(%02x, %02x)\n", dev->command, val);

#if 0
    switch (val) {
	case 0xc0:	/* sent by Acer V30 BIOS */
		return 0;
    }
#endif

    return write64_generic(dev, val);
}


static uint8_t
write60_ami(atkbd_t *dev, uint8_t val)
{
    switch(dev->command) {
	/* 0x40 - 0x5F are aliases for 0x60-0x7F */
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47:
	case 0x48: case 0x49: case 0x4a: case 0x4b:
	case 0x4c: case 0x4d: case 0x4e: case 0x4f:
	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5a: case 0x5b:
	case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		DEBUG("ATkbd: alias write to %02x (AMI)\n", dev->command);
		dev->mem[dev->command & 0x1f] = val;
		if (dev->command == 0x60)
			write_cmd(dev, val);
		return 0;

	case 0xaf:	/* set extended controller RAM */
		DEBUG("ATkbd: set extended controller RAM (AMI)\n");
		if (dev->secr_phase == 1) {
			dev->mem_addr = val;
			dev->want60 = 1;
			dev->secr_phase = 2;
		} else if (dev->secr_phase == 2) {
			dev->mem[dev->mem_addr] = val;
			dev->secr_phase = 0;
		}
		return 0;

	case 0xcb:	/* set keyboard mode */
		DEBUG("ATkbd: set keyboard mode (AMI)\n");
		return 0;
    }

    return 1;
}


static uint8_t
write64_ami(atkbd_t *dev, uint8_t val)
{
    switch (val) {
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x0c: case 0x0d: case 0x0e: case 0x0f:
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		DEBUG("ATkbd: alias read from %02x (AMI)\n", val);
		add_data(dev, dev->mem[val]);
		return 0;

	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47:
	case 0x48: case 0x49: case 0x4a: case 0x4b:
	case 0x4c: case 0x4d: case 0x4e: case 0x4f:
	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5a: case 0x5b:
	case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		DEBUG("ATkbd: alias write to %02x (AMI)\n", dev->command);
		dev->want60 = 1;
		return 0;

	case 0xa1:	/* get controller version */
		DEBUG("ATkbd: get controller version (AMI)\n");
		return 0;

	case 0xa2:	/* clear keyboard controller lines P22/P23 */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: clear KBC lines P22/P23 (AMI)\n");
			write_output(dev, dev->output_port & 0xf3);
			add_data(dev, 0x00);
			return 0;
		}
		break;

	case 0xa3:	/* set keyboard controller lines P22/P23 */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: set KBC lines P22/P23 (AMI)\n");
			write_output(dev, dev->output_port | 0x0c);
			add_data(dev, 0x00);
			return 0;
		}
		break;

	case 0xa4:	/* write clock = low */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: write clock=low (AMI)\n");
			dev->ami_stat &= 0xfe;
			return 0;
		}
		break;

	case 0xa5:	/* write clock = high */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: write clock=high (AMI)\n");
			dev->ami_stat |= 0x01;
			return 0;
		}
		break;

	case 0xa6:	/* read clock */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: read clock (AMI)\n");
			add_data(dev, !!(dev->ami_stat & 1));
			return 0;
		}
		break;

	case 0xa7:	/* write cache bad */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: write cache bad (AMI)\n");
			dev->ami_stat &= 0xfd;
			return 0;
		}
		break;

	case 0xa8:	/* write cache good */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: write cache good (AMI)\n");
			dev->ami_stat |= 0x02;
			return 0;
		}
		break;

	case 0xa9:	/* read cache */
		if (KBC_TYPE(dev) < KBC_TYPE_PS2_NOREF) {
			DEBUG("ATkbd: read cache (AMI)\n");
			add_data(dev, !!(dev->ami_stat & 2));
			return 0;
		}
		break;

	case 0xaf:	/* set extended controller RAM */
		DEBUG("ATkbd: set extended controller RAM (AMI)\n");
		dev->want60 = 1;
		dev->secr_phase = 1;
		return 0;

	case 0xb0: case 0xb1: case 0xb2: case 0xb3:
		/* set KBC lines P10-P13 (input port bits 0-3) low */
		if (!PCI || (val > 0xb1)) {
			dev->input_port &= ~(1 << (val & 0x03));
			add_data(dev, 0x00);
		}
		return 0;

	case 0xb4: case 0xb5:
		/* set KBC lines P22-P23 (output port bits 2-3) low */
		if (! PCI) {
			write_output(dev, dev->output_port & ~(4 << (val & 0x01)));
			add_data(dev, 0x00);
		}
		return 0;

	case 0xb8: case 0xb9: case 0xba: case 0xbb:
		/* set KBC lines P10-P13 (input port bits 0-3) high */
		if (!PCI || (val > 0xb9)) {
			dev->input_port |= (1 << (val & 0x03));
			add_data(dev, 0x00);
		}
		return 0;

	case 0xbc: case 0xbd:
		/* set KBC lines P22-P23 (output port bits 2-3) high */
		if (! PCI) {
			write_output(dev, dev->output_port | (4 << (val & 0x01)));
			add_data(dev, 0x00);
		}
		return 0;

	case 0xc8:
		/*
		 * unblock KBC lines P22/P23
		 * (allow command D1 to change bits 2/3 of the output port)
		 */
		DEBUG("ATkbd: unblock KBC lines P22/P23 (AMI)\n");
		dev->output_locked = 1;
		return 0;

	case 0xc9:
		/*
		 * block KBC lines P22/P23
		 * (disallow command D1 from changing bits 2/3 of the port)
		 */
		DEBUG("ATkbd: block KBC lines P22/P23 (AMI)\n");
		dev->output_locked = 1;
		return 0;

	case 0xca:	/* read keyboard mode */
		DEBUG("ATkbd: read keyboard mode (AMI)\n");
		add_data(dev, 0x00); /*ISA mode*/
		return 0;

	case 0xcb:	/* set keyboard mode */
		DEBUG("ATkbd: set keyboard mode (AMI)\n");
		dev->want60 = 1;
		return 0;

	case 0xef:	/* ??? - sent by AMI486 */
		DEBUG("ATkbd: ??? - sent by AMI486\n");
		return 0;
    }

    return write64_generic(dev, val);
}


static uint8_t
write64_ibm_mca(atkbd_t *dev, uint8_t val)
{
    switch (val) {
	case 0xc1: /*Copy bits 0 to 3 of input port to status bits 4 to 7*/
		DEBUG("ATkbd: copy bits 0 to 3 of input port to status bits 4 to 7\n");
		dev->status &= 0x0f;
		dev->status |= ((((dev->input_port & 0xfc) | 0x84) & 0x0f) << 4);
		return 0;

	case 0xc2: /*Copy bits 4 to 7 of input port to status bits 4 to 7*/
		DEBUG("ATkbd: copy bits 4 to 7 of input port to status bits 4 to 7\n");
		dev->status &= 0x0f;
		dev->status |= (((dev->input_port & 0xfc) | 0x84) & 0xf0);
		return 0;

	case 0xaf:
		DEBUG("ATkbd: bad KBC command AF\n");
		return 1;

	case 0xf0: case 0xf1: case 0xf2: case 0xf3:
	case 0xf4: case 0xf5: case 0xf6: case 0xf7:
	case 0xf8: case 0xf9: case 0xfa: case 0xfb:
	case 0xfc: case 0xfd: case 0xfe: case 0xff:
		DEBUG("ATkbd: pulse: %01X\n", (val & 0x03) | 0x0c);
		pulse_output(dev, (val & 0x03) | 0x0c);
		return 0;
    }

    return write64_generic(dev, val);
}


static uint8_t
write60_quadtel(atkbd_t *dev, uint8_t val)
{
    switch(dev->command) {
	case 0xcf:	/*??? - sent by MegaPC BIOS*/
		DEBUG("ATkbd: ??? - sent by MegaPC BIOS\n");
		return 0;
    }

    return 1;
}


static uint8_t
write64_quadtel(atkbd_t *dev, uint8_t val)
{
    switch (val) {
	case 0xaf:
		DEBUG("ATkbd: bad KBC command AF\n");
		return 1;

	case 0xcf:	/*??? - sent by MegaPC BIOS*/
		DEBUG("ATkbd: ??? - sent by MegaPC BIOS\n");
		dev->want60 = 1;
		return 0;
    }

    return write64_generic(dev, val);
}


static uint8_t
write60_toshiba(atkbd_t *dev, uint8_t val)
{
    switch(dev->command) {
	case 0xb6:	/* T3100e - set color/mono switch */
		t3100e_mono_set(dev->func_priv, val);
		return 0;
    }

    return 1;
}


static uint8_t
write64_toshiba(atkbd_t *dev, uint8_t val)
{
    switch (val) {
	case 0xaf:
		DEBUG("ATkbd: bad KBC command AF\n");
		return 1;

	case 0xb0:	/* T3100e: Turbo on */
		t3100e_turbo_set(dev->func_priv, 1);
		return 0;

	case 0xb1:	/* T3100e: Turbo off */
		t3100e_turbo_set(dev->func_priv, 0);
		return 0;

	case 0xb2:	/* T3100e: Select external display */
		t3100e_display_set(0x00);
		return 0;

	case 0xb3:	/* T3100e: Select internal display */
		t3100e_display_set(0x01);
		return 0;

	case 0xb4:	/* T3100e: Get configuration / status */
		add_data(dev, t3100e_config_get(dev->func_priv));
		return 0;

	case 0xb5:	/* T3100e: Get colour / mono byte */
		add_data(dev, t3100e_mono_get(dev->func_priv));
		return 0;

	case 0xb6:	/* T3100e: Set colour / mono byte */
		dev->want60 = 1;
		return 0;

	case 0xb7:	/* T3100e: Emulate PS/2 keyboard - not implemented */
	case 0xb8:	/* T3100e: Emulate AT keyboard - not implemented */
		return 0;

	case 0xbb:	/* T3100e: Read 'Fn' key.
			   Return it for right Ctrl and right Alt; on the real
			   T3100e, these keystrokes could only be generated
			   using 'Fn'. */
		if (keyboard_recv(0xb8) ||	/* Right Alt */
		    keyboard_recv(0x9d))	/* Right Ctrl */
			add_data(dev, 0x04);
		else	add_data(dev, 0x00);
		return 0;

	case 0xbc:	/* T3100e: Reset Fn+Key notification */
		t3100e_notify_set(dev->func_priv, 0x00);
		return 0;

	case 0xc0:	/*Read input port*/
		DEBUG("ATkbd: read input port\n");

		/* The T3100e returns all bits set except bit 6 which
		 * is set by t3100e_mono_set() */
		dev->input_port = (t3100e_mono_get(dev->func_priv) & 1) ? 0xff : 0xbf;
		add_data(dev, dev->input_port);
		return 0;

    }

    return write64_generic(dev, val);
}


static void
kbd_write(uint16_t port, uint8_t val, priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;
    int i = 0;
    int bad = 1;
    uint8_t mask;

    if ((KBC_VENDOR(dev) == KBC_VEN_XI8088) && (port == 0x63))
	port = 0x61;

    DBGLOG(2, "ATkbd: write(%04x, %02x)\n", port, val);

    switch (port) {
	case 0x60:
		if (dev->want60) {
			/* Write data to controller. */
			dev->want60 = 0;

			switch (dev->command) {
				case 0x60: case 0x61: case 0x62: case 0x63:
				case 0x64: case 0x65: case 0x66: case 0x67:
				case 0x68: case 0x69: case 0x6a: case 0x6b:
				case 0x6c: case 0x6d: case 0x6e: case 0x6f:
				case 0x70: case 0x71: case 0x72: case 0x73:
				case 0x74: case 0x75: case 0x76: case 0x77:
				case 0x78: case 0x79: case 0x7a: case 0x7b:
				case 0x7c: case 0x7d: case 0x7e: case 0x7f:
					dev->mem[dev->command & 0x1f] = val;
					if (dev->command == 0x60)
						write_cmd(dev, val);
					break;

				case 0xd1: /* write output port */
					DEBUG("ATkbd: write output port\n");
					if (dev->output_locked) {
						/*If keyboard controller lines P22-P23 are blocked,
						  we force them to remain unchanged.*/
						val &= ~0x0c;
						val |= (dev->output_port & 0x0c);
					}
					write_output(dev, val);
					break;

				case 0xd2: /* write to keyboard output buffer */
					DEBUG("ATkbd: write to keyboard output buffer\n");
					add_data_kbd(val);
					break;

				case 0xd3: /* write to mouse output buffer */
					DEBUG("ATkbd: write to mouse output buffer\n");
					if (mouse_write && (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF))
						keyboard_at_adddata_mouse(val);
					break;

				case 0xd4: /* write to mouse */
					DEBUG("ATkbd: write to mouse (%02x)\n", val);

					/* FIXME: What does this do? --FvK */
					if (val == 0xbb)
						break;

					set_enable_mouse(dev, 1);
					if (mouse_write && (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF))
						mouse_write(val, mouse_p);
					break;

				default:
					/*
					 * Run the vendor-specific handler
					 * if we have one. Otherwise, or if
					 * it returns an error, log a bad
					 * controller command.
					 */
					if (dev->write60_ven)
						bad = dev->write60_ven(dev, val);

					if (bad) {
						ERRLOG("ATkbd: bad controller command %02x data %02x\n", dev->command, val);
						add_data_kbd(0xfe);
					}
			}
		} else {
			/* Write data to keyboard. */
			dev->mem[0] &= ~0x10;

			if (dev->key_wantdata) {
				dev->key_wantdata = 0;

				/*
				 * Several system BIOSes and OS device drivers
				 * mess up with this, and repeat the command
				 * code many times.  Fun!
				 */
				if (val == dev->key_command) {
#if USE_IGNORE
					/* Respond NAK and ignore it. */
					add_data_kbd(0xfe);
					dev->key_command = 0x00;
					break;
#else
					goto do_command;
#endif
				}

				switch (dev->key_command) {
					case 0xed: /* set/reset LEDs */
						add_data_kbd(0xfa);
						INFO("ATkbd: set LEDs [%02x]\n",
								val);
						break;

					case 0xf0: /* get/set scancode set */
						add_data_kbd(0xfa);
						if (val == 0) {
							DEBUG("ATkbd: get scan code set: %02x\n", keyboard_mode & 3);
							add_data_kbd(keyboard_mode & 3);
						} else {
							if ((val <= 3) && (val != 1)) {
								keyboard_mode &= 0xfc;
								keyboard_mode |= (val & 3);
								DEBUG("ATkbd: scan code set now: %02x\n", val);
							}
							set_scancode_map(dev);
						}
						break;

					case 0xf3: /* set typematic rate/delay */
						add_data_kbd(0xfa);
						break;
					
					default:
						DEBUG("ATkbd: bad keyboard %02x data %02x\n", dev->key_command, val);
						add_data_kbd(0xfe);
						break;
				}

				/* Keyboard command is now done. */
				dev->key_command = 0x00;
			} else {
#if !USE_IGNORE
do_command:
#endif
				/* No keyboard command in progress. */
				dev->key_command = 0x00;

				set_enable_kbd(dev, 1);

				switch (val) {
					case 0x00:
						DEBUG("ATkbd: command 00\n");
						add_data_kbd(0xfa);
						break;

					case 0x05: /*??? - sent by NT 4.0*/
						DEBUG("ATkbd: command 05 (NT 4.0)\n");
						add_data_kbd(0xfe);
						break;

					/* Sent by Pentium-era AMI BIOS'es.*/
					case 0x71:
					case 0x82:
						DEBUG("ATkbd: command %02x (Pentium-era AMI BIOS)\n", val);
						break;

					case 0xed: /* set/reset LEDs */
						DEBUG("ATkbd: set/reset leds\n");
						add_data_kbd(0xfa);

						dev->key_wantdata = 1;
						break;

					case 0xee: /* diagnostic echo */
						DEBUG("ATkbd: ECHO\n");
						add_data_kbd(0xee);
						break;

					case 0xef: /* NOP (reserved for future use) */
						DEBUG("ATkbd: NOP\n");
						break;

					case 0xf0: /* get/set scan code set */
						DEBUG("ATkbd: scan code set\n");
						add_data_kbd(0xfa);
						dev->key_wantdata = 1;
						break;

					case 0xf2: /* read ID */
						/* Fixed as translation will be done in add_data_kbd(). */
						DEBUG("ATkbd: read keyboard id\n");
						add_data_kbd(0xfa);
						add_data_kbd(0xab);
						add_data_kbd(0x83);
						break;

					case 0xf3: /* set typematic rate/delay */
						DEBUG("ATkbd: set typematic rate/delay\n");
						add_data_kbd(0xfa);
						dev->key_wantdata = 1;
						break;

					case 0xf4: /* enable keyboard */
						DEBUG("ATkbd: enable keyboard via keyboard\n");
						add_data_kbd(0xfa);
						keyboard_scan = 1;
						break;

					case 0xf5: /* disable keyboard */
						DEBUG("ATkbd: disable keyboard via keyboard\n");
						keyboard_scan = 0;

						/*
						 * Disabling the keyboard also
						 * resets it to the default
						 * values.
						 */
						/*FALLTHROUGH*/

					case 0xf6: /* set defaults */
						DEBUG("ATkbd: set defaults\n");
						add_data_kbd(0xfa);

						keyboard_set3_all_break = 0;
						keyboard_set3_all_repeat = 0;
						memset(keyboard_set3_flags, 0, 512);
						keyboard_mode = (keyboard_mode & 0xfc) | 0x02;
						set_scancode_map(dev);
						break;

					case 0xf7: /* set all keys to repeat */
						DEBUG("ATkbd: set all keys to repeat\n");
						add_data_kbd(0xfa);
						keyboard_set3_all_break = 1;
						break;

					case 0xf8: /* set all keys to give make/break codes */
						DEBUG("ATkbd: set all keys to give make/break codes\n");
						add_data_kbd(0xfa);
						keyboard_set3_all_break = 1;
						break;

					case 0xf9: /* set all keys to give make codes only */
						DEBUG("ATkbd: set all keys to give make codes only\n");
						add_data_kbd(0xfa);
						keyboard_set3_all_break = 0;
						break;

					case 0xfa: /* set all keys to repeat and give make/break codes */
						DEBUG("ATkbd: set all keys to repeat and give make/break codes\n");
						add_data_kbd(0xfa);
						keyboard_set3_all_repeat = 1;
						keyboard_set3_all_break = 1;
						break;

					case 0xfe: /* resend last scan code */
						DEBUG("ATkbd: reset last scan code\n");
						add_data_kbd(dev->last_scan_code);
						break;

					case 0xff: /* reset */
						DEBUG("ATkbd: kbd reset\n");
						key_queue_start = key_queue_end = 0; /*Clear key queue*/
						add_data_kbd(0xfa);
						add_data_kbd(0xaa);

						/* Set scan code set to 2. */
						keyboard_mode = (keyboard_mode & 0xfc) | 0x02;
						set_scancode_map(dev);
						break;

					default:
						ERRLOG("ATkbd: bad keyboard command %02x\n", val);
						add_data_kbd(0xfe);
				}

				/* If command needs data, remember command. */
				if (dev->key_wantdata == 1)
					dev->key_command = val;
			}
		}
		break;

	case 0x61:
		ppi.pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();
		speaker_gated = val & 1;
		speaker_enable = val & 2;
		if (speaker_enable) 
			speaker_was_enable = 1;
		pit_set_gate(&pit, 2, val & 1);

                if (KBC_VENDOR(dev) == KBC_VEN_XI8088)
			dev->write_func(dev->func_priv, !!(val & 0x04));
		break;

	case 0x64:
		/* Controller command. */
		dev->want60 = 0;

		switch (val) {
			/* Read data from KBC memory. */
			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b:
			case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			case 0x30: case 0x31: case 0x32: case 0x33:
			case 0x34: case 0x35: case 0x36: case 0x37:
			case 0x38: case 0x39: case 0x3a: case 0x3b:
			case 0x3c: case 0x3d: case 0x3e: case 0x3f:
				add_data(dev, dev->mem[val & 0x1f]);
				break;

			/* Write data to KBC memory. */
			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67:
			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7e: case 0x7f:
				dev->want60 = 1;
				break;

			case 0xaa:	/* self-test */
				DEBUG("ATkbd: self-test\n");
				if (KBC_VENDOR(dev) == KBC_VEN_TOSHIBA)
					dev->status |= STAT_IFULL;
				if (! dev->initialized) {
					dev->initialized = 1;
					key_ctrl_queue_start = key_ctrl_queue_end = 0;
					dev->status &= ~STAT_OFULL;
				}
				dev->status |= STAT_SYSFLAG;
				dev->mem[0] |= 0x04;
				set_enable_kbd(dev, 1);
				if (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF)
					set_enable_mouse(dev, 1);
				write_output(dev, 0xcf);
				add_data(dev, 0x55);
				break;

			case 0xab:	/* interface test */
				DEBUG("ATkbd: interface test\n");
				add_data(dev, 0x00); /*no error*/
				break;

			case 0xac:	/* diagnostic dump */
				DEBUG("ATkbd: diagnostic dump\n");
				for (i = 0; i < 16; i++)
					add_data(dev, dev->mem[i]);
				add_data(dev, (dev->input_port & 0xf0) | 0x80);
				add_data(dev, dev->output_port);
				add_data(dev, dev->status);
				break;

			case 0xad:	/* disable keyboard */
				DEBUG("ATkbd: disable keyboard\n");
				set_enable_kbd(dev, 0);
				break;

			case 0xae:	/* enable keyboard */
				DEBUG("ATkbd: enable keyboard\n");
				set_enable_kbd(dev, 1);
				break;

			case 0xd0:	/* read output port */
				DEBUG("ATkbd: read output port\n");
				mask = 0xff;
				if (! keyboard_scan)
					mask &= 0xbf;
				if (!mouse_scan && (KBC_TYPE(dev) >= KBC_TYPE_PS2_NOREF))
					mask &= 0xf7;
				add_data(dev, dev->output_port & mask);
				break;

			case 0xd1:	/* write output port */
				DEBUG("ATkbd: write output port\n");
				dev->want60 = 1;
				break;

			case 0xd2:	/* write keyboard output buffer */
				DEBUG("ATkbd: write keyboard output buffer\n");
				dev->want60 = 1;
				break;

			case 0xd4:	/* dunno, but OS/2 2.00LA sends it */
				dev->want60 = 1;
				break;

			case 0xdd:	/* disable A20 address line */
				DEBUG("ATkbd: disable A20\n");
				write_output(dev, dev->output_port & 0xfd);
				break;

			case 0xdf:	/* enable A20 address line */
				DEBUG("ATkbd: enable A20\n");
				write_output(dev, dev->output_port | 0x02);
				break;

			case 0xe0:	/* read test inputs */
				DEBUG("ATkbd: read test inputs\n");
				add_data(dev, 0x00);
				break;

			default:
				/*
				 * Unrecognized controller command.
				 *
				 * If we have a vendor-specific handler, run
				 * that. Otherwise, or if that handler fails,
				 * log a bad command.
				 */
				if (dev->write64_ven)
					bad = dev->write64_ven(dev, val);

				if (bad) {
					DEBUG("ATkbd: bad controller command %02x\n", val);
				}
		}

		/* If the command needs data, remember the command. */
		if (dev->want60)
			dev->command = val;
		break;
    }
}


static uint8_t
kbd_read(uint16_t port, priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;
    uint8_t ret = 0xff;

    if ((KBC_VENDOR(dev) == KBC_VEN_XI8088) && (port == 0x63))
	port = 0x61;

    switch (port) {
	case 0x60:
		ret = dev->out;
		dev->status &= ~(STAT_OFULL);
		if (dev->last_irq) {
			picintc(dev->last_irq);
			dev->last_irq = 0;
		}
		break;

	case 0x61:
		ret = ppi.pb & ~0xe0;
		if (ppispeakon)
			ret |= 0x20;
		if (KBC_TYPE(dev) > KBC_TYPE_PS2_NOREF) {
			if (dev->refresh)
				ret |= 0x10;
			else
				ret &= ~0x10;
		}
                if (KBC_VENDOR(dev) == KBC_VEN_XI8088) {
			if (dev->read_func(dev->func_priv))
                                ret |= 0x04;
                        else
                                ret &= ~0x04;
                }
		break;

	case 0x64:
		ret = (dev->status & 0xFB) | (keyboard_mode & CCB_SYSTEM);
		ret |= STAT_UNLOCKED;
		/* The transmit timeout (TTIMEOUT) flag should *NOT* be cleared, otherwise
		   the IBM PS/2 Model 80's BIOS gives error 8601 (mouse error). */
		dev->status &= ~(STAT_RTIMEOUT/* | STAT_TTIMEOUT*/);
		break;

	default:
		ERRLOG("ATkbd: read(%04x) invalid!\n", port);
		break;
    }

    DBGLOG(2, "ATkbd: read(%04x) = %02x\n", port, ret);

    return(ret);
}


static void
kbd_refresh(priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;

    dev->refresh = !dev->refresh;
    dev->refresh_time += PS2_REFRESH_TIME;
}


static void
kbd_reset(priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;

    dev->initialized = 0;
    dev->dtrans = 0;
    dev->first_write = 1;
    dev->status = STAT_UNLOCKED | STAT_CD;
    dev->mem[0] = 0x01;
    dev->wantirq = 0;
    write_output(dev, 0xcf);
    dev->out_new = -1;
    dev->last_irq = 0;
    dev->secr_phase = 0;
    dev->key_wantdata = 0;

    /* Set up the correct Video Type bits. */
    if (KBC_VENDOR(dev) == KBC_VEN_XI8088)
	dev->input_port = (video_type() == VID_TYPE_MDA) ? 0xb0 : 0xf0;
    else
	dev->input_port = (video_type() == VID_TYPE_MDA) ? 0xf0 : 0xb0;

    /* For now, always assume high-speed mode. */
    dev->input_port |= 0x04;
    DEBUG("ATkbd: input port = %02x\n", dev->input_port);

    keyboard_mode = 0x02 | dev->dtrans;

    /* Enable keyboard, disable mouse. */
    set_enable_kbd(dev, 1);
    set_enable_mouse(dev, 0);

    sc_or = 0;

    memset(keyboard_set3_flags, 0, 512);

    set_scancode_map(dev);
}


static void
kbd_close(priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)priv;

    /* Stop timers */
    dev->refresh_time = 0;
    keyboard_delay = 0;

    keyboard_scan = 0;
    keyboard_send = NULL;

    /* Disable the scancode maps. */
    keyboard_set_table(NULL);

    SavedKbd = NULL;

    free(dev);
}


static priv_t
kbd_init(const device_t *info, UNUSED(void *parent))
{
    atkbd_t *dev;

    dev = (atkbd_t *)mem_alloc(sizeof(atkbd_t));
    memset(dev, 0x00, sizeof(atkbd_t));
    dev->flags = info->local;

    kbd_reset(dev);

    io_sethandler(0x0060, 5,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    keyboard_send = add_data_kbd;

    timer_add(kbd_poll, dev, &keyboard_delay, TIMER_ALWAYS_ENABLED);

    if (KBC_TYPE(dev) > KBC_TYPE_PS2_NOREF)
	timer_add(kbd_refresh, dev, &dev->refresh_time, TIMER_ALWAYS_ENABLED);

    timer_add(pulse_poll, dev, &dev->pulse_cb, &dev->pulse_cb);

    dev->write60_ven = NULL;
    dev->write64_ven = NULL;

    switch(KBC_VENDOR(dev)) {
	case KBC_VEN_GENERIC:
	case KBC_VEN_IBM_PS1:
		dev->write64_ven = write64_generic;
		break;

	case KBC_VEN_AMI:
		dev->write60_ven = write60_ami;
		dev->write64_ven = write64_ami;
		break;

	case KBC_VEN_IBM_MCA:
		dev->write64_ven = write64_ibm_mca;
		break;

	case KBC_VEN_QUADTEL:
		dev->write60_ven = write60_quadtel;
		dev->write64_ven = write64_quadtel;
		break;

	case KBC_VEN_TOSHIBA:
		dev->write60_ven = write60_toshiba;
		dev->write64_ven = write64_toshiba;
		break;

	case KBC_VEN_ACER:
		dev->write60_ven = write60_acer;
		dev->write64_ven = write64_acer;
		break;
    }

    /* We need this, sadly. */
    SavedKbd = dev;

    return((priv_t)dev);
}


const device_t keyboard_at_device = {
    "PC/AT Keyboard",
    0,
    KBC_TYPE_ISA | KBC_VEN_GENERIC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_at_ami_device = {
    "PC/AT Keyboard (AMI)",
    0,
    KBC_TYPE_ISA | KBC_VEN_AMI,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_at_toshiba_device = {
    "PC/AT Keyboard (Toshiba)",
    0,
    KBC_TYPE_ISA | KBC_VEN_TOSHIBA,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_device = {
    "PS/2 Keyboard (Generic)",
    0,
    KBC_TYPE_PS2_NOREF | KBC_VEN_GENERIC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_pci_device = {
    "PS/2 Keyboard (Generic, PCI)",
    DEVICE_PCI,
    KBC_TYPE_PS2_NOREF | KBC_VEN_GENERIC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_ps1_device = {
    "PS/2 Keyboard (IBM PS/1)",
    0,
    KBC_TYPE_PS2_NOREF | KBC_VEN_IBM_PS1,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_ps2_device = {
    "PS/2 Keyboard (IBM PS/2)",
    0,
    KBC_TYPE_PS2_1 | KBC_VEN_GENERIC,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_acer_device = {
    "PS/2 Keyboard (Acer 90M002A)",
    0,
#ifdef KBC_VEN_ACER
    KBC_TYPE_PS2_NOREF | KBC_VEN_ACER,
#else
    KBC_TYPE_PS2_NOREF | KBC_VEN_GENERIC,
#endif
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_ami_device = {
    "PS/2 Keyboard (AMIKEY)",
    0,
    KBC_TYPE_PS2_NOREF | KBC_VEN_AMI,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_ami_pci_device = {
    "PS/2 Keyboard (AMIKEY PCI)",
    DEVICE_PCI,
    KBC_TYPE_PS2_NOREF | KBC_VEN_AMI,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_mca_device = {
    "PS/2 Keyboard (MCA #1)",
    0,
    KBC_TYPE_PS2_1 | KBC_VEN_IBM_MCA,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_mca_2_device = {
    "PS/2 Keyboard (MCA #2)",
    0,
    KBC_TYPE_PS2_2 | KBC_VEN_IBM_MCA,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_quadtel_device = {
    "PS/2 Keyboard (Quadtel/MegaPC)",
    0,
    KBC_TYPE_PS2_NOREF | KBC_VEN_QUADTEL,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t keyboard_ps2_xi8088_device = {
    "PS/2 Keyboard (Xi8088)",
    0,
    KBC_TYPE_PS2_NOREF | KBC_VEN_XI8088,
    NULL,
    kbd_init, kbd_close, kbd_reset,
    NULL, NULL, NULL, NULL,
    NULL
};


void
keyboard_at_set_mouse(void (*func)(uint8_t val, priv_t), priv_t priv)
{
    mouse_write = func;
    mouse_p = priv;
}


void
keyboard_at_adddata_keyboard_raw(uint8_t val)
{
    key_queue[key_queue_end] = val;
    key_queue_end = (key_queue_end + 1) & 0xf;
}


void
keyboard_at_adddata_mouse(uint8_t val)
{
    mouse_queue[mouse_queue_end] = val;
    mouse_queue_end = (mouse_queue_end + 1) & 0xf;
}


/* Set custom machine-dependent keyboard stuff. */
void
keyboard_at_set_funcs(priv_t arg, uint8_t (*readfunc)(priv_t), void (*writefunc)(priv_t, uint8_t), priv_t priv)
{
    atkbd_t *dev = (atkbd_t *)arg;

    dev->read_func = readfunc;
    dev->write_func = writefunc;
    dev->func_priv = priv;
}


void
keyboard_at_set_mouse_scan(uint8_t val)
{
    atkbd_t *dev = SavedKbd;
    uint8_t temp_mouse_scan = val ? 1 : 0;

    if (temp_mouse_scan == mouse_scan) return;

    set_enable_mouse(dev, val ? 1 : 0);

    INFO("ATkbd: mouse scan %sabled via PCI\n", mouse_scan ? "en" : "dis");
}


uint8_t
keyboard_at_get_mouse_scan(void)
{
    return(mouse_scan ? 0x10 : 0x00);
}
