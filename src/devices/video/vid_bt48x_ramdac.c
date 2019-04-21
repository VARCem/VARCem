/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Brooktree Bt48x series true color RAMDAC emulation.
 *
 * Version:	@(#)vid_bt48x_ramdac.c	1.0.14	2019/04/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_bt48x_ramdac.h"


enum {
    BT484 = 0,
    ATT20C504,
    BT485,
    ATT20C505,
    BT485A
};


static void
set_bpp(bt48x_ramdac_t *dev, svga_t *svga)
{
    if ((!(dev->cr2 & 0x20)) || ((dev->type >= BT485A) &&
	((dev->cr3 & 0x60) == 0x60)))
	svga->bpp = 8;
    else if ((dev->type >= BT485A) && ((dev->cr3 & 0x60) == 0x40))
	svga->bpp = 24;
    else switch (dev->cr1 & 0x60) {
	case 0x00:
		svga->bpp = 32;
		break;

	case 0x20:
		if (dev->cr1 & 0x08)
			svga->bpp = 16;
		else
			svga->bpp = 15;
		break;

	case 0x40:
		svga->bpp = 8;
		break;

	case 0x60:
		svga->bpp = 4;
		break;
    }

    svga_recalctimings(svga);
}


void
bt48x_ramdac_out(uint16_t addr, int rs2, int rs3, uint8_t val, bt48x_ramdac_t *dev, svga_t *svga)
{
    uint32_t o32;
    uint16_t indx;
    uint8_t *cd;
    uint8_t rs = (addr & 0x03);
    uint16_t da_mask = 0x03ff;

    if (dev->type < BT485)
	da_mask = 0x00ff;

    rs |= (!!rs2 << 2);
    rs |= (!!rs3 << 3);

    switch (rs) {
	case 0x00:	/* Palette Write Index Register (RS value = 0000) */
	case 0x04:	/* Ext Palette Write Index Register (RS value = 0100) */
	case 0x03:
	case 0x07:	/* Ext Palette Read Index Register (RS value = 0111) */
		svga->dac_pos = 0;
		svga->dac_status = addr & 0x03;
		svga->dac_addr = val;
		if (dev->type >= BT485)
			svga->dac_addr |= ((int) (dev->cr3 & 0x03) << 8);
		if (svga->dac_status)
			svga->dac_addr = (svga->dac_addr + 1) & da_mask;
		break;

	case 0x01:	/* Palette Data Register (RS value = 0001) */
	case 0x02:	/* Pixel Read Mask Register (RS value = 0010) */
		svga_out(addr, val, svga);
		break;

	case 0x05:	/* Ext Palette Data Register (RS value = 0101) */
		svga->dac_status = 0;
		svga->fullchange = changeframecount;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_r = val;
				svga->dac_pos++;
				break;

			case 1:
				svga->dac_g = val;
				svga->dac_pos++;
				break;

			case 2:
				indx = svga->dac_addr & 3;
				dev->extpal[indx].r = svga->dac_r;
				dev->extpal[indx].g = svga->dac_g;
				dev->extpal[indx].b = val;
				if (svga->ramdac_type == RAMDAC_8BIT)
					dev->extpallook[indx] = makecol32(dev->extpal[indx].r, dev->extpal[indx].g, dev->extpal[indx].b);
				else
					dev->extpallook[indx] = makecol32(video_6to8[dev->extpal[indx].r & 0x3f], video_6to8[dev->extpal[indx].g & 0x3f], video_6to8[dev->extpal[indx].b & 0x3f]);

				if (svga->ext_overscan && !indx) {
					o32 = svga->overscan_color;
					svga->overscan_color = dev->extpallook[0];
					if (o32 != svga->overscan_color)
						svga_recalctimings(svga);
				}
				svga->dac_addr = (svga->dac_addr + 1) & 0xff;
				svga->dac_pos = 0;
				break;
		}
		break;

	case 0x06:	/* Command Register 0 (RS value = 0110) */
		dev->cr0 = val;
		svga->ramdac_type = (val & 0x02) ? RAMDAC_8BIT : RAMDAC_6BIT;
		break;

	case 0x08:	/* Command Register 1 (RS value = 1000) */
		dev->cr1 = val;
		set_bpp(dev, svga);
		break;

	case 0x09:	/* Command Register 2 (RS value = 1001) */
		dev->cr2 = val;
		svga->hwcursor.ena = !!(val & 0x03);
		set_bpp(dev, svga);
		break;

	case 0x0a:
		if ((dev->type >= BT485) && (dev->cr0 & 0x80)) {
			switch ((svga->dac_addr & ((dev->type >= BT485A) ? 0xff : 0x3f))) {
				case 0x01:
					/* Command Register 3 (RS value = 1010) */
					dev->cr3 = val;
					if (dev->type >= BT485A)
						set_bpp(dev, svga);
					svga->hwcursor.xsize = svga->hwcursor.ysize = (val & 4) ? 64 : 32;
					svga->hwcursor.yoff = (svga->hwcursor.ysize == 32) ? 32 : 0;
					svga->hwcursor.x = dev->hwc_x - svga->hwcursor.xsize;
					svga->hwcursor.y = dev->hwc_y - svga->hwcursor.ysize;
					svga->dac_addr = (svga->dac_addr & 0x00ff) | ((val & 0x03) << 8);
					svga_recalctimings(svga);
					break;

				case 0x02:
				case 0x20:
				case 0x21:
				case 0x22:
					if (dev->type != BT485A)
						break;
					else if (svga->dac_addr == 2) {
						dev->cr4 = val;
						break;
					}
					break;
			}
		}
		break;

	case 0x0b:	/* Cursor RAM Data Register (RS value = 1011) */
		indx = svga->dac_addr & da_mask;
		if ((dev->type >= BT485) && (svga->hwcursor.xsize == 64))
			cd = (uint8_t *) dev->cursor64_data;
		else {
			indx &= 0xff;
			cd = (uint8_t *) dev->cursor32_data;
		}
		cd[indx] = val;
		svga->dac_addr = (svga->dac_addr + 1) & da_mask;
		break;

	case 0x0c:	/* Cursor X Low Register (RS value = 1100) */
		dev->hwc_x = (dev->hwc_x & 0x0f00) | val;
		svga->hwcursor.x = dev->hwc_x - svga->hwcursor.xsize;
		break;

	case 0x0d:	/* Cursor X High Register (RS value = 1101) */
		dev->hwc_x = (dev->hwc_x & 0x00ff) | ((val & 0x0f) << 8);
		svga->hwcursor.x = dev->hwc_x - svga->hwcursor.xsize;
		break;

	case 0x0e:	/* Cursor Y Low Register (RS value = 1110) */
		dev->hwc_y = (dev->hwc_y & 0x0f00) | val;
		svga->hwcursor.y = dev->hwc_y - svga->hwcursor.ysize;
		break;

	case 0x0f:	/* Cursor Y High Register (RS value = 1111) */
		dev->hwc_y = (dev->hwc_y & 0x00ff) | ((val & 0x0f) << 8);
		svga->hwcursor.y = dev->hwc_y - svga->hwcursor.ysize;
		break;
    }
}


