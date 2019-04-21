/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Paradise VGA emulation
 *		 PC2086, PC3086 use PVGA1A
 *		 MegaPC uses W90C11A
 *
 * NOTE:	The MegaPC video device should be moved to the MegaPC
 *		machine file.
 *
 * Version:	@(#)vid_paradise.c	1.0.10	2019/04/19
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
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"


enum type {
    PVGA1A = 0,
    WD90C11,
    WD90C30
};


typedef struct {
    enum type type;

    svga_t svga;

    rom_t bios_rom;

    uint32_t read_bank[4], write_bank[4];
} paradise_t;


static void paradise_remap(paradise_t *paradise);


static void paradise_out(uint16_t addr, uint8_t val, void *p)
{
        paradise_t *paradise = (paradise_t *)p;
        svga_t *svga = &paradise->svga;
        uint8_t old;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
        switch (addr)
        {
                case 0x3c5:
                if (svga->seqaddr > 7)                        
                {
                        if (paradise->type < WD90C11 || svga->seqregs[6] != 0x48) 
                           return;
                        svga->seqregs[svga->seqaddr & 0x1f] = val;
                        if (svga->seqaddr == 0x11)
                           paradise_remap(paradise);
                        return;
                }
                break;

                case 0x3cf:
                if (svga->gdcaddr >= 0x9 && svga->gdcaddr < 0xf)
                {
                        if ((svga->gdcreg[0xf] & 7) != 5)
                           return;
                }
                if (svga->gdcaddr == 6)
                {
                        if ((svga->gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                switch (val&0xC)
                                {
                                        case 0x0: /*128k at A0000*/
                                        mem_map_set_addr(&svga->mapping, 0xa0000, 0x20000);
                                        svga->banked_mask = 0xffff;
                                        break;
                                        case 0x4: /*64k at A0000*/
                                        mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
                                        svga->banked_mask = 0xffff;
                                        break;
                                        case 0x8: /*32k at B0000*/
                                        mem_map_set_addr(&svga->mapping, 0xb0000, 0x08000);
                                        svga->banked_mask = 0x7fff;
                                        break;
                                        case 0xC: /*32k at B8000*/
                                        mem_map_set_addr(&svga->mapping, 0xb8000, 0x08000);
                                        svga->banked_mask = 0x7fff;
                                        break;
                                }
                        }
                        svga->gdcreg[6] = val;
                        paradise_remap(paradise);
                        return;
                }
                if (svga->gdcaddr == 0x9 || svga->gdcaddr == 0xa)
                {
                        svga->gdcreg[svga->gdcaddr] = val;
                        paradise_remap(paradise);
                        return;
                }
                if (svga->gdcaddr == 0xe)
                {
                        svga->gdcreg[0xe] = val;
                        paradise_remap(paradise);
                        return;
                }
                break;
                
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if ((paradise->type == PVGA1A) && (svga->crtcreg & 0x20))
                        return;
                if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                if (svga->crtcreg > 0x29 && (svga->crtc[0x29] & 7) != 5)
                   return;
                if (svga->crtcreg >= 0x31 && svga->crtcreg <= 0x37)
                   return;
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;

                if (old != val)
                {
                        if (svga->crtcreg < 0xe ||  svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(&paradise->svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

static uint8_t paradise_in(uint16_t addr, void *p)
{
        paradise_t *paradise = (paradise_t *)p;
        svga_t *svga = &paradise->svga;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1))
                addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3c2:
                return 0x10;
                
                case 0x3c5:
                if (svga->seqaddr > 7)
                {
                        if (paradise->type < WD90C11 || svga->seqregs[6] != 0x48) 
                           return 0xff;
                        if (svga->seqaddr > 0x12) 
                           return 0xff;
                        return svga->seqregs[svga->seqaddr & 0x1f];
                }
                break;
                        
                case 0x3cf:
                if (svga->gdcaddr >= 0x9 && svga->gdcaddr < 0xf)
                {
                        if (svga->gdcreg[0xf] & 0x10)
                           return 0xff;
                        switch (svga->gdcaddr)
                        {
                                case 0xf:
                                return (svga->gdcreg[0xf] & 0x17) | 0x80;
                        }
                }
                break;

                case 0x3D4:
                return svga->crtcreg;
                case 0x3D5:
                if ((paradise->type == PVGA1A) && (svga->crtcreg & 0x20))
                        return 0xff;
                if (svga->crtcreg > 0x29 && svga->crtcreg < 0x30 && (svga->crtc[0x29] & 0x88) != 0x80)
                   return 0xff;
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

static void paradise_remap(paradise_t *paradise)
{
        svga_t *svga = &paradise->svga;

	uint8_t mask = (paradise->type == WD90C11) ? 0x7f : 0xff;
        
        if (svga->seqregs[0x11] & 0x80)
        {
                paradise->read_bank[0]  = paradise->read_bank[2]  =  (svga->gdcreg[0x9] & mask) << 12;
                paradise->read_bank[1]  = paradise->read_bank[3]  = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                paradise->write_bank[0] = paradise->write_bank[2] =  (svga->gdcreg[0xa] & mask) << 12;
                paradise->write_bank[1] = paradise->write_bank[3] = ((svga->gdcreg[0xa] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
        }
        else if (svga->gdcreg[0xe] & 0x08)
        {
                if (svga->gdcreg[0x6] & 0xc)
                {
                        paradise->read_bank[0]  = paradise->read_bank[2]  =  (svga->gdcreg[0xa] & mask) << 12;
                        paradise->write_bank[0] = paradise->write_bank[2] =  (svga->gdcreg[0xa] & mask) << 12;
                        paradise->read_bank[1]  = paradise->read_bank[3]  = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                        paradise->write_bank[1] = paradise->write_bank[3] = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                }
                else
                {
                        paradise->read_bank[0] = paradise->write_bank[0] =  (svga->gdcreg[0xa] & mask) << 12;
                        paradise->read_bank[1] = paradise->write_bank[1] = ((svga->gdcreg[0xa] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                        paradise->read_bank[2] = paradise->write_bank[2] =  (svga->gdcreg[0x9] & mask) << 12;
                        paradise->read_bank[3] = paradise->write_bank[3] = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                }
        }
        else
        {
                paradise->read_bank[0]  = paradise->read_bank[2]  =  (svga->gdcreg[0x9] & mask) << 12;
                paradise->read_bank[1]  = paradise->read_bank[3]  = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
                paradise->write_bank[0] = paradise->write_bank[2] =  (svga->gdcreg[0x9] & mask) << 12;
                paradise->write_bank[1] = paradise->write_bank[3] = ((svga->gdcreg[0x9] & mask) << 12) + ((svga->gdcreg[6] & 0x08) ? 0 : 0x8000);
        }
}


static void
paradise_recalctimings(svga_t *svga)
{
    paradise_t *paradise = (paradise_t *) svga->p;

    if (paradise->type == WD90C30)
	svga->interlace = (svga->crtc[0x2d] & 0x20);

    svga->lowres = !(svga->gdcreg[0xe] & 0x01);
    if (svga->bpp == 8 && !svga->lowres)
	svga->render = svga_render_8bpp_highres;
}


static void
paradise_write(uint32_t addr, uint8_t val, void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    addr = (addr & 0x7fff) + paradise->write_bank[(addr >> 15) & 3];

    svga_write_linear(addr, val, &paradise->svga);
}


static void
paradise_writew(uint32_t addr, uint16_t val, void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    addr = (addr & 0x7fff) + paradise->write_bank[(addr >> 15) & 3];
    svga_writew_linear(addr, val, &paradise->svga);
}


static uint8_t
paradise_read(uint32_t addr, void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    addr = (addr & 0x7fff) + paradise->read_bank[(addr >> 15) & 3];

    return svga_read_linear(addr, &paradise->svga);
}


static uint16_t
paradise_readw(uint32_t addr, void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    addr = (addr & 0x7fff) + paradise->read_bank[(addr >> 15) & 3];

    return svga_readw_linear(addr, &paradise->svga);
}


static paradise_t *
pvga1a_init(const device_t *info, uint32_t memsize)
{
    paradise_t *paradise = (paradise_t *)mem_alloc(sizeof(paradise_t));
    svga_t *svga = &paradise->svga;

    memset(paradise, 0, sizeof(paradise_t));
        
    io_sethandler(0x03c0, 0x0020,
		  paradise_in, NULL, NULL, paradise_out, NULL, NULL, paradise);

    svga_init(&paradise->svga, paradise, memsize, /*256kb*/
              NULL,
              paradise_in, paradise_out,
              NULL, NULL);

    mem_map_set_handler(&paradise->svga.mapping,
		        paradise_read, paradise_readw, NULL,
		        paradise_write, paradise_writew, NULL);
    mem_map_set_p(&paradise->svga.mapping, paradise);
        
    svga->crtc[0x31] = 'W';
    svga->crtc[0x32] = 'D';
    svga->crtc[0x33] = '9';
    svga->crtc[0x34] = '0';
    svga->crtc[0x35] = 'C';

    svga->bpp = 8;
    svga->miscout = 1;

    paradise->type = PVGA1A;               
        
    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return paradise;
}


static paradise_t *
wd90c11_init(const device_t *info)
{
    paradise_t *paradise = (paradise_t *)mem_alloc(sizeof(paradise_t));
    svga_t *svga = &paradise->svga;

    memset(paradise, 0, sizeof(paradise_t));
        
    io_sethandler(0x03c0, 0x0020,
		  paradise_in, NULL, NULL, paradise_out, NULL, NULL, paradise);

    svga_init(&paradise->svga, paradise, 1 << 19, /*512kb*/
              paradise_recalctimings,
              paradise_in, paradise_out,
              NULL,
              NULL);

    mem_map_set_handler(&paradise->svga.mapping,
			paradise_read, paradise_readw, NULL,
		        paradise_write, paradise_writew, NULL);
    mem_map_set_p(&paradise->svga.mapping, paradise);

    svga->crtc[0x31] = 'W';
    svga->crtc[0x32] = 'D';
    svga->crtc[0x33] = '9';
    svga->crtc[0x34] = '0';
    svga->crtc[0x35] = 'C';
    svga->crtc[0x36] = '1';
    svga->crtc[0x37] = '1';

    svga->bpp = 8;
    svga->miscout = 1;
        
    paradise->type = WD90C11;               
        
    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return paradise;
}


static paradise_t *
wd90c30_init(const device_t *info, uint32_t memsize)
{
    paradise_t *paradise = (paradise_t *)mem_alloc(sizeof(paradise_t));
    svga_t *svga = &paradise->svga;

    memset(paradise, 0, sizeof(paradise_t));
        
    io_sethandler(0x03c0, 0x0020,
		  paradise_in, NULL, NULL, paradise_out, NULL, NULL, paradise);

    svga_init(&paradise->svga, paradise, memsize,
              paradise_recalctimings,
              paradise_in, paradise_out,
              NULL,
              NULL);

    mem_map_set_handler(&paradise->svga.mapping,
			paradise_read, paradise_readw, NULL,
			paradise_write, paradise_writew, NULL);
    mem_map_set_p(&paradise->svga.mapping, paradise);

    svga->crtc[0x31] = 'W';
    svga->crtc[0x32] = 'D';
    svga->crtc[0x33] = '9';
    svga->crtc[0x34] = '0';
    svga->crtc[0x35] = 'C';
    svga->crtc[0x36] = '3';
    svga->crtc[0x37] = '0';

    svga->bpp = 8;
    svga->miscout = 1;
        
    paradise->type = WD90C11;               
        
    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return paradise;
}


static void *
pvga1a_pc2086_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise = pvga1a_init(info, 1 << 18);

    if (paradise)
                rom_init(&paradise->bios_rom, info->path,
			 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
                
    return paradise;
}


static void *
pvga1a_pc3086_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise = pvga1a_init(info, 1 << 18);

    if (paradise)
	rom_init(&paradise->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
                
    return paradise;
}


static void *
pvga1a_standalone_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise;
    uint32_t memory = 512;

    memory = device_get_config_int("memory");
    memory <<= 10;

    paradise = pvga1a_init(info, memory);
        
    if (paradise)
	rom_init(&paradise->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    return paradise;
}


static void *
wd90c11_megapc_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise = wd90c11_init(info);

    if (paradise)
	rom_init_interleaved(&paradise->bios_rom,
			     L"machines/amstrad/megapc/41651-bios lo.u18",
			     L"machines/amstrad/megapc/211253-bios hi.u19",
			     0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    return paradise;
}


static void *
wd90c11_standalone_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise = wd90c11_init(info);

    if (paradise)
	rom_init(&paradise->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    return paradise;
}


static void *
wd90c30_standalone_init(const device_t *info, UNUSED(void *parent))
{
    paradise_t *paradise;
    uint32_t memory = 512;

    memory = device_get_config_int("memory");
    memory <<= 10;

    paradise = wd90c30_init(info, memory);
        
    if (paradise)
	rom_init(&paradise->bios_rom, L"video/wd/wd90c30/90c30-lr.vbi", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    return paradise;
}


static void
paradise_close(void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    svga_close(&paradise->svga);

    free(paradise);
}


static void
speed_changed(void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    svga_recalctimings(&paradise->svga);
}


static void
force_redraw(void *p)
{
    paradise_t *paradise = (paradise_t *)p;

    paradise->svga.fullchange = changeframecount;
}


static const device_config_t pvga1a_config[] = {
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


static const video_timings_t pvga1a_timing = {VID_ISA,8,16,32,8,16,32};


const device_t paradise_pvga1a_device = {
    "Paradise PVGA1A",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    L"video/paradise/pvga1a/bios.bin",
    pvga1a_standalone_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &pvga1a_timing,
    pvga1a_config
};

const device_t paradise_pvga1a_pc2086_device = {
    "Paradise PVGA1A (Amstrad PC2086)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | 0,
    0,
    L"machines/pc2086/40186.ic171",
    pvga1a_pc2086_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &pvga1a_timing,
    NULL
};

const device_t paradise_pvga1a_pc3086_device = {
    "Paradise PVGA1A (Amstrad PC3086)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | 0,
    0,
    L"machines/pc3086/c000.bin",
    pvga1a_pc3086_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &pvga1a_timing,
    NULL
};


static const video_timings_t wd90c11_timing = {VID_ISA,8,16,32,8,16,32};


const device_t paradise_wd90c11_device = {
    "Paradise WD90C11-LR",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    L"video/wd/wd90c11/wd90c11.vbi",
    wd90c11_standalone_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &wd90c11_timing,
    NULL
};

const device_t paradise_wd90c11_megapc_device = {
    "Paradise WD90C11 (Amstrad MegaPC)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | 0,
    0,
    NULL,
    wd90c11_megapc_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &wd90c11_timing,
    NULL
};


static const device_config_t wd90c30_config[] = {
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

static const video_timings_t wd90c30_timing = {VID_ISA,6,8,16,6,8,16};


const device_t paradise_wd90c30_device = {
    "Paradise WD90C30-LR",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    0,
    L"video/wd/wd90c30/90c30-lr.vbi",
    wd90c30_standalone_init, paradise_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &wd90c30_timing,
    wd90c30_config
};
