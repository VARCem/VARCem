/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Main video-rendering module.
 *
 * Version:	@(#)video.c	1.0.28	2019/04/19
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <math.h>
#define HAVE_STDARG_H
#define dbglog video_log
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../machines/machine.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../timer.h"
#include "../../plat.h"
#include "video.h"
#include "vid_mda.h"
#include "vid_svga.h"


#ifdef ENABLE_VIDEO_LOG
int		video_do_log = ENABLE_VIDEO_LOG;
#endif

/* These will go away soon. */
uint8_t		fontdat[2048][8];		/* IBM CGA font */
uint8_t		fontdatm[2048][16];		/* IBM MDA font */
dbcs_font_t	*fontdatk = NULL,		/* Korean KSC-5601 font */
		*fontdatk_user = NULL;		/* Korean KSC-5601 font (user)*/

bitmap_t	*screen = NULL;

uint32_t	*video_6to8 = NULL,
		*video_15to32 = NULL,
		*video_16to32 = NULL;
uint32_t	pal_lookup[256];
uint8_t		edatlookup[4][4];
int		xsize = 1,
		ysize = 1;
int		cga_palette = 0;
int		changeframecount = 2;
int		frames = 0;
int		fullchange = 0;
int		displine = 0;
int		update_overscan = 0;
int		overscan_x = 0,
		overscan_y = 0;

int		video_timing_read_b = 0,
		video_timing_read_w = 0,
		video_timing_read_l = 0;
int		video_timing_write_b = 0,
		video_timing_write_w = 0,
		video_timing_write_l = 0;
int		video_res_x = 0,
		video_res_y = 0,
		video_bpp = 0;

PALETTE		cgapal = {
    {0,0,0},    {0,42,0},   {42,0,0},   {42,21,0},
    {0,0,0},    {0,42,42},  {42,0,42},  {42,42,42},
    {0,0,0},    {21,63,21}, {63,21,21},  {63,63,21},
    {0,0,0},    {21,63,63}, {63,21,63}, {63,63,63},

    {0,0,0},    {0,0,42},   {0,42,0},   {0,42,42},
    {42,0,0},   {42,0,42},  {42,21,00}, {42,42,42},
    {21,21,21}, {21,21,63}, {21,63,21}, {21,63,63},
    {63,21,21}, {63,21,63}, {63,63,21}, {63,63,63},

    {0,0,0},    {0,21,0},   {0,0,42},   {0,42,42},
    {42,0,21},  {21,10,21}, {42,0,42},  {42,0,63},
    {21,21,21}, {21,63,21}, {42,21,42}, {21,63,63},
    {63,0,0},   {42,42,0},  {63,21,42}, {41,41,41},
        
    {0,0,0},   {0,42,42},   {42,0,0},   {42,42,42},
    {0,0,0},   {0,42,42},   {42,0,0},   {42,42,42},
    {0,0,0},   {0,63,63},   {63,0,0},   {63,63,63},
    {0,0,0},   {0,63,63},   {63,0,0},   {63,63,63},
};
PALETTE		cgapal_mono[6] = {
    {	/* 0 - green, 4-color-optimized contrast. */
	{0x00,0x00,0x00},{0x00,0x0d,0x03},{0x01,0x17,0x05},
	{0x01,0x1a,0x06},{0x02,0x28,0x09},{0x02,0x2c,0x0a},
	{0x03,0x39,0x0d},{0x03,0x3c,0x0e},{0x00,0x07,0x01},
	{0x01,0x13,0x04},{0x01,0x1f,0x07},{0x01,0x23,0x08},
	{0x02,0x31,0x0b},{0x02,0x35,0x0c},{0x05,0x3f,0x11},
	{0x0d,0x3f,0x17},
    },
    {	/* 1 - green, 16-color-optimized contrast. */
	{0x00,0x00,0x00},{0x00,0x0d,0x03},{0x01,0x15,0x05},
	{0x01,0x17,0x05},{0x01,0x21,0x08},{0x01,0x24,0x08},
	{0x02,0x2e,0x0b},{0x02,0x31,0x0b},{0x01,0x22,0x08},
	{0x02,0x28,0x09},{0x02,0x30,0x0b},{0x02,0x32,0x0c},
	{0x03,0x39,0x0d},{0x03,0x3b,0x0e},{0x09,0x3f,0x14},
	{0x0d,0x3f,0x17},
    },
    {	/* 2 - amber, 4-color-optimized contrast. */
	{0x00,0x00,0x00},{0x15,0x05,0x00},{0x20,0x0b,0x00},
	{0x24,0x0d,0x00},{0x33,0x18,0x00},{0x37,0x1b,0x00},
	{0x3f,0x26,0x01},{0x3f,0x2b,0x06},{0x0b,0x02,0x00},
	{0x1b,0x08,0x00},{0x29,0x11,0x00},{0x2e,0x14,0x00},
	{0x3b,0x1e,0x00},{0x3e,0x21,0x00},{0x3f,0x32,0x0a},
	{0x3f,0x38,0x0d},
    },
    {	/* 3 - amber, 16-color-optimized contrast. */
	{0x00,0x00,0x00},{0x15,0x05,0x00},{0x1e,0x09,0x00},
	{0x21,0x0b,0x00},{0x2b,0x12,0x00},{0x2f,0x15,0x00},
	{0x38,0x1c,0x00},{0x3b,0x1e,0x00},{0x2c,0x13,0x00},
	{0x32,0x17,0x00},{0x3a,0x1e,0x00},{0x3c,0x1f,0x00},
	{0x3f,0x27,0x01},{0x3f,0x2a,0x04},{0x3f,0x36,0x0c},
	{0x3f,0x38,0x0d},
    },
    {	/* 4 - grey, 4-color-optimized contrast. */
	{0x00,0x00,0x00},{0x0e,0x0f,0x10},{0x15,0x17,0x18},
	{0x18,0x1a,0x1b},{0x24,0x25,0x25},{0x27,0x28,0x28},
	{0x33,0x34,0x32},{0x37,0x38,0x35},{0x09,0x0a,0x0b},
	{0x11,0x12,0x13},{0x1c,0x1e,0x1e},{0x20,0x22,0x22},
	{0x2c,0x2d,0x2c},{0x2f,0x30,0x2f},{0x3c,0x3c,0x38},
	{0x3f,0x3f,0x3b},
    },
    {	/* 5 - grey, 16-color-optimized contrast. */
	{0x00,0x00,0x00},{0x0e,0x0f,0x10},{0x13,0x14,0x15},
	{0x15,0x17,0x18},{0x1e,0x20,0x20},{0x20,0x22,0x22},
	{0x29,0x2a,0x2a},{0x2c,0x2d,0x2c},{0x1f,0x21,0x21},
	{0x23,0x25,0x25},{0x2b,0x2c,0x2b},{0x2d,0x2e,0x2d},
	{0x34,0x35,0x33},{0x37,0x37,0x34},{0x3e,0x3e,0x3a},
	{0x3f,0x3f,0x3b},
    }
};

static const video_timings_t timing_default = {
    VID_ISA, 8, 16, 32, 8, 16, 32
};
static const uint32_t	shade[5][256] = {
    {0},/* RGB Color (unused) */
    {0},/* RGB Grayscale (unused) */
    {	/* Amber monitor */
	0x000000, 0x060000, 0x090000, 0x0d0000,
	0x100000, 0x120100, 0x150100, 0x170100,
	0x1a0100, 0x1c0100, 0x1e0200, 0x210200,
	0x230200, 0x250300, 0x270300, 0x290300,
	0x2b0400, 0x2d0400, 0x2f0400, 0x300500,
	0x320500, 0x340500, 0x360600, 0x380600,
	0x390700, 0x3b0700, 0x3d0700, 0x3f0800,
	0x400800, 0x420900, 0x440900, 0x450a00,
	0x470a00, 0x480b00, 0x4a0b00, 0x4c0c00,
	0x4d0c00, 0x4f0d00, 0x500d00, 0x520e00,
	0x530e00, 0x550f00, 0x560f00, 0x581000,
	0x591000, 0x5b1100, 0x5c1200, 0x5e1200,
	0x5f1300, 0x601300, 0x621400, 0x631500,
	0x651500, 0x661600, 0x671600, 0x691700,
	0x6a1800, 0x6c1800, 0x6d1900, 0x6e1a00,
	0x701a00, 0x711b00, 0x721c00, 0x741c00,
	0x751d00, 0x761e00, 0x781e00, 0x791f00,
	0x7a2000, 0x7c2000, 0x7d2100, 0x7e2200,
	0x7f2300, 0x812300, 0x822400, 0x832500,
	0x842600, 0x862600, 0x872700, 0x882800,
	0x8a2900, 0x8b2900, 0x8c2a00, 0x8d2b00,
	0x8e2c00, 0x902c00, 0x912d00, 0x922e00,
	0x932f00, 0x953000, 0x963000, 0x973100,
	0x983200, 0x993300, 0x9b3400, 0x9c3400,
	0x9d3500, 0x9e3600, 0x9f3700, 0xa03800,
	0xa23900, 0xa33a00, 0xa43a00, 0xa53b00,
	0xa63c00, 0xa73d00, 0xa93e00, 0xaa3f00,
	0xab4000, 0xac4000, 0xad4100, 0xae4200,
	0xaf4300, 0xb14400, 0xb24500, 0xb34600,
	0xb44700, 0xb54800, 0xb64900, 0xb74a00,
	0xb94a00, 0xba4b00, 0xbb4c00, 0xbc4d00,
	0xbd4e00, 0xbe4f00, 0xbf5000, 0xc05100,
	0xc15200, 0xc25300, 0xc45400, 0xc55500,
	0xc65600, 0xc75700, 0xc85800, 0xc95900,
	0xca5a00, 0xcb5b00, 0xcc5c00, 0xcd5d00,
	0xce5e00, 0xcf5f00, 0xd06000, 0xd26101,
	0xd36201, 0xd46301, 0xd56401, 0xd66501,
	0xd76601, 0xd86701, 0xd96801, 0xda6901,
	0xdb6a01, 0xdc6b01, 0xdd6c01, 0xde6d01,
	0xdf6e01, 0xe06f01, 0xe17001, 0xe27201,
	0xe37301, 0xe47401, 0xe57501, 0xe67602,
	0xe77702, 0xe87802, 0xe97902, 0xeb7a02,
	0xec7b02, 0xed7c02, 0xee7e02, 0xef7f02,
	0xf08002, 0xf18103, 0xf28203, 0xf38303,
	0xf48403, 0xf58503, 0xf68703, 0xf78803,
	0xf88903, 0xf98a04, 0xfa8b04, 0xfb8c04,
	0xfc8d04, 0xfd8f04, 0xfe9005, 0xff9105,
	0xff9205, 0xff9305, 0xff9405, 0xff9606,
	0xff9706, 0xff9806, 0xff9906, 0xff9a07,
	0xff9b07, 0xff9d07, 0xff9e08, 0xff9f08,
	0xffa008, 0xffa109, 0xffa309, 0xffa409,
	0xffa50a, 0xffa60a, 0xffa80a, 0xffa90b,
	0xffaa0b, 0xffab0c, 0xffac0c, 0xffae0d,
	0xffaf0d, 0xffb00e, 0xffb10e, 0xffb30f,
	0xffb40f, 0xffb510, 0xffb610, 0xffb811,
	0xffb912, 0xffba12, 0xffbb13, 0xffbd14,
	0xffbe14, 0xffbf15, 0xffc016, 0xffc217,
	0xffc317, 0xffc418, 0xffc619, 0xffc71a,
	0xffc81b, 0xffca1c, 0xffcb1d, 0xffcc1e,
	0xffcd1f, 0xffcf20, 0xffd021, 0xffd122,
	0xffd323, 0xffd424, 0xffd526, 0xffd727,
	0xffd828, 0xffd92a, 0xffdb2b, 0xffdc2c,
	0xffdd2e, 0xffdf2f, 0xffe031, 0xffe133,
	0xffe334, 0xffe436, 0xffe538, 0xffe739
    },
    {	/* Green monitor. */
	0x000000, 0x000400, 0x000700, 0x000900,
	0x000b00, 0x000d00, 0x000f00, 0x001100,
	0x001300, 0x001500, 0x001600, 0x001800,
	0x001a00, 0x001b00, 0x001d00, 0x001e00,
	0x002000, 0x002100, 0x002300, 0x002400,
	0x002601, 0x002701, 0x002901, 0x002a01,
	0x002b01, 0x002d01, 0x002e01, 0x002f01,
	0x003101, 0x003201, 0x003301, 0x003401,
	0x003601, 0x003702, 0x003802, 0x003902,
	0x003b02, 0x003c02, 0x003d02, 0x003e02,
	0x004002, 0x004102, 0x004203, 0x004303,
	0x004403, 0x004503, 0x004703, 0x004803,
	0x004903, 0x004a03, 0x004b04, 0x004c04,
	0x004d04, 0x004e04, 0x005004, 0x005104,
	0x005205, 0x005305, 0x005405, 0x005505,
	0x005605, 0x005705, 0x005806, 0x005906,
	0x005a06, 0x005b06, 0x005d06, 0x005e07,
	0x005f07, 0x006007, 0x006107, 0x006207,
	0x006308, 0x006408, 0x006508, 0x006608,
	0x006708, 0x006809, 0x006909, 0x006a09,
	0x006b09, 0x016c0a, 0x016d0a, 0x016e0a,
	0x016f0a, 0x01700b, 0x01710b, 0x01720b,
	0x01730b, 0x01740c, 0x01750c, 0x01760c,
	0x01770c, 0x01780d, 0x01790d, 0x017a0d,
	0x017b0d, 0x017b0e, 0x017c0e, 0x017d0e,
	0x017e0f, 0x017f0f, 0x01800f, 0x018110,
	0x028210, 0x028310, 0x028410, 0x028511,
	0x028611, 0x028711, 0x028812, 0x028912,
	0x028a12, 0x028a13, 0x028b13, 0x028c13,
	0x028d14, 0x028e14, 0x038f14, 0x039015,
	0x039115, 0x039215, 0x039316, 0x039416,
	0x039417, 0x039517, 0x039617, 0x039718,
	0x049818, 0x049918, 0x049a19, 0x049b19,
	0x049c19, 0x049c1a, 0x049d1a, 0x049e1b,
	0x059f1b, 0x05a01b, 0x05a11c, 0x05a21c,
	0x05a31c, 0x05a31d, 0x05a41d, 0x06a51e,
	0x06a61e, 0x06a71f, 0x06a81f, 0x06a920,
	0x06aa20, 0x07aa21, 0x07ab21, 0x07ac21,
	0x07ad22, 0x07ae22, 0x08af23, 0x08b023,
	0x08b024, 0x08b124, 0x08b225, 0x09b325,
	0x09b426, 0x09b526, 0x09b527, 0x0ab627,
	0x0ab728, 0x0ab828, 0x0ab929, 0x0bba29,
	0x0bba2a, 0x0bbb2a, 0x0bbc2b, 0x0cbd2b,
	0x0cbe2c, 0x0cbf2c, 0x0dbf2d, 0x0dc02d,
	0x0dc12e, 0x0ec22e, 0x0ec32f, 0x0ec42f,
	0x0fc430, 0x0fc530, 0x0fc631, 0x10c731,
	0x10c832, 0x10c932, 0x11c933, 0x11ca33,
	0x11cb34, 0x12cc35, 0x12cd35, 0x12cd36,
	0x13ce36, 0x13cf37, 0x13d037, 0x14d138,
	0x14d139, 0x14d239, 0x15d33a, 0x15d43a,
	0x16d43b, 0x16d53b, 0x17d63c, 0x17d73d,
	0x17d83d, 0x18d83e, 0x18d93e, 0x19da3f,
	0x19db40, 0x1adc40, 0x1adc41, 0x1bdd41,
	0x1bde42, 0x1cdf43, 0x1ce043, 0x1de044,
	0x1ee145, 0x1ee245, 0x1fe346, 0x1fe446,
	0x20e447, 0x20e548, 0x21e648, 0x22e749,
	0x22e74a, 0x23e84a, 0x23e94b, 0x24ea4c,
	0x25ea4c, 0x25eb4d, 0x26ec4e, 0x27ed4e,
	0x27ee4f, 0x28ee50, 0x29ef50, 0x29f051,
	0x2af152, 0x2bf153, 0x2cf253, 0x2cf354,
	0x2df455, 0x2ef455, 0x2ff556, 0x2ff657,
	0x30f758, 0x31f758, 0x32f859, 0x32f95a,
	0x33fa5a, 0x34fa5b, 0x35fb5c, 0x36fc5d,
	0x37fd5d, 0x38fd5e, 0x38fe5f, 0x39ff60
    },
    {	/* White monitor. */
	0x000000, 0x010102, 0x020203, 0x020304,
	0x030406, 0x040507, 0x050608, 0x060709,
	0x07080a, 0x08090c, 0x080a0d, 0x090b0e,
	0x0a0c0f, 0x0b0d10, 0x0c0e11, 0x0d0f12,
	0x0e1013, 0x0f1115, 0x101216, 0x111317,
	0x121418, 0x121519, 0x13161a, 0x14171b,
	0x15181c, 0x16191d, 0x171a1e, 0x181b1f,
	0x191c20, 0x1a1d21, 0x1b1e22, 0x1c1f23,
	0x1d2024, 0x1e2125, 0x1f2226, 0x202327,
	0x212428, 0x222529, 0x22262b, 0x23272c,
	0x24282d, 0x25292e, 0x262a2f, 0x272b30,
	0x282c30, 0x292d31, 0x2a2e32, 0x2b2f33,
	0x2c3034, 0x2d3035, 0x2e3136, 0x2f3237,
	0x303338, 0x313439, 0x32353a, 0x33363b,
	0x34373c, 0x35383d, 0x36393e, 0x373a3f,
	0x383b40, 0x393c41, 0x3a3d42, 0x3b3e43,
	0x3c3f44, 0x3d4045, 0x3e4146, 0x3f4247,
	0x404348, 0x414449, 0x42454a, 0x43464b,
	0x44474c, 0x45484d, 0x46494d, 0x474a4e,
	0x484b4f, 0x484c50, 0x494d51, 0x4a4e52,
	0x4b4f53, 0x4c5054, 0x4d5155, 0x4e5256,
	0x4f5357, 0x505458, 0x515559, 0x52565a,
	0x53575b, 0x54585b, 0x55595c, 0x565a5d,
	0x575b5e, 0x585c5f, 0x595d60, 0x5a5e61,
	0x5b5f62, 0x5c6063, 0x5d6164, 0x5e6265,
	0x5f6366, 0x606466, 0x616567, 0x626668,
	0x636769, 0x64686a, 0x65696b, 0x666a6c,
	0x676b6d, 0x686c6e, 0x696d6f, 0x6a6e70,
	0x6b6f70, 0x6c7071, 0x6d7172, 0x6f7273,
	0x707374, 0x707475, 0x717576, 0x727677,
	0x747778, 0x757879, 0x767979, 0x777a7a,
	0x787b7b, 0x797c7c, 0x7a7d7d, 0x7b7e7e,
	0x7c7f7f, 0x7d8080, 0x7e8181, 0x7f8281,
	0x808382, 0x818483, 0x828584, 0x838685,
	0x848786, 0x858887, 0x868988, 0x878a89,
	0x888b89, 0x898c8a, 0x8a8d8b, 0x8b8e8c,
	0x8c8f8d, 0x8d8f8e, 0x8e908f, 0x8f9190,
	0x909290, 0x919391, 0x929492, 0x939593,
	0x949694, 0x959795, 0x969896, 0x979997,
	0x989a98, 0x999b98, 0x9a9c99, 0x9b9d9a,
	0x9c9e9b, 0x9d9f9c, 0x9ea09d, 0x9fa19e,
	0xa0a29f, 0xa1a39f, 0xa2a4a0, 0xa3a5a1,
	0xa4a6a2, 0xa6a7a3, 0xa7a8a4, 0xa8a9a5,
	0xa9aaa5, 0xaaaba6, 0xabaca7, 0xacada8,
	0xadaea9, 0xaeafaa, 0xafb0ab, 0xb0b1ac,
	0xb1b2ac, 0xb2b3ad, 0xb3b4ae, 0xb4b5af,
	0xb5b6b0, 0xb6b7b1, 0xb7b8b2, 0xb8b9b2,
	0xb9bab3, 0xbabbb4, 0xbbbcb5, 0xbcbdb6,
	0xbdbeb7, 0xbebfb8, 0xbfc0b8, 0xc0c1b9,
	0xc1c2ba, 0xc2c3bb, 0xc3c4bc, 0xc5c5bd,
	0xc6c6be, 0xc7c7be, 0xc8c8bf, 0xc9c9c0,
	0xcacac1, 0xcbcbc2, 0xccccc3, 0xcdcdc3,
	0xcecec4, 0xcfcfc5, 0xd0d0c6, 0xd1d1c7,
	0xd2d2c8, 0xd3d3c9, 0xd4d4c9, 0xd5d5ca,
	0xd6d6cb, 0xd7d7cc, 0xd8d8cd, 0xd9d9ce,
	0xdadacf, 0xdbdbcf, 0xdcdcd0, 0xdeddd1,
	0xdfded2, 0xe0dfd3, 0xe1e0d4, 0xe2e1d4,
	0xe3e2d5, 0xe4e3d6, 0xe5e4d7, 0xe6e5d8,
	0xe7e6d9, 0xe8e7d9, 0xe9e8da, 0xeae9db,
	0xebeadc, 0xecebdd, 0xedecde, 0xeeeddf,
	0xefeedf, 0xf0efe0, 0xf1f0e1, 0xf2f1e2,
	0xf3f2e3, 0xf4f3e3, 0xf6f3e4, 0xf7f4e5,
	0xf8f5e6, 0xf9f6e7, 0xfaf7e8, 0xfbf8e9,
	0xfcf9e9, 0xfdfaea, 0xfefbeb, 0xfffcec
    }
};
static uint32_t	cga_2_table[16];
static uint8_t	rotatevga[8][256];
static int	video_force_resize;
static int	video_card_type;
static const video_timings_t *video_timing;


static struct blitter {
    int		x, y, y1, y2, w, h;

    volatile int busy;
    event_t	*busy_ev;

    volatile int inuse;
    event_t	*inuse_ev;

    thread_t	*thread;
    event_t	*wake_ev;

    void	(*func)(bitmap_t *,int x, int y, int y1, int y2, int w, int h);
}		video_blit;


static void
blit_thread(void *param)
{
    struct blitter *blit = (struct blitter *)param;

    for (;;) {
	thread_wait_event(blit->wake_ev, -1);
	thread_reset_event(blit->wake_ev);

	if (blit->func != NULL)
		blit->func(screen, blit->x, blit->y,
			   blit->y1, blit->y2, blit->w, blit->h);

	blit->busy = 0;
	thread_set_event(blit->busy_ev);
    }
}


/* Set address of renderer blit function. */
void
video_blit_set(void(*blit)(bitmap_t *,int,int,int,int,int,int))
{
    video_blit.func = blit;
}


/* Renderer blit function is done. */
void
video_blit_done(void)
{
    video_blit.inuse = 0;

    thread_set_event(video_blit.inuse_ev);
}


void
video_blit_wait(void)
{
    while (video_blit.busy)
	thread_wait_event(video_blit.busy_ev, -1);

    thread_reset_event(video_blit.busy_ev);
}


void
video_blit_wait_buffer(void)
{
    while (video_blit.inuse)
	thread_wait_event(video_blit.inuse_ev, -1);

    thread_reset_event(video_blit.inuse_ev);
}


static uint8_t
pixels8(pel_t *pixels)
{
    uint8_t temp = 0;
    int i;

    for (i = 0; i < 8; i++)
	temp |= (!!pixels[i].val << (i ^ 7));

    return temp;
}


static uint32_t
pixel_to_color(uint8_t *pixels32, uint8_t pos)
{
    uint32_t temp;

    temp = *(pixels32 + pos) & 0x03;

    switch (temp) {
	case 0:
		return 0;

	case 1:
		return 0x07;

	case 2:
		return 0x0f;

	default:
		break;
    }

    return 0;
}


void
video_blend(int x, int y)
{
    static unsigned int carry = 0;
    uint32_t pixels32_1, pixels32_2;
    unsigned int val1, val2;
    int xx;

    if (x == 0)
	carry = 0;

    val1 = pixels8(&screen->line[y][x]);
    val2 = (val1 >> 1) + carry;
    carry = (val1 & 1) << 7;

    pixels32_1 = cga_2_table[val1 >> 4] + cga_2_table[val2 >> 4];
    pixels32_2 = cga_2_table[val1 & 0xf] + cga_2_table[val2 & 0xf];

    for (xx = 0; xx < 4; xx++) {
	screen->line[y][x + xx].val = pixel_to_color((uint8_t *)&pixels32_1, xx);
	screen->line[y][x + (xx | 4)].val = pixel_to_color((uint8_t *)&pixels32_2, xx);
    }
}


/* Set up the blitter, and then wake it up to process the screen buffer. */
void
video_blit_start(int pal, int x, int y, int y1, int y2, int w, int h)
{
    int yy, xx;
    uint32_t val;
    pel_t *p;

    if (h <= 0) return;

    if (pal) {
	/* In palette mode, first convert the values. */
	for (yy = 0; yy < h; yy++) {
		if ((y + yy) >= 0 && (y + yy) < screen->h) {
			for (xx = 0; xx < w; xx++) {
				p = &screen->line[y + yy][x + xx];
				val = pal_lookup[p->pal];
				p->val = val;
			}
		}
	}
    }

    /* Wait for access to the blitter. */
    video_blit_wait();

    video_blit.busy = 1;
    video_blit.inuse = 1;

    video_blit.x = x;
    video_blit.y = y;
    video_blit.y1 = y1;
    video_blit.y2 = y2;
    video_blit.w = w;
    video_blit.h = h;

    /* Wake up the blitter. */
    thread_set_event(video_blit.wake_ev);
}


void
video_palette_rebuild(void)
{
    int c;

    /* We cannot do this (yet) if we have not been enabled yet. */
    if (video_6to8 == NULL) return;

    for (c = 0; c < 256; c++) {
	pal_lookup[c] = makecol(video_6to8[cgapal[c].r],
			        video_6to8[cgapal[c].g],
			        video_6to8[cgapal[c].b]);
    }

    if ((cga_palette > 1) && (cga_palette < 8)) {
	if (vid_cga_contrast != 0) {
		for (c = 0; c < 16; c++) {
			pal_lookup[c] = makecol(video_6to8[cgapal_mono[cga_palette - 2][c].r],
						video_6to8[cgapal_mono[cga_palette - 2][c].g],
						video_6to8[cgapal_mono[cga_palette - 2][c].b]);
			pal_lookup[c+16] = makecol(video_6to8[cgapal_mono[cga_palette - 2][c].r],
						   video_6to8[cgapal_mono[cga_palette - 2][c].g],
						   video_6to8[cgapal_mono[cga_palette - 2][c].b]);
			pal_lookup[c+32] = makecol(video_6to8[cgapal_mono[cga_palette - 2][c].r],
						   video_6to8[cgapal_mono[cga_palette - 2][c].g],
						   video_6to8[cgapal_mono[cga_palette - 2][c].b]);
			pal_lookup[c+48] = makecol(video_6to8[cgapal_mono[cga_palette - 2][c].r],
						   video_6to8[cgapal_mono[cga_palette - 2][c].g],
						   video_6to8[cgapal_mono[cga_palette - 2][c].b]);
		}
	} else {
		for (c = 0; c < 16; c++) {
			pal_lookup[c] = makecol(video_6to8[cgapal_mono[cga_palette - 1][c].r],
						video_6to8[cgapal_mono[cga_palette - 1][c].g],
						video_6to8[cgapal_mono[cga_palette - 1][c].b]);
			pal_lookup[c+16] = makecol(video_6to8[cgapal_mono[cga_palette - 1][c].r],
						   video_6to8[cgapal_mono[cga_palette - 1][c].g],
						   video_6to8[cgapal_mono[cga_palette - 1][c].b]);
			pal_lookup[c+32] = makecol(video_6to8[cgapal_mono[cga_palette - 1][c].r],
						   video_6to8[cgapal_mono[cga_palette - 1][c].g],
						   video_6to8[cgapal_mono[cga_palette - 1][c].b]);
			pal_lookup[c+48] = makecol(video_6to8[cgapal_mono[cga_palette - 1][c].r],
						   video_6to8[cgapal_mono[cga_palette - 1][c].g],
						   video_6to8[cgapal_mono[cga_palette - 1][c].b]);
		}
	}
    }

    if (cga_palette == 8)
	pal_lookup[0x16] = makecol(video_6to8[42],video_6to8[42],video_6to8[0]);
}


static int
calc_6to8(int c)
{
    int ic, i8;
    double d8;

    ic = c;
    if (ic == 64)
	ic = 63;
      else
	ic &= 0x3f;
    d8 = (ic / 63.0) * 255.0;
    i8 = (int) d8;

    return(i8 & 0xff);
}


static int
calc_15to32(int c)
{
    int b, g, r;
    double d_b, d_g, d_r;

    b = (c & 31);
    g = ((c >> 5) & 31);
    r = ((c >> 10) & 31);
    d_b = (((double) b) / 31.0) * 255.0;
    d_g = (((double) g) / 31.0) * 255.0;
    d_r = (((double) r) / 31.0) * 255.0;
    b = (int) d_b;
    g = ((int) d_g) << 8;
    r = ((int) d_r) << 16;

    return(b | g | r);
}


static int
calc_16to32(int c)
{
    int b, g, r;
    double d_b, d_g, d_r;

    b = (c & 31);
    g = ((c >> 5) & 63);
    r = ((c >> 11) & 31);
    d_b = (((double) b) / 31.0) * 255.0;
    d_g = (((double) g) / 63.0) * 255.0;
    d_r = (((double) r) / 31.0) * 255.0;
    b = (int) d_b;
    g = ((int) d_g) << 8;
    r = ((int) d_r) << 16;

    return(b | g | r);
}


static void
destroy_bitmap(bitmap_t *b)
{
    if (b->pels != NULL)
	free(b->pels);

    free(b);
}


static bitmap_t *
create_bitmap(int x, int y)
{
    bitmap_t *b;
    int c;

    b = (bitmap_t *)mem_alloc(sizeof(bitmap_t) + (y * sizeof(pel_t *)));
    b->w = x;
    b->h = y;

    b->pels = (pel_t *)mem_alloc(x * y * sizeof(pel_t));
    for (c = 0; c < y; c++)
	b->line[c] = &b->pels[c * x];

    return(b);
}


#ifdef _LOGGING
void
video_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_VIDEO_LOG
    va_list ap;

    if (video_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
video_init(void)
{
    uint8_t total[2] = { 0, 1 };
    int c, d, e;

    /* Initialize video type and timing. */
    video_card_type = -1;
    video_timing = &timing_default;

    for (c = 0; c < 16; c++) {
	cga_2_table[c] = (total[(c >> 3) & 1] << 0 ) |
			 (total[(c >> 2) & 1] << 8 ) |
			 (total[(c >> 1) & 1] << 16) |
			 (total[(c >> 0) & 1] << 24);
    }

    for (c = 0; c < 64; c++) {
	cgapal[c + 64].r = (((c & 4) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
	cgapal[c + 64].g = (((c & 2) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
	cgapal[c + 64].b = (((c & 1) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
	if ((c & 0x17) == 6) 
		cgapal[c + 64].g >>= 1;
    }
    for (c = 0; c < 64; c++) {
	cgapal[c + 128].r = (((c & 4) ? 2 : 0) | ((c & 0x20) ? 1 : 0)) * 21;
	cgapal[c + 128].g = (((c & 2) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
	cgapal[c + 128].b = (((c & 1) ? 2 : 0) | ((c & 0x08) ? 1 : 0)) * 21;
    }

    for (c = 0; c < 256; c++) {
	e = c;
	for (d = 0; d < 8; d++) {
		rotatevga[d][c] = e;
		e = (e >> 1) | ((e & 1) ? 0x80 : 0);
	}
    }

    for (c = 0; c < 4; c++) {
	for (d = 0; d < 4; d++) {
		edatlookup[c][d] = 0;
		if (c & 1) edatlookup[c][d] |= 1;
		if (d & 1) edatlookup[c][d] |= 2;
		if (c & 2) edatlookup[c][d] |= 0x10;
		if (d & 2) edatlookup[c][d] |= 0x20;
	}
    }

    video_6to8 = (uint32_t *)mem_alloc(4 * 256);
    for (c = 0; c < 256; c++)
	video_6to8[c] = calc_6to8(c);
    video_15to32 = (uint32_t *)mem_alloc(4 * 65536);
    for (c = 0; c < 65536; c++)
	video_15to32[c] = calc_15to32(c);

    video_16to32 = (uint32_t *)mem_alloc(4 * 65536);
    for (c = 0; c < 65536; c++)
	video_16to32[c] = calc_16to32(c);

    /* Create the screen buffer. */
    screen = create_bitmap(2048, 2048);

    video_blit.wake_ev = thread_create_event();
    video_blit.busy_ev = thread_create_event();
    video_blit.inuse_ev = thread_create_event();
    video_blit.thread = thread_create(blit_thread, &video_blit);
}


void
video_close(void)
{
    thread_kill(video_blit.thread);
    thread_destroy_event(video_blit.inuse_ev);
    thread_destroy_event(video_blit.busy_ev);
    thread_destroy_event(video_blit.wake_ev);

    free(video_6to8);
    free(video_15to32);
    free(video_16to32);

    destroy_bitmap(screen);

    video_reset_font();

    /* Close up. */
    video_card_type = -1;
    video_timing = &timing_default;
}


void
video_reset(void)
{
    const device_t *dev;

    INFO("VIDEO: reset (video_card=%i, internal=%i)\n",
       	 video_card, (machine->flags & MACHINE_VIDEO) ? 1 : 0);

    /* Initialize the video font tables. */
    video_load_font(MDA_FONT_ROM_PATH, FONT_MDA);

    /* Do not initialize internal cards here. */
    if ((video_card == VID_NONE) || \
	(video_card == VID_INTERNAL) || \
	(machine->flags_fixed & MACHINE_VIDEO)) return;

    /* Configure default timing parameters for the card. */
    video_inform(VID_TYPE_SPEC, NULL);

    /* Reset the CGA palette. */
    cga_palette = 0;
    video_palette_rebuild();

    /* Clear (deallocate) any video font memory. */
    video_reset_font();

    dev = video_card_getdevice(video_card);
    if (dev != NULL)
	device_add(dev);

    /* Enable the Voodoo if configured. */
    if (voodoo_enabled)
       	device_add(&voodoo_device);
}


/* Inform the video module what type a card is, and what its timings are. */
void
video_inform(int type, const video_timings_t *ptr)
{
    /* Save the video card type. */
    video_card_type = (type == VID_TYPE_DFLT) ? VID_TYPE_SPEC : type;

    /* Save the card's timing parameters. */
    video_timing = (ptr == NULL) ? &timing_default : ptr;

    /* Make sure these are used. */
    video_update_timing();

    if (type != VID_TYPE_DFLT)
        DEBUG("VIDEO: card type %i, timings {%i: %i,%i,%i %i,%i,%i}\n",
	      video_card_type, video_timing->type,
	      video_timing->write_b,video_timing->write_w,video_timing->write_l,
	      video_timing->read_b,video_timing->read_w,video_timing->read_l);
}


/* Return the current video card's type. */
int
video_type(void)
{
    const device_t *dev;
    int type = -1;

    if (video_card_type == -1) {
	/* No video device loaded yet. */
	dev = video_card_getdevice(video_card);
	if (dev != NULL)
		type = DEVICE_VIDEO_GET(dev->flags);
    } else
	type = video_card_type;

    return(type);
}


/* Called by PIT to (re-)set our timings. */
void
video_update_timing(void)
{
    /* Update the video timing paramaters. */
    if (video_timing->type == VID_ISA) {
	video_timing_read_b = ISA_CYCLES(video_timing->read_b);
	video_timing_read_w = ISA_CYCLES(video_timing->read_w);
	video_timing_read_l = ISA_CYCLES(video_timing->read_l);
	video_timing_write_b = ISA_CYCLES(video_timing->write_b);
	video_timing_write_w = ISA_CYCLES(video_timing->write_w);
	video_timing_write_l = ISA_CYCLES(video_timing->write_l);
    } else {
	video_timing_read_b = (int)(bus_timing * video_timing->read_b);
	video_timing_read_w = (int)(bus_timing * video_timing->read_w);
	video_timing_read_l = (int)(bus_timing * video_timing->read_l);
	video_timing_write_b = (int)(bus_timing * video_timing->write_b);
	video_timing_write_w = (int)(bus_timing * video_timing->write_w);
	video_timing_write_l = (int)(bus_timing * video_timing->write_l);
    }

    /* Fix up for 16-bit buses. */
    if (cpu_16bitbus) {
	video_timing_read_l = video_timing_read_w * 2;
	video_timing_write_l = video_timing_write_w * 2;
    }
}


/* Zap any font memory allocated previously. */
void
video_reset_font(void)
{
    if (fontdatk != NULL) {
	free(fontdatk);
	fontdatk = NULL;
    }

    if (fontdatk_user != NULL) {
	free(fontdatk_user);
	fontdatk_user = NULL;
    }
}


/* Load a font from its ROM source. */
void
video_load_font(const wchar_t *fn, fontformat_t num)
{
    FILE *fp;
    int c;

    fp = plat_fopen(rom_path(fn), L"rb");
    if (fp == NULL) {
	ERRLOG("VIDEO: cannot load font '%ls', fmt=%i\n", fn, num);
	return;
    }

    switch (num) {
	case FONT_MDA:		/* MDA */
		for (c = 0; c < 256; c++)
                       	(void)fread(&fontdatm[c][0], 1, 8, fp);
		for (c = 0; c < 256; c++)
                       	(void)fread(&fontdatm[c][8], 1, 8, fp);
		break;

	case FONT_CGA_THIN:	/* CGA, thin font */
	case FONT_CGA_THICK:	/* CGA, thick font */
		if (num == FONT_CGA_THICK) {
			/* Use the second ("thick") font in the ROM. */
			(void)fseek(fp, 2048, SEEK_SET);
		}
		for (c = 0; c < 256; c++)
                       	(void)fread(&fontdat[c][0], 1, 8, fp);
		break;
    }

    DEBUG("VIDEO: font #%i loaded from %ls\n", num, fn);

    (void)fclose(fp);
}


uint8_t
video_force_resize_get(void)
{
    return video_force_resize;
}


void
video_force_resize_set(uint8_t res)
{
    video_force_resize = res;
}


uint32_t
video_color_transform(uint32_t color)
{
    uint8_t *clr8 = (uint8_t *)&color;

#if 0
    if (!vid_grayscale && !invert_display) return color;
#endif

    if (vid_grayscale) {
	if (vid_graytype) {
		if (vid_graytype == 1)
			color = ((54 * (uint32_t)clr8[2]) + (183 * (uint32_t)clr8[1]) + (18 * (uint32_t)clr8[0])) / 255;
		else
			color = ((uint32_t)clr8[2] + (uint32_t)clr8[1] + (uint32_t)clr8[0]) / 3;
	} else
		color = ((76 * (uint32_t)clr8[2]) + (150 * (uint32_t)clr8[1]) + (29 * (uint32_t)clr8[0])) / 255;

	switch (vid_grayscale) {
		case 2:
		case 3:
		case 4:
			color = (uint32_t)shade[vid_grayscale][color];
			break;

		default:
			clr8[3] = 0;
			clr8[0] = color;
			clr8[1] = clr8[2] = clr8[0];
			break;
	}
    }

    if (invert_display)
	color ^= 0x00ffffff;

    return color;
}


void
video_transform_copy(uint32_t *dst, pel_t *src, int len)
{
    int i;

    for (i = 0; i < len; i++)
	*dst++ = video_color_transform(src[i].val);
}
