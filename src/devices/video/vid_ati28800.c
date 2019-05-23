/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ATI 28800 emulation (VGA Charger and Korean VGA)
 *
 * Version:	@(#)vid_ati28800.c	1.0.23	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		greatpsycho, <greatpsycho@yahoo.com>
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
#include "../../timer.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_sc1502x_ramdac.h"
#include "vid_ati.h"


enum {
    VID_VGACHARGER = 0,		/* ATI VGA Charger (28800-5) */
    VID_VGAWONDERXL,		/* Compaq ATI VGA Wonder XL (28800-5) */
#if defined(DEV_BRANCH) && defined(USE_XL24)
    VID_VGAWONDERXL24,		/* Compaq ATI VGA Wonder XL24 (28800-6) */
#endif
    VID_ATIKOREANVGA		/* ATI Korean VGA (28800-5) */
};


#define BIOS_ATIKOR_PATH	L"video/ati/ati28800/atikorvga.bin"
#define FONT_ATIKOR_PATH        L"video/ati/ati28800/ati_ksc5601.rom"

#define BIOS_VGAXL_EVEN_PATH	L"video/ati/ati28800/xleven.bin"
#define BIOS_VGAXL_ODD_PATH	L"video/ati/ati28800/xlodd.bin"

#if defined(DEV_BRANCH) && defined(USE_XL24)
#define BIOS_XL24_EVEN_PATH	L"video/ati/ati28800/112-14318-102.bin"
#define BIOS_XL24_ODD_PATH	L"video/ati/ati28800/112-14319-102.bin"
#endif

#define BIOS_ROM_PATH		L"video/ati/ati28800/bios.bin"


typedef struct {
    svga_t	svga;
    ati_eeprom_t eeprom;

    rom_t	bios_rom;

    uint8_t	regs[256];
    int		index;

    uint32_t	memory;
    uint8_t	id;

    uint8_t	port_03dd_val;
    uint16_t	get_korean_font_kind;
    int		in_get_korean_font_kind_set;
    int		get_korean_font_enabled;
    int		get_korean_font_index;
    uint16_t	get_korean_font_base;
    int		ksc5601_mode_enabled;
} ati28800_t;


static void
ati28800_out(uint16_t port, uint8_t val, priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

    if (((port&0xfff0) == 0x3d0 ||
	 (port&0xfff0) == 0x3b0) && !(svga->miscout&1)) port ^= 0x60;

    switch (port) {
	case 0x1ce:
		dev->index = val;
		break;

	case 0x1cf:
		old = dev->regs[dev->index];
		dev->regs[dev->index] = val;
		switch (dev->index) {
			case 0xbd:
			case 0xbe:
				if (dev->regs[0xbe] & 8) /*Read/write bank mode*/ {
					svga->read_bank  = (((dev->regs[0xb2] >> 5) & 7) * 0x10000);
					svga->write_bank = (((dev->regs[0xb2] >> 1) & 7) * 0x10000);
				} else {	/*Single bank mode*/
					svga->read_bank  = (((dev->regs[0xb2] >> 1) & 7) * 0x10000);
					svga->write_bank = (((dev->regs[0xb2] >> 1) & 7) * 0x10000);
				}
				break;

			case 0xb3:
				ati_eeprom_write(&dev->eeprom, val & 8, val & 2, val & 1);
				break;

			case 0xb6:
				if (val & 1)
					DEBUG("Extended 0xB6 bit 0 enabled\n");

				if ((old ^ val) & 0x10)
					svga_recalctimings(svga);
				break;

			case 0xb8:
				if ((old ^ val) & 0x40)
					svga_recalctimings(svga);
				break;

			case 0xb9:
				if ((old ^ val) & 2)
					svga_recalctimings(svga);
		}
		break;

	case 0x3c6:
	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		sc1502x_ramdac_out(port, val, svga->ramdac, svga);
		return;					

	case 0x3d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x3d5:
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


static void
ati28800k_out(uint16_t port, uint8_t val, priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;
    svga_t *svga = &dev->svga;
    uint16_t oldport = port;

    if (((port&0xfff0) == 0x3d0 ||
	 (port&0xfff0) == 0x3b0) && !(svga->miscout&1)) port ^= 0x60;

    switch (port) {
	case 0x1cf:
		if (dev->index == 0xBF && ((dev->regs[0xBF] ^ val) & 0x20)) {
			dev->ksc5601_mode_enabled = val & 0x20;
			svga_recalctimings(svga);

		}
		ati28800_out(oldport, val, priv);
		break;

	case 0x3dd:
		dev->port_03dd_val = val;
		if (val == 1) dev->get_korean_font_enabled = 0;
		if (dev->in_get_korean_font_kind_set) {
			dev->get_korean_font_kind = (val << 8) | (dev->get_korean_font_kind & 0xFF);
			dev->get_korean_font_enabled = 1;
			dev->get_korean_font_index = 0;
			dev->in_get_korean_font_kind_set = 0;
		}
		break;

	case 0x3de:
		dev->in_get_korean_font_kind_set = 0;
		if (dev->get_korean_font_enabled) {
			if ((dev->get_korean_font_base & 0x7F) > 0x20 && (dev->get_korean_font_base & 0x7F) < 0x7F)
				fontdatk_user[(dev->get_korean_font_kind & 4) * 24 + (dev->get_korean_font_base & 0x7F) - 0x20].chr[dev->get_korean_font_index] = val;
			dev->get_korean_font_index++;
			dev->get_korean_font_index &= 0x1F;
		} else {
			switch(dev->port_03dd_val) {
				case 0x10:
					dev->get_korean_font_base = ((val & 0x7F) << 7) | (dev->get_korean_font_base & 0x7F);
					break;

				case 8:
					dev->get_korean_font_base = (dev->get_korean_font_base & 0x3F80) | (val & 0x7F);
					break;
				case 1:
					dev->get_korean_font_kind = (dev->get_korean_font_kind & 0xFF00) | val;
					if (val & 2)
						dev->in_get_korean_font_kind_set = 1;
					break;
			}
		}
		break;

	default:
		ati28800_out(oldport, val, priv);
		break;
    }
}


static uint8_t
ati28800_in(uint16_t port, priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t ret = 0xff;

    if (((port&0xfff0) == 0x3d0 ||
	 (port&0xfff0) == 0x3b0) && !(svga->miscout&1)) port ^= 0x60;

    switch (port) {
	case 0x1ce:
		ret = dev->index;
		break;

	case 0x1cf:
		switch (dev->index) {
			case 0xaa:
				ret = dev->id;
				break;

			case 0xb0:
				if (dev->memory == 1024)
					ret = 0x08;
				else if (dev->memory == 512)
					ret = 0x10;
				else
					ret = 0x00;
				dev->regs[0xb0] |= ret;
				break;

			case 0xb7:
				ret = dev->regs[dev->index] & ~8;
				if (ati_eeprom_read(&dev->eeprom))
					ret |= 8;
				break;

			default:
				ret = dev->regs[dev->index];
				break;
		}
		break;

	case 0x3c2:
		if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x50)
			ret = 0;
		else
			ret = 0x10;
		break;

	case 0x3c6:
	case 0x3c7:
	case 0x3c8:
	case 0x3c9:
		ret = sc1502x_ramdac_in(port, svga->ramdac, svga);

	case 0x3d4:
		ret = svga->crtcreg;
		break;

	case 0x3d5:
		ret = svga->crtc[svga->crtcreg];
		break;

	default:
		ret = svga_in(port, svga);
		break;
    }

    return ret;
}


static uint8_t
ati28800k_in(uint16_t port, priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;
    svga_t *svga = &dev->svga;
    uint16_t oldport = port;
    uint8_t ret = 0xff;

    if (((port&0xfff0) == 0x3d0 ||
	 (port&0xfff0) == 0x3b0) && !(svga->miscout&1)) port ^= 0x60;

    switch (port) {
	case 0x3de:
		if (dev->get_korean_font_enabled) {
			switch(dev->get_korean_font_kind >> 8) {
				case 4: /* ROM font */
					ret = fontdatk[dev->get_korean_font_base].chr[dev->get_korean_font_index++];
					break;

				case 2: /* User defined font */
					if ((dev->get_korean_font_base & 0x7F) > 0x20 && (dev->get_korean_font_base & 0x7F) < 0x7F)
						ret = fontdatk_user[(dev->get_korean_font_kind & 4) * 24 + (dev->get_korean_font_base & 0x7F) - 0x20].chr[dev->get_korean_font_index];
					else
						ret = 0xff;
					dev->get_korean_font_index++;
					break;

				default:
					break;
			}
			dev->get_korean_font_index &= 0x1F;
		}
		break;

	default:
		ret = ati28800_in(oldport, priv);
		break;
    }

    return ret;
}


static void
ati28800_recalctimings(svga_t *svga)
{
    ati28800_t *dev = (ati28800_t *)svga->p;

    switch(((dev->regs[0xbe] & 0x10) >> 1) | ((dev->regs[0xb9] & 2) << 1) | ((svga->miscout & 0x0C) >> 2)) {
	case 0x00: svga->clock = cpu_clock / 42954000.0; break;
	case 0x01: svga->clock = cpu_clock / 48771000.0; break;
#if 0
	case 0x02: INFO("ATI: Clock 2\n", break;
#endif
	case 0x03: svga->clock = cpu_clock / 36000000.0; break;
	case 0x04: svga->clock = cpu_clock / 50350000.0; break;
	case 0x05: svga->clock = cpu_clock / 56640000.0; break;
#if 0
	case 0x06: INFO("ATI: Clock 2\n", break;
#endif
	case 0x07: svga->clock = cpu_clock / 44900000.0; break;
	case 0x08: svga->clock = cpu_clock / 30240000.0; break;
	case 0x09: svga->clock = cpu_clock / 32000000.0; break;
	case 0x0A: svga->clock = cpu_clock / 37500000.0; break;
	case 0x0B: svga->clock = cpu_clock / 39000000.0; break;
	case 0x0C: svga->clock = cpu_clock / 40000000.0; break;
	case 0x0D: svga->clock = cpu_clock / 56644000.0; break;
	case 0x0E: svga->clock = cpu_clock / 75000000.0; break;
	case 0x0F: svga->clock = cpu_clock / 65000000.0; break;
	default: break;
    }

    if (dev->regs[0xb8] & 0x40)
	svga->clock *= 2;

    if (dev->regs[0xb6] & 0x10) {
	svga->hdisp <<= 1;
	svga->htotal <<= 1;
	svga->rowoffset <<= 1;
    }

    if (svga->crtc[0x17] & 4) {
	svga->vtotal <<= 1;
	svga->dispend <<= 1;
	svga->vsyncstart <<= 1;
	svga->split <<= 1;
	svga->vblankstart <<= 1;
    }

    if (!svga->scrblank && (dev->regs[0xb0] & 0x20)) {
	/* Extended 256 color modes. */
	switch (svga->bpp) {
		case 8:
			svga->render = svga_render_8bpp_highres;
			svga->rowoffset <<= 1;
			svga->ma <<= 1;
			break;

		case 15:
			svga->render = svga_render_15bpp_highres;
			svga->hdisp >>= 1;
			svga->rowoffset <<= 1;
			svga->ma <<= 1;
			break;
	}
    }
}


static void
ati28800k_recalctimings(svga_t *svga)
{
    ati28800_t *dev = (ati28800_t *)svga->p;

    ati28800_recalctimings(svga);

    if (svga->render == svga_render_text_80 && dev->ksc5601_mode_enabled) {
	svga->render = svga_render_text_80_ksc5601;
    }
}


/*
 * Also used by the Korean ET4000AX.
 *
 * This will eventually need the 'svga' pointer to store
 * the font data in to, which is global data right now.
 */
int
ati28800k_load_font(svga_t *svga, const wchar_t *fn)
{
    FILE *fp;
    int c, d;

    fp = rom_fopen(fn, L"rb");
    if (fp == NULL) {
	ERRLOG("ATI28800K: cannot load font '%ls'\n", fn);
	return(0);
    }

    if (fontdatk == NULL)
	fontdatk = (dbcs_font_t *)mem_alloc(16384 * sizeof(dbcs_font_t));

    if (fontdatk_user == NULL) {
	c = 192 * sizeof(dbcs_font_t);
	fontdatk_user = (dbcs_font_t *)mem_alloc(c);
	memset(fontdatk_user, 0x00, c);
    }

    for (c = 0; c < 16384; c++) {
	for (d = 0; d < 32; d++)
		fontdatk[c].chr[d] = fgetc(fp);
    }

    (void)fclose(fp);

    return(1);
}


static priv_t
ati28800_init(const device_t *info, UNUSED(void *parent))
{
    ati28800_t *dev;

    dev = (ati28800_t *)mem_alloc(sizeof(ati28800_t));
    memset(dev, 0x00, sizeof(ati28800_t));

    dev->memory = device_get_config_int("memory");

    switch(info->local) {
	case 0:
		dev->id = 5;
		rom_init(&dev->bios_rom, info->path,
			 0xc0000, 0x8000, 0x7fff,
			 0, MEM_MAPPING_EXTERNAL);
		ati_eeprom_load(&dev->eeprom, L"ati28800.nvr", 0);
		break;

	case VID_VGAWONDERXL:
		dev->id = 6;
		rom_init_interleaved(&dev->bios_rom,
				     BIOS_VGAXL_EVEN_PATH,
				     BIOS_VGAXL_ODD_PATH,
				     0xc0000, 0x10000, 0xffff,
				     0, MEM_MAPPING_EXTERNAL);
		ati_eeprom_load(&dev->eeprom, L"ati28800xl.nvr", 0);
		break;

#if defined(DEV_BRANCH) && defined(USE_XL24)
	case VID_VGAWONDERXL24:
		dev->id = 6;
		rom_init_interleaved(&dev->bios_rom,
				     BIOS_XL24_EVEN_PATH,
				     BIOS_XL24_ODD_PATH,
				     0xc0000, 0x10000, 0xffff,
				     0, MEM_MAPPING_EXTERNAL);
		ati_eeprom_load(&dev->eeprom, L"ati28800xl24.nvr", 0);
		break;
#endif

	case VID_ATIKOREANVGA:
		dev->port_03dd_val = 0;
		dev->get_korean_font_base = 0;
		dev->get_korean_font_index = 0;
		dev->get_korean_font_enabled = 0;
		dev->get_korean_font_kind = 0;
		dev->in_get_korean_font_kind_set = 0;
		dev->ksc5601_mode_enabled = 0;

		rom_init(&dev->bios_rom, info->path,
			 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
		ati_eeprom_load(&dev->eeprom, L"atikorvga.nvr", 0);
		break;

	default:
		break;
    }

    if (info->local == VID_ATIKOREANVGA) {
	svga_init(&dev->svga, (priv_t)dev, dev->memory << 10, /*default: 512KB*/
		  ati28800k_recalctimings,
		  ati28800k_in, ati28800k_out, NULL, NULL);
	dev->svga.ksc5601_sbyte_mask = 0;

	dev->svga.ramdac = device_add(&sc1502x_ramdac_device);

	io_sethandler(0x01ce, 2,
		      ati28800k_in,NULL,NULL, ati28800k_out,NULL,NULL, (priv_t)dev);
	io_sethandler(0x03c0, 32,
		      ati28800k_in,NULL,NULL, ati28800k_out,NULL,NULL, (priv_t)dev);

    } else {
	svga_init(&dev->svga, (priv_t)dev, dev->memory << 10, /*default: 512kb*/
		  ati28800_recalctimings,
		  ati28800_in, ati28800_out, NULL, NULL);

	dev->svga.ramdac = device_add(&sc1502x_ramdac_device);

	io_sethandler(0x01ce, 2,
		      ati28800_in,NULL,NULL, ati28800_out,NULL,NULL, (priv_t)dev);
	io_sethandler(0x03c0, 32,
		      ati28800_in,NULL,NULL, ati28800_out,NULL,NULL, (priv_t)dev);
    }

    dev->svga.miscout = 1;

    if (info->local == VID_ATIKOREANVGA) {
	if (! ati28800k_load_font(&dev->svga, FONT_ATIKOR_PATH)) {
		svga_close(&dev->svga);
		free(dev);
		return(NULL);
	}
    }

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


static void
ati28800_close(priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;

    svga_close(&dev->svga);

    free(dev);
}


static int
ati28800k_available(void)
{
    return((rom_present(BIOS_ATIKOR_PATH) &&
	   rom_present(FONT_ATIKOR_PATH)));
}

static int
ati28800_compaq_available(void)
{
    return((rom_present(BIOS_VGAXL_EVEN_PATH) &&
	   rom_present(BIOS_VGAXL_ODD_PATH)));
}

#if defined(DEV_BRANCH) && defined(USE_XL24)
static int
ati28800_wonderxl24_available(void)
{
    return((rom_present(BIOS_XL24_EVEN_PATH) &&
	   rom_present(BIOS_XL24_ODD_PATH)));
}
#endif


static void
speed_changed(priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    ati28800_t *dev = (ati28800_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static const device_config_t ati28800_config[] = {
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

#if defined(DEV_BRANCH) && defined(USE_XL24)
static const device_config_t ati28800_wonderxl_config[] = {
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
#endif


static const video_timings_t ati28800_timing = {VID_ISA,3,3,6,5,5,10};
const device_t ati28800_device = {
    "ATI-28800",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    BIOS_ROM_PATH,
    ati28800_init, ati28800_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ati28800_timing,
    ati28800_config
};

static const video_timings_t ati28800k_timing = {VID_ISA,3,3,6,5,5,10};
const device_t ati28800k_device = {
    "ATI Korean VGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    VID_ATIKOREANVGA,
    BIOS_ATIKOR_PATH,
    ati28800_init, ati28800_close, NULL,
    ati28800k_available,
    speed_changed,
    force_redraw,
    &ati28800k_timing,
    ati28800_config
};

static const video_timings_t ati28800_compaq_timing = {VID_ISA,3,3,6,5,5,10};
const device_t ati28800_compaq_device = {
    "Compaq ATI-28800",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    VID_VGAWONDERXL,
    NULL,
    ati28800_init, ati28800_close, NULL,
    ati28800_compaq_available,
    speed_changed,
    force_redraw,
    &ati28800_compaq_timing,
    ati28800_config
};

#if defined(DEV_BRANCH) && defined(USE_XL24)
static const video_timings_t ati28800w_timing = {VID_ISA,3,3,6,5,5,10};
const device_t ati28800_wonderxl24_device = {
    "ATI-28800 (VGA Wonder XL24)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA | DEVICE_UNSTABLE,
    VID_VGAWONDERXL24,
    NULL,
    ati28800_init, ati28800_close, NULL,
    ati28800_wonderxl24_available,
    speed_changed,
    force_redraw,
    &ati28800w_timing,
    ati28800_wonderxl_config
};
#endif
