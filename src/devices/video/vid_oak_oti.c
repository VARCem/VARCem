/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Oak OTI037C/67/077 emulation.
 *
 * Version:	@(#)vid_oak_oti.c	1.0.11	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "video.h"
#include "vid_svga.h"


#define BIOS_37C_PATH		L"video/oti/oti037c/bios.bin"
//#define BIOS_67_PATH		L"video/oti/oti067.bin"
#define BIOS_77_PATH		L"video/oti/oti077.vbi"


typedef struct {
    uint8_t	chip_id;
    uint8_t	enable_register;
    uint8_t	pos;
    uint8_t	indx;

    uint32_t	vram_size;
    uint32_t	vram_mask;

    rom_t	bios_rom;

    uint8_t	regs[32];

    svga_t	svga;
} oti_t;


static void
oti_out(uint16_t addr, uint8_t val, void *priv)
{
    oti_t *dev = (oti_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old, idx;

    if (!(dev->enable_register & 1) && addr != 0x03c3) return;	

    if ((((addr&0xfff0) == 0x03d0 || (addr&0xfff0) == 0x03b0) && addr < 0x3de) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c3:
		dev->enable_register = val & 1;
		return;

	case 0x03d4:
		svga->crtcreg = val;
		return;

	case 0x03d5:
		if (svga->crtcreg & 0x20)
			return;
		if (((svga->crtcreg & 31) < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if (((svga->crtcreg & 31) == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		old = svga->crtc[svga->crtcreg & 31];
		svga->crtc[svga->crtcreg & 31] = val;
		if (old != val) {
			if ((svga->crtcreg & 31) < 0x0e || (svga->crtcreg & 31) > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		break;

	case 0x03de:
		dev->indx = val;
		return;

	case 0x03df:
		idx = dev->indx & 0x1f;
		dev->regs[idx] = val;
		switch (idx) {
			case 0x0d:
				if (dev->chip_id) {
					svga->vram_display_mask = (val & 0xc) ? dev->vram_mask : 0x3ffff;
					if ((val & 0x80) && dev->vram_size == 256)
						mem_map_disable(&svga->mapping);
					else
						mem_map_enable(&svga->mapping);
					if (!(val & 0x80))
						svga->vram_display_mask = 0x3ffff;
				} else {
					if (val & 0x80)
						mem_map_disable(&svga->mapping);
					else
						mem_map_enable(&svga->mapping);
				}
				break;

			case 0x11:
				svga->read_bank = (val & 0xf) * 65536;
				svga->write_bank = (val >> 4) * 65536;
				break;
		}
		return;
    }

    svga_out(addr, val, svga);
}


static uint8_t
oti_in(uint16_t addr, void *priv)
{
    oti_t *dev = (oti_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t ret = 0xff;

    if (!(dev->enable_register & 1) && addr != 0x03c3) return ret;

    if ((((addr&0xfff0) == 0x03d0 || (addr&0xfff0) == 0x03b0) && addr < 0x3de) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c3:
		ret = dev->enable_register;
		break;

	case 0x03d4:
		ret = svga->crtcreg;
		break;

	case 0x03d5:
		if (svga->crtcreg & 0x20)
			ret = 0xff;
		else
			ret = svga->crtc[svga->crtcreg & 31];
		break;

	case 0x03da:
                svga->attrff = 0;
                svga->attrff = 0;
                svga->cgastat &= ~0x30;

                /* copy color diagnostic info from the overscan color register */
                switch (svga->attrregs[0x12] & 0x30) {
                        case 0x00: /* P0 and P2 */
                        	if (svga->attrregs[0x11] & 0x01)
                                	svga->cgastat |= 0x10;
                        	if (svga->attrregs[0x11] & 0x04)
                                	svga->cgastat |= 0x20;
                        	break;

                        case 0x10: /* P4 and P5 */
                        	if (svga->attrregs[0x11] & 0x10)
                                	svga->cgastat |= 0x10;
                        	if (svga->attrregs[0x11] & 0x20)
                                	svga->cgastat |= 0x20;
                        	break;

                        case 0x20: /* P1 and P3 */
                        	if (svga->attrregs[0x11] & 0x02)
                                	svga->cgastat |= 0x10;
                        	if (svga->attrregs[0x11] & 0x08)
                                	svga->cgastat |= 0x20;
                        	break;

                        case 0x30: /* P6 and P7 */
                        	if (svga->attrregs[0x11] & 0x40)
                                	svga->cgastat |= 0x10;
                        	if (svga->attrregs[0x11] & 0x80)
                                	svga->cgastat |= 0x20;
                        	break;
                }
                return svga->cgastat;

	case 0x03de:
		ret = dev->indx | (dev->chip_id << 5);
		break;

	case 0x03df:
		if ((dev->indx & 0x1f)==0x10)
			ret = 0x18;
		  else
			ret = dev->regs[dev->indx & 0x1f];
		break;

	default:
		ret = svga_in(addr, svga);
		break;
    }

    return(ret);
}


static void
oti_pos_out(uint16_t addr, uint8_t val, void *priv)
{
    oti_t *dev = (oti_t *)priv;

    if ((val & 8) != (dev->pos & 8)) {
	if (val & 8)
		io_sethandler(0x03c0, 32,
			      oti_in,NULL,NULL, oti_out,NULL,NULL, dev);
	else
		io_removehandler(0x03c0, 32,
				 oti_in,NULL,NULL, oti_out,NULL,NULL, dev);
    }

    dev->pos = val;
}


static uint8_t
oti_pos_in(uint16_t addr, void *priv)
{
    oti_t *dev = (oti_t *)priv;

    return(dev->pos);
}


static void
recalc_timings(svga_t *svga)
{
    oti_t *dev = (oti_t *)svga->p;

    if (dev->regs[0x14] & 0x08) svga->ma_latch |= 0x10000;

    if (dev->regs[0x0d] & 0x0c) svga->rowoffset <<= 1;

    svga->interlace = dev->regs[0x14] & 0x80;
}


static void
speed_changed(void *priv)
{
    oti_t *dev = (oti_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(void *priv)
{
    oti_t *dev = (oti_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static void *
oti_init(const device_t *info)
{
    oti_t *dev;
    wchar_t *fn;

    dev = (oti_t *)mem_alloc(sizeof(oti_t));
    memset(dev, 0x00, sizeof(oti_t));
    dev->chip_id = info->local;

    fn = NULL;
    switch(dev->chip_id) {
	case 0:
		fn = BIOS_37C_PATH;
		break;

	case 2:
#ifdef BIOS_67_PATH
		fn = BIOS_67_PATH;
#else
		fn = BIOS_77_PATH;
#endif
		break;

	case 2+128:
		/* Onboard OTI067; ROM set up by machine. */
		dev->chip_id = 2;
		break;

	case 5:
		fn = BIOS_77_PATH;
		break;
    }

    if (fn != NULL)
	rom_init(&dev->bios_rom, fn,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    dev->vram_size = device_get_config_int("memory");
    dev->vram_mask = (dev->vram_size << 10) - 1;

    svga_init(&dev->svga, dev, dev->vram_size << 10,
	      recalc_timings, oti_in, oti_out, NULL, NULL);

    io_sethandler(0x03c0, 32,
		  oti_in,NULL,NULL, oti_out,NULL,NULL, dev);

    io_sethandler(0x46e8, 1,
		  oti_pos_in,NULL,NULL, oti_pos_out,NULL,NULL, dev);

    dev->svga.miscout = 1;

    /* FIXME: BIOS wants to read this there (undocumented.)*/
    dev->regs[0] = 0x08;

    return(dev);
}


static void
oti_close(void *priv)
{
    oti_t *dev = (oti_t *)priv;

    svga_close(&dev->svga);

    free(dev);
}


static int
oti037c_available(void)
{
    return(rom_present(BIOS_37C_PATH));
}

static int
oti067_available(void)
{
#ifdef BIOS_67_PATH
    return(rom_present(BIOS_67_PATH));
#else
    return(rom_present(BIOS_77_PATH));
#endif
}


static const device_config_t oti067_config[] = {
	{
		"memory", "Memory size", CONFIG_SELECTION, "", 512,
		{
			{
				"256 KB", 256
			},
			{
				"512 KB", 512
			},
			{
				""
			}
		}
	},
	{
		"", "", -1
	}
};


static int
oti077_available(void)
{
    return(rom_present(BIOS_77_PATH));
}


static const device_config_t oti077_config[] = {
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
			{
				""
			}
		}
	},
	{
		"", "", -1
	}
};

const device_t oti037c_device = {
    "Oak OTI-037C",
    DEVICE_ISA,
    0,
    oti_init, oti_close, NULL,
    oti037c_available,
    speed_changed,
    force_redraw,
    NULL,
    oti067_config
};

const device_t oti067_device = {
    "Oak OTI-067",
    DEVICE_ISA,
    2,
    oti_init, oti_close, NULL,
    oti067_available,
    speed_changed,
    force_redraw,
    NULL,
    oti067_config
};

const device_t oti067_onboard_device = {
    "Oak OTI-067 (onboard)",
    DEVICE_ISA,
    2+128,
    oti_init, oti_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    NULL,
    oti067_config
};

const device_t oti077_device = {
    "Oak OTI-077",
    DEVICE_ISA,
    5,
    oti_init, oti_close, NULL,
    oti077_available,
    speed_changed,
    force_redraw,
    NULL,
    oti077_config
};
