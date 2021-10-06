/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the 3DFX Voodoo Graphics Banshee controller.
 *
 * Version:	@(#)vid_voodoo_banshee.c	1.0.1	2021/09/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
 *		Copyright 2008-2020 Sarah Walker.
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
#include <stddef.h>
#include <wchar.h>
#include <math.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../io.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../system/i2c.h"
#include "../system/pci.h"
#include "video.h"
#include "vid_ddc.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_voodoo_banshee.h"
#include "vid_voodoo_common.h"
#include "vid_voodoo_regs.h"
#include "vid_voodoo_render.h"
#include "vid_voodoo_fifo.h"

#ifdef _MSC_VER
# include <malloc.h>
#endif


#ifdef CLAMP
#undef CLAMP
#endif

#define ROM_VOODOO_BANSHEE_PCI	L"video/creativelabs/banshee/blasterpci.rom"

typedef struct banshee_t
{
        svga_t svga;

        rom_t bios_rom;

        uint8_t pci_regs[256];

	int mem_size;

        uint32_t memBaseAddr0;
        uint32_t memBaseAddr1;
        uint32_t ioBaseAddr;

        uint32_t agpInit0;
        uint32_t dramInit0, dramInit1;
        uint32_t lfbMemoryConfig;
        uint32_t miscInit0, miscInit1;
        uint32_t pciInit0;
        uint32_t vgaInit0, vgaInit1;

        uint32_t command_2d;
        uint32_t srcBaseAddr_2d;

        uint32_t pllCtrl0, pllCtrl1, pllCtrl2;

        uint32_t dacMode;
        int dacAddr;

        uint32_t vidDesktopOverlayStride;
        uint32_t vidDesktopStartAddr;
        uint32_t vidProcCfg;
        uint32_t vidScreenSize;

        int overlay_pix_fmt;

        uint32_t hwCurPatAddr, hwCurLoc, hwCurC0, hwCurC1;

        uint32_t intrCtrl;

        uint32_t overlay_buffer[4096];

	uint32_t desktop_addr,
		 desktop_stride_tiled;

	int desktop_y;

        mem_map_t linear_mapping;

        mem_map_t reg_mapping_low;  /*0000000-07fffff*/
        mem_map_t reg_mapping_high; /*0c00000-1ffffff - Windows 2000 puts the BIOS ROM in between these two areas*/

        voodoo_t *voodoo;
	void *i2c, *ddc;
} banshee_t;

static const video_timings_t banshee_pci_timing	= {VID_BUS, 2,  2,  1,  20, 20, 21};

enum
{
        Init_status     = 0x00,
        Init_pciInit0   = 0x04,
        Init_lfbMemoryConfig = 0x0c,
        Init_miscInit0  = 0x10,
        Init_miscInit1  = 0x14,
        Init_dramInit0  = 0x18,
        Init_dramInit1  = 0x1c,
        Init_agpInit0   = 0x20,
        Init_vgaInit0   = 0x28,
        Init_vgaInit1   = 0x2c,
        Init_2dCommand     = 0x30,
        Init_2dSrcBaseAddr = 0x34,
        Init_strapInfo  = 0x38,

        PLL_pllCtrl0    = 0x40,
        PLL_pllCtrl1    = 0x44,
        PLL_pllCtrl2    = 0x48,

        DAC_dacMode     = 0x4c,
        DAC_dacAddr     = 0x50,
        DAC_dacData     = 0x54,

        Video_vidProcCfg = 0x5c,
        Video_hwCurPatAddr = 0x60,
        Video_hwCurLoc     = 0x64,
        Video_hwCurC0      = 0x68,
        Video_hwCurC1      = 0x6c,
        Video_vidScreenSize = 0x98,
        Video_vidOverlayStartCoords = 0x9c,
        Video_vidOverlayEndScreenCoords = 0xa0,
        Video_vidOverlayDudx = 0xa4,
        Video_vidOverlayDudxOffsetSrcWidth = 0xa8,
        Video_vidOverlayDvdy = 0xac,
        Video_vidOverlayDvdyOffset = 0xe0,
        Video_vidDesktopStartAddr = 0xe4,
        Video_vidDesktopOverlayStride = 0xe8
};

enum
{
        cmdBaseAddr0  = 0x20,
        cmdBaseSize0  = 0x24,
        cmdBump0      = 0x28,
        cmdRdPtrL0    = 0x2c,
        cmdRdPtrH0    = 0x30,
        cmdAMin0      = 0x34,
        cmdAMax0      = 0x3c,
        cmdFifoDepth0 = 0x44,
        cmdHoleCnt0   = 0x48
};

#define VGAINIT0_EXTENDED_SHIFT_OUT (1 << 12)

#define VIDPROCCFG_OVERLAY_ENABLE (1 << 8)
#define VIDPROCCFG_H_SCALE_ENABLE (1 << 14)
#define VIDPROCCFG_V_SCALE_ENABLE (1 << 15)
#define VIDPROCCFG_DESKTOP_PIX_FORMAT ((banshee->vidProcCfg >> 18) & 7)
#define VIDPROCCFG_OVERLAY_PIX_FORMAT ((banshee->vidProcCfg >> 21) & 7)
#define VIDPROCCFG_OVERLAY_PIX_FORMAT_SHIFT (21)
#define VIDPROCCFG_OVERLAY_PIX_FORMAT_MASK (7 << VIDPROCCFG_OVERLAY_PIX_FORMAT_SHIFT)
#define VIDPROCCFG_DESKTOP_TILE (1 << 24)
#define VIDPROCCFG_OVERLAY_TILE (1 << 25)
#define VIDPROCCFG_2X_MODE      (1 << 26)
#define VIDPROCCFG_HWCURSOR_ENA (1 << 27)

#define OVERLAY_FMT_565        (1)
#define OVERLAY_FMT_YUYV422    (5)
#define OVERLAY_FMT_UYVY422    (6)
#define OVERLAY_FMT_565_DITHER (7)

#define OVERLAY_START_X_MASK (0xfff)
#define OVERLAY_START_Y_SHIFT (12)
#define OVERLAY_START_Y_MASK (0xfff << OVERLAY_START_Y_SHIFT)

#define OVERLAY_END_X_MASK (0xfff)
#define OVERLAY_END_Y_SHIFT (12)
#define OVERLAY_END_Y_MASK (0xfff << OVERLAY_END_Y_SHIFT)

#define OVERLAY_SRC_WIDTH_SHIFT (19)
#define OVERLAY_SRC_WIDTH_MASK  (0x1fff << OVERLAY_SRC_WIDTH_SHIFT)

#define VID_STRIDE_OVERLAY_SHIFT (16)
#define VID_STRIDE_OVERLAY_MASK (0x7fff << VID_STRIDE_OVERLAY_SHIFT)

#define VID_DUDX_MASK (0xffffff)
#define VID_DVDY_MASK (0xffffff)

#define PIX_FORMAT_8      0
#define PIX_FORMAT_RGB565 1
#define PIX_FORMAT_RGB24  2
#define PIX_FORMAT_RGB32  3

#define CLAMP(x) do                                     \
        {                                               \
                if ((x) & ~0xff)                        \
                        x = ((x) < 0) ? 0 : 0xff;       \
        }                               \
        while (0)

#define DECODE_RGB565()                                                  \
        do                                                              \
        {                                                               \
                int c;                                                  \
                int wp = 0;                                             \
                                                                        \
                for (c = 0; c < voodoo->overlay.overlay_bytes; c += 2)  \
                {                                                       \
                        uint16_t data = *(uint16_t *)src;               \
                        int r = data & 0x1f;                            \
                        int g = (data >> 5) & 0x3f;                     \
                        int b = data >> 11;                             \
                                                                        \
                        banshee->overlay_buffer[wp++] = (r << 3) | (g << 10) | (b << 19); \
                        src += 2;                                       \
                }                                                       \
        } while (0)

#define DECODE_RGB565_TILED()                                           \
        do                                                              \
        {                                                               \
                int c;                                                  \
                int wp = 0;                                             \
                                                                        \
                for (c = 0; c < voodoo->overlay.overlay_bytes; c += 2) \
                {                                                       \
                        uint16_t data = *(uint16_t *)&src[(c & 127) + (c >> 7)*128*32];               \
                        int r = data & 0x1f;                            \
                        int g = (data >> 5) & 0x3f;                     \
                        int b = data >> 11;                             \
                                                                        \
                        banshee->overlay_buffer[wp++] = (r << 3) | (g << 10) | (b << 19); \
                }                                                       \
        } while (0)

#define DECODE_YUYV422()                                                  \
        do                                                              \
        {                                                               \
                int c;                                                  \
                int wp = 0;                                             \
                                                                        \
                for (c = 0; c < voodoo->overlay.overlay_bytes; c += 4)  \
                {                                                       \
                        uint8_t y1, y2;                                 \
                        int8_t Cr, Cb;                                  \
                        int dR, dG, dB;                                 \
                        int r, g, b;                                    \
                                                                        \
                        y1 = src[0];                                    \
                        Cr = src[1] - 0x80;                             \
                        y2 = src[2];                                    \
                        Cb = src[3] - 0x80;                             \
                        src += 4;                                       \
                                                                        \
                        dR = (359*Cr) >> 8;                             \
                        dG = (88*Cb + 183*Cr) >> 8;                     \
                        dB = (453*Cb) >> 8;                             \
                                                                        \
                        r = y1 + dR;                                    \
                        CLAMP(r);                                       \
                        g = y1 - dG;                                    \
                        CLAMP(g);                                       \
                        b = y1 + dB;                                    \
                        CLAMP(b);                                       \
                        banshee->overlay_buffer[wp++] = r | (g << 8) | (b << 16); \
                                                                        \
                        r = y2 + dR;                                    \
                        CLAMP(r);                                       \
                        g = y2 - dG;                                    \
                        CLAMP(g);                                       \
                        b = y2 + dB;                                    \
                        CLAMP(b);                                       \
                        banshee->overlay_buffer[wp++] = r | (g << 8) | (b << 16); \
                }                                                       \
        } while (0)

#define DECODE_UYUV422()                                                  \
        do                                                              \
        {                                                               \
                int c;                                                  \
                int wp = 0;                                             \
                                                                        \
                for (c = 0; c < voodoo->overlay.overlay_bytes; c += 4)  \
                {                                                       \
                        uint8_t y1, y2;                                 \
                        int8_t Cr, Cb;                                  \
                        int dR, dG, dB;                                 \
                        int r, g, b;                                    \
                                                                        \
                        Cr = src[0] - 0x80;                             \
                        y1 = src[1];                                    \
                        Cb = src[2] - 0x80;                             \
                        y2 = src[3];                                    \
                        src += 4;                                       \
                                                                        \
                        dR = (359*Cr) >> 8;                             \
                        dG = (88*Cb + 183*Cr) >> 8;                     \
                        dB = (453*Cb) >> 8;                             \
                                                                        \
                        r = y1 + dR;                                    \
                        CLAMP(r);                                       \
                        g = y1 - dG;                                    \
                        CLAMP(g);                                       \
                        b = y1 + dB;                                    \
                        CLAMP(b);                                       \
                        banshee->overlay_buffer[wp++] = r | (g << 8) | (b << 16); \
                                                                        \
                        r = y2 + dR;                                    \
                        CLAMP(r);                                       \
                        g = y2 - dG;                                    \
                        CLAMP(g);                                       \
                        b = y2 + dB;                                    \
                        CLAMP(b);                                       \
                        banshee->overlay_buffer[wp++] = r | (g << 8) | (b << 16); \
                }                                                       \
        } while (0)

#define OVERLAY_SAMPLE()                        \
        do                                      \
        {                                       \
                switch (banshee->overlay_pix_fmt)       \
                {                                       \
                        case 0:                         \
                        break;                          \
                                                        \
                        case OVERLAY_FMT_YUYV422:       \
                        DECODE_YUYV422();               \
                        break;                          \
                                                        \
                        case OVERLAY_FMT_UYVY422:       \
                        DECODE_UYUV422();               \
                        break;                          \
                                                        \
			case OVERLAY_FMT_565:           \
                        case OVERLAY_FMT_565_DITHER:    \
                        if (banshee->vidProcCfg & VIDPROCCFG_OVERLAY_TILE)      \
                                DECODE_RGB565_TILED();                          \
                        else                                                    \
                                DECODE_RGB565();                                \
                        break;                          \
                                                        \
                        default:                        \
                        fatal("Unknown overlay pix fmt %i\n", banshee->overlay_pix_fmt);        \
                }                                       \
        } while (0)

static uint32_t banshee_status(banshee_t *banshee);

static uint32_t banshee_readl_reg(uint32_t addr, priv_t priv);

static void 
banshee_out(uint16_t addr, uint8_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        svga_t *svga = &banshee->svga;
        uint8_t old;

        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

        switch (addr) {

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
        svga_out(addr, val, svga);
}

static uint8_t 
banshee_in(uint16_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        svga_t *svga = &banshee->svga;
        uint8_t temp;


        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

        switch (addr) {
                case 0x3c2:
                	if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x40)
                        	temp = 0;
                	else
                        	temp = 0x10;
                	break;
                
		case 0x3d4:
                	temp = svga->crtcreg;
                	break;
                case 0x3d5:
                	temp = svga->crtc[svga->crtcreg];
                	break;
                
		default:
                	temp = svga_in(addr, svga);
                	break;
        }

        return temp;
}

static void 
banshee_updatemapping(banshee_t *banshee)
{
        svga_t *svga = &banshee->svga;

        if (!(banshee->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM)) {
                mem_map_disable(&svga->mapping);
                mem_map_disable(&banshee->linear_mapping);
                mem_map_disable(&banshee->reg_mapping_low);
                mem_map_disable(&banshee->reg_mapping_high);
                return;
        }

        DEBUG("Update mapping - bank %02X ", svga->gdcreg[6] & 0xc);        
        switch (svga->gdcreg[6] & 0xc ) { /*Banked framebuffer*/
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

        DEBUG("Linear framebuffer %08X  ", banshee->memBaseAddr1);
        mem_map_set_addr(&banshee->linear_mapping, banshee->memBaseAddr1, 32 << 20);
        DEBUG("registers %08X\n", banshee->memBaseAddr0);
        mem_map_set_addr(&banshee->reg_mapping_low, banshee->memBaseAddr0, 8 << 20);
        mem_map_set_addr(&banshee->reg_mapping_high, banshee->memBaseAddr0 + 0xc00000, 20 << 20);
}

static void 
banshee_render_16bpp_tiled(svga_t *svga)
{
        banshee_t *banshee = (banshee_t *)svga->p;
        int x;
        int offset = 32;
        pel_t *p;
	uint32_t dat;
        uint32_t addr = banshee->desktop_addr + (banshee->desktop_y & 31) * 128 +
			((banshee->desktop_y >> 5) * banshee->desktop_stride_tiled);

        p = &screen->line[svga->displine][offset];

	if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        for (x = 0; x <= svga->hdisp; x += 64) {
                int xx;

                for (xx = 0; xx < 64; xx += 2) {
                        dat = *(uint32_t *)(&svga->vram[(addr + (xx << 1)) & svga->vram_display_mask]);
                        p++->val = video_16to32[dat & 0xffff];
                        p++->val = video_16to32[dat >> 16];
                }

                addr += 128*32;
        }

        banshee->desktop_y++;
}

static void banshee_recalctimings(svga_t *svga)
{
        banshee_t *banshee = (banshee_t *)svga->p;
        voodoo_t *voodoo = banshee->voodoo;

/*7 R/W Horizontal Retrace End bit 5. -
  6 R/W Horizontal Retrace Start bit 8 0x4
  5 R/W Horizontal Blank End bit 6. -
  4 R/W Horizontal Blank Start bit 8. 0x3
  3 R/W Reserved. -
  2 R/W Horizontal Display Enable End bit 8. 0x1
  1 R/W Reserved. -
  0 R/W Horizontal Total bit 8. 0x0*/
        if (svga->crtc[0x1a] & 0x01) svga->htotal      += 0x100;
        if (svga->crtc[0x1a] & 0x04) svga->hdisp       += 0x100;
/*6 R/W Vertical Retrace Start bit 10 0x10
  5 R/W Reserved. -
  4 R/W Vertical Blank Start bit 10. 0x15
  3 R/W Reserved. -
  2 R/W Vertical Display Enable End bit 10 0x12
  1 R/W Reserved. -
  0 R/W Vertical Total bit 10. 0x6*/
        if (svga->crtc[0x1b] & 0x01) svga->vtotal      += 0x400;
        if (svga->crtc[0x1b] & 0x04) svga->dispend     += 0x400;
        if (svga->crtc[0x1b] & 0x10) svga->vblankstart += 0x400;
        if (svga->crtc[0x1b] & 0x40) svga->vsyncstart  += 0x400;
        DEBUG("svga->hdisp=%i\n", svga->hdisp);

        if (banshee->vgaInit0 & VGAINIT0_EXTENDED_SHIFT_OUT)
        {
                switch (VIDPROCCFG_DESKTOP_PIX_FORMAT) {
                        case PIX_FORMAT_8:
                        svga->render = svga_render_8bpp_highres;
                        svga->bpp = 8;
                        break;
                        case PIX_FORMAT_RGB565:
                        svga->render = (banshee->vidProcCfg & VIDPROCCFG_DESKTOP_TILE) ? banshee_render_16bpp_tiled : svga_render_16bpp_highres;
                        svga->bpp = 16;
                        break;
                        case PIX_FORMAT_RGB24:
                        svga->render = svga_render_24bpp_highres;
                        svga->bpp = 24;
                        break;
                        case PIX_FORMAT_RGB32:
                        svga->render = svga_render_32bpp_highres;
                        svga->bpp = 32;
                        break;
                        default:
                        fatal("Unknown pixel format %08x\n", banshee->vgaInit0);
                }
                if (banshee->vidProcCfg & VIDPROCCFG_DESKTOP_TILE)
                        svga->rowoffset = ((banshee->vidDesktopOverlayStride & 0x3fff) * 128) >> 3;
                else
                        svga->rowoffset = (banshee->vidDesktopOverlayStride & 0x3fff) >> 3;
                svga->ma_latch = banshee->vidDesktopStartAddr >> 2;
		banshee->desktop_stride_tiled = (banshee->vidDesktopOverlayStride & 0x3fff) * 128 * 32;
                DEBUG("Extended shift out %i rowoffset=%i %02x\n", VIDPROCCFG_DESKTOP_PIX_FORMAT, svga->rowoffset, svga->crtc[1]);

                svga->char_width = 8;
                svga->split = 99999;

                if (banshee->vidProcCfg & VIDPROCCFG_2X_MODE) {
                        svga->hdisp *= 2;
                        svga->htotal *= 2;
                }

                svga->overlay.ena = banshee->vidProcCfg & VIDPROCCFG_OVERLAY_ENABLE;

                svga->overlay.x = voodoo->overlay.start_x;
                svga->overlay.y = voodoo->overlay.start_y;
                svga->overlay.xsize = voodoo->overlay.size_x;
                svga->overlay.ysize = voodoo->overlay.size_y;
                svga->overlay.pitch = (banshee->vidDesktopOverlayStride & VID_STRIDE_OVERLAY_MASK) >> VID_STRIDE_OVERLAY_SHIFT;
                if (banshee->vidProcCfg & VIDPROCCFG_OVERLAY_TILE)
                        svga->overlay.pitch *= 128*32;
                if (svga->overlay.xsize <= 0 || svga->overlay.ysize <= 0)
                        svga->overlay.ena = 0;
                if (svga->overlay.ena) {
                        DEBUG("Overlay enabled : start=%i,%i end=%i,%i size=%i,%i pitch=%x\n",
                                voodoo->overlay.start_x, voodoo->overlay.start_y,
                                voodoo->overlay.end_x, voodoo->overlay.end_y,
                                voodoo->overlay.size_x, voodoo->overlay.size_y,
                                svga->overlay.pitch);
                }

                svga->set_override = 1;
                svga->video_res_x = svga->hdisp;
                svga->video_res_y = svga->dispend;
                svga->video_bpp = svga->bpp;
        } else {
                DEBUG("Normal shift out\n");
                svga->bpp = 8;
                svga->set_override = 0;
        }

        if (((svga->miscout >> 2) & 3) == 3) {
                int k = banshee->pllCtrl0 & 3;
                int m = (banshee->pllCtrl0 >> 2) & 0x3f;
                int n = (banshee->pllCtrl0 >> 8) & 0xff;
                double freq = (((double)n + 2) / (((double)m + 2) * (double)(1 << k))) * 14318184.0;

                //svga->clock = (cpuclock * (float)(1ull << 32)) / freq;
                svga->clock = cpu_clock / freq;

        }
}

static uint32_t banshee_status(banshee_t *banshee)
{
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;
        int fifo_entries = FIFO_ENTRIES;
        int fifo_size = 0xffff - fifo_entries;
        int swap_count = voodoo->swap_count;
        int written = voodoo->cmd_written + voodoo->cmd_written_fifo;
        int busy = (written - voodoo->cmd_read) || (voodoo->cmdfifo_depth_rd != voodoo->cmdfifo_depth_wr) ||
			voodoo->render_voodoo_busy[0] || voodoo->render_voodoo_busy[1];
        uint32_t ret;

        ret = 0;
        if (fifo_size < 0x20)
                ret |= fifo_size;
        else
                ret |= 0x1f;
        if (fifo_size)
                ret |= 0x20;
        if (swap_count < 7)
                ret |= (swap_count << 28);
        else
                ret |= (7 << 28);
        if (!(svga->cgastat & 8))
                ret |= 0x40;

        if (busy)
                ret |= 0x780; /*Busy*/

        if (voodoo->cmdfifo_depth_rd != voodoo->cmdfifo_depth_wr)
                ret |= (1 << 11);

        if (!voodoo->voodoo_busy)
                voodoo_wake_fifo_thread(voodoo);

        return ret;
}

static void 
banshee_ext_out(uint16_t addr, uint8_t val, priv_t priv)
{

        switch (addr & 0xff) {
                case 0xb0: case 0xb1: case 0xb2: case 0xb3:
                case 0xb4: case 0xb5: case 0xb6: case 0xb7:
                case 0xb8: case 0xb9: case 0xba: case 0xbb:
                case 0xbc: case 0xbd: case 0xbe: case 0xbf:
                case 0xc0: case 0xc1: case 0xc2: case 0xc3:
                case 0xc4: case 0xc5: case 0xc6: case 0xc7:
                case 0xc8: case 0xc9: case 0xca: case 0xcb:
                case 0xcc: case 0xcd: case 0xce: case 0xcf:
                case 0xd0: case 0xd1: case 0xd2: case 0xd3:
                case 0xd4: case 0xd5: case 0xd6: case 0xd7:
                case 0xd8: case 0xd9: case 0xda: case 0xdb:
                case 0xdc: case 0xdd: case 0xde: case 0xdf:
                	banshee_out((addr & 0xff)+0x300, val, priv);
                	break;

                default:
                	DEBUG("bad banshee_ext_out: addr=%04x val=%02x\n", addr, val);
        }
}

static void 
banshee_ext_outl(uint16_t addr, uint32_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        switch (addr & 0xff) {
                case Init_pciInit0:
                banshee->pciInit0 = val;
                voodoo->read_time = pci_nonburst_time + pci_burst_time * ((val & 0x100) ? 2 : 1);
                voodoo->burst_time = pci_burst_time * ((val & 0x200) ? 1 : 0);
                voodoo->write_time = pci_nonburst_time + voodoo->burst_time;
                break;

                case Init_lfbMemoryConfig:
                banshee->lfbMemoryConfig = val;
                voodoo->tile_base = (val & 0x1fff) << 12;
                voodoo->tile_stride = 1024 << ((val >> 13) & 7);
                voodoo->tile_stride_shift = 10 + ((val >> 13) & 7);
                voodoo->tile_x = ((val >> 16) & 0x7f) * 128;
		voodoo->tile_x_real = ((val >> 16) & 0x7f) * 128*32;
                break;

                case Init_miscInit0:
                banshee->miscInit0 = val;
                break;
                case Init_miscInit1:
                banshee->miscInit1 = val;
                break;
                case Init_dramInit0:
                banshee->dramInit0 = val;
                break;
                case Init_dramInit1:
                banshee->dramInit1 = val;
                break;
                case Init_agpInit0:
                banshee->agpInit0 = val;
                break;

                case Init_2dCommand:
                banshee->command_2d = val;
                break;
                case Init_2dSrcBaseAddr:
                banshee->srcBaseAddr_2d = val;
                break;
                case Init_vgaInit0:
                banshee->vgaInit0 = val;
                break;
                case Init_vgaInit1:
                banshee->vgaInit1 = val;
                svga->write_bank = (val & 0x3ff) << 15;
                svga->read_bank = ((val >> 10) & 0x3ff) << 15;
                break;

                case PLL_pllCtrl0:
                banshee->pllCtrl0 = val;
                break;
                case PLL_pllCtrl1:
                banshee->pllCtrl1 = val;
                break;
                case PLL_pllCtrl2:
                banshee->pllCtrl2 = val;
                break;

                case DAC_dacMode:
                banshee->dacMode = val;
                break;
                case DAC_dacAddr:
                banshee->dacAddr = val & 0x1ff;
                break;
                case DAC_dacData:
                svga->pallook[banshee->dacAddr] = val & 0xffffff;
                svga->fullchange = changeframecount;
                break;

                case Video_vidProcCfg:                                
                banshee->vidProcCfg = val;
                banshee->overlay_pix_fmt = (val & VIDPROCCFG_OVERLAY_PIX_FORMAT_MASK) >> VIDPROCCFG_OVERLAY_PIX_FORMAT_SHIFT;
                svga->hwcursor.ena = val & VIDPROCCFG_HWCURSOR_ENA;
                svga->fullchange = changeframecount;
                svga_recalctimings(svga);
                break;

                case Video_hwCurPatAddr:
                banshee->hwCurPatAddr = val;
                svga->hwcursor.addr = val & 0xfffff0;
                break;

                case Video_hwCurLoc:
                banshee->hwCurLoc = val;
                svga->hwcursor.x = (val & 0x7ff) - 32;
                svga->hwcursor.y = ((val >> 16) & 0x7ff) - 64;
                if (svga->hwcursor.y < 0) {
                        svga->hwcursor.yoff = -svga->hwcursor.y;
                        svga->hwcursor.y = 0;
                }
                else
                        svga->hwcursor.yoff = 0;
                svga->hwcursor.xsize = 64;
                svga->hwcursor.ysize = 64;
                break;

                case Video_hwCurC0:
                banshee->hwCurC0 = val;
                break;
                case Video_hwCurC1:
                banshee->hwCurC1 = val;
                break;

                case Video_vidScreenSize:
                banshee->vidScreenSize = val;
                voodoo->h_disp = (val & 0xfff) + 1;
                voodoo->v_disp = (val >> 12) & 0xfff;
                break;
                
		case Video_vidOverlayStartCoords:
                voodoo->overlay.vidOverlayStartCoords = val;
                voodoo->overlay.start_x = val & OVERLAY_START_X_MASK;
                voodoo->overlay.start_y = (val & OVERLAY_START_Y_MASK) >> OVERLAY_START_Y_SHIFT;
                voodoo->overlay.size_x = voodoo->overlay.end_x - voodoo->overlay.start_x;
                voodoo->overlay.size_y = voodoo->overlay.end_y - voodoo->overlay.start_y;
                svga_recalctimings(svga);
                break;
                
		case Video_vidOverlayEndScreenCoords:
                voodoo->overlay.vidOverlayEndScreenCoords = val;
                voodoo->overlay.end_x = val & OVERLAY_END_X_MASK;
                voodoo->overlay.end_y = (val & OVERLAY_END_Y_MASK) >> OVERLAY_END_Y_SHIFT;
                voodoo->overlay.size_x = (voodoo->overlay.end_x - voodoo->overlay.start_x) + 1;
                voodoo->overlay.size_y = (voodoo->overlay.end_y - voodoo->overlay.start_y) + 1;
                svga_recalctimings(svga);
                break;
                
		case Video_vidOverlayDudx:
                voodoo->overlay.vidOverlayDudx = val & VID_DUDX_MASK;
                break;

                case Video_vidOverlayDudxOffsetSrcWidth:
                voodoo->overlay.vidOverlayDudxOffsetSrcWidth = val;
                voodoo->overlay.overlay_bytes = (val & OVERLAY_SRC_WIDTH_MASK) >> OVERLAY_SRC_WIDTH_SHIFT;
                break;

                case Video_vidOverlayDvdy:
                voodoo->overlay.vidOverlayDvdy = val & VID_DVDY_MASK;
                break;

                case Video_vidOverlayDvdyOffset:
                voodoo->overlay.vidOverlayDvdyOffset = val;
                break;

                case Video_vidDesktopStartAddr:
                banshee->vidDesktopStartAddr = val;
                svga->fullchange = changeframecount;
                svga_recalctimings(svga);
                break;

                case Video_vidDesktopOverlayStride:
                banshee->vidDesktopOverlayStride = val;
                svga->fullchange = changeframecount;
                svga_recalctimings(svga);
                break;
                default:
 	        	DEBUG("bad banshee_ext_outl: addr=%04x val=%08x\n", addr, val);
        }
}

static uint8_t 
banshee_ext_in(uint16_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        uint8_t ret = 0xff;

        switch (addr & 0xff) {
                case Init_status: case Init_status+1: case Init_status+2: case Init_status+3:
                ret = (banshee_status(banshee) >> ((addr & 3) * 8)) & 0xff;
                break;

                case 0xb0: case 0xb1: case 0xb2: case 0xb3:
                case 0xb4: case 0xb5: case 0xb6: case 0xb7:
                case 0xb8: case 0xb9: case 0xba: case 0xbb:
                case 0xbc: case 0xbd: case 0xbe: case 0xbf:
                case 0xc0: case 0xc1: case 0xc2: case 0xc3:
                case 0xc4: case 0xc5: case 0xc6: case 0xc7:
                case 0xc8: case 0xc9: case 0xca: case 0xcb:
                case 0xcc: case 0xcd: case 0xce: case 0xcf:
                case 0xd0: case 0xd1: case 0xd2: case 0xd3:
                case 0xd4: case 0xd5: case 0xd6: case 0xd7:
                case 0xd8: case 0xd9: case 0xda: case 0xdb:
                case 0xdc: case 0xdd: case 0xde: case 0xdf:
                ret = banshee_in((addr & 0xff)+0x300, priv);
                break;

                default:
                	DEBUG("bad banshee_ext_in: addr=%04x\n", addr);
                	break;
        }

        return ret;
}

static uint32_t banshee_ext_inl(uint16_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;
        uint32_t ret = 0xffffffff;

        cycles -= voodoo->read_time;

        switch (addr & 0xff) {
                case Init_status:
                ret = banshee_status(banshee);
                break;
                case Init_lfbMemoryConfig:
                ret = banshee->lfbMemoryConfig;
                break;

                case Init_miscInit0:
                ret = banshee->miscInit0;
                break;
                case Init_miscInit1:
                ret = banshee->miscInit1;
                break;
                case Init_dramInit0:
                ret = banshee->dramInit0;
                break;
                case Init_dramInit1:
                ret = banshee->dramInit1;
                break;
                case Init_agpInit0:
                ret = banshee->agpInit0;
                break;

                case Init_vgaInit0:
                ret = banshee->vgaInit0;
                break;
                case Init_vgaInit1:
                ret = banshee->vgaInit1;
                break;

                case Init_2dCommand:
                ret = banshee->command_2d;
                break;
                case Init_2dSrcBaseAddr:
                ret = banshee->srcBaseAddr_2d;
                break;
                case Init_strapInfo:
                ret = 0x00000040; /*8 MB SGRAM, PCI, IRQ enabled, 32kB BIOS*/
                break;

                case PLL_pllCtrl0:
                ret = banshee->pllCtrl0;
                break;
                case PLL_pllCtrl1:
                ret = banshee->pllCtrl1;
                break;
                case PLL_pllCtrl2:
                ret = banshee->pllCtrl2;
                break;

                case DAC_dacMode:
                ret = banshee->dacMode;
                break;
                case DAC_dacAddr:
                ret = banshee->dacAddr;
                break;
                case DAC_dacData:
                ret = svga->pallook[banshee->dacAddr];
                break;

                case Video_vidProcCfg:                                
                ret = banshee->vidProcCfg;
                break;

                case Video_hwCurPatAddr:
                ret = banshee->hwCurPatAddr;
                break;
                case Video_hwCurLoc:
                ret = banshee->hwCurLoc;
                break;
                case Video_hwCurC0:
                ret = banshee->hwCurC0;
                break;
                case Video_hwCurC1:
                ret = banshee->hwCurC1;
                break;

                case Video_vidScreenSize:
                ret = banshee->vidScreenSize;
                break;
                case Video_vidOverlayStartCoords:
                ret = voodoo->overlay.vidOverlayStartCoords;
                break;
                case Video_vidOverlayEndScreenCoords:
                ret = voodoo->overlay.vidOverlayEndScreenCoords;
                break;
                case Video_vidOverlayDudx:
                ret = voodoo->overlay.vidOverlayDudx;
                break;
                case Video_vidOverlayDudxOffsetSrcWidth:
                ret = voodoo->overlay.vidOverlayDudxOffsetSrcWidth;
                break;
                case Video_vidOverlayDvdy:
                ret = voodoo->overlay.vidOverlayDvdy;
                break;
                case Video_vidOverlayDvdyOffset:
                ret = voodoo->overlay.vidOverlayDvdyOffset;
                break;

                case Video_vidDesktopStartAddr:
                ret = banshee->vidDesktopStartAddr;
                break;
                case Video_vidDesktopOverlayStride:
                ret = banshee->vidDesktopOverlayStride;
                break;

                default:
//                fatal("bad banshee_ext_inl: addr=%04x\n", addr);
                break;
        }

        return ret;
}

static uint32_t 
banshee_cmd_read(banshee_t *banshee, uint32_t addr)
{
        voodoo_t *voodoo = banshee->voodoo;
        uint32_t ret = 0xffffffff;

        switch (addr & 0x1fc) {
                case cmdBaseAddr0:
                ret = voodoo->cmdfifo_base >> 12;
                break;

                case cmdRdPtrL0:
                ret = voodoo->cmdfifo_rp;
                break;

                case cmdFifoDepth0:
                ret = voodoo->cmdfifo_depth_wr - voodoo->cmdfifo_depth_rd;
                break;

                case 0x108:
                break;

                default:
                fatal("Unknown banshee_cmd_read %08x\n", addr);
        }

        return ret;
}

static void 
banshee_cmd_write(banshee_t *banshee, uint32_t addr, uint32_t val)
{
        voodoo_t *voodoo = banshee->voodoo;

        switch (addr & 0x1fc) {
                case cmdBaseAddr0:
                voodoo->cmdfifo_base = (val & 0xfff) << 12;
                voodoo->cmdfifo_end = voodoo->cmdfifo_base + (((voodoo->cmdfifo_size & 0xff) + 1) << 12);
                break;

                case cmdBaseSize0:
                voodoo->cmdfifo_size = val;
                voodoo->cmdfifo_end = voodoo->cmdfifo_base + (((voodoo->cmdfifo_size & 0xff) + 1) << 12);
                break;


                case cmdRdPtrL0:
                voodoo->cmdfifo_rp = val;
                break;
                case cmdAMin0:
                voodoo->cmdfifo_amin = val;
                break;
                case cmdAMax0:
                voodoo->cmdfifo_amax = val;
                break;
                case cmdFifoDepth0:
                voodoo->cmdfifo_depth_rd = 0;
                voodoo->cmdfifo_depth_wr = val & 0xffff;
                break;

                default:
                DEBUG("Unknown banshee_cmd_write: addr=%08x val=%08x\n", addr, val);
                break;
        }

/*        cmdBaseSize0  = 0x24,
        cmdBump0      = 0x28,
        cmdRdPtrL0    = 0x2c,
        cmdRdPtrH0    = 0x30,
        cmdAMin0      = 0x34,
        cmdAMax0      = 0x3c,
        cmdFifoDepth0 = 0x44,
        cmdHoleCnt0   = 0x48
        }*/
}

static uint8_t banshee_read_reg(uint32_t addr, priv_t priv)
{
        return banshee_readl_reg(addr & ~3, priv) >> (8*(addr & 3));
}

static uint16_t banshee_readw_reg(uint32_t addr, priv_t priv)
{
        return banshee_readl_reg(addr & ~3, priv) >> (8*(addr & 2));
}

static uint32_t 
banshee_readl_reg(uint32_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        uint32_t ret = 0xffffffff;

        cycles -= voodoo->read_time;

        switch (addr & 0x1f00000) {
                case 0x0000000: /*IO remap*/
                if (!(addr & 0x80000))
                        ret = banshee_ext_inl(addr & 0xff, banshee);
                else
                        ret = banshee_cmd_read(banshee, addr);
                break;

                case 0x0100000: /*2D registers*/
                voodoo_flush(voodoo);
                switch (addr & 0x1fc) {
                        case 0x08:
                        ret = voodoo->banshee_blt.clip0Min;
                        break;
                        case 0x0c:
                        ret = voodoo->banshee_blt.clip0Max;
                        break;
                        case 0x10:
                        ret = voodoo->banshee_blt.dstBaseAddr;
                        break;
                        case 0x14:
                        ret = voodoo->banshee_blt.dstFormat;
                        break;
                        case 0x34:
                        ret = voodoo->banshee_blt.srcBaseAddr;
                        break;
                        case 0x38:
                        ret = voodoo->banshee_blt.commandExtra;
                        break;
                        case 0x5c:
                        ret = voodoo->banshee_blt.srcXY;
                        break;
                        case 0x60:
                        ret = voodoo->banshee_blt.colorBack;
                        break;
                        case 0x64:
                        ret = voodoo->banshee_blt.colorFore;
                        break;
                        case 0x68:
                        ret = voodoo->banshee_blt.dstSize;
                        break;
                        case 0x6c:
                        ret = voodoo->banshee_blt.dstXY;
                        break;
                        case 0x70:
                        ret = voodoo->banshee_blt.command;
                        break;
                        default:
                        DEBUG("banshee_readl_reg: addr=%08x\n", addr);
                }
                break;

                case 0x0200000: case 0x0300000: case 0x0400000: case 0x0500000: /*3D registers*/
                switch (addr & 0x3fc) {
                        case SST_status:
                        ret = banshee_status(banshee);
                        break;

                        case SST_intrCtrl:
                        ret = banshee->intrCtrl & 0x0030003f;
                        break;

                        case SST_fbzColorPath:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.fbzColorPath;
                        break;
                        case SST_fogMode:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.fogMode;
                        break;
                        case SST_alphaMode:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.alphaMode;
                        break;
                        case SST_fbzMode:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.fbzMode;
                        break;
                        case SST_lfbMode:
                        voodoo_flush(voodoo);
                        ret = voodoo->lfbMode;
                        break;
                        case SST_clipLeftRight:
                        ret = voodoo->params.clipRight | (voodoo->params.clipLeft << 16);
                        break;
                        case SST_clipLowYHighY:
                        ret = voodoo->params.clipHighY | (voodoo->params.clipLowY << 16);
                        break;

                        case SST_clipLeftRight1:
                        ret = voodoo->params.clipRight1 | (voodoo->params.clipLeft1 << 16);
                        break;
                        case SST_clipTopBottom1:
                        ret = voodoo->params.clipHighY1 | (voodoo->params.clipLowY1 << 16);
                        break;

                        case SST_stipple:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.stipple;
                        break;
                        case SST_color0:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.color0;
                        break;
                        case SST_color1:
                        voodoo_flush(voodoo);
                        ret = voodoo->params.color1;
                        break;

                        case SST_fbiPixelsIn:
                        ret = voodoo->fbiPixelsIn & 0xffffff;
                        break;
                        case SST_fbiChromaFail:
                        ret = voodoo->fbiChromaFail & 0xffffff;
                        break;
                        case SST_fbiZFuncFail:
                        ret = voodoo->fbiZFuncFail & 0xffffff;
                        break;
                        case SST_fbiAFuncFail:
                        ret = voodoo->fbiAFuncFail & 0xffffff;
                        break;
                        case SST_fbiPixelsOut:
                        ret = voodoo->fbiPixelsOut & 0xffffff;
                        break;

                        default:
                        fatal("banshee_readl_reg: 3D addr=%08x\n", addr);
                        break;
                }
                break;
        }

        return ret;
}

static void 
banshee_write_reg(uint32_t addr, uint8_t val, priv_t priv)
{
//        DEBUG("banshee_writeb_reg: addr=%08x val=%02x\n", addr, val);
}

static void 
banshee_writew_reg(uint32_t addr, uint16_t val, priv_t priv)
{       
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;

        cycles -= voodoo->write_time;

        switch (addr & 0x1f00000) {
                case 0x1000000: case 0x1100000: case 0x1200000: case 0x1300000: /*3D LFB*/
                case 0x1400000: case 0x1500000: case 0x1600000: case 0x1700000:
                case 0x1800000: case 0x1900000: case 0x1a00000: case 0x1b00000:
                case 0x1c00000: case 0x1d00000: case 0x1e00000: case 0x1f00000:
                voodoo_queue_command(voodoo, (addr & 0xffffff) | FIFO_WRITEW_FB, val);
                break;
        }
}

static void 
banshee_writel_reg(uint32_t addr, uint32_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;

        if (addr == voodoo->last_write_addr+4)
                cycles -= voodoo->burst_time;
        else
                cycles -= voodoo->write_time;
        voodoo->last_write_addr = addr;

        switch (addr & 0x1f00000) {
                case 0x0000000: /*IO remap*/
                if (!(addr & 0x80000))
                        banshee_ext_outl(addr & 0xff, val, banshee);
                else
                        banshee_cmd_write(banshee, addr, val);
                break;

                case 0x0100000: /*2D registers*/
                voodoo_queue_command(voodoo, (addr & 0x1fc) | FIFO_WRITEL_2DREG, val);
                break;

                case 0x0200000: case 0x0300000: case 0x0400000: case 0x0500000: /*3D registers*/
                switch (addr & 0x3fc) {
                        case SST_intrCtrl:
                        banshee->intrCtrl = val & 0x0030003f;
                        break;

                        case SST_userIntrCMD:
                        fatal("userIntrCMD write %08x\n", val);
                        break;

                        case SST_swapbufferCMD:
                        voodoo->cmd_written++;
                        voodoo_queue_command(voodoo, (addr & 0x3fc) | FIFO_WRITEL_REG, val);
                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_threads(voodoo->set, voodoo);
                        break;
                        case SST_triangleCMD:
                        voodoo->cmd_written++;
                        voodoo_queue_command(voodoo, (addr & 0x3fc) | FIFO_WRITEL_REG, val);
                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_threads(voodoo->set, voodoo);
                        break;
                        case SST_ftriangleCMD:
                        voodoo->cmd_written++;
                        voodoo_queue_command(voodoo, (addr & 0x3fc) | FIFO_WRITEL_REG, val);
                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_threads(voodoo->set, voodoo);
                        break;
                        case SST_fastfillCMD:
                        voodoo->cmd_written++;
                        voodoo_queue_command(voodoo, (addr & 0x3fc) | FIFO_WRITEL_REG, val);
                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_threads(voodoo->set, voodoo);
                        break;
                        case SST_nopCMD:
                        voodoo->cmd_written++;
                        voodoo_queue_command(voodoo, (addr & 0x3fc) | FIFO_WRITEL_REG, val);
                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_threads(voodoo->set, voodoo);
                        break;

                        case SST_swapPending:
                        voodoo->swap_count++;
                        break;

                        default:
                        voodoo_queue_command(voodoo, (addr & 0x3ffffc) | FIFO_WRITEL_REG, val);
                        break;
                }
                break;

                case 0x0600000: case 0x0700000: /*Texture download*/
                voodoo->tex_count++;
                voodoo_queue_command(voodoo, (addr & 0x1ffffc) | FIFO_WRITEL_TEX, val);
                break;

                case 0x1000000: case 0x1100000: case 0x1200000: case 0x1300000: /*3D LFB*/
                case 0x1400000: case 0x1500000: case 0x1600000: case 0x1700000:
                case 0x1800000: case 0x1900000: case 0x1a00000: case 0x1b00000:
                case 0x1c00000: case 0x1d00000: case 0x1e00000: case 0x1f00000:
                voodoo_queue_command(voodoo, (addr & 0xfffffc) | FIFO_WRITEL_FB, val);
                break;
        }
}

static uint8_t banshee_read_linear(uint32_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        cycles -= voodoo->read_time;

        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;

                addr -= voodoo->tile_base;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) + 
			((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
        }
        if (addr >= svga->vram_max)
                return 0xff;

        //egareads++;
        cycles -= video_timing_read_b;
        //cycles_lost += video_timing_read_b;

        return svga->vram[addr & svga->vram_mask];
}

static uint16_t 
banshee_readw_linear(uint32_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        cycles -= voodoo->read_time;

        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;

                addr -= voodoo->tile_base;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) + 
			((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
        }
        if (addr >= svga->vram_max)
                return 0xff;

        //egareads++;
        cycles -= video_timing_read_w;
        //cycles_lost += video_timing_read_w;

        return *(uint16_t *)&svga->vram[addr & svga->vram_mask];
}

static uint32_t 
banshee_readl_linear(uint32_t addr, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        cycles -= voodoo->read_time;

        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;

                addr -= voodoo->tile_base;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) 
			+ ((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
        }
        if (addr >= svga->vram_max)
                return 0xff;

        //egareads++;
        cycles -= video_timing_read_l;
        //cycles_lost += video_timing_read_l;

        return *(uint32_t *)&svga->vram[addr & svga->vram_mask];
}

static void 
banshee_write_linear(uint32_t addr, uint8_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        cycles -= voodoo->write_time;

        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;
                uint32_t old_addr = addr;

                addr -= voodoo->tile_base;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) 
			+ ((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
        }
        if (addr >= svga->vram_max)
                return;

        //egawrites++;

        cycles -= video_timing_write_b;
        //cycles_lost += video_timing_write_b;

        svga->changedvram[addr >> 12] = changeframecount;
        svga->vram[addr & svga->vram_mask] = val;
}

static void 
banshee_writew_linear(uint32_t addr, uint16_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        cycles -= voodoo->write_time;

        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;
                uint32_t old_addr = addr;

                addr -= voodoo->tile_base;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) 
			+ ((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
        }
        if (addr >= svga->vram_max)
                return;

        //egawrites++;

        cycles -= video_timing_write_w;
        //cycles_lost += video_timing_write_w;

        svga->changedvram[addr >> 12] = changeframecount;
        *(uint16_t *)&svga->vram[addr & svga->vram_mask] = val;
}

static void 
banshee_writel_linear(uint32_t addr, uint32_t val, priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        voodoo_t *voodoo = banshee->voodoo;
        svga_t *svga = &banshee->svga;

        if (addr == voodoo->last_write_addr+4)
                cycles -= voodoo->burst_time;
        else
                cycles -= voodoo->write_time;
        voodoo->last_write_addr = addr;

//        /*if (val) */pclog("write_linear_l: addr=%08x val=%08x  %08x\n", addr, val, voodoo->tile_base);
        addr &= svga->decode_mask;
        if (addr >= voodoo->tile_base) {
                int x, y;
                uint32_t old_addr = addr;
                uint32_t addr_off;
                uint32_t addr2;

                addr -= voodoo->tile_base;
                addr_off = addr;
                x = addr & (voodoo->tile_stride-1);
                y = addr >> voodoo->tile_stride_shift;

                addr = voodoo->tile_base + (x & 127) + ((x >> 7) * 128*32) 
			+ ((y & 31) * 128) + (y >> 5)*voodoo->tile_x_real;
                addr2 = x + y*voodoo->tile_x;
//              DEBUG("  Tile %08x->%08x->%08x->%08x %i %i  stride=%i tile_x=%i\n", old_addr, addr_off, addr2, addr, x, y, voodoo->tile_x_real);
        }

        if (addr >= svga->vram_max)
                return;

        //egawrites += 4;

        cycles -= video_timing_write_l;
        //cycles_lost += video_timing_write_l;

        svga->changedvram[addr >> 12] = changeframecount;
        *(uint32_t *)&svga->vram[addr & svga->vram_mask] = val;
        if (addr >= voodoo->cmdfifo_base && addr < voodoo->cmdfifo_end) {
                //DEBUG("CMDFIFO write %08x %08x  old amin=%08x amax=%08x hlcnt=%i depth_wr=%i rp=%08x\n", addr, val, voodoo->cmdfifo_amin, voodoo->cmdfifo_amax, voodoo->cmdfifo_holecount, voodoo->cmdfifo_depth_wr, voodoo->cmdfifo_rp);
                if (addr == voodoo->cmdfifo_base && !voodoo->cmdfifo_holecount) {
//                        if (voodoo->cmdfifo_holecount)
//                                fatal("CMDFIFO reset pointers while outstanding holes\n");
                        /*Reset pointers*/
                        voodoo->cmdfifo_amin = voodoo->cmdfifo_base;
                        voodoo->cmdfifo_amax = voodoo->cmdfifo_base;
                        voodoo->cmdfifo_depth_wr++;
                        voodoo_wake_fifo_thread(voodoo);
                }
                else if (voodoo->cmdfifo_holecount) {
//                      if ((addr <= voodoo->cmdfifo_amin && voodoo->cmdfifo_amin != -4) || addr >= voodoo->cmdfifo_amax)
//                                fatal("CMDFIFO holecount write outside of amin/amax - amin=%08x amax=%08x holecount=%i\n", voodoo->cmdfifo_amin, voodoo->cmdfifo_amax, voodoo->cmdfifo_holecount);
//                      DEBUG("holecount %i\n", voodoo->cmdfifo_holecount);
                        voodoo->cmdfifo_holecount--;
                        if (!voodoo->cmdfifo_holecount) {
                                int words_to_add = ((voodoo->cmdfifo_amax - voodoo->cmdfifo_amin) >> 2) + 1;
                                /*Filled in holes, resume normal operation*/
                                voodoo->cmdfifo_depth_wr += ((voodoo->cmdfifo_amax - voodoo->cmdfifo_amin) >> 2);
                                voodoo->cmdfifo_amin = voodoo->cmdfifo_amax;
                                voodoo_wake_fifo_thread(voodoo);
//                              DEBUG("hole filled! amin=%08x amax=%08x added %i words\n", voodoo->cmdfifo_amin, voodoo->cmdfifo_amax, words_to_add);
                        }
                }
                else if (addr == voodoo->cmdfifo_amax+4)
                {
                        /*In-order write*/
                        voodoo->cmdfifo_amin = addr;
                        voodoo->cmdfifo_amax = addr;
                        voodoo->cmdfifo_depth_wr++;
                        voodoo_wake_fifo_thread(voodoo);
		} else {
                        /*Out-of-order write*/
                        if (addr < voodoo->cmdfifo_amin) {
                                /*Reset back to start. Note that write is still out of order!*/
                                voodoo->cmdfifo_amin = voodoo->cmdfifo_base-4;
                        }
//                      else if (addr < voodoo->cmdfifo_amax)
//                                fatal("Out-of-order write really out of order\n");
                        voodoo->cmdfifo_amax = addr;
                        voodoo->cmdfifo_holecount = ((voodoo->cmdfifo_amax - voodoo->cmdfifo_amin) >> 2) - 1;
//                      DEBUG("CMDFIFO out of order: amin=%08x amax=%08x holecount=%i\n", voodoo->cmdfifo_amin, voodoo->cmdfifo_amax, voodoo->cmdfifo_holecount);
                }
        }
}

void 
banshee_hwcursor_draw(svga_t *svga, int displine)
{
        banshee_t *banshee = (banshee_t *)svga->p;
        int x, c;
        int x_off;
        uint32_t col0 = banshee->hwCurC0;
        uint32_t col1 = banshee->hwCurC1;
        uint8_t plane0[8], plane1[8];

        for (c = 0; c < 8; c++)
                plane0[c] = svga->vram[svga->hwcursor_latch.addr + c];
        for (c = 0; c < 8; c++)
                plane1[c] = svga->vram[svga->hwcursor_latch.addr + c + 8];
        svga->hwcursor_latch.addr += 16;

        x_off = svga->hwcursor_latch.x;

        for (x = 0; x < 64; x += 8) {
                if (x_off > (32-8)) {
                        int xx;

                        for (xx = 0; xx < 8; xx++) {
                                if (!(plane0[x >> 3] & (1 << 7)))
                                        screen->line[displine][x_off + xx].val = (plane1[x >> 3] & (1 << 7)) ? col1 : col0;
                                else if (plane1[x >> 3] & (1 << 7))
                                        screen->line[displine][x_off + xx].val ^= 0xffffff;

                                plane0[x >> 3] <<= 1;
                                plane1[x >> 3] <<= 1;
                        }
                }

                x_off += 8;
        }
}

static void 
banshee_overlay_draw(svga_t *svga, int displine)
{
        banshee_t *banshee = (banshee_t *)svga->p;
        voodoo_t *voodoo = banshee->voodoo;
        pel_t *p;
	int x;
        int y = voodoo->overlay.src_y >> 20;
        uint32_t src_addr = svga->overlay_latch.addr + ((banshee->vidProcCfg & VIDPROCCFG_OVERLAY_TILE) ?
                ((y & 31) * 128 + (y >> 5) * svga->overlay_latch.pitch) :
                y * svga->overlay_latch.pitch);	
        uint8_t *src = &svga->vram[src_addr & svga->vram_mask];
	//int y_add = enable_overscan ? 16 : 0;
	//int x_add = enable_overscan ? 8 : 0;
        uint32_t src_x = 0;

        p = &screen->line[displine][svga->overlay_latch.x + 32];
	//p = &screen->line[displine + y_add][offset + 32 + x_add];

        OVERLAY_SAMPLE();

        if (banshee->vidProcCfg & VIDPROCCFG_H_SCALE_ENABLE) {
                for (x = 0; x < svga->overlay_latch.xsize; x++) {
                        p[x].val = banshee->overlay_buffer[src_x >> 20];
                        src_x += voodoo->overlay.vidOverlayDudx;
                }
        } else {
                for (x = 0; x < svga->overlay_latch.xsize; x++)
                        p[x].val = banshee->overlay_buffer[x];
        }

        if (banshee->vidProcCfg & VIDPROCCFG_V_SCALE_ENABLE)
                voodoo->overlay.src_y += voodoo->overlay.vidOverlayDvdy;
        else
                voodoo->overlay.src_y += (1 << 20);
}

void 
banshee_set_overlay_addr(priv_t priv, uint32_t addr)
{
        banshee_t *banshee = (banshee_t *)priv;

        banshee->svga.overlay.addr = banshee->voodoo->leftOverlayBuf & 0xfffffff;
        banshee->svga.overlay_latch.addr = banshee->voodoo->leftOverlayBuf & 0xfffffff;
}

static void 
banshee_vsync_callback(svga_t *svga)
{
        banshee_t *banshee = (banshee_t *)svga->p;
        voodoo_t *voodoo = banshee->voodoo;

        voodoo->retrace_count++;
        if (voodoo->swap_pending && (voodoo->retrace_count > voodoo->swap_interval)) {
                memset(voodoo->dirty_line, 1, 1024);
                voodoo->retrace_count = 0;
                banshee_set_overlay_addr(banshee, voodoo->swap_offset);
                if (voodoo->swap_count > 0)
                        voodoo->swap_count--;
                voodoo->swap_pending = 0;
                thread_set_event(voodoo->wake_fifo_thread);
                voodoo->frame_count++;
        }

        voodoo->overlay.src_y = 0;
	banshee->desktop_addr = banshee->vidDesktopStartAddr;
        banshee->desktop_y = 0;
}

uint8_t banshee_pci_read(int func, int addr, void *priv)
{
        banshee_t *banshee = (banshee_t *)priv;

        uint8_t ret = 0;

        DEBUG("Banshee PCI read %08X  ", addr);
        switch (addr) {
                case 0x00: ret = 0x1a; break; /*3DFX*/
                case 0x01: ret = 0x12; break;

                case 0x02: ret = 0x03; break;
                case 0x03: ret = 0x00; break;

                case 0x04: ret = banshee->pci_regs[0x04] & 0x27; break;

                case 0x07: ret = banshee->pci_regs[0x07] & 0x36; break;

                case 0x08: ret = 1; break; /*Revision ID*/
                case 0x09: ret = 0; break; /*Programming interface*/

                case 0x0a: ret = 0x00; break; /*Supports VGA interface*/
                case 0x0b: ret = 0x03; /*output = 3; */break;

                case 0x0d: ret = banshee->pci_regs[0x0d] & 0xf8; break;

                case 0x10: ret = 0x00; break; /*memBaseAddr0*/
                case 0x11: ret = 0x00; break;
                case 0x12: ret = 0x00; break;
                case 0x13: ret = banshee->memBaseAddr0 >> 24; break;

                case 0x14: ret = 0x00; break; /*memBaseAddr1*/
                case 0x15: ret = 0x00; break;
                case 0x16: ret = 0x00; break;
                case 0x17: ret = banshee->memBaseAddr1 >> 24; break;

                case 0x18: ret = 0x01; break; /*ioBaseAddr*/
                case 0x19: ret = banshee->ioBaseAddr >> 8; break;
                case 0x1a: ret = 0x00; break;
                case 0x1b: ret = 0x00; break;

                case 0x30: ret = banshee->pci_regs[0x30] & 0x01; break; /*BIOS ROM address*/
                case 0x31: ret = 0x00; break;
                case 0x32: ret = banshee->pci_regs[0x32]; break;
                case 0x33: ret = banshee->pci_regs[0x33]; break;

                case 0x3c: ret = banshee->pci_regs[0x3c]; break;

                case 0x3d: ret = 0x01; break; /*INTA*/

                case 0x3e: ret = 0x04; break;
                case 0x3f: ret = 0xff; break;

        }
        DEBUG("%02X\n", ret);
        return ret;

}

void banshee_pci_write(int func, int addr, uint8_t val, void *priv)
{
        banshee_t *banshee = (banshee_t *)priv;
        
        DEBUG("Banshee PCI write %08X %02X %04X:%08X\n", addr, val, CS, cpu_state.pc);
        switch (addr) {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x3d: case 0x3e: case 0x3f:
                return;

                case PCI_REG_COMMAND:
                if (val & PCI_COMMAND_IO) {
                        io_removehandler(0x03c0, 0x0020, banshee_in, NULL, NULL, banshee_out, NULL, NULL, banshee);
                        if (banshee->ioBaseAddr)
                                io_removehandler(banshee->ioBaseAddr, 0x0100, banshee_ext_in, NULL, banshee_ext_inl, banshee_ext_out, NULL, banshee_ext_outl, banshee);

                        io_sethandler(0x03c0, 0x0020, banshee_in, NULL, NULL, banshee_out, NULL, NULL, banshee);
                        if (banshee->ioBaseAddr)
                                io_sethandler(banshee->ioBaseAddr, 0x0100, banshee_ext_in, NULL, banshee_ext_inl, banshee_ext_out, NULL, banshee_ext_outl, banshee);
                } else {
                        io_removehandler(0x03c0, 0x0020, banshee_in, NULL, NULL, banshee_out, NULL, NULL, banshee);
                        io_removehandler(banshee->ioBaseAddr, 0x0100, banshee_ext_in, NULL, banshee_ext_inl, banshee_ext_out, NULL, banshee_ext_outl, banshee);
                }
                banshee->pci_regs[PCI_REG_COMMAND] = val & 0x27;
		banshee_updatemapping(banshee);
                return;
                case 0x07:
                banshee->pci_regs[0x07] = val & 0x3e;
                return;
                case 0x0d: 
                banshee->pci_regs[0x0d] = val & 0xf8;
                return;

                case 0x13:
                banshee->memBaseAddr0 = (val & 0xfe) << 24;
                banshee_updatemapping(banshee); 
                return;

                case 0x17:
                banshee->memBaseAddr1 = (val & 0xfe) << 24;
                banshee_updatemapping(banshee);
                return;

                case 0x19:
                if (banshee->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO)
                        io_removehandler(banshee->ioBaseAddr, 0x0100, banshee_ext_in, NULL, banshee_ext_inl, banshee_ext_out, NULL, banshee_ext_outl, banshee);
                banshee->ioBaseAddr = val << 8;
                if ((banshee->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO) && banshee->ioBaseAddr)
                        io_sethandler(banshee->ioBaseAddr, 0x0100, banshee_ext_in, NULL, banshee_ext_inl, banshee_ext_out, NULL, banshee_ext_outl, banshee);
                DEBUG("Banshee ioBaseAddr=%08x\n", banshee->ioBaseAddr); 
                return;

                case 0x30: case 0x32: case 0x33:
                banshee->pci_regs[addr] = val;
                if (banshee->pci_regs[0x30] & 0x01) {
                        uint32_t addr = (banshee->pci_regs[0x32] << 16) | (banshee->pci_regs[0x33] << 24);
                        DEBUG("Banshee bios_rom enabled at %08x\n", addr);
                        mem_map_set_addr(&banshee->bios_rom.mapping, addr, 0x8000);
                        mem_map_enable(&banshee->bios_rom.mapping);
                } else {
                        DEBUG("Banshee bios_rom disabled\n");
                        mem_map_disable(&banshee->bios_rom.mapping);
                }
                return;
                case 0x3c: 
                banshee->pci_regs[0x3c] = val;
                return;
        }
}

static priv_t 
banshee_init(const device_t *info, UNUSED(void *parent))
{
        banshee_t *banshee;

        banshee = (banshee_t *)mem_alloc(sizeof(banshee_t));
        memset(banshee, 0, sizeof(banshee_t));

        rom_init(&banshee->bios_rom, info->path, 
	     0xc0000, 0x10000, 0xffff, 0, MEM_MAPPING_EXTERNAL);
	mem_map_disable(&banshee->bios_rom.mapping);

        banshee->mem_size = device_get_config_int("Memory");

        svga_init(&banshee->svga, banshee, (banshee->mem_size << 20),
                   banshee_recalctimings,
                   banshee_in, banshee_out,
                   banshee_hwcursor_draw,
                   banshee_overlay_draw);

        banshee->svga.vsync_callback = banshee_vsync_callback;
	
        mem_map_add(&banshee->linear_mapping, 0, 0, 
		banshee_read_linear,  banshee_readw_linear,  banshee_readl_linear,
                banshee_write_linear, banshee_writew_linear, banshee_writel_linear,
                NULL, MEM_MAPPING_EXTERNAL, &banshee->svga);
        mem_map_add(&banshee->reg_mapping_low, 0, 0,
		banshee_read_reg,  banshee_readw_reg,  banshee_readl_reg,
                banshee_write_reg, banshee_writew_reg, banshee_writel_reg,
                NULL, MEM_MAPPING_EXTERNAL, banshee);
        mem_map_add(&banshee->reg_mapping_high, 0,0,
            	banshee_read_reg,  banshee_readw_reg,  banshee_readl_reg,
                banshee_write_reg, banshee_writew_reg, banshee_writel_reg,
                NULL, MEM_MAPPING_EXTERNAL, banshee);

	//banshee->svga.vblank_start = banshee_vblank_start;

//        io_sethandler(0x03c0, 0x0020, 
//		banshee_in, NULL, NULL, banshee_out, NULL, NULL, banshee);

	video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

        banshee->svga.bpp = 8;
        banshee->svga.miscout = 1;

        banshee->dramInit0 = 1 << 27;
        if (banshee->mem_size == 16)
                banshee->dramInit0 |= (1 << 26); /*2xSGRAM = 16 MB*/
//        banshee->dramInit1 = 1 << 30; /*SDRAM*/
        banshee->svga.decode_mask = 0x1ffffff;

        pci_add_card(PCI_ADD_VIDEO, banshee_pci_read, banshee_pci_write, banshee);

        banshee->voodoo = voodoo_2d3d_card_init(VOODOO_BANSHEE);
        banshee->voodoo->priv = banshee;
        banshee->voodoo->vram = banshee->svga.vram;
        banshee->voodoo->changedvram = banshee->svga.changedvram;
        banshee->voodoo->fb_mem = banshee->svga.vram;
        banshee->voodoo->fb_mask = banshee->svga.vram_mask;
        banshee->voodoo->tex_mem[0] = banshee->svga.vram;
        banshee->voodoo->tex_mem_w[0] = (uint16_t *)banshee->svga.vram;
        banshee->voodoo->texture_mask = banshee->svga.vram_mask;
	//voodoo_generate_filter_v1(banshee->voodoo);
	
	banshee->i2c = i2c_gpio_init("i2c_voodoo_banshee");
        //banshee->i2c_ddc = i2c_gpio_init("ddc_voodoo_banshee");
        //banshee->ddc = ddc_init(i2c_gpio_get_bus(banshee->i2c_ddc));

        return (priv_t)banshee;
}

static void
banshee_close(priv_t priv)
{
    banshee_t *banshee = (banshee_t *)priv;

    voodoo_card_close(banshee->voodoo);
    svga_close(&banshee->svga);

    //ddc_close(banshee->ddc);
    //i2c_gpio_close(banshee->i2c);

    free(banshee);
}

static void
banshee_speed_changed(priv_t priv)
{
    banshee_t *banshee = (banshee_t *)priv;

    svga_recalctimings(&banshee->svga);
}

static void 
banshee_force_redraw(priv_t priv)
{
        banshee_t *banshee = (banshee_t *)priv;

        banshee->svga.fullchange = changeframecount;
}

static const device_config_t banshee_config[] =
{
        {
                "Memory","Memory size",CONFIG_SELECTION,"",16,
                {
                        {
                                "8 MB",8
                        },
                        {
                                "16 MB",16
                        },
                        {
                                NULL
                        }
                },
        },
        {
                "bilinear","Bilinear filtering",CONFIG_BINARY,"",1
        },
        {
                "render_threads","Render threads",CONFIG_SELECTION,"",2,
                {
                        {
                                "1",1
                        },
                        {
                                "2",2
                        },
                        {
                                NULL
                        }
                },
        },
#ifndef NO_CODEGEN
        {
                "recompiler","Recompiler",CONFIG_BINARY,"",1
        },
#endif
        {
                NULL
        }
};

const device_t voodoo_banshee_device = {
    "Creative Labs 3D Blaster Banshee PCI",
    DEVICE_PCI,
    0,
    ROM_VOODOO_BANSHEE_PCI,
    banshee_init, banshee_close, NULL,
    NULL,
    banshee_speed_changed,
    banshee_force_redraw,
    &banshee_pci_timing,
    banshee_config
};