uint8_t
bt48x_ramdac_in(uint16_t addr, int rs2, int rs3, bt48x_ramdac_t *dev, svga_t *svga)
{
    uint8_t temp = 0xff;
    uint16_t indx;
    uint8_t *cd;
    uint8_t rs = (addr & 0x03);
    uint16_t da_mask = 0x03ff;

    rs |= (!!rs2 << 2);
    rs |= (!!rs3 << 3);

    switch (rs) {
	case 0x00:	/* Palette Write Index Register (RS value = 0000) */
	case 0x01:	/* Palette Data Register (RS value = 0001) */
	case 0x02:	/* Pixel Read Mask Register (RS value = 0010) */
	case 0x04:	/* Ext Palette Write Index Register (RS value = 0100) */
		temp = svga_in(addr, svga);
		break;

	case 0x03:	/* Palette Read Index Register (RS value = 0011) */
	case 0x07:	/* Ext Palette Read Index Register (RS value = 0111) */
		temp = svga->dac_addr & 0xff;
		break;

	case 0x05:	/* Ext Palette Data Register (RS value = 0101) */
		indx = (svga->dac_addr - 1) & 3;
		svga->dac_status = 3;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_pos++;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = dev->extpal[indx].r;
				else
					temp = dev->extpal[indx].r & 0x3f;
				break;

			case 1:
				svga->dac_pos++;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = dev->extpal[indx].g;
				else
					temp = dev->extpal[indx].g & 0x3f;
				break;

			case 2:
				svga->dac_pos = 0;
				svga->dac_addr = svga->dac_addr + 1;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = dev->extpal[indx].b;
				else
					temp = dev->extpal[indx].b & 0x3f;
				break;
		}
		break;

	case 0x06:	/* Command Register 0 (RS value = 0110) */
		temp = dev->cr0;
		break;

	case 0x08:	/* Command Register 1 (RS value = 1000) */
		temp = dev->cr1;
		break;

	case 0x09:	/* Command Register 2 (RS value = 1001) */
		temp = dev->cr2;
		break;

	case 0x0a:
		if ((dev->type >= BT485) && (dev->cr0 & 0x80)) {
			switch ((svga->dac_addr & ((dev->type >= BT485A) ? 0xff : 0x3f))) {
				case 0x00:
				default:
					temp = dev->status | (svga->dac_status ? 0x04 : 0x00);
					break;

				case 0x01:
					temp = dev->cr3 & 0xfc;
					temp |= (svga->dac_addr & 0x300) >> 8;
					break;

				case 0x02:
				case 0x20:
				case 0x21:
				case 0x22:
					if (dev->type != BT485A)
						break;
					else if (svga->dac_addr == 2) {
						temp = dev->cr4;
						break;
					} else {
						/* TODO: Red, Green, and Blue Signature Analysis Registers */
						temp = 0xff;
						break;
					}
					break;
			}
		} else
			temp = dev->status | (svga->dac_status ? 0x04 : 0x00);
		break;

	case 0x0b:	/* Cursor RAM Data Register (RS value = 1011) */
		indx = (svga->dac_addr - 1) & da_mask;
		if ((dev->type >= BT485) && (svga->hwcursor.xsize == 64))
			cd = (uint8_t *) dev->cursor64_data;
		else {
			indx &= 0xff;
			cd = (uint8_t *) dev->cursor32_data;
		}

		temp = cd[indx];
		svga->dac_addr = (svga->dac_addr + 1) & da_mask;
		break;

	case 0x0c:	/* Cursor X Low Register (RS value = 1100) */
		temp = dev->hwc_x & 0xff;
		break;

	case 0x0d:	/* Cursor X High Register (RS value = 1101) */
		temp = (dev->hwc_x >> 8) & 0xff;
		break;

	case 0x0e:	/* Cursor Y Low Register (RS value = 1110) */
		temp = dev->hwc_y & 0xff;
		break;

	case 0x0f:	/* Cursor Y High Register (RS value = 1111) */
		temp = (dev->hwc_y >> 8) & 0xff;
		break;
    }

    return temp;
}


