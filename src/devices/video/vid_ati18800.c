/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ATI 18800 emulation (VGA Edge-16)
 *
 * Version:	@(#)vid_ati18800.c	1.0.15	2019/05/17
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
#include "../../timer.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_ati.h"


# define BIOS_ROM_PATH_WONDER	L"video/ati/ati18800/vga_wonder_v3-1.02.bin"
#if defined(DEV_BRANCH) && defined(USE_WONDER)
#endif
#define BIOS_ROM_PATH_VGA88	L"video/ati/ati18800/vga88.bin"
#define BIOS_ROM_PATH_EDGE16	L"video/ati/ati18800/vgaedge16.vbi"

enum {
#if defined(DEV_BRANCH) && defined(USE_WONDER)
	ATI18800_WONDER = 0,
	ATI18800_VGA88,
#else
	ATI18800_VGA88 = 0,
#endif
	ATI18800_EDGE16
};


typedef struct ati18800_t
{
        svga_t svga;
        ati_eeprom_t eeprom;

        rom_t bios_rom;
        
        uint8_t regs[256];
        int index;
} ati18800_t;


static void ati18800_out(uint16_t addr, uint8_t val, priv_t priv)
{
        ati18800_t *ati18800 = (ati18800_t *)priv;
        svga_t *svga = &ati18800->svga;
        uint8_t old;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1))
                addr ^= 0x60;

        switch (addr)
        {
                case 0x1ce:
                ati18800->index = val;
                break;
                case 0x1cf:
                ati18800->regs[ati18800->index] = val;
                switch (ati18800->index)
                {
                        case 0xb0:
                        svga_recalctimings(svga);
                        case 0xb2:
                        case 0xbe:
                        if (ati18800->regs[0xbe] & 8) /*Read/write bank mode*/
                        {
                                svga->read_bank  = ((ati18800->regs[0xb2] >> 5) & 7) * 0x10000;
                                svga->write_bank = ((ati18800->regs[0xb2] >> 1) & 7) * 0x10000;
                        }
                        else                    /*Single bank mode*/
                                svga->read_bank = svga->write_bank = ((ati18800->regs[0xb2] >> 1) & 7) * 0x10000;
                        break;
                        case 0xb3:
                        ati_eeprom_write(&ati18800->eeprom, val & 8, val & 2, val & 1);
                        break;
                }
                break;
                
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80) && !(ati18800->regs[0xb4] & 0x80))
                        return;
                if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80) && !(ati18800->regs[0xb4] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                if (old != val)
                {
                        if (svga->crtcreg < 0xe || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

static uint8_t ati18800_in(uint16_t addr, priv_t priv)
{
        ati18800_t *ati18800 = (ati18800_t *)priv;
        svga_t *svga = &ati18800->svga;
        uint8_t temp;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x1ce:
                temp = ati18800->index;
                break;
                case 0x1cf:
                switch (ati18800->index)
                {
                        case 0xb7:
                        temp = ati18800->regs[ati18800->index] & ~8;
                        if (ati_eeprom_read(&ati18800->eeprom))
                                temp |= 8;
                        break;
                        default:
                        temp = ati18800->regs[ati18800->index];
                        break;
                }
                break;

                case 0x3D4:
                temp = svga->crtcreg;
                break;
                case 0x3D5:
                temp = svga->crtc[svga->crtcreg];
                break;
                default:
                temp = svga_in(addr, svga);
                break;
        }
        return temp;
}

static void ati18800_recalctimings(svga_t *svga)
{
        ati18800_t *ati18800 = (ati18800_t *)svga->p;

        if(svga->crtc[0x17] & 4)
        {
                svga->vtotal <<= 1;
                svga->dispend <<= 1;
                svga->vsyncstart <<= 1;
                svga->split <<= 1;
                svga->vblankstart <<= 1;
        }

        if (!svga->scrblank && ((ati18800->regs[0xb0] & 0x02) || (ati18800->regs[0xb0] & 0x04))) /*Extended 256 color modes*/
        {
                svga->render = svga_render_8bpp_highres;
				svga->bpp = 8;
                svga->rowoffset <<= 1;
                svga->ma <<= 1;
        }
}


static priv_t
ati18800_init(const device_t *info, UNUSED(void *parent))
{
    ati18800_t *dev;

    dev = (ati18800_t *)mem_alloc(sizeof(ati18800_t));
    memset(dev, 0x00, sizeof(ati18800_t));

    if (info->path != NULL) {
	rom_init(&dev->bios_rom, info->path, 0xc0000, 0x8000, 0x7fff,
		 0, MEM_MAPPING_EXTERNAL);
    }

    svga_init(&dev->svga, (priv_t)dev, 1 << 20, /*512KB*/
              ati18800_recalctimings, ati18800_in, ati18800_out, NULL, NULL);

    io_sethandler(0x01ce, 0x0002,
		  ati18800_in,NULL,NULL, ati18800_out,NULL,NULL, (priv_t)dev);
    io_sethandler(0x03c0, 0x0020,
		  ati18800_in,NULL,NULL, ati18800_out,NULL,NULL, (priv_t)dev);

    dev->svga.miscout = 1;

    ati_eeprom_load(&dev->eeprom, L"ati18800.nvr", 0);

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return (priv_t)dev;
}


static void
ati18800_close(priv_t priv)
{
    ati18800_t *dev = (ati18800_t *)priv;

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    ati18800_t *dev = (ati18800_t *)priv;
        
    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    ati18800_t *dev = (ati18800_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static const video_timings_t ati18800_timing = {VID_ISA,8,16,32,8,16,32};

#if defined(DEV_BRANCH) && defined(USE_WONDER)
const device_t ati18800_wonder_device = {
    "ATI-18800",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA | DEVICE_UNSTABLE,
    ATI18800_WONDER,
    BIOS_ROM_PATH_WONDER,
    ati18800_init, ati18800_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ati18800_timing,
    NULL
};
#endif

const device_t ati18800_vga88_device = {
    "ATI-18800-1",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    ATI18800_VGA88,
    BIOS_ROM_PATH_VGA88,
    ati18800_init, ati18800_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ati18800_timing,
    NULL
};

const device_t ati18800_device = {
    "ATI-18800-5",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    ATI18800_EDGE16,
    BIOS_ROM_PATH_EDGE16,
    ati18800_init, ati18800_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &ati18800_timing,
    NULL
};
