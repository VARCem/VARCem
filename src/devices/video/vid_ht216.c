/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Video7 VGA 1024i emulation.
 *
 * Version:	@(#)vid_ht216.c	1.0.7	2020/01/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2019,2020 Fred N. van Kempen.
 *		Copyright 2019 Miran Grca.
 *		Copyright 2019 Sarah Walker.
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
#include <wchar.h>
#include "../../emu.h"
#include "../../config.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"


#define BIOS_VIDEO7_VGA_1024I_PATH	L"video/video7/video seven vga 1024i - bios - v2.19 - 435-0062-05 - u17 - 27c256.bin"


#define HT_MISC_PAGE_SEL (1 << 5)


/*Shifts CPU VRAM read address by 3 bits, for use with fat pixel colour expansion*/
#define HT_REG_C8_MOVSB (1 << 0)
#define HT_REG_C8_E256  (1 << 4)
#define HT_REG_C8_XLAM  (1 << 6)

#define HT_REG_CD_FP8PCEXP (1 << 1)
#define HT_REG_CD_BMSKSL   (3 << 2)
#define HT_REG_CD_RMWMDE   (1 << 5)
/*Use GDC data rotate as offset when reading VRAM data into latches*/
#define HT_REG_CD_ASTODE   (1 << 6)
#define HT_REG_CD_EXALU    (1 << 7)

#define HT_REG_E0_SBAE  (1 << 7)

#define HT_REG_F9_XPSEL (1 << 0)

/*Enables A[14:15] of VRAM address in chain-4 modes*/
#define HT_REG_FC_ECOLRE (1 << 2)

#define HT_REG_FE_FBRC  (1 << 1)
#define HT_REG_FE_FBMC  (3 << 2)
#define HT_REG_FE_FBRSL (3 << 4)


typedef struct {
    svga_t	svga;

    mem_map_t	linear_mapping;

    rom_t	bios_rom;

    uint32_t	vram_mask;

    int		ext_reg_enable;
    int		clk_sel;

    uint8_t	read_bank_reg[2],
		write_bank_reg[2];
    uint32_t	read_bank[2],
		write_bank[2];
    uint8_t	misc,
		pad;
    uint16_t	id;

    uint8_t	bg_latch[8];

    uint8_t	ht_regs[256];
} ht216_t;


static const video_timings_t	v7vga_timings = {VID_ISA,5,5,9,20,20,30};


static void
ht216_remap(ht216_t *dev)
{
    svga_t *svga = &dev->svga;

    mem_map_disable(&dev->linear_mapping);

    if (dev->ht_regs[0xc8] & HT_REG_C8_XLAM) {
	uint32_t linear_base = ((dev->ht_regs[0xc9] & 0xf) << 20) | (dev->ht_regs[0xcf] << 24);

	mem_map_set_addr(&dev->linear_mapping, linear_base, 0x100000);


	/*Linear mapping enabled*/
    } else {
	uint8_t read_bank_reg[2] = {
		dev->read_bank_reg[0], dev->read_bank_reg[1]
	};
	uint8_t write_bank_reg[2] = {
		dev->write_bank_reg[0], dev->write_bank_reg[1]
	};

	if (!svga->chain4 || !(dev->ht_regs[0xfc] & HT_REG_FC_ECOLRE)) {
		read_bank_reg[0] &= ~0x30;
		read_bank_reg[1] &= ~0x30;
		write_bank_reg[0] &= ~0x30;
		write_bank_reg[1] &= ~0x30;
	}

	dev->read_bank[0] = read_bank_reg[0] << 12;
	dev->write_bank[0] = write_bank_reg[0] << 12;
	if (dev->ht_regs[0xe0] & HT_REG_E0_SBAE) {
		/* split bank */
		dev->read_bank[1] = read_bank_reg[1] << 12;
		dev->write_bank[1] = write_bank_reg[1] << 12;
	} else {
		dev->read_bank[1] = dev->read_bank[0] + (svga->chain4 ? 0x8000 : 0x20000);
		dev->write_bank[1] = dev->write_bank[0] + (svga->chain4 ? 0x8000 : 0x20000);
	}

	if (! svga->chain4) {
		dev->read_bank[0] >>= 2;
		dev->read_bank[1] >>= 2;
		dev->write_bank[0] >>= 2;
		dev->write_bank[1] >>= 2;
	}
    }
}


static void
recalc_timings(svga_t *svga)
{
    ht216_t *dev = (ht216_t *)svga->p;

    switch (dev->clk_sel) {
	case 5:
		svga->clock = (cpu_clock * (double)(1ULL << 32)) / 65000000.0;
		break;

	case 6:
		svga->clock = (cpu_clock * (double)(1ULL << 32)) / 40000000.0;
		break;

	case 10:
		svga->clock = (cpu_clock * (double)(1ULL << 32)) / 80000000.0;
		break;
    }

    svga->lowres = !(dev->ht_regs[0xc8] & HT_REG_C8_E256);
    svga->ma_latch |= ((dev->ht_regs[0xf6] & 0x30) << 12);
    svga->interlace = dev->ht_regs[0xe0] & 1;

    if ((svga->bpp == 8) && !svga->lowres)
	svga->render = svga_render_8bpp_highres;
}


static void
hwcursor_draw(svga_t *svga, int displine)
{
    int offset = svga->hwcursor_latch.x + 32;
    uint32_t dat[2];
    int x;

    if (svga->interlace && svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += 4;

    dat[0] = (svga->vram[svga->hwcursor_latch.addr]   << 24) |
	     (svga->vram[svga->hwcursor_latch.addr+1] << 16) |
	     (svga->vram[svga->hwcursor_latch.addr+2] <<  8) |
	      svga->vram[svga->hwcursor_latch.addr+3];
    dat[1] = (svga->vram[svga->hwcursor_latch.addr+128]   << 24) |
	     (svga->vram[svga->hwcursor_latch.addr+128+1] << 16) |
	     (svga->vram[svga->hwcursor_latch.addr+128+2] <<  8) |
	      svga->vram[svga->hwcursor_latch.addr+128+3];

    for (x = 0; x < 32; x++) {
	if (! (dat[0] & 0x80000000))
		screen->line[displine][offset + x].val = 0;
	if (dat[1] & 0x80000000)
		screen->line[displine][offset + x].val ^= 0xffffff;

	dat[0] <<= 1;
	dat[1] <<= 1;
    }

    svga->hwcursor_latch.addr += 4;
    if (svga->interlace && !svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += 4;
}


static __inline uint8_t
extalu(int op, uint8_t input_a, uint8_t input_b)
{
    uint8_t val;

    switch (op) {
	case 0x0: val = 0;                    break;
	case 0x1: val = ~(input_a | input_b); break;
	case 0x2: val = input_a & ~input_b;   break;
	case 0x3: val = ~input_b;             break;
	case 0x4: val = ~input_a & input_b;   break;
	case 0x5: val = ~input_a;             break;
	case 0x6: val = input_a ^ input_b;    break;
	case 0x7: val = ~(input_a & input_b); break;
	case 0x8: val = input_a & input_b;    break;
	case 0x9: val = ~(input_a ^ input_b); break;
	case 0xa: val = input_a;              break;
	case 0xb: val = input_a | ~input_b;   break;
	case 0xc: val = input_b;              break;
	case 0xd: val = ~input_a | input_b;   break;
	case 0xe: val = input_a | input_b;    break;
	case 0xf: default: val = 0xff;        break;
    }

    return val;
}


static void
dm_write(ht216_t *dev, uint32_t addr, uint8_t cpu_dat, uint8_t cpu_dat_unexpanded)
{
    svga_t *svga = &dev->svga;
    uint8_t vala, valb, valc, vald, wm = svga->writemask;
    int writemask2 = svga->writemask;
    uint8_t fg_data[4] = {0, 0, 0, 0};

    if (! (svga->gdcreg[6] & 1))
	svga->fullchange = 2;
    if (svga->chain4 || svga->fb_only) {
	writemask2 = 1 << (addr & 3);
	addr &= ~3;
    } else if (svga->chain2_write) {
	writemask2 &= ~0xa;
	if (addr & 1)
		writemask2 <<= 1;
	addr &= ~1;
	addr <<= 2;
    } else
	addr <<= 2;
    if (addr >= svga->vram_max)
	return;

    svga->changedvram[addr >> 12] = changeframecount;

    switch (dev->ht_regs[0xfe] & HT_REG_FE_FBMC) {
	case 0x00:
		fg_data[0] = fg_data[1] = fg_data[2] = fg_data[3] = cpu_dat;
		break;

	case 0x04:
		if (dev->ht_regs[0xfe] & HT_REG_FE_FBRC) {
			if (addr & 4) {
				fg_data[0] = (cpu_dat_unexpanded & (1 << (((addr + 4) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[1] = (cpu_dat_unexpanded & (1 << (((addr + 5) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[2] = (cpu_dat_unexpanded & (1 << (((addr + 6) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[3] = (cpu_dat_unexpanded & (1 << (((addr + 7) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
			} else {
				fg_data[0] = (cpu_dat_unexpanded & (1 << (((addr + 0) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[1] = (cpu_dat_unexpanded & (1 << (((addr + 1) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[2] = (cpu_dat_unexpanded & (1 << (((addr + 2) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[3] = (cpu_dat_unexpanded & (1 << (((addr + 3) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
			}
		} else {
			if (addr & 4) {
				fg_data[0] = (dev->ht_regs[0xf5] & (1 << (((addr + 4) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[1] = (dev->ht_regs[0xf5] & (1 << (((addr + 5) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[2] = (dev->ht_regs[0xf5] & (1 << (((addr + 6) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[3] = (dev->ht_regs[0xf5] & (1 << (((addr + 7) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
			} else {
				fg_data[0] = (dev->ht_regs[0xf5] & (1 << (((addr + 0) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[1] = (dev->ht_regs[0xf5] & (1 << (((addr + 1) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[2] = (dev->ht_regs[0xf5] & (1 << (((addr + 2) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
				fg_data[3] = (dev->ht_regs[0xf5] & (1 << (((addr + 3) & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
			}
		}
		break;

	case 0x08:
		fg_data[0] = dev->ht_regs[0xec];
		fg_data[1] = dev->ht_regs[0xed];
		fg_data[2] = dev->ht_regs[0xee];
		fg_data[3] = dev->ht_regs[0xef];
		break;

	case 0x0c:
		fg_data[0] = dev->ht_regs[0xec];
		fg_data[1] = dev->ht_regs[0xed];
		fg_data[2] = dev->ht_regs[0xee];
		fg_data[3] = dev->ht_regs[0xef];
		break;
    }

    switch (svga->writemode) {
	case 1:
		if (writemask2 & 1) svga->vram[addr]       = svga->latch.b[0];
		if (writemask2 & 2) svga->vram[addr | 0x1] = svga->latch.b[1];
		if (writemask2 & 4) svga->vram[addr | 0x2] = svga->latch.b[2];
		if (writemask2 & 8) svga->vram[addr | 0x3] = svga->latch.b[3];
		break;

	case 0:
		if (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) &&
		    (!svga->gdcreg[1] || svga->set_reset_disabled)) {
			if (writemask2 & 1) svga->vram[addr]       = fg_data[0];
			if (writemask2 & 2) svga->vram[addr | 0x1] = fg_data[1];
			if (writemask2 & 4) svga->vram[addr | 0x2] = fg_data[2];
			if (writemask2 & 8) svga->vram[addr | 0x3] = fg_data[3];
		} else {
			if (svga->gdcreg[1] & 1) vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
			else                     vala = fg_data[0];
			if (svga->gdcreg[1] & 2) valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
			else                     valb = fg_data[1];
			if (svga->gdcreg[1] & 4) valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
			else                     valc = fg_data[2];
			if (svga->gdcreg[1] & 8) vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
			else                     vald = fg_data[3];

			switch (svga->gdcreg[3] & 0x18) {
				case 0: /* set */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | (svga->latch.b[0] & ~svga->gdcreg[8]);
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->latch.b[1] & ~svga->gdcreg[8]);
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->latch.b[2] & ~svga->gdcreg[8]);
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->latch.b[3] & ~svga->gdcreg[8]);
					break;

				case 8: /* AND */
					if (writemask2 & 1) svga->vram[addr]       = (vala | ~svga->gdcreg[8]) & svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->latch.b[3];
					break;

				case 0x10: /* OR */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->latch.b[3];
					break;

				case 0x18: /* XOR */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) ^ svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->latch.b[3];
					break;
			}
		}
		break;

	case 2:
		if (!(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
			if (writemask2 & 1) svga->vram[addr]       = (((cpu_dat & 1) ? 0xff : 0) & svga->gdcreg[8]) | (svga->latch.b[0] & ~svga->gdcreg[8]);
			if (writemask2 & 2) svga->vram[addr | 0x1] = (((cpu_dat & 2) ? 0xff : 0) & svga->gdcreg[8]) | (svga->latch.b[1] & ~svga->gdcreg[8]);
			if (writemask2 & 4) svga->vram[addr | 0x2] = (((cpu_dat & 4) ? 0xff : 0) & svga->gdcreg[8]) | (svga->latch.b[2] & ~svga->gdcreg[8]);
			if (writemask2 & 8) svga->vram[addr | 0x3] = (((cpu_dat & 8) ? 0xff : 0) & svga->gdcreg[8]) | (svga->latch.b[3] & ~svga->gdcreg[8]);
		} else {
			vala = ((cpu_dat & 1) ? 0xff : 0);
			valb = ((cpu_dat & 2) ? 0xff : 0);
			valc = ((cpu_dat & 4) ? 0xff : 0);
			vald = ((cpu_dat & 8) ? 0xff : 0);
			switch (svga->gdcreg[3] & 0x18) {
				case 0: /* set */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | (svga->latch.b[0] & ~svga->gdcreg[8]);
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->latch.b[1] & ~svga->gdcreg[8]);
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->latch.b[2] & ~svga->gdcreg[8]);
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->latch.b[3] & ~svga->gdcreg[8]);
					break;

				case 8: /* AND */
					if (writemask2 & 1) svga->vram[addr]       = (vala | ~svga->gdcreg[8]) & svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->latch.b[3];
					break;

				case 0x10: /* OR */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->latch.b[3];
					break;

				case 0x18: /* XOR */
					if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) ^ svga->latch.b[0];
					if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->latch.b[1];
					if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->latch.b[2];
					if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->latch.b[3];
					break;
			}
		}
		break;

	case 3:
		wm = svga->gdcreg[8];
		svga->gdcreg[8] &= cpu_dat;

		vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
		valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
		valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
		vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
		switch (svga->gdcreg[3] & 0x18) {
			case 0: /* set */
				if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | (svga->latch.b[0] & ~svga->gdcreg[8]);
				if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->latch.b[1] & ~svga->gdcreg[8]);
				if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->latch.b[2] & ~svga->gdcreg[8]);
				if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->latch.b[3] & ~svga->gdcreg[8]);
				break;

			case 8: /* AND */
				if (writemask2 & 1) svga->vram[addr]       = (vala | ~svga->gdcreg[8]) & svga->latch.b[0];
				if (writemask2 & 2) svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->latch.b[1];
				if (writemask2 & 4) svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->latch.b[2];
				if (writemask2 & 8) svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->latch.b[3];
				break;

			case 0x10: /* OR */
				if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) | svga->latch.b[0];
				if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->latch.b[1];
				if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->latch.b[2];
				if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->latch.b[3];
				break;

			case 0x18: /* XOR */
				if (writemask2 & 1) svga->vram[addr]       = (vala & svga->gdcreg[8]) ^ svga->latch.b[0];
				if (writemask2 & 2) svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->latch.b[1];
				if (writemask2 & 4) svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->latch.b[2];
				if (writemask2 & 8) svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->latch.b[3];
				break;
		}
		svga->gdcreg[8] = wm;
		break;
    }
}


static void
dm_extalu_write(ht216_t *dev, uint32_t addr, uint8_t cpu_dat, uint8_t bit_mask, uint8_t cpu_dat_unexpanded, uint8_t rop_select)
{
    /*Input B = CD.5
      Input A = FE[3:2]
            00 = Set/Reset output mode
                    output = CPU-side ALU input
            01 = Solid fg/bg mode (3C4:FA/FB)
                    Bit mask = 3CF.F5 or CPU byte
            10 = Dithered fg  (3CF:EC-EF)
            11 = RMW (dest data) (set if CD.5 = 1)
      F/B ROP select = FE[5:4]
            00 = CPU byte
            01 = Bit mask (3CF:8)
            1x = (3C4:F5)*/
    svga_t *svga = &dev->svga;
    uint8_t input_a = 0, input_b = 0;
    uint8_t fg, bg;
    uint8_t output;

    if (dev->ht_regs[0xcd] & HT_REG_CD_RMWMDE) /*RMW*/
	input_b = svga->vram[addr];
    else
	input_b = dev->bg_latch[addr & 7];

    switch (dev->ht_regs[0xfe] & HT_REG_FE_FBMC) {
	case 0x00:
		input_a = cpu_dat;
		break;

	case 0x04:
		if (dev->ht_regs[0xfe] & HT_REG_FE_FBRC)
			input_a = (cpu_dat_unexpanded & (1 << ((addr & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
		else
			input_a = (dev->ht_regs[0xf5] & (1 << ((addr & 7) ^ 7))) ? dev->ht_regs[0xfa] : dev->ht_regs[0xfb];
		break;

	case 0x08:
		input_a = dev->ht_regs[0xec + (addr & 3)];
		break;

	case 0x0c:
		input_a = dev->bg_latch[addr & 7];
		break;
    }

    fg = extalu(dev->ht_regs[0xce] >> 4, input_a, input_b);
    bg = extalu(dev->ht_regs[0xce] & 0xf,  input_a, input_b);
    output = (fg & rop_select) | (bg & ~rop_select);
    svga->vram[addr] = (svga->vram[addr] & ~bit_mask) | (output & bit_mask);
    svga->changedvram[addr >> 12] = changeframecount;
}


static void
write_common(ht216_t *dev, uint32_t addr, uint8_t val)
{
    /*Input B = CD.5
      Input A = FE[3:2]
	    00 = Set/Reset output mode
		 output = CPU-side ALU input
	    01 = Solid fg/bg mode (3C4:FA/FB)
		 Bit mask = 3CF.F5 or CPU byte
	    10 = Dithered fg  (3CF:EC-EF)
	    11 = RMW (dest data) (set if CD.5 = 1)
      F/B ROP select = FE[5:4]
	    00 = CPU byte
	    01 = Bit mask (3CF:8)
	    1x = (3C4:F5)
    */
    svga_t *svga = &dev->svga;
    uint8_t bit_mask = 0, rop_select = 0;

    cycles -= video_timing_write_b;

    addr &= 0xfffff;

    val = svga_rotate[svga->gdcreg[3] & 7][val];

    if (dev->ht_regs[0xcd] & HT_REG_CD_EXALU) {
	/*Extended ALU*/
	switch (dev->ht_regs[0xfe] & HT_REG_FE_FBRSL) {
		case 0x00:
			rop_select = val;
			break;

		case 0x10:
			rop_select = svga->gdcreg[8];
			break;

		case 0x20:
		case 0x30:
			rop_select = dev->ht_regs[0xf5];
			break;
	}

	switch (dev->ht_regs[0xcd] & HT_REG_CD_BMSKSL) {
		case 0x00:
			bit_mask = svga->gdcreg[8];
			break;

		case 0x04:
			bit_mask = val;
			break;

		case 0x08:
		case 0x0c:
			bit_mask = dev->ht_regs[0xf5];
			break;
	}

	if (dev->ht_regs[0xcd] & HT_REG_CD_FP8PCEXP) {	/*1->8 bit expansion*/
		addr = (addr << 3) & 0xfffff;
		dm_extalu_write(dev, addr,     (val & 0x80) ? 0xff : 0, (bit_mask & 0x80) ? 0xff : 0, val, (rop_select & 0x80) ? 0xff : 0);
		dm_extalu_write(dev, addr + 1, (val & 0x40) ? 0xff : 0, (bit_mask & 0x40) ? 0xff : 0, val, (rop_select & 0x40) ? 0xff : 0);
		dm_extalu_write(dev, addr + 2, (val & 0x20) ? 0xff : 0, (bit_mask & 0x20) ? 0xff : 0, val, (rop_select & 0x20) ? 0xff : 0);
		dm_extalu_write(dev, addr + 3, (val & 0x10) ? 0xff : 0, (bit_mask & 0x10) ? 0xff : 0, val, (rop_select & 0x10) ? 0xff : 0);
		dm_extalu_write(dev, addr + 4, (val & 0x08) ? 0xff : 0, (bit_mask & 0x08) ? 0xff : 0, val, (rop_select & 0x08) ? 0xff : 0);
		dm_extalu_write(dev, addr + 5, (val & 0x04) ? 0xff : 0, (bit_mask & 0x04) ? 0xff : 0, val, (rop_select & 0x04) ? 0xff : 0);
		dm_extalu_write(dev, addr + 6, (val & 0x02) ? 0xff : 0, (bit_mask & 0x02) ? 0xff : 0, val, (rop_select & 0x02) ? 0xff : 0);
		dm_extalu_write(dev, addr + 7, (val & 0x01) ? 0xff : 0, (bit_mask & 0x01) ? 0xff : 0, val, (rop_select & 0x01) ? 0xff : 0);
	} else
		dm_extalu_write(dev, addr, val, bit_mask, val, rop_select);
    } else {
	if (dev->ht_regs[0xcd] & HT_REG_CD_FP8PCEXP) {	/*1->8 bit expansion*/
		addr = (addr << 3) & 0xfffff;
		dm_write(dev, addr,     (val & 0x80) ? 0xff : 0, val);
		dm_write(dev, addr + 1, (val & 0x40) ? 0xff : 0, val);
		dm_write(dev, addr + 2, (val & 0x20) ? 0xff : 0, val);
		dm_write(dev, addr + 3, (val & 0x10) ? 0xff : 0, val);
		dm_write(dev, addr + 4, (val & 0x08) ? 0xff : 0, val);
		dm_write(dev, addr + 5, (val & 0x04) ? 0xff : 0, val);
		dm_write(dev, addr + 6, (val & 0x02) ? 0xff : 0, val);
		dm_write(dev, addr + 7, (val & 0x01) ? 0xff : 0, val);
	} else
		dm_write(dev, addr, val, val);
    }
}


static void
ht216_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->write_bank[(addr >> 15) & 1];

    if (dev->ht_regs[0xcd])
	write_common(dev, addr, val);
    else
	svga_write_linear(addr, val, &dev->svga);
}


static void
ht216_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->write_bank[(addr >> 15) & 1];
    if (dev->ht_regs[0xcd]) {
	write_common(dev, addr, val & 0xff);
	write_common(dev, addr+1, val >> 8);
    } else
	svga_writew_linear(addr, val, &dev->svga);
}


static void
ht216_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->write_bank[(addr >> 15) & 1];

    if (dev->ht_regs[0xcd]) {
	write_common(dev, addr, val & 0xff);
	write_common(dev, addr+1, val >> 8);
	write_common(dev, addr+2, val >> 16);
	write_common(dev, addr+3, val >> 24);
    } else
	svga_writel_linear(addr, val, svga);
}


static void
write_linear(uint32_t addr, uint8_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    if (! svga->chain4) /*Bits 16 and 17 of linear address seem to be unused in planar modes*/
	addr = (addr & 0xffff) | ((addr & 0xc0000) >> 2);

    if (dev->ht_regs[0xcd])
	write_common(dev, addr, val);
    else
	svga_write_linear(addr, val, svga);
}


static void
writew_linear(uint32_t addr, uint16_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    if (! svga->chain4)
	addr = (addr & 0xffff) | ((addr & 0xc0000) >> 2);

    if (dev->ht_regs[0xcd]) {
	write_common(dev, addr, val & 0xff);
	write_common(dev, addr+1, val >> 8);
    } else
	svga_writew_linear(addr, val, svga);
}


static void
writel_linear(uint32_t addr, uint32_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    if (! svga->chain4)
	addr = (addr & 0xffff) | ((addr & 0xc0000) >> 2);

    if (dev->ht_regs[0xcd]) {
	write_common(dev, addr, val & 0xff);
	write_common(dev, addr+1, val >> 8);
	write_common(dev, addr+2, val >> 16);
	write_common(dev, addr+3, val >> 24);
    } else
	svga_writel_linear(addr, val, svga);
}


static uint8_t
read_common(ht216_t *dev, uint32_t addr)
{
    svga_t *svga = &dev->svga;
    uint8_t temp, temp2, temp3, temp4, or;
    int readplane = svga->readplane;
    int offset;
    uint32_t latch_addr;

    if (dev->ht_regs[0xc8] & HT_REG_C8_MOVSB)
	addr <<= 3;

    addr &= 0xfffff;

    cycles -= video_timing_read_b;

    if (svga->chain4 || svga->fb_only) {
	addr &= svga->decode_mask;
	if (addr >= svga->vram_max)
		return 0xff;

	latch_addr = (addr & svga->vram_mask) & ~7;
	if (dev->ht_regs[0xcd] & HT_REG_CD_ASTODE)
		latch_addr += (svga->gdcreg[3] & 7);
	dev->bg_latch[0] = svga->vram[latch_addr];
	dev->bg_latch[1] = svga->vram[latch_addr + 1];
	dev->bg_latch[2] = svga->vram[latch_addr + 2];
	dev->bg_latch[3] = svga->vram[latch_addr + 3];
	dev->bg_latch[4] = svga->vram[latch_addr + 4];
	dev->bg_latch[5] = svga->vram[latch_addr + 5];
	dev->bg_latch[6] = svga->vram[latch_addr + 6];
	dev->bg_latch[7] = svga->vram[latch_addr + 7];

	return svga->vram[addr & svga->vram_mask];
    }

    if (svga->chain2_read) {
	readplane = (readplane & 2) | (addr & 1);
	addr &= ~1;
	addr <<= 2;
    } else
	addr <<= 2;

    addr &= svga->decode_mask;

    if (addr >= svga->vram_max)
	return 0xff;

    addr &= svga->vram_mask;

    latch_addr = addr & ~7;
    if (dev->ht_regs[0xcd] & HT_REG_CD_ASTODE) {
	offset = addr & 7;

	dev->bg_latch[0] = svga->vram[latch_addr | offset];
	dev->bg_latch[1] = svga->vram[latch_addr | ((offset + 1) & 7)];
	dev->bg_latch[2] = svga->vram[latch_addr | ((offset + 2) & 7)];
	dev->bg_latch[3] = svga->vram[latch_addr | ((offset + 3) & 7)];
	dev->bg_latch[4] = svga->vram[latch_addr | ((offset + 4) & 7)];
	dev->bg_latch[5] = svga->vram[latch_addr | ((offset + 5) & 7)];
	dev->bg_latch[6] = svga->vram[latch_addr | ((offset + 6) & 7)];
	dev->bg_latch[7] = svga->vram[latch_addr | ((offset + 7) & 7)];
    } else {
	dev->bg_latch[0] = svga->vram[latch_addr];
	dev->bg_latch[1] = svga->vram[latch_addr | 1];
	dev->bg_latch[2] = svga->vram[latch_addr | 2];
	dev->bg_latch[3] = svga->vram[latch_addr | 3];
	dev->bg_latch[4] = svga->vram[latch_addr | 4];
	dev->bg_latch[5] = svga->vram[latch_addr | 5];
	dev->bg_latch[6] = svga->vram[latch_addr | 6];
	dev->bg_latch[7] = svga->vram[latch_addr | 7];
    }

    or = addr & 4;
    svga->latch.d[0] = dev->bg_latch[0 | or] | (dev->bg_latch[1 | or] << 8) |
		       (dev->bg_latch[2 | or] << 16) | (dev->bg_latch[3 | or] << 24);

    if (svga->readmode) {
	temp   = svga->latch.b[0];
	temp  ^= (svga->colourcompare & 1) ? 0xff : 0;
	temp  &= (svga->colournocare & 1)  ? 0xff : 0;
	temp2  = svga->latch.b[1];
	temp2 ^= (svga->colourcompare & 2) ? 0xff : 0;
	temp2 &= (svga->colournocare & 2)  ? 0xff : 0;
	temp3  = svga->latch.b[2];
	temp3 ^= (svga->colourcompare & 4) ? 0xff : 0;
	temp3 &= (svga->colournocare & 4)  ? 0xff : 0;
	temp4  = svga->latch.b[3];
	temp4 ^= (svga->colourcompare & 8) ? 0xff : 0;
	temp4 &= (svga->colournocare & 8)  ? 0xff : 0;
	return ~(temp | temp2 | temp3 | temp4);
    }

    return svga->vram[addr | readplane];
}


static uint8_t
ht216_read(uint32_t addr, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->read_bank[(addr >> 15) & 1];

    return read_common(dev, addr);
}


static uint8_t
read_linear(uint32_t addr, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    if (svga->chain4)
	return read_common(dev, addr);

    return read_common(dev, (addr & 0xffff) | ((addr & 0xc0000) >> 2));
}


static uint8_t
ht216_in(uint16_t addr, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;

    if (((addr & 0xfff0) == 0x03d0 ||
	 (addr & 0xfff0) == 0x03b0) && !(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c2:
		break;

	case 0x03c5:
		if (svga->seqaddr == 6)
			return dev->ext_reg_enable;

		if (svga->seqaddr >= 0x80) {
			if (dev->ext_reg_enable) {
				switch (svga->seqaddr & 0xff) {
					case 0x83:
						if (svga->attrff)
							return svga->attraddr | 0x80;
						return svga->attraddr;

					case 0x8e:
						return dev->id & 0xff;

					case 0x8f:
						return (dev->id >> 8) & 0xff;

					case 0xa0:
						return svga->latch.b[0];

					case 0xa1:
						return svga->latch.b[1];
					case 0xa2:
						return svga->latch.b[2];
					case 0xa3:
						return svga->latch.b[3];
					case 0xf7:
						return 0x01;

					case 0xff:
						return 0x80;
				}

				return dev->ht_regs[svga->seqaddr & 0xff];
			} else
				return 0xff;
		}
		break;

	case 0x03d4:
		return svga->crtcreg;

	case 0x03d5:
		if (svga->crtcreg == 0x1f)
			return svga->crtc[0xc] ^ 0xea;
		return svga->crtc[svga->crtcreg];
    }

    return svga_in(addr, svga);
}


static void
ht216_out(uint16_t addr, uint8_t val, priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

    if (((addr & 0xfff0) == 0x03d0 ||
	 (addr & 0xfff0) == 0x03b0) && !(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c2:
		dev->clk_sel = (dev->clk_sel & ~3) | ((val & 0x0c) >> 2);
		dev->misc = val;
                dev->read_bank_reg[0] = (dev->read_bank_reg[0] & ~0x20) | ((val & HT_MISC_PAGE_SEL) ? 0x20 : 0);
                dev->write_bank_reg[0] = (dev->write_bank_reg[0] & ~0x20) | ((val & HT_MISC_PAGE_SEL) ? 0x20 : 0);
		ht216_remap(dev);
		svga_recalctimings(&dev->svga);
		break;

	case 0x03c5:
		if (svga->seqaddr == 4) {
			svga->chain4 = val & 8;
			ht216_remap(dev);
		} else if (svga->seqaddr == 6) {
			if (val == 0xea)
				dev->ext_reg_enable = 1;
			else if (val == 0xae)
				dev->ext_reg_enable = 0;
		} else if (svga->seqaddr >= 0x80 && dev->ext_reg_enable) {
			old = dev->ht_regs[svga->seqaddr & 0xff];
			dev->ht_regs[svga->seqaddr & 0xff] = val;
			switch (svga->seqaddr & 0xff) {
				case 0x83:
					svga->attraddr = val & 0x1f;
					svga->attrff = (val & 0x80) ? 1 : 0;
					break;

				case 0x94:
					svga->hwcursor.addr = ((val << 6) | (3 << 14) | ((dev->ht_regs[0xff] & 0x60) << 11)) << 2;
					break;

				case 0x9c:
				case 0x9d:
					svga->hwcursor.x = dev->ht_regs[0x9d] | ((dev->ht_regs[0x9c] & 7) << 8);
					break;

				case 0x9e:
				case 0x9f:
					svga->hwcursor.y = dev->ht_regs[0x9f] | ((dev->ht_regs[0x9e] & 3) << 8);
					break;

				case 0xa0:
					svga->latch.b[0] = val;
					break;

				case 0xa1:
					svga->latch.b[1] = val;
					break;

				case 0xa2:
					svga->latch.b[2] = val;
					break;

				case 0xa3:
					svga->latch.b[3] = val;
					break;

				case 0xa4:
					dev->clk_sel = (val >> 2) & 0xf;
					svga->miscout = (svga->miscout & ~0xc) | ((dev->clk_sel & 3) << 2);
					break;

				case 0xa5:
					svga->hwcursor.ena = val & 0x80;
					break;

				case 0xc8:
					if ((old ^ val) & 0x10) {
						svga->fullchange = changeframecount;
						svga_recalctimings(svga);
					}
					break;

				case 0xe8:
					dev->read_bank_reg[0] = val;
					dev->write_bank_reg[0] = val;
					break;

				case 0xe9:
					dev->read_bank_reg[1] = val;
					dev->write_bank_reg[1] = val;
					break;

				case 0xf6:
					svga->vram_display_mask = (val & 0x40) ? dev->vram_mask : 0x3ffff;
					dev->read_bank_reg[0]  = (dev->read_bank_reg[0] & ~0xc0) | ((val & 0xc) << 4);
					dev->write_bank_reg[0] = (dev->write_bank_reg[0] & ~0xc0) | ((val & 0x3) << 6);
					break;

				case 0xf9:
					dev->read_bank_reg[0]  = (dev->read_bank_reg[0] & ~0x10) | ((val & 1) ? 0x10 : 0);
					dev->write_bank_reg[0] = (dev->write_bank_reg[0] & ~0x10) | ((val & 1) ? 0x10 : 0);
					break;

				case 0xff:
					svga->hwcursor.addr = ((dev->ht_regs[0x94] << 6) | (3 << 14) | ((val & 0x60) << 11)) << 2;
					break;
			}

			switch (svga->seqaddr & 0xff) {
				case 0xa4:
				case 0xf6:
				case 0xfc:
					svga->fullchange = changeframecount;
					svga_recalctimings(&dev->svga);
					break;
			}

			switch (svga->seqaddr & 0xff) {
				case 0xc8:
				case 0xc9:
				case 0xcf:
				case 0xe0:
				case 0xe8:
				case 0xe9:
				case 0xf6:
				case 0xf9:
					ht216_remap(dev);
					break;
			}
			return;
		}
		break;

	case 0x03cf:
		if (svga->gdcaddr == 6) {
			if (val & 8)
				svga->banked_mask = 0x7fff;
			else
				svga->banked_mask = 0xffff;
		}
		break;

	case 0x03d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x03d5:
		if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);

		old = svga->crtc[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;

		if (old != val) {
			if (svga->crtcreg < 0xe ||  svga->crtcreg > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(&dev->svga);
			}
		}
		break;

	case 0x46e8:
		io_removehandler(0x03c0, 32,
				 ht216_in,NULL,NULL, ht216_out,NULL,NULL, dev);
		mem_map_disable(&dev->svga.mapping);
		mem_map_disable(&dev->linear_mapping);
		if (val & 8) {
			io_sethandler(0x03c0, 32,
				      ht216_in,NULL,NULL, ht216_out,NULL,NULL, dev);
			mem_map_enable(&dev->svga.mapping);
			ht216_remap(dev);
		}
		break;
    }

    svga_out(addr, val, svga);
}


static void
ht216_close(priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;

    svga_close(&dev->svga);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    ht216_t *dev = (ht216_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static priv_t
ht216_init(const device_t *info, UNUSED(void *parent))
{
    ht216_t *dev;
    svga_t *svga;
    int vram_sz = 0;

    dev = (ht216_t *)mem_alloc(sizeof(ht216_t));
    memset(dev, 0x00, sizeof(ht216_t));
    dev->id = info->local;

    switch(dev->id) {
	case 0x7010:	/* Standard Video7 card */
		vram_sz = device_get_config_int("memory") << 10;
		break;

	case 0x7861:	/* Packard-Bell 410A On-Board */
		vram_sz = (1 << 20);
		break;
    }
    dev->vram_mask = vram_sz - 1;

    dev->ht_regs[0xb4] = 0x08; /*32-bit DRAM bus*/

    io_sethandler(0x03c0, 32,
		  ht216_in,NULL,NULL, ht216_out,NULL,NULL, (priv_t)dev);
    io_sethandler(0x46e8, 1,
		  ht216_in,NULL,NULL, ht216_out,NULL,NULL, (priv_t)dev);

    if (info->path != NULL)
	rom_init(&dev->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    svga = &dev->svga;
    svga_init(svga, (priv_t)dev, vram_sz,
	      recalc_timings, ht216_in, ht216_out, hwcursor_draw, NULL);
    svga->hwcursor.ysize = 32;
    svga->decode_mask = vram_sz - 1;
    svga->bpp = 8;
    svga->miscout = 1;

    mem_map_set_handler(&svga->mapping,
			ht216_read,NULL,NULL,
			ht216_write,ht216_writew,ht216_writel);
    mem_map_set_p(&svga->mapping, (priv_t)dev);

    mem_map_add(&dev->linear_mapping, 0, 0,
		read_linear,NULL,NULL,
		write_linear,writew_linear,writel_linear,
		NULL, 0, (priv_t)svga);

    video_inform(VID_TYPE_SPEC, &v7vga_timings);

    return((priv_t)dev);
}


static const device_config_t v7_vga_1024i_config[] = {
    {
	"memory", "Memory size", CONFIG_SELECTION, "", 1024,
	{
		{
			"512 KB", 512
		},
		{
			"1 MB", 1024
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


const device_t video7_vga_1024i_device = {
    "Video7 VGA 1024i",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0x7010,
    BIOS_VIDEO7_VGA_1024I_PATH,
    ht216_init, ht216_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    NULL,
    v7_vga_1024i_config
};

const device_t ht216_32_pb410a_device = {
    "Headland HT216-32 (Packard Bell PB410A)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0x7861,	/*HT216-32*/
    NULL,
    ht216_init, ht216_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    NULL,
    NULL
};
