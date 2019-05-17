/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the TI CF62011 SVGA chip.
 *
 *		This chip was used in several of IBM's later machines, such
 *		as the PS/1 Model 2121, and a number of PS/2 models. As noted
 *		in an article on Usenet:
 *
 *		"In the early 90s IBM looked for some cheap VGA card to
 *		 substitute the (relatively) expensive XGA-2 adapter for
 *		 *servers*, where the primary purpose is supervision of the
 *		 machine rather than real *work* with it in Hi-Res. It was
 *		 just to supply a base video, where a XGA-2 were a waste of
 *		 potential. They had a contract with TI for some DSPs in
 *		 multimedia already (the MWave for instance is based on
 *		 TI-DSPs as well as many Thinkpad internal chipsets) and TI
 *		 offered them a rather cheap – and inexpensive – chipset
 *		 and combined it with a cheap clock oscillator and an Inmos
 *		 RAMDAC. That chipset was already pretty much outdated at
 *		 that time but IBM decided it would suffice for that low
 *		 end purpose.
 *
 *		 Driver support was given under DOS and OS/2 only for base
 *		 functions like selection of the vertical refresh and few
 *		 different modes only. Not even the Win 3.x support has
 *		 been finalized. Technically the adapter could do better
 *		 than VGA, but its video BIOS is largely undocumented and
 *		 intentionally crippled down to a few functions."
 *
 *		This chip is reportedly the same one as used in the MCA
 *		IBM SVGA Adapter/A (ID 090EEh), which mostly had faster
 *		VRAM and RAMDAC. The VESA DOS graphics driver for that
 *		card can be used: m95svga.exe
 *
 *		The controller responds at ports in the range 0x2100-0x210F,
 *		which are the same as the XGA. It supports up to 1MB of VRAM,
 *		but we lock it down to 512K. The PS/1 2122 had 256K.
 *
 * Version:	@(#)vid_ti_cf62011.c	1.0.11	2019/05/13
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
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"


typedef struct {
    svga_t	svga;

    rom_t	bios_rom;

    int		enabled;

    uint32_t	vram_size;

    uint8_t	banking;
    uint8_t	reg_2100;
    uint8_t	reg_210a;
} tivga_t;


static const video_timings_t ti_cf62011_timing = {VID_ISA,8,16,32,8,16,32};


static void
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    tivga_t *dev = (tivga_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

#if 0
    if (((addr & 0xfff0) == 0x03d0 ||
	(addr & 0xfff0) == 0x03b0) && !(svga->miscout & 1)) addr ^= 0x60;
#endif

    switch (addr) {
	case 0x0102:
		dev->enabled = (val & 0x01);
		return;

	case 0x03d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x03d5:
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

	case 0x2100:
		dev->reg_2100 = val;
		if ((val & 7) < 4)
			svga->read_bank = svga->write_bank = 0;
		  else
			svga->read_bank = svga->write_bank = (dev->banking & 0x7) * 0x10000;
		break;

	case 0x2108:
		if ((dev->reg_2100 & 7) >= 4)
			svga->read_bank = svga->write_bank = (val & 0x7) * 0x10000;
		dev->banking = val;
		break;

	case 0x210a:
		dev->reg_210a = val;
		break;
    }

    svga_out(addr, val, svga);
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    tivga_t *dev = (tivga_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t ret;

#if 0
    if (((addr & 0xfff0) == 0x03d0 ||
	(addr & 0xfff0) == 0x03b0) && !(svga->miscout & 1)) addr ^= 0x60;
#endif

    switch (addr) {
	case 0x0100:
		ret = 0xfe;
		break;

	case 0x0101:
		ret = 0xe8;
		break;

	case 0x0102:
		ret = dev->enabled;
		break;

	case 0x03d4:
		ret = svga->crtcreg;
		break;

	case 0x03d5:
		if (svga->crtcreg & 0x20)
			ret = 0xff;
		else
			ret = svga->crtc[svga->crtcreg];
		break;

	case 0x2100:
		ret = dev->reg_2100;
		break;

	case 0x2108:
		ret = dev->banking;
		break;

	case 0x210a:
		ret = dev->reg_210a;
		break;

	default:
		ret = svga_in(addr, svga);
		break;
    }

    return(ret);
}


static void
speed_changed(priv_t priv)
{
    tivga_t *dev = (tivga_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    tivga_t *dev = (tivga_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static void
vid_close(priv_t priv)
{
    tivga_t *dev = (tivga_t *)priv;

    svga_close(&dev->svga);

    free(dev);
}


static priv_t
vid_init(const device_t *info, UNUSED(void *parent))
{
    tivga_t *dev;

    /* Allocate control block and initialize. */
    dev = (tivga_t *)mem_alloc(sizeof(tivga_t));
    memset(dev, 0x00, sizeof(tivga_t));

    /* Set amount of VRAM in KB. */
    if (info->local == 0)
	dev->vram_size = device_get_config_int("vram_size");
      else
	dev->vram_size = info->local;

    DEBUG("VIDEO: initializing %s, %iK VRAM\n", info->name, dev->vram_size);

    svga_init(&dev->svga, (priv_t)dev,
	      dev->vram_size << 10, NULL, vid_in, vid_out, NULL, NULL);

    io_sethandler(0x0100, 2,
		  vid_in,NULL,NULL, NULL,NULL,NULL, (priv_t)dev);
    io_sethandler(0x03c0, 32,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, (priv_t)dev);
    io_sethandler(0x2100, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, (priv_t)dev);

    dev->svga.bpp = 8;
    dev->svga.miscout = 1;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


#if defined(DEV_BRANCH) && defined(USE_TI)
static const device_config_t ti_cf62011_config[] = {
    {
	"vram_size", "Memory Size", CONFIG_SELECTION, "", 256,
	{
		{
			"256K", 256
		},
		{
			"512K", 512
		},
		{
			"1024K", 1024
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


const device_t ti_cf62011_device = {
    "TI CF62011 SVGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ti_cf62011_timing,
    ti_cf62011_config
};
#endif

const device_t ti_ps1_device = {
    "IBM PS/1 Model 2121 SVGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    512,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ti_cf62011_timing,
    NULL
};