void
bt48x_hwcursor_draw(svga_t *svga, int displine)
{
    bt48x_ramdac_t *dev = (bt48x_ramdac_t *)svga->ramdac;
    int x, xx, comb, b0, b1;
    uint16_t dat[2];
    int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
    int y_add, x_add;
    int pitch, bppl, mode, x_pos, y_pos;
    uint32_t clr1, clr2, clr3;
    uint8_t *cd;
    pel_t *p;

    clr1 = dev->extpallook[1];
    clr2 = dev->extpallook[2];
    clr3 = dev->extpallook[3];

    y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
    x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;

    /* The planes come in two parts, and each plane is 1bpp,
       so a 32x32 cursor has 4 bytes per line, and a 64x64
       cursor has 8 bytes per line. */
    pitch = (svga->hwcursor_latch.xsize >> 3);		/* Bytes per line. */

    /* A 32x32 cursor has 128 bytes per line, and a 64x64
       cursor has 512 bytes per line. */
    bppl = (pitch * svga->hwcursor_latch.ysize);	/* Bytes per plane. */
    mode = dev->cr2 & 0x03;

    if (svga->interlace && svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;
    if (svga->hwcursor_latch.xsize == 64)
	cd = (uint8_t *) dev->cursor64_data;
    else
	cd = (uint8_t *) dev->cursor32_data;

    for (x = 0; x < svga->hwcursor_latch.xsize; x += 16) {
	dat[0] = (cd[svga->hwcursor_latch.addr]        << 8) |
		  cd[svga->hwcursor_latch.addr + 1];
	dat[1] = (cd[svga->hwcursor_latch.addr + bppl] << 8) |
		  cd[svga->hwcursor_latch.addr + bppl + 1];

	for (xx = 0; xx < 16; xx++) {
		b0 = (dat[0] >> (15 - xx)) & 1;
		b1 = (dat[1] >> (15 - xx)) & 1;
		comb = (b0 | (b1 << 1));

		y_pos = displine + y_add;
		x_pos = offset + 32 + x_add;
		p = &screen->line[y_pos][x_pos];

		if (offset >= svga->hwcursor_latch.x) {
			switch (mode) {
				case 1:		/* Three Color */
					switch (comb) {
						case 1:
							p->val = clr1;
							break;

						case 2:
							p->val = clr2;
							break;

						case 3:
							p->val = clr3;
							break;
					}
					break;

				case 2:		/* PM/Windows */
					switch (comb) {
						case 0:
							p->val = clr1;
							break;

						case 1:
							p->val = clr2;
							break;

						case 3:
							p->val ^= 0xffffff;
							break;
					}
					break;

				case 3:		/* X-Windows */
					switch (comb) {
						case 2:
							p->val = clr1;
							break;

						case 3:
							p->val = clr2;
							break;
					}
					break;
			}
		}

		offset++;
	}

	svga->hwcursor_latch.addr += 2;
    }

    if (svga->interlace && !svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;
}


static void *
bt48x_init(const device_t *info, UNUSED(void *parent))
{
    bt48x_ramdac_t *dev;

    dev = (bt48x_ramdac_t *)mem_alloc(sizeof(bt48x_ramdac_t));
    memset(dev, 0x00, sizeof(bt48x_ramdac_t));
    dev->type = info->local;

    /*
     * Set the RAM DAC status byte to the correct ID bits.
     *
     * Both the BT484 and BT485 datasheets say this:
     * SR7-SR6: These bits are identification values. SR7=0 and SR6=1.
     * But all other sources seem to assume SR7=1 and SR6=0.
     */
    switch (dev->type) {
	case BT484:
		dev->status = 0x40;
		break;

	case ATT20C504:
		dev->status = 0x40;
		break;

	case BT485:
		dev->status = 0x60;
		break;

	case ATT20C505:
		dev->status = 0xd0;
		break;

	case BT485A:
		dev->status = 0x20;
		break;
    }

    return(dev);
}


static void
bt48x_close(void *priv)
{
    bt48x_ramdac_t *dev = (bt48x_ramdac_t *)priv;

    if (dev != NULL)
	free(dev);
}


const device_t bt484_ramdac_device = {
    "Brooktree Bt484 RAMDAC",
    0,
    BT484,
    NULL,
    bt48x_init, bt48x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t att20c504_ramdac_device = {
    "AT&T 20c504 RAMDAC",
    0,
    ATT20C504,
    NULL,
    bt48x_init, bt48x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t bt485_ramdac_device = {
    "Brooktree Bt485 RAMDAC",
    0,
    BT485,
    NULL,
    bt48x_init, bt48x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t att20c505_ramdac_device = {
    "AT&T 20c505 RAMDAC",
    0,
    ATT20C505,
    NULL,
    bt48x_init, bt48x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t bt485a_ramdac_device = {
    "Brooktree Bt485A RAMDAC",
    0,
    BT485A,
    NULL,
    bt48x_init, bt48x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
