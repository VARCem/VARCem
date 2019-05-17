/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Trident TVGA (8900B/8900C/8900D) emulation.
 *
 * Version:	@(#)vid_tvga.c	1.0.15	2019/05/13
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
#include <wchar.h>
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_tkd8001_ramdac.h"


#define ROM_TVGA_8900B		L"video/trident/tvga/tvga8900b.vbi"
#define ROM_TVGA_8900CLD	L"video/trident/tvga/trident.bin"
#define ROM_TVGA_8900CX		L"video/trident/8916cx2/bios.bin"
#define ROM_TVGA_8900D		L"video/trident/tvga/trident.bin"

#define TVGA8900B_ID		0x03
#define TVGA8900CLD_ID		0x33


typedef struct {
    mem_map_t	linear_mapping;
    mem_map_t	accel_mapping;

    svga_t	svga;

    rom_t	bios_rom;
    uint8_t	card_id;

    uint8_t	tvga_3d8, tvga_3d9;
    int		oldmode;
    uint8_t	oldctrl1;
    uint8_t	oldctrl2, newctrl2;

    int		vram_size;
    uint32_t	vram_mask;
} tvga_t;


static const uint8_t crtc_mask[0x40] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x7f, 0xff, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xef,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
    0x7f, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const video_timings_t tvga8900_timing = {VID_ISA,3,3,6,8,8,12};


static void
recalc_banking(tvga_t *dev)
{
    svga_t *svga = &dev->svga;

    svga->write_bank = (dev->tvga_3d8 & 0x1f) * 65536;

    if (svga->gdcreg[0xf] & 1)
	svga->read_bank = (dev->tvga_3d9 & 0x1f) * 65536;
    else
	svga->read_bank = svga->write_bank;
}


static void
recalc_timings(svga_t *svga)
{
    tvga_t *dev = (tvga_t *)svga->p;

    /*
     * This is the only sensible way I can see this being handled,
     * given that TVGA8900D has no overflow bits.
     * Some sort of overflow is required for 320x200x24 and 1024x768x16
     */
    if (!svga->rowoffset) svga->rowoffset = 0x100;

    if (svga->crtc[0x29] & 0x10)
	svga->rowoffset += 0x100;

    if (svga->bpp == 24)
	svga->hdisp = (svga->crtc[1] + 1) * 8;

    if ((svga->crtc[0x1e] & 0xA0) == 0xA0) svga->ma_latch |= 0x10000;
    if ((svga->crtc[0x27] & 0x01) == 0x01) svga->ma_latch |= 0x20000;
    if ((svga->crtc[0x27] & 0x02) == 0x02) svga->ma_latch |= 0x40000;
	
    if (dev->oldctrl2 & 0x10) {
	svga->rowoffset <<= 1;
	svga->ma_latch <<= 1;
    }

    if (svga->gdcreg[0xf] & 0x08) {
	svga->htotal *= 2;
	svga->hdisp *= 2;
	svga->hdisp_time *= 2;
    }

    svga->interlace = (svga->crtc[0x1e] & 4);

    if (svga->interlace)
	svga->rowoffset >>= 1;

    switch (((svga->miscout >> 2) & 3) | ((dev->newctrl2 << 2) & 4)) {
	case 2: svga->clock = cpuclock/44900000.0; break;
	case 3: svga->clock = cpuclock/36000000.0; break;
	case 4: svga->clock = cpuclock/57272000.0; break;
	case 5: svga->clock = cpuclock/65000000.0; break;
	case 6: svga->clock = cpuclock/50350000.0; break;
	case 7: svga->clock = cpuclock/40000000.0; break;
    }

    if (dev->oldctrl2 & 0x10) {
	switch (svga->bpp) {
		case 8:
			svga->render = svga_render_8bpp_highres;
			break;

		case 15:
			svga->render = svga_render_15bpp_highres;
			svga->hdisp /= 2;
			break;

		case 16:
			svga->render = svga_render_16bpp_highres;
			svga->hdisp /= 2;
			break;

		case 24:
			svga->render = svga_render_24bpp_highres;
			svga->hdisp /= 3;
			break;
	}
	svga->lowres = 0;
    }
}


static void
tvga_out(uint16_t addr, uint8_t val, priv_t priv)
{
    tvga_t *dev = (tvga_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

    if (((addr&0xfff0) == 0x3d0 ||
	(addr&0xfff0) == 0x3b0) && !(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x3c5:
		switch (svga->seqaddr & 0x0f) {
			case 0x0b:
				dev->oldmode = 1;
				break;

			case 0x0c:
				if (svga->seqregs[0xe] & 0x80)
					svga->seqregs[0xc] = val; 
				break;

			case 0x0d:
				if (dev->oldmode)
					dev->oldctrl2 = val;
				else {
					dev->newctrl2 = val;
					svga_recalctimings(svga);
				}
				break;

			case 0x0e:
				if (dev->oldmode)
					dev->oldctrl1 = val;
				else {
					svga->seqregs[0xe] = val ^ 2;
					dev->tvga_3d8 = svga->seqregs[0xe] & 0xf;
					recalc_banking(dev);
				}
				return;
		}
		break;

	case 0x3c6:
	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		tkd8001_ramdac_out(addr, val, svga->ramdac, svga);
		return;

	case 0x3cf:
		switch (svga->gdcaddr & 15) {
			case 0x0e:
				svga->gdcreg[0xe] = val ^ 2;
				dev->tvga_3d9 = svga->gdcreg[0xe] & 0xf;
				recalc_banking(dev);
				break;

			case 0x0f:
				svga->gdcreg[0xf] = val;
				recalc_banking(dev);
				break;
		}
		break;

	case 0x3d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x3d5:
		if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		old = svga->crtc[svga->crtcreg];
		val &= crtc_mask[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;
		if (old != val) {
			if (svga->crtcreg < 0xE || svga->crtcreg > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		switch (svga->crtcreg) {
			case 0x1e:
				svga->vram_display_mask = (val & 0x80) ? dev->vram_mask : 0x3ffff;
				break;

			default:
				break;
		}
		return;

	case 0x3d8:
		if (svga->gdcreg[0xf] & 4) {
			dev->tvga_3d8 = val;
			recalc_banking(dev);
		}
		return;

	case 0x3d9:
		if (svga->gdcreg[0xf] & 4) {
			dev->tvga_3d9 = val;
			recalc_banking(dev);
		}
		return;
    }

    svga_out(addr, val, svga);
}


static uint8_t
tvga_in(uint16_t addr, priv_t priv)
{
    tvga_t *dev = (tvga_t *)priv;
    svga_t *svga = &dev->svga;

    if (((addr&0xfff0) == 0x3d0 ||
	(addr&0xfff0) == 0x3b0) && !(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x3c5:
		if ((svga->seqaddr & 0x0f) == 0x0b) {
			dev->oldmode = 0;
                        return dev->card_id; /*Must be at least a TVGA8900*/
		}
		if ((svga->seqaddr & 0x0f) == 0x0d) {
			if (dev->oldmode)
				return dev->oldctrl2;
			return dev->newctrl2;
		}
		if ((svga->seqaddr & 0x0f) == 0x0e) {
			if (dev->oldmode)
				return dev->oldctrl1;
		}
		break;

	case 0x3c6:
	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		return tkd8001_ramdac_in(addr, svga->ramdac, svga);

	case 0x3d4:
		return svga->crtcreg;

	case 0x3d5:
		if (svga->crtcreg > 0x18 && svga->crtcreg < 0x1e)
			return 0xff;
		return svga->crtc[svga->crtcreg];

	case 0x3d8:
		return dev->tvga_3d8;

	case 0x3d9:
		return dev->tvga_3d9;
    }

    return svga_in(addr, svga);
}


static void
tvga_close(priv_t priv)
{
    tvga_t *dev = (tvga_t *)priv;

    svga_close(&dev->svga);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    tvga_t *dev = (tvga_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    tvga_t *dev = (tvga_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static priv_t
tvga_init(const device_t *info, void *parent)
{
    tvga_t *dev;

    dev = (tvga_t *)mem_alloc(sizeof(tvga_t));
    memset(dev, 0x00, sizeof(tvga_t));
    dev->card_id = info->local;

    dev->vram_size = device_get_config_int("memory") << 10;
    dev->vram_mask = dev->vram_size - 1;

    svga_init(&dev->svga, (priv_t)dev, dev->vram_size,
	      recalc_timings, tvga_in, tvga_out, NULL, NULL);

    switch(info->local) {
	case TVGA8900B_ID:	/* TVGA 8900B */
		dev->svga.ramdac = device_add_parent(&tkd8001_ramdac_device, parent);
		break;

	case TVGA8900CLD_ID:	/* TVGA 8900CX LC2 */
		dev->svga.ramdac = device_add_parent(&tkd8001_ramdac_device, parent);
		break;

	case 1:		/* TVGA 8900D */
		dev->svga.ramdac = device_add_parent(&tkd8001_ramdac_device, parent);
		break;
    }

    if (info->path)
	rom_init(&dev->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    io_sethandler(0x03c0, 32,
		  tvga_in,NULL,NULL, tvga_out,NULL,NULL, (priv_t)dev);

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


static const device_config_t tvga_config[] = {
    {
	"memory", "Memory size", CONFIG_SELECTION, "", 1024,
	{
		{
			"256 KB", 256
		},
		{
			"512 KB", 512
		},
		{
			"1 MB", 1024
		},
		 /*Chip supports 2MB, but drivers are buggy*/
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


const device_t tvga8900b_device = {
    "Trident TVGA 8900B",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    TVGA8900B_ID,
    ROM_TVGA_8900B,
    tvga_init, tvga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &tvga8900_timing,
    tvga_config
};

const device_t tvga8900cx_device = {
    "Trident TVGA 8900CX 2/4/8 LC2 Rev.A",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    TVGA8900CLD_ID,
    ROM_TVGA_8900CLD,
    tvga_init, tvga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &tvga8900_timing,
    tvga_config
};

const device_t tvga8900d_device = {
    "Trident TVGA 8900D",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    1,
    ROM_TVGA_8900D,
    tvga_init, tvga_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &tvga8900_timing,
    tvga_config
};
