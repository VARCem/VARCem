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
 * Version:	@(#)vid_oak_oti.c	1.0.13	2018/10/08
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


#define BIOS_037C_PATH		L"video/oti/oti037c/bios.bin"
//#define BIOS_067_PATH		L"video/oti/oti067.bin"
#define BIOS_077_PATH		L"video/oti/oti077.vbi"


enum {
    OTI_037C = 0,
    OTI_067 = 2,
    OTI_067_AMA932J,
    OTI_077 = 5
};


typedef struct {
    uint8_t	chip_id;
    uint8_t	enable_register;
    uint8_t	pos;
    uint8_t	indx;
    uint8_t	dipswitch_val;

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
    uint8_t old, idx, enable;

    if (!dev->chip_id && !(dev->enable_register & 1) && (addr != 0x3C3))
	return;

    if ((((addr&0xfff0) == 0x03d0 || (addr&0xfff0) == 0x03b0) && addr < 0x3de) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c3:
		if (! dev->chip_id) {
			dev->enable_register = val & 1;
			return;
		}
		break;

	case 0x03d4:
		if (dev->chip_id)
			svga->crtcreg = val & 0x3f;
		else
			svga->crtcreg = val;	/* FIXME: The BIOS wants to set the test bit? */
		return;

	case 0x03d5:
		if (dev->chip_id && (svga->crtcreg & 0x20))
			return;
		idx = svga->crtcreg;
		if (! dev->chip_id)
			idx &= 0x1f;
		if ((idx < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((idx == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		old = svga->crtc[idx];
		svga->crtc[idx] = val;
		if (old != val) {
			if ((idx < 0x0e) || (idx > 0x10)) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		break;

	case 0x03de:
		if (dev->chip_id)
			dev->indx = val & 0x1f;
		else
			dev->indx = val;
		return;

	case 0x03df:
		idx = dev->indx;
		if (! dev->chip_id)
			idx &= 0x1f;
		dev->regs[idx] = val;
		switch (idx) {
			case 0x0d:
				if (dev->chip_id == OTI_067) {
					svga->vram_display_mask = (val & 0xc) ? dev->vram_mask : 0x3ffff;
					if (! (val & 0x80))
						svga->vram_display_mask = 0x3ffff;

					if ((val & 0x80) && dev->vram_size == 256)
						mem_map_disable(&svga->mapping);
					else
						mem_map_enable(&svga->mapping);
				} else if (dev->chip_id == OTI_077) {
					svga->vram_display_mask = (val & 0xc) ? dev->vram_mask : 0x3ffff;
					switch ((val & 0xc0) >> 6) {
						case 0x00:	/* 256 kB of memory */
						default:
							enable = (dev->vram_size >= 256);
							if (val & 0xc)
								svga->vram_display_mask = MIN(dev->vram_mask, 0x3ffff);
							break;
						case 0x01:	/* 1 MB of memory */
						case 0x03:
							enable = (dev->vram_size >= 1024);
							if (val & 0xc)
								svga->vram_display_mask = MIN(dev->vram_mask, 0x7ffff);
							break;
						case 0x02:	/* 512 kB of memory */
							enable = (dev->vram_size >= 512);
							if (val & 0xc)
								svga->vram_display_mask = MIN(dev->vram_mask, 0xfffff);
							break;
					}

					if (enable)
						mem_map_enable(&svga->mapping);
					else
						mem_map_disable(&svga->mapping);
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
    uint8_t idx, ret = 0xff;

    if (!dev->chip_id && !(dev->enable_register & 1) &&
	(addr != 0x3c3)) return 0xff;

    if ((((addr&0xfff0) == 0x03d0 || (addr&0xfff0) == 0x03b0) && addr < 0x3de) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c2:
		if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x50)
			ret = 0;
		else
			ret = 0x10;
		break;

	case 0x03c3:
		if (dev->chip_id)
			ret = svga_in(addr, svga);
		else
			ret = dev->enable_register;
		break;

	case 0x03d4:
		ret = svga->crtcreg;
		break;

	case 0x03d5:
		if (dev->chip_id) {
			if (svga->crtcreg & 0x20)
				ret = 0xff;
			else
				ret = svga->crtc[svga->crtcreg];
		} else
			ret = svga->crtc[svga->crtcreg & 0x1f];
		break;

	case 0x03da:
		if (dev->chip_id) {
			ret = svga_in(addr, svga);
			break;
		}

                svga->attrff = 0;

		/*
		 * The OTI-037C BIOS waits for bits 0 and 3 in 0x3da to go
		 * low, then reads 0x3da again and expects the diagnostic
		 * bits to equal the current border colour. As I understand
		 * it, the 0x3da active enable status does not include the
		 * border time, so this may be an area where OTI-037C is
		 * not entirely VGA compatible.
		 */
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
                ret = svga->cgastat;
		break;

	case 0x03de:
		ret = dev->indx;
		if (dev->chip_id)
			ret |= (dev->chip_id << 5);
		break;

	case 0x03df:
		idx = dev->indx;
		if (! dev->chip_id)
			idx &= 0x1f; 
		if (idx == 0x10)
			ret = dev->dipswitch_val;
		else
			ret = dev->regs[idx];
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

    if ((val ^ dev->pos) & 8) {
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

    dev->dipswitch_val = 0x18;

    fn = NULL;
    switch(dev->chip_id) {
	case OTI_037C:
		fn = BIOS_037C_PATH;
		dev->vram_size = 256;

		/*
		 * FIXME: The BIOS wants to read this at index 0?
		 * This index is undocumented.
		 */
		dev->regs[0] = 0x08;
		break;

	case OTI_067:
#ifdef BIOS_067_PATH
		fn = BIOS_067_PATH;
#else
		fn = BIOS_077_PATH;
#endif

		dev->vram_size = device_get_config_int("memory");

		/*
		 * Tell the BIOS the I/O ports are already enabled
		 * to avoid a double I/O handler mess.
		 */
		dev->pos = 0x08;

		io_sethandler(0x46e8, 1,
			      oti_pos_in,NULL,NULL, oti_pos_out,NULL,NULL, dev);
		break;

	case OTI_067_AMA932J:
		/* Onboard OTI067; ROM set up by machine. */
		dev->chip_id = OTI_067;
		dev->vram_size = 512;
		dev->dipswitch_val |= 0x20;

		/*
		 * Tell the BIOS the I/O ports are already enabled
		 * to avoid a double I/O handler mess.
		 */
		dev->pos = 0x08;

		io_sethandler(0x46e8, 1,
			      oti_pos_in,NULL,NULL, oti_pos_out,NULL,NULL, dev);
		break;

	case OTI_077:
		fn = BIOS_077_PATH;
		dev->vram_size = device_get_config_int("memory");

		/*
		 * Tell the BIOS the I/O ports are already enabled
		 * to avoid a double I/O handler mess.
		 */
		dev->pos = 0x08;

		io_sethandler(0x46e8, 1,
			      oti_pos_in,NULL,NULL, oti_pos_out,NULL,NULL, dev);
		break;
    }

    if (fn != NULL)
	rom_init(&dev->bios_rom, fn,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    dev->vram_mask = (dev->vram_size << 10) - 1;

    INFO("VIDEO: %s (chip=%02x(%i), mem=%i)\n",
	info->name, dev->chip_id, info->local, dev->vram_size);

    svga_init(&dev->svga, dev, dev->vram_size << 10,
	      recalc_timings, oti_in, oti_out, NULL, NULL);

    io_sethandler(0x03c0, 32,
		  oti_in,NULL,NULL, oti_out,NULL,NULL, dev);

    dev->svga.miscout = 1;

    video_inform(VID_TYPE_SPEC,
		 (const video_timings_t *)info->vid_timing);

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
    return(rom_present(BIOS_037C_PATH));
}

static int
oti067_available(void)
{
#ifdef BIOS_67_PATH
    return(rom_present(BIOS_067_PATH));
#else
    return(rom_present(BIOS_077_PATH));
#endif
}

static int
oti077_available(void)
{
    return(rom_present(BIOS_077_PATH));
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

static const video_timings_t oti_timing = {VID_ISA,6,8,16,6,8,16};


const device_t oti037c_device = {
    "Oak OTI-037C",
    DEVICE_ISA,
    OTI_037C,
    oti_init, oti_close, NULL,
    oti037c_available,
    speed_changed,
    force_redraw,
    &oti_timing,
    NULL
};

const device_t oti067_device = {
    "Oak OTI-067",
    DEVICE_ISA,
    OTI_067,
    oti_init, oti_close, NULL,
    oti067_available,
    speed_changed,
    force_redraw,
    &oti_timing,
    oti067_config
};

const device_t oti067_onboard_device = {
    "Onboard Oak OTI-067",
    DEVICE_ISA,
    OTI_067_AMA932J,
    oti_init, oti_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &oti_timing,
    NULL
};

const device_t oti077_device = {
    "Oak OTI-077",
    DEVICE_ISA,
    OTI_077,
    oti_init, oti_close, NULL,
    oti077_available,
    speed_changed,
    force_redraw,
    &oti_timing,
    oti077_config
};
