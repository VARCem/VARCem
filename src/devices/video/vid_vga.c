/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		IBM VGA emulation.
 *
 * Version:	@(#)vid_vga.c	1.0.12	2019/05/17
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
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_vga.h"


#define BIOS_ROM_PATH	L"video/ibm/vga/ibm_vga.bin"

svga_t *mb_vga = NULL;

static video_timings_t vga_timing = {VID_ISA, 8,16,32, 8,16,32};


static void
vga_out(uint16_t port, uint8_t val, priv_t priv)
{
    vga_t *dev = (vga_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

    if (((port & 0xfff0) == 0x3d0 ||
	 (port & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) port ^= 0x60;

    switch (port) {
	case 0x3d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x3d5:
		if (svga->crtcreg & 0x20)
			return;
		if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		old = svga->crtc[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;
		if (old != val) {
			if (svga->crtcreg < 0xe || svga->crtcreg > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		break;
    }

    svga_out(port, val, svga);
}


static uint8_t
vga_in(uint16_t port, priv_t priv)
{
    vga_t *dev = (vga_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t ret;

    if (((port & 0xfff0) == 0x3d0 ||
	 (port & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) port ^= 0x60;

    switch (port) {
	case 0x3d4:
		ret = svga->crtcreg;
		break;

	case 0x3d5:
		if (svga->crtcreg & 0x20)
			ret = 0xff;
		else
			ret = svga->crtc[svga->crtcreg];
		break;

	default:
		ret = svga_in(port, svga);
		break;
    }

    return(ret);
}


static void
vga_close(priv_t priv)
{
    vga_t *dev = (vga_t *)priv;

    mb_vga = NULL;

    svga_close(&dev->svga);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    vga_t *dev = (vga_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    vga_t *dev = (vga_t *)priv;

    dev->svga.fullchange = changeframecount;
}

#if 0
void 
vga_disable(priv_t priv)
{
        vga_t *dev = (vga_t *)priv;
        svga_t *svga = &dev->svga;

        io_removehandler(0x03a0, 0x0040, vga_in, NULL, NULL, vga_out, NULL, NULL, dev);
        mem_map_disable(&svga->mapping);
}

void 
vga_enable(priv_t priv)
{
        vga_t *dev = (vga_t *)priv;
        svga_t *svga = &dev->svga;

        io_sethandler(0x03c0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, dev);
        if (!(svga->miscout & 1))
                io_sethandler(0x03a0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, dev);

        mem_map_enable(&svga->mapping);
}
#endif

static priv_t
vga_init(const device_t *info, UNUSED(void *parent))
{
    vga_t *dev;

    dev = (vga_t *)mem_alloc(sizeof(vga_t));
    memset(dev, 0x00, sizeof(vga_t));

    if (info->path != NULL)
	rom_init(&dev->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0x2000, MEM_MAPPING_EXTERNAL);

    svga_init(&dev->svga, (priv_t)dev, 1 << 18, /*256kb*/
	      NULL, vga_in, vga_out, NULL, NULL);

    io_sethandler(0x03c0, 32,
		  vga_in,NULL,NULL, vga_out,NULL,NULL, (priv_t)dev);

    dev->svga.bpp = 8;
    dev->svga.miscout = 1;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


const device_t vga_device = {
    "VGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    BIOS_ROM_PATH,
    vga_init, vga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &vga_timing,
    NULL
};


static const video_timings_t ps1vga_timing = {VID_ISA,6,8,16,6,8,16};

const device_t vga_ps1_device = {
    "VGA (PS/1)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    1,
    NULL,
    vga_init, vga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ps1vga_timing,
    NULL
};

const device_t vga_ps1_mca_device = {
    "VGA (PS/1, MCA)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_MCA,
    1,
    NULL,
    vga_init, vga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ps1vga_timing,
    NULL
};
