/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Trident TGUI9400CXi and TGUI9440 emulation.
 *
 *		TGUI9400CXi has extended write modes, controlled by extended
 *		GDC registers :
 *
 *		GDC[0x10] - Control
 *		      bit 0 - pixel width (1 = 16 bit, 0 = 8 bit)
 *		      bit 1 - mono->colour expansion (1 = enabled,
 *			      0 = disabled)
 *		      bit 2 - mono->colour expansion transparency
 *			      (1 = transparent, 0 = opaque)
 *		      bit 3 - extended latch copy
 *		GDC[0x11] - Background colour (low byte)
 *		GDC[0x12] - Background colour (high byte)
 *		GDC[0x14] - Foreground colour (low byte)
 *		GDC[0x15] - Foreground colour (high byte)
 *		GDC[0x17] - Write mask (low byte)
 *		GDC[0x18] - Write mask (high byte)
 *
 *		Mono->colour expansion will expand written data 8:1 to 8/16
 *		consecutive bytes.
 *		MSB is processed first. On word writes, low byte is processed
 *		first. 1 bits write foreground colour, 0 bits write background
 *		colour unless transparency is enabled.
 *		If the relevant bit is clear in the write mask then the data
 *		is not written.
 *
 *		With 16-bit pixel width, each bit still expands to one byte,
 *		so the TGUI driver doubles up monochrome data.
 *
 *		While there is room in the register map for three byte colours,
 *		I don't believe 24-bit colour is supported. The TGUI9440
 *		blitter has the same limitation.
 *
 *		I don't think double word writes are supported.
 *
 *		Extended latch copy uses an internal 16 byte latch. Reads load
 *		the latch, writing writes out 16 bytes. I don't think the
 *		access size or host data has any affect, but the Windows 3.1
 *		driver always reads bytes and write words of 0xffff.
 *
 * Version:	@(#)vid_tgui9440.c	1.0.19	2021/03/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../system/pci.h"
#include "../system/i2c.h"
#include "video.h"
#include "vid_ddc.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_tkd8001_ramdac.h"


#define ROM_TGUI_9400CXI	L"video/trident/tgui9440/9400cxi.vbi"
#define ROM_TGUI_9440		L"video/trident/tgui9440/9440.vbi"


/*TGUI9400CXi has extended write modes, controlled by extended GDC registers :
        
  GDC[0x10] - Control
        bit 0 - pixel width (1 = 16 bit, 0 = 8 bit)
        bit 1 - mono->colour expansion (1 = enabled, 0 = disabled)
        bit 2 - mono->colour expansion transparency (1 = tranparent, 0 = opaque)
        bit 3 - extended latch copy
  GDC[0x11] - Background colour (low byte)
  GDC[0x12] - Background colour (high byte)
  GDC[0x14] - Foreground colour (low byte)
  GDC[0x15] - Foreground colour (high byte)
  GDC[0x17] - Write mask (low byte)
  GDC[0x18] - Write mask (high byte)
  
  Mono->colour expansion will expand written data 8:1 to 8/16 consecutive bytes.
  MSB is processed first. On word writes, low byte is processed first. 1 bits write
  foreground colour, 0 bits write background colour unless transparency is enabled.
  If the relevant bit is clear in the write mask then the data is not written.

  With 16-bit pixel width, each bit still expands to one byte, so the TGUI driver
  doubles up monochrome data.
  
  While there is room in the register map for three byte colours, I don't believe
  24-bit colour is supported. The TGUI9440 blitter has the same limitation.
  
  I don't think double word writes are supported.
  
  Extended latch copy uses an internal 16 byte latch. Reads load the latch, writing
  writes out 16 bytes. I don't think the access size or host data has any affect,
  but the Windows 3.1 driver always reads bytes and write words of 0xffff.*/  

#define EXT_CTRL_16BIT            0x01
#define EXT_CTRL_MONO_EXPANSION   0x02
#define EXT_CTRL_MONO_TRANSPARENT 0x04
#define EXT_CTRL_LATCH_COPY       0x08

#define FIFO_SIZE 65536
#define FIFO_MASK (FIFO_SIZE - 1)
#define FIFO_ENTRY_SIZE (1 << 31)

#define FIFO_ENTRIES (dev->fifo_write_idx - dev->fifo_read_idx)
#define FIFO_FULL    ((dev->fifo_write_idx - dev->fifo_read_idx) >= FIFO_SIZE)
#define FIFO_EMPTY   (dev->fifo_read_idx == dev->fifo_write_idx)

#define FIFO_TYPE 0xff000000
#define FIFO_ADDR 0x00ffffff

enum
{
        TGUI_9400CXI = 0,
        TGUI_9440
};

enum
{
        FIFO_INVALID       = (0x00 << 24),
        FIFO_WRITE_BYTE    = (0x01 << 24),
        FIFO_WRITE_FB_BYTE = (0x04 << 24),
        FIFO_WRITE_FB_WORD = (0x05 << 24),
        FIFO_WRITE_FB_LONG = (0x06 << 24)
};

typedef struct
{
        uint32_t addr_type;
        uint32_t val;
} fifo_entry_t;

typedef struct tgui_t
{
        mem_map_t linear_mapping;
        mem_map_t accel_mapping;

        rom_t bios_rom;

        svga_t svga;
	int pci;

        int type;

        struct
        {
        	uint16_t src_x, src_y;
        	uint16_t dst_x, dst_y;
        	uint16_t size_x, size_y;
        	uint16_t fg_col, bg_col;
        	uint8_t rop;
        	uint16_t flags;
        	uint8_t pattern[0x80];
        	int command;
        	int offset;
        	uint8_t ger22;

        	int x, y;
        	uint32_t src, dst, src_old, dst_old;
        	int pat_x, pat_y;
        	int use_src;

        	int pitch, bpp;

                uint16_t tgui_pattern[8][8];
        } accel;

        uint8_t ext_gdc_regs[16]; /*TGUI9400CXi only*/
        uint8_t copy_latch[16];

        uint8_t tgui_3d8, tgui_3d9;
        int oldmode;
        uint8_t oldctrl1;
        uint8_t oldctrl2,newctrl2;

        uint32_t linear_base, linear_size;

        int ramdac_state;
        uint8_t ramdac_ctrl;

        int clock_m, clock_n, clock_k;

        uint32_t vram_size, vram_mask;

        fifo_entry_t fifo[FIFO_SIZE];
        volatile int fifo_read_idx, fifo_write_idx;

        thread_t *fifo_thread;
        event_t *wake_fifo_thread;
        event_t *fifo_not_full_event;

        int blitter_busy;
        uint64_t blitter_time;
        uint64_t status_time;

        volatile int write_blitter;

        void *i2c, *ddc;
} tgui_t;


static const video_timings_t tgui_timing = {VID_BUS,4,8,16,4,8,16};


void tgui_recalcmapping(tgui_t *tgui);

static void fifo_thread(void *param);

uint8_t tgui_accel_read(uint32_t addr, priv_t);
uint16_t tgui_accel_read_w(uint32_t addr, priv_t);
uint32_t tgui_accel_read_l(uint32_t addr, priv_t);

void tgui_accel_write(uint32_t addr, uint8_t val, priv_t);
void tgui_accel_write_w(uint32_t addr, uint16_t val, priv_t);
void tgui_accel_write_l(uint32_t addr, uint32_t val, priv_t);

void tgui_accel_write_fb_b(uint32_t addr, uint8_t val, priv_t);
void tgui_accel_write_fb_w(uint32_t addr, uint16_t val, priv_t);
void tgui_accel_write_fb_l(uint32_t addr, uint32_t val, priv_t);

static uint8_t tgui_ext_linear_read(uint32_t addr, priv_t);
static void tgui_ext_linear_write(uint32_t addr, uint8_t val, priv_t);
static void tgui_ext_linear_writew(uint32_t addr, uint16_t val, priv_t);
static void tgui_ext_linear_writel(uint32_t addr, uint32_t val, priv_t);

static uint8_t tgui_ext_read(uint32_t addr, priv_t);
static void tgui_ext_write(uint32_t addr, uint8_t val, priv_t);
static void tgui_ext_writew(uint32_t addr, uint16_t val, priv_t);
static void tgui_ext_writel(uint32_t addr, uint32_t val, priv_t);

void tgui_out(uint16_t addr, uint8_t val, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
        svga_t *svga = &dev->svga;

        uint8_t old;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) addr ^= 0x60;

        switch (addr) {
                case 0x3C5:
                switch (svga->seqaddr & 0xf) {
                        case 0xB: 
                                dev->oldmode=1; 
                                break;
                        case 0xC: 
                                if (svga->seqregs[0xe] & 0x80) 
                        	        svga->seqregs[0xc] = val; 
                                break;
                        case 0xd: 
                                if (dev->oldmode)
                                        dev->oldctrl2 = val; 
                                else 
                                        dev->newctrl2=val; 
                                break;
                        case 0xE:
                                if (dev->oldmode)
                                        dev->oldctrl1 = val; 
                                else {
                                        svga->seqregs[0xe] = val ^ 2;
                                        svga->write_bank = (svga->seqregs[0xe] & 0xf) * 65536;
                                        if (!(svga->gdcreg[0xf] & 1)) 
                                                svga->read_bank = svga->write_bank;
                                }
                                return;
                }
                        break;

                case 0x3C6:
                        if (dev->type == TGUI_9400CXI) {
                        tkd8001_ramdac_out(addr, val, svga->ramdac, svga);
                        return;
                        }
                        if (dev->ramdac_state == 4) {
                                dev->ramdac_state = 0;
                                dev->ramdac_ctrl = val;
                                switch (dev->ramdac_ctrl & 0xf0) {
                                        case 0x10:
                                                svga->bpp = 15;
                                                break;
                                        case 0x30:
                                                svga->bpp = 16;
                                                break;
                                        case 0xd0:
                                                svga->bpp = 24;
                                                break;
                                        default:
                                                svga->bpp = 8;
                                                break;
                                }
                                return;
                        }
                case 0x3C7: case 0x3C8: case 0x3C9:
                        if (dev->type == TGUI_9400CXI) {
                                tkd8001_ramdac_out(addr, val, svga->ramdac, svga);
                                return;
                        }
                        dev->ramdac_state = 0;
		        break;

                case 0x3CF:
                        if (dev->type == TGUI_9400CXI && svga->gdcaddr >= 16 && svga->gdcaddr < 32) {
                                old = dev->ext_gdc_regs[svga->gdcaddr & 15];
                                dev->ext_gdc_regs[svga->gdcaddr & 15] = val;
                        if (svga->gdcaddr == 16)
				tgui_recalcmapping(dev);
                        return;
                        }
                        switch (svga->gdcaddr & 15) {
			case 0x6:
			        if (svga->gdcreg[6] != val) {
				        svga->gdcreg[6] = val;
				        tgui_recalcmapping(dev);
			        }
			        return;
			
                        case 0xE:
                                svga->gdcreg[0xe] = val ^ 2;
                                if ((svga->gdcreg[0xf] & 1) == 1)
                                        svga->read_bank = (svga->gdcreg[0xe] & 0xf) * 65536;
                                break;
                        case 0xF:
                                if (val & 1) svga->read_bank = (svga->gdcreg[0xe]  & 0xf)  *65536;
                                else         svga->read_bank = (svga->seqregs[0xe] & 0xf)  *65536;
                                svga->write_bank = (svga->seqregs[0xe] & 0xf) * 65536;
                                break;
                        } 
                        break;
                case 0x3D4:
		        svga->crtcreg = val & 0x7f;
                        return;
                case 0x3D5:
                        if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                                return;
                        if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                                val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                        old = svga->crtc[svga->crtcreg];
                        svga->crtc[svga->crtcreg] = val;
                        if (old != val) {
                                if (svga->crtcreg < 0xE || svga->crtcreg > 0x10) {
                                        svga->fullchange = changeframecount;
                                        svga_recalctimings(svga);
                                }
                        }
                        switch (svga->crtcreg) {
			        case 0x21:
			                if (old != val)	{
                                                if (!dev->pci) {
               	                                        dev->linear_base = ((val & 0xf) | ((val >> 2) & 0x30)) << 20;
                       	                                dev->linear_size = (val & 0x10) ? 0x200000 : 0x100000;
                                                        dev->svga.decode_mask = (val & 0x10) ? 0x1fffff : 0xfffff;
                                                }
        			        tgui_recalcmapping(dev);
                                        }
			                break;

                                case 0x37:
			                i2c_gpio_set(dev->i2c, (val & 0x02) || !(val & 0x04), (val & 0x01) || !(val & 0x08));
			                break;

			        case 0x40: case 0x41: case 0x42: case 0x43:
			        case 0x44: case 0x45: case 0x46: case 0x47:
                                        if (dev->type >= TGUI_9440) {
        			                svga->hwcursor.x = (svga->crtc[0x40] | (svga->crtc[0x41] << 8)) & 0x7ff;
        			                svga->hwcursor.y = (svga->crtc[0x42] | (svga->crtc[0x43] << 8)) & 0x7ff;
        			                svga->hwcursor.xoff = svga->crtc[0x46] & 0x3f;
        			                svga->hwcursor.yoff = svga->crtc[0x47] & 0x3f;
        			                svga->hwcursor.addr = (svga->crtc[0x44] << 10) | ((svga->crtc[0x45] & 0x7) << 18) | (svga->hwcursor.yoff * 8);
                                        }
			                break;
			
			        case 0x50:
                                        if (dev->type >= TGUI_9440) {
        			                svga->hwcursor.ena = val & 0x80;
        			                svga->hwcursor.xsize = (val & 1) ? 64 : 32;
        			                svga->hwcursor.ysize = (val & 1) ? 64 : 32;
                                        }
			                break;
		        }
                        return;
                case 0x3D8:
                        dev->tgui_3d8 = val;
                        if (svga->gdcreg[0xf] & 4) {
                                svga->write_bank = (val & 0x1f) * 65536;
                                if (!(svga->gdcreg[0xf] & 1))
                                        svga->read_bank = (val & 0x1f) * 65536;
                        }
                        return;
                case 0x3D9:
                        dev->tgui_3d9 = val;
                        if ((svga->gdcreg[0xf] & 5) == 5)
                                svga->read_bank = (val & 0x1F) * 65536;
                        return;
                
                case 0x43c8:
                        dev->clock_n = val & 0x7f;
                        dev->clock_m = (dev->clock_m & ~1) | (val >> 7);
                        break;
                case 0x43c9:
                        dev->clock_m = (dev->clock_m & ~0x1e) | ((val << 1) & 0x1e);
                        dev->clock_k = (val & 0x10) >> 4;
                        break;
        }
        svga_out(addr, val, svga);
}

uint8_t tgui_in(uint16_t addr, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
        svga_t *svga = &dev->svga;
        uint8_t temp;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) addr ^= 0x60;
        
        switch (addr) {
                case 0x3C5:
                        if ((svga->seqaddr & 0xf) == 0xb) {
                                dev->oldmode = 0;
                                switch (dev->type) {
                                        case TGUI_9400CXI:
                                                return 0x93; /*TGUI9400CXi*/
                                        case TGUI_9440:
                                                return 0xe3; /*TGUI9440AGi*/
                                }
                        }
                        if ((svga->seqaddr & 0xf) == 0xd) {
                                if (dev->oldmode)
                                        return dev->oldctrl2;
                                return dev->newctrl2;
                        }
                        if ((svga->seqaddr & 0xf) == 0xe) {
                                if (dev->oldmode)
                                        return dev->oldctrl1; 
                        }
                        break;
                case 0x3C6:
                        if (dev->type == TGUI_9400CXI)
                                return tkd8001_ramdac_in(addr, svga->ramdac, svga);
                        if (dev->ramdac_state == 4)
                                return dev->ramdac_ctrl;
                        dev->ramdac_state++;
                        break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                        if (dev->type == TGUI_9400CXI)
                                return tkd8001_ramdac_in(addr, svga->ramdac, svga);
                        dev->ramdac_state = 0;
                        break;
                case 0x3CF:
                        if (dev->type == TGUI_9400CXI && svga->gdcaddr >= 16 && svga->gdcaddr < 32)
                                return dev->ext_gdc_regs[svga->gdcaddr & 15];
                        break;
                case 0x3D4:
                        return svga->crtcreg;
                case 0x3D5:
                        temp = svga->crtc[svga->crtcreg];
                        if (svga->crtcreg == 0x37) {
                                if (!(svga->crtc[0x37] & 0x04)) {
                                        temp &= 0xfd;
                                        if (i2c_gpio_get_scl(dev->i2c))
                                                temp |= 0x02;
                                }
                                if (!(svga->crtc[0x37] & 0x08)) {
                                        temp &= 0xfe;
                                        if (i2c_gpio_get_sda(dev->i2c))
                                                temp |= 0x01;
                                }
                        }
                        return temp;
                case 0x3d8:
		        return dev->tgui_3d8;
		case 0x3d9:
		        return dev->tgui_3d9;
        }
        return svga_in(addr, svga);
}

void tgui_recalctimings(svga_t *svga)
{
        tgui_t *dev = (tgui_t *)svga->p;

        if (svga->crtc[0x29] & 0x10)
                svga->rowoffset += 0x100;

        if (dev->type >= TGUI_9440 && svga->bpp == 24)
                svga->hdisp = (svga->crtc[1] + 1) * 8;
        
        if ((svga->crtc[0x1e] & 0xA0) == 0xA0) svga->ma_latch |= 0x10000;
        if ((svga->crtc[0x27] & 0x01) == 0x01) svga->ma_latch |= 0x20000;
        if ((svga->crtc[0x27] & 0x02) == 0x02) svga->ma_latch |= 0x40000;
        
        if (dev->oldctrl2 & 0x10)
                svga->rowoffset <<= 1;
        if ((dev->oldctrl2 & 0x10) || (svga->crtc[0x2a] & 0x40))
                svga->ma_latch <<= 1;

        if (dev->oldctrl2 & 0x10) /*I'm not convinced this is the right register for this function*/
           svga->lowres=0;

	svga->lowres = !(svga->crtc[0x2a] & 0x40); 

        svga->interlace = svga->crtc[0x1e] & 4;
        if (svga->interlace && dev->type < TGUI_9440)
                svga->rowoffset >>= 1;

        if (dev->type >= TGUI_9440) {
                if (svga->miscout & 8)
                        svga->clock = cpu_clock / (((dev->clock_n + 8) * 14318180.0) / ((dev->clock_m + 2) * (1 << dev->clock_k)));
                        
                if (svga->gdcreg[0xf] & 0x08)
                        svga->clock *= 2;
                else if (svga->gdcreg[0xf] & 0x40)
                        svga->clock *= 3;
        } else {
                switch (((svga->miscout >> 2) & 3) | ((dev->newctrl2 << 2) & 4) | ((dev->newctrl2 >> 3) & 8)) {
                        case 0x02: svga->clock = cpu_clock/ 44900000.0; break;
                        case 0x03: svga->clock = cpu_clock/ 36000000.0; break;
                        case 0x04: svga->clock = cpu_clock/ 57272000.0; break;
                        case 0x05: svga->clock = cpu_clock/ 65000000.0; break;
                        case 0x06: svga->clock = cpu_clock/ 50350000.0; break;
                        case 0x07: svga->clock = cpu_clock/ 40000000.0; break;
                        case 0x08: svga->clock = cpu_clock/ 88000000.0; break;
                        case 0x09: svga->clock = cpu_clock/ 98000000.0; break;
                        case 0x0a: svga->clock = cpu_clock/118800000.0; break;
                        case 0x0b: svga->clock = cpu_clock/108000000.0; break;
                        case 0x0c: svga->clock = cpu_clock/ 72000000.0; break;
                        case 0x0d: svga->clock = cpu_clock/ 77000000.0; break;
                        case 0x0e: svga->clock = cpu_clock/ 80000000.0; break;
                        case 0x0f: svga->clock = cpu_clock/ 75000000.0; break;
                }
                if (svga->gdcreg[0xf] & 0x08) {
                        svga->htotal *= 2;
                        svga->hdisp *= 2;
                        svga->hdisp_time *= 2;
                }
        }
                                
        if ((dev->oldctrl2 & 0x10) || (svga->crtc[0x2a] & 0x40)) {
                switch (svga->bpp) {
                        case 8: 
                                svga->render = svga_render_8bpp_highres; 
                                break;
                        case 15: 
                                svga->render = svga_render_15bpp_highres;
                                if (dev->type < TGUI_9440)
                                        svga->hdisp /= 2;
                                break;
                        case 16: 
                                svga->render = svga_render_16bpp_highres; 
                                if (dev->type < TGUI_9440)
                                        svga->hdisp /= 2;
                                break;
                        case 24: 
                                svga->render = svga_render_24bpp_highres;
                                if (dev->type < TGUI_9440)
                                        svga->hdisp = (svga->hdisp * 2) / 3;
                                break;
                }
        }
}

void tgui_recalcmapping(tgui_t *dev)
{
        svga_t *svga = &dev->svga;

        if (dev->type == TGUI_9400CXI) {
                if (dev->ext_gdc_regs[0] & EXT_CTRL_LATCH_COPY) {
                        mem_map_set_handler(&dev->linear_mapping,
                                        tgui_ext_linear_read, NULL, NULL,
                                        tgui_ext_linear_write, tgui_ext_linear_writew, tgui_ext_linear_writel);
                        mem_map_set_handler(&svga->mapping,
                                        tgui_ext_read, NULL, NULL,
                                        tgui_ext_write, tgui_ext_writew, tgui_ext_writel);
                }
                else if (dev->ext_gdc_regs[0] & EXT_CTRL_MONO_EXPANSION) {
                        mem_map_set_handler(&dev->linear_mapping,
                                        svga_read_linear, svga_readw_linear, svga_readl_linear,
                                        tgui_ext_linear_write, tgui_ext_linear_writew, tgui_ext_linear_writel);
                        mem_map_set_handler(&svga->mapping,
                                        svga_read, svga_readw, svga_readl,
                                        tgui_ext_write, tgui_ext_writew, tgui_ext_writel);
                } else {
                        mem_map_set_handler(&dev->linear_mapping,
                                        svga_read_linear,  svga_readw_linear,  svga_readl_linear,
                                        svga_write_linear, svga_writew_linear, svga_writel_linear);
                        mem_map_set_handler(&svga->mapping,
                                        svga_read, svga_readw, svga_readl,
                                        svga_write, svga_writew, svga_writel);
                }
        }

	if (svga->crtc[0x21] & 0x20) {
                mem_map_disable(&svga->mapping);
                mem_map_set_addr(&dev->linear_mapping, dev->linear_base, dev->linear_size);
                if (dev->type >= TGUI_9440) {
                        mem_map_enable(&dev->accel_mapping);
                        mem_map_disable(&svga->mapping);
                } else {
                        switch (svga->gdcreg[6] & 0xC) {
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
	} else {
                mem_map_disable(&dev->linear_mapping);
                mem_map_disable(&dev->accel_mapping);
                switch (svga->gdcreg[6] & 0xC) {
                        case 0x0: /*128k at A0000*/
                                mem_map_set_addr(&svga->mapping, 0xa0000, 0x20000);
                                svga->banked_mask = 0xffff;
                                break;
                        case 0x4: /*64k at A0000*/
                                mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
                                mem_map_enable(&dev->accel_mapping);
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
}

void tgui_hwcursor_draw(svga_t *svga, int displine)
{
        uint32_t dat[2];
        int xx;
        int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
	int y_add, x_add;

	y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
	x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;
        
        if (svga->interlace && svga->hwcursor_oddeven)
                svga->hwcursor_latch.addr += 8;

        dat[0] = (svga->vram[svga->hwcursor_latch.addr]     << 24) | (svga->vram[svga->hwcursor_latch.addr + 1] << 16) 
                 | (svga->vram[svga->hwcursor_latch.addr + 2] << 8) | svga->vram[svga->hwcursor_latch.addr + 3];
        dat[1] = (svga->vram[svga->hwcursor_latch.addr + 4] << 24) | (svga->vram[svga->hwcursor_latch.addr + 5] << 16) 
                 | (svga->vram[svga->hwcursor_latch.addr + 6] << 8) | svga->vram[svga->hwcursor_latch.addr + 7];
        for (xx = 0; xx < 32; xx++) {
                if (offset >= svga->hwcursor_latch.x) {
                        if (!(dat[0] & 0x80000000))
                                screen->line[displine + y_add][offset + 32 + x_add].val  = (dat[1] & 0x80000000) ? 0xffffff : 0;
                        else if (dat[1] & 0x80000000)

                                screen->line[displine + y_add][offset + 32 + x_add].val ^= 0xffffff;
                }
                           
                offset++;
                dat[0] <<= 1;
                dat[1] <<= 1;
        }
        svga->hwcursor_latch.addr += 8;
        
        if (svga->interlace && !svga->hwcursor_oddeven)
                svga->hwcursor_latch.addr += 8;
}

uint8_t tgui_pci_read(int func, int addr, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;

        switch (addr) {
                case 0x00: return 0x23; /*Trident*/
                case 0x01: return 0x10;
                
                case 0x02: return 0x40; /*TGUI9440 (9682)*/
                case 0x03: return 0x94;
                
                case 0x04: return 0x03; /*Respond to IO and memory accesses*/

                case 0x07: return 1 << 1; /*Medium DEVSEL timing*/
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x01; /*Supports VGA interface, XGA compatible*/
                case 0x0b: return 0x03;
                
                case 0x10: return 0x00; /*Linear frame buffer address*/
                case 0x11: return 0x00;
                case 0x12: return dev->linear_base >> 16;
                case 0x13: return dev->linear_base >> 24;

                case 0x30: return 0x01; /*BIOS ROM address*/
                case 0x31: return 0x00;
                case 0x32: return 0x0C;
                case 0x33: return 0x00;
        }
        return 0;
}

void tgui_pci_write(int func, int addr, uint8_t val, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
        svga_t *svga = &dev->svga;

        switch (addr) {
                case 0x12:
                        dev->linear_base = (dev->linear_base & 0xff000000) | ((val & 0xe0) << 16);
                        dev->linear_size = 2 << 20;
                        dev->svga.decode_mask = 0x1fffff;
                        svga->crtc[0x21] = (svga->crtc[0x21] & ~0xf) | (val >> 4);
                        tgui_recalcmapping(dev);
                        break;
                case 0x13:
                        dev->linear_base = (dev->linear_base & 0xe00000) | (val << 24);
                        dev->linear_size = 2 << 20;
                        dev->svga.decode_mask = 0x1fffff;
                        svga->crtc[0x21] = (svga->crtc[0x21] & ~0xc0) | (val >> 6);
                        tgui_recalcmapping(dev);
                        break;
        }
}


static uint8_t tgui_ext_linear_read(uint32_t addr, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;
        int c;
  
        cycles -= video_timing_read_b;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xff;
        
        addr &= ~0xf;
        for (c = 0; c < 16; c++)
                dev->copy_latch[c] = svga->vram[addr+c];

        return svga->vram[addr & svga->vram_mask]; 
}

static uint8_t tgui_ext_read(uint32_t addr, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        
        addr = (addr & svga->banked_mask) + svga->read_bank;
        
        return tgui_ext_linear_read(addr, svga);
}

static void tgui_ext_linear_write(uint32_t addr, uint8_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;
        int c;
        uint8_t fg[2] = {dev->ext_gdc_regs[4], dev->ext_gdc_regs[5]};
        uint8_t bg[2] = {dev->ext_gdc_regs[1], dev->ext_gdc_regs[2]};
        uint8_t mask = dev->ext_gdc_regs[7];

        cycles -= video_timing_write_b;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        addr &= ~0x7;
        svga->changedvram[addr >> 12] = changeframecount;
        
        switch (dev->ext_gdc_regs[0] & 0xf) {
                /*8-bit mono->colour expansion, unmasked*/
                case 2:
                        for (c = 7; c >= 0; c--) {
                                if (mask & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = (val & (1 << c)) ? fg[0] : bg[0];
                                addr++;
                        }
                        break;

                /*16-bit mono->colour expansion, unmasked*/
                case 3:
                        for (c = 7; c >= 0; c--) {
                                if (mask & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = (val & (1 << c)) ? fg[(c & 1) ^ 1] : bg[(c & 1) ^ 1];
                                addr++;
                        }
                        break;

                /*8-bit mono->colour expansion, masked*/
                case 6:
                        for (c = 7; c >= 0; c--) {
                                if ((val & mask) & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = fg[0];
                                addr++;
                        }
                        break;
                
                /*16-bit mono->colour expansion, masked*/
                case 7:
                        for (c = 7; c >= 0; c--) {
                                if ((val & mask) & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = fg[(c & 1) ^ 1];
                                addr++;
                        }
                        break;

                case 0x8: case 0x9: case 0xa: case 0xb:
                case 0xc: case 0xd: case 0xe: case 0xf:
                        addr &= ~0xf;
                        for (c = 0; c < 16; c++)
                                *(uint8_t *)&svga->vram[addr+c] = dev->copy_latch[c];
                        break;
        }
}

static void tgui_ext_linear_writew(uint32_t addr, uint16_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;
        int c;
        uint8_t fg[2] = {dev->ext_gdc_regs[4], dev->ext_gdc_regs[5]};
        uint8_t bg[2] = {dev->ext_gdc_regs[1], dev->ext_gdc_regs[2]};
        uint16_t mask = (dev->ext_gdc_regs[7] << 8) | dev->ext_gdc_regs[8];
        
        cycles -= video_timing_write_w;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        addr &= ~0xf;
        svga->changedvram[addr >> 12] = changeframecount;
        
        val = (val >> 8) | (val << 8);

        switch (dev->ext_gdc_regs[0] & 0xf) {
                /*8-bit mono->colour expansion, unmasked*/
                case 2:
                        for (c = 15; c >= 0; c--) {
                                if (mask & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = (val & (1 << c)) ? fg[0] : bg[0];
                                addr++;
                        }
                        break;

                /*16-bit mono->colour expansion, unmasked*/
                case 3:
                        for (c = 15; c >= 0; c--) {
                                if (mask & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = (val & (1 << c)) ? fg[(c & 1) ^ 1] : bg[(c & 1) ^ 1];
                                addr++;
                        }
                        break;

                /*8-bit mono->colour expansion, masked*/
                case 6:
                        for (c = 15; c >= 0; c--) {
                                if ((val & mask) & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = fg[0];
                                addr++;
                        }
                        break;

                /*16-bit mono->colour expansion, masked*/
                case 7:
                        for (c = 15; c >= 0; c--) {
                                if ((val & mask) & (1 << c))
                                        *(uint8_t *)&svga->vram[addr] = fg[(c & 1) ^ 1];
                                addr++;
                        }
                        break;
                                
                case 0x8: case 0x9: case 0xa: case 0xb:
                case 0xc: case 0xd: case 0xe: case 0xf:
                        for (c = 0; c < 16; c++)
                                *(uint8_t *)&svga->vram[addr+c] = dev->copy_latch[c];
                        break;
        }
}

static void tgui_ext_linear_writel(uint32_t addr, uint32_t val, priv_t priv)
{
        tgui_ext_linear_writew(addr, val, priv);
}

static void tgui_ext_write(uint32_t addr, uint8_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        
        addr = (addr & svga->banked_mask) + svga->read_bank;

        tgui_ext_linear_write(addr, val, svga);
}
static void tgui_ext_writew(uint32_t addr, uint16_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        
        addr = (addr & svga->banked_mask) + svga->read_bank;

        tgui_ext_linear_writew(addr, val, svga);
}
static void tgui_ext_writel(uint32_t addr, uint32_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        
        addr = (addr & svga->banked_mask) + svga->read_bank;

        tgui_ext_linear_writel(addr, val, svga);
}


enum
{
	TGUI_BITBLT = 1
};

enum
{
        TGUI_SRCCPU = 0,
        
	TGUI_SRCDISP = 0x04,	/*Source is from display*/
	TGUI_PATMONO = 0x20,	/*Pattern is monochrome and needs expansion*/
	TGUI_SRCMONO = 0x40,	/*Source is monochrome from CPU and needs expansion*/
	TGUI_TRANSENA  = 0x1000, /*Transparent (no draw when source == bg col)*/
	TGUI_TRANSREV  = 0x2000, /*Reverse fg/bg for transparent*/
	TGUI_SOLIDFILL = 0x4000	/*Pattern all zero?*/
};

#define READ(addr, dat) if (dev->accel.bpp == 0) dat = svga->vram[addr & 0x1fffff]; \
                        else                     dat = vram_w[addr & 0xfffff];
                        
#define MIX() do \
	{								\
		out = 0;						\
        	for (c=0;c<16;c++)					\
	        {							\
			d=(dst_dat & (1<<c)) ? 1:0;			\
                       	if (src_dat & (1<<c)) d|=2;			\
               	        if (pat_dat & (1<<c)) d|=4;			\
                        if (dev->accel.rop & (1<<d)) out|=(1<<c);	\
	        }							\
	} while (0)

#define WRITE(addr, dat)        if (dev->accel.bpp == 0)                                                \
                                {                                                                       \
                                        svga->vram[addr & 0x1fffff] = dat;                                    \
                                        svga->changedvram[((addr) & 0x1fffff) >> 12] = changeframecount;      \
                                }                                                                       \
                                else                                                                    \
                                {                                                                       \
                                        vram_w[addr & 0xfffff] = dat;                                   \
                                        svga->changedvram[((addr) & 0xfffff) >> 11] = changeframecount;        \
                                }
                                
void tgui_accel_command(int count, uint32_t cpu_dat, tgui_t *dev)
{
        svga_t *svga = &dev->svga;
	int x, y;
	int c, d;
	uint16_t src_dat, dst_dat, pat_dat;
	uint8_t out;
	int xdir = (dev->accel.flags & 0x200) ? -1 : 1;
	int ydir = (dev->accel.flags & 0x100) ? -1 : 1;
	uint16_t trans_col = (dev->accel.flags & TGUI_TRANSREV) ? dev->accel.fg_col : dev->accel.bg_col;
        uint16_t *vram_w = (uint16_t *)svga->vram;
        
	if (dev->accel.bpp == 0)
                      trans_col &= 0xff;
	
	if (count != -1 && !dev->accel.x && (dev->accel.flags & TGUI_SRCMONO)) {
		count -= dev->accel.offset;
		cpu_dat <<= dev->accel.offset;
	}
	if (count == -1) {
		dev->accel.x = dev->accel.y = 0;
	}
	if (dev->accel.flags & TGUI_SOLIDFILL) {
		for (y = 0; y < 8; y++)	{
			for (x = 0; x < 8; x++)	{
				dev->accel.tgui_pattern[y][x] = dev->accel.fg_col;
			}
		}
	}
	else if (dev->accel.flags & TGUI_PATMONO) {
		for (y = 0; y < 8; y++) {
			for (x = 0; x < 8; x++)	{
				dev->accel.tgui_pattern[y][x] = (dev->accel.pattern[y] & (1 << x)) ? dev->accel.fg_col : dev->accel.bg_col;
			}
		}
	} else {
                if (dev->accel.bpp == 0) {
        		for (y = 0; y < 8; y++) {
        			for (x = 0; x < 8; x++) {
        				dev->accel.tgui_pattern[y][x] = dev->accel.pattern[x + y*8];
        			}
                        }
		} else {
        		for (y = 0; y < 8; y++) {
        			for (x = 0; x < 8; x++) {
        				dev->accel.tgui_pattern[y][x] = dev->accel.pattern[x*2 + y*16] | (dev->accel.pattern[x*2 + y*16 + 1] << 8);
        			}
                        }
		}
	}
        switch (dev->accel.command) {
		case TGUI_BITBLT:
		        if (count == -1) {
			        dev->accel.src = dev->accel.src_old = dev->accel.src_x + (dev->accel.src_y * dev->accel.pitch);
			        dev->accel.dst = dev->accel.dst_old = dev->accel.dst_x + (dev->accel.dst_y * dev->accel.pitch);
			        dev->accel.pat_x = dev->accel.dst_x;
			        dev->accel.pat_y = dev->accel.dst_y;
		        }

		switch (dev->accel.flags & (TGUI_SRCMONO|TGUI_SRCDISP)) {
			case TGUI_SRCCPU:
			        if (count == -1) {
				        if (svga->crtc[0x21] & 0x20) {
                                                dev->write_blitter = 1;
                                        }
				        if (dev->accel.use_src)
                                                return;
			        }
			        else
			                count >>= 3;
			        while (count) {
				        if (dev->accel.bpp == 0) {
                                                src_dat = cpu_dat >> 24;
                                                cpu_dat <<= 8;
                                        } else {
                                                src_dat = (cpu_dat >> 24) | ((cpu_dat >> 8) & 0xff00);
                                                cpu_dat <<= 16;
                                                count--;
                                        }
				        READ(dev->accel.dst, dst_dat);
				        pat_dat = dev->accel.tgui_pattern[dev->accel.pat_y & 7][dev->accel.pat_x & 7];
        
                                        if (!(dev->accel.flags & TGUI_TRANSENA) || src_dat != trans_col) {
        				        MIX();
                                                WRITE(dev->accel.dst, out);
                                        }

				        dev->accel.src += xdir;
				        dev->accel.dst += xdir;
				        dev->accel.pat_x += xdir;
	
				        dev->accel.x++;
				        if (dev->accel.x > dev->accel.size_x) {
					        dev->accel.x = 0;
					        dev->accel.y++;
					
                                                dev->accel.pat_x = dev->accel.dst_x;
	
					        dev->accel.src = dev->accel.src_old = dev->accel.src_old + (ydir * dev->accel.pitch);
					        dev->accel.dst = dev->accel.dst_old = dev->accel.dst_old + (ydir * dev->accel.pitch);
					        dev->accel.pat_y += ydir;
					
					if (dev->accel.y > dev->accel.size_y)	{
						if (svga->crtc[0x21] & 0x20) {
                                                        dev->write_blitter = 0;
						}
						return;
					}
					if (dev->accel.use_src)
                                                return;
				}
				count--;
			}
			break;
						
			case TGUI_SRCMONO | TGUI_SRCCPU:
			        if (count == -1) {
				        if (svga->crtc[0x21] & 0x20)
                                                dev->write_blitter = 1;

				        if (dev->accel.use_src)
                                                return;
			        }
			        while (count) {
				        src_dat = ((cpu_dat >> 31) ? dev->accel.fg_col : dev->accel.bg_col);
				        if (dev->accel.bpp == 0)
				                src_dat &= 0xff;
				    
				        READ(dev->accel.dst, dst_dat);
				        pat_dat = dev->accel.tgui_pattern[dev->accel.pat_y & 7][dev->accel.pat_x & 7];

                                        if (!(dev->accel.flags & TGUI_TRANSENA) || src_dat != trans_col) {
        				        MIX();
				                WRITE(dev->accel.dst, out);
                                        }
				        cpu_dat <<= 1;
				        dev->accel.src += xdir;
				        dev->accel.dst += xdir;
				        dev->accel.pat_x += xdir;
	
				        dev->accel.x++;
				        if (dev->accel.x > dev->accel.size_x) {
					        dev->accel.x = 0;
					        dev->accel.y++;
					
        					dev->accel.pat_x = dev->accel.dst_x;
	
	        				dev->accel.src = dev->accel.src_old = dev->accel.src_old + (ydir * dev->accel.pitch);
		        			dev->accel.dst = dev->accel.dst_old = dev->accel.dst_old + (ydir * dev->accel.pitch);
			        		dev->accel.pat_y += ydir;
					
					        if (dev->accel.y > dev->accel.size_y) {
						        if (svga->crtc[0x21] & 0x20) {
                                                                dev->write_blitter = 0;
						        }
						        return;
					        }
					        if (dev->accel.use_src)
                                                        return;
				        }
				        count--;
			        }
			        break;

			default:
			        while (count) {
				        READ(dev->accel.src, src_dat);
				        READ(dev->accel.dst, dst_dat);                                                                
				        pat_dat = dev->accel.tgui_pattern[dev->accel.pat_y & 7][dev->accel.pat_x & 7];
	
                                        if (!(dev->accel.flags & TGUI_TRANSENA) || src_dat != trans_col) {
        				        MIX();
	                                        WRITE(dev->accel.dst, out);
                                        }

				        dev->accel.src += xdir;
				        dev->accel.dst += xdir;
				        dev->accel.pat_x += xdir;
				        dev->accel.x++;

				        if (dev->accel.x > dev->accel.size_x) {
					        dev->accel.x = 0;
					        dev->accel.y++;
						dev->accel.pat_x = dev->accel.dst_x;
					        dev->accel.src = dev->accel.src_old = dev->accel.src_old + (ydir * dev->accel.pitch);
					        dev->accel.dst = dev->accel.dst_old = dev->accel.dst_old + (ydir * dev->accel.pitch);
					        dev->accel.pat_y += ydir;
					
					        if (dev->accel.y > dev->accel.size_y)
						        return;
				        }
				        count--;
			        }
			        break;
		}
		break;
	}
}

static void tgui_accel_write_fifo(tgui_t *dev, uint32_t addr, uint8_t val)
{
	switch (addr & 0xff) {
                case 0x22:
                        dev->accel.ger22 = val;
                        dev->accel.pitch = 512 << ((val >> 2) & 3);
                        dev->accel.bpp = (val & 3) ? 1 : 0;
                        dev->accel.pitch >>= dev->accel.bpp;
                        break;
                
		case 0x24: /*Command*/
		        dev->accel.command = val;
		        tgui_accel_command(-1, 0, dev);
		        break;
		
		case 0x27: /*ROP*/
		        dev->accel.rop = val;
		        dev->accel.use_src = (val & 0x33) ^ ((val >> 2) & 0x33);
		        break;
		
		case 0x28: /*Flags*/
		        dev->accel.flags = (dev->accel.flags & 0xff00) | val;
		        break;
		case 0x29: /*Flags*/
		        dev->accel.flags = (dev->accel.flags & 0xff) | (val << 8);
		        break;

		case 0x2b:
		        dev->accel.offset = val & 7;
		        break;
		
		case 0x2c: /*Foreground colour*/
		        dev->accel.fg_col = (dev->accel.fg_col & 0xff00) | val;
		        break;
		case 0x2d: /*Foreground colour*/
		        dev->accel.fg_col = (dev->accel.fg_col & 0xff) | (val << 8);
		        break;

		case 0x30: /*Background colour*/
		        dev->accel.bg_col = (dev->accel.bg_col & 0xff00) | val;
		        break;
		case 0x31: /*Background colour*/
		        dev->accel.bg_col = (dev->accel.bg_col & 0xff) | (val << 8);
		        break;

		case 0x38: /*Dest X*/
		        dev->accel.dst_x = (dev->accel.dst_x & 0xff00) | val;
		        break;
		case 0x39: /*Dest X*/
		        dev->accel.dst_x = (dev->accel.dst_x & 0xff) | (val << 8);
		        break;
		case 0x3a: /*Dest Y*/
		        dev->accel.dst_y = (dev->accel.dst_y & 0xff00) | val;
		        break;
		case 0x3b: /*Dest Y*/
		        dev->accel.dst_y = (dev->accel.dst_y & 0xff) | (val << 8);
		        break;

		case 0x3c: /*Src X*/
		        dev->accel.src_x = (dev->accel.src_x & 0xff00) | val;
		        break;
		case 0x3d: /*Src X*/
		        dev->accel.src_x = (dev->accel.src_x & 0xff) | (val << 8);
		        break;
		case 0x3e: /*Src Y*/
		        dev->accel.src_y = (dev->accel.src_y & 0xff00) | val;
		        break;
		case 0x3f: /*Src Y*/
		        dev->accel.src_y = (dev->accel.src_y & 0xff) | (val << 8);
		        break;

		case 0x40: /*Size X*/
		        dev->accel.size_x = (dev->accel.size_x & 0xff00) | val;
		        break;
		case 0x41: /*Size X*/
		        dev->accel.size_x = (dev->accel.size_x & 0xff) | (val << 8);
		        break;
		case 0x42: /*Size Y*/
		        dev->accel.size_y = (dev->accel.size_y & 0xff00) | val;
		        break;
		case 0x43: /*Size Y*/
		        dev->accel.size_y = (dev->accel.size_y & 0xff) | (val << 8);
		        break;
		
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
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
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
		        dev->accel.pattern[addr & 0x7f] = val;
		        break;
	}
}

static void tgui_accel_write_fifo_fb_b(tgui_t *dev, uint32_t addr, uint8_t val)
{
	tgui_accel_command(8, val << 24, dev);
}
static void tgui_accel_write_fifo_fb_w(tgui_t *dev, uint32_t addr, uint16_t val)
{
        tgui_accel_command(16, (((val & 0xff00) >> 8) | ((val & 0x00ff) << 8)) << 16, dev);
}
static void tgui_accel_write_fifo_fb_l(tgui_t *dev, uint32_t addr, uint32_t val)
{
	tgui_accel_command(32, ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24), dev);
}

static void fifo_thread(void *param)
{
        tgui_t *dev = (tgui_t *)param;
        
        while (1) {
                thread_set_event(dev->fifo_not_full_event);
                thread_wait_event(dev->wake_fifo_thread, -1);
                thread_reset_event(dev->wake_fifo_thread);
                dev->blitter_busy = 1;
                while (!FIFO_EMPTY) {
                        uint64_t start_time = plat_timer_read();
                        uint64_t end_time;
                        fifo_entry_t *fifo = &dev->fifo[dev->fifo_read_idx & FIFO_MASK];

                        switch (fifo->addr_type & FIFO_TYPE) {
                                case FIFO_WRITE_BYTE:
                                tgui_accel_write_fifo(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
                                break;
                                case FIFO_WRITE_FB_BYTE:
                                tgui_accel_write_fifo_fb_b(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
                                break;
                                case FIFO_WRITE_FB_WORD:
                                tgui_accel_write_fifo_fb_w(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
                                break;
                                case FIFO_WRITE_FB_LONG:
                                tgui_accel_write_fifo_fb_l(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
                                break;
                        }
                                                
                        dev->fifo_read_idx++;
                        fifo->addr_type = FIFO_INVALID;

                        if (FIFO_ENTRIES > 0xe000)
                                thread_set_event(dev->fifo_not_full_event);

                        end_time = plat_timer_read();
                        dev->blitter_time += end_time - start_time;
                }
                dev->blitter_busy = 0;
        }
}

static inline void wake_fifo_thread(tgui_t *dev)
{
        thread_set_event(dev->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

static void tgui_wait_fifo_idle(tgui_t *dev)
{
        while (!FIFO_EMPTY)
        {
                wake_fifo_thread(dev);
                thread_wait_event(dev->fifo_not_full_event, 1);
        }
}

static void tgui_queue(tgui_t *dev, uint32_t addr, uint32_t val, uint32_t type)
{
        fifo_entry_t *fifo = &dev->fifo[dev->fifo_write_idx & FIFO_MASK];

        if (FIFO_FULL) {
                thread_reset_event(dev->fifo_not_full_event);
                if (FIFO_FULL)
                        thread_wait_event(dev->fifo_not_full_event, -1); /*Wait for room in ringbuffer*/
        }

        fifo->val = val;
        fifo->addr_type = (addr & FIFO_ADDR) | type;

        dev->fifo_write_idx++;
        
        if (FIFO_ENTRIES > 0xe000 || FIFO_ENTRIES < 8)
                wake_fifo_thread(dev);
}


void tgui_accel_write(uint32_t addr, uint8_t val, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	if ((addr & ~0xff) != 0xbff00)
		return;
	tgui_queue(dev, addr, val, FIFO_WRITE_BYTE);
}

void tgui_accel_write_w(uint32_t addr, uint16_t val, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	tgui_accel_write(addr, val & 0xff, dev);
	tgui_accel_write(addr + 1, val >> 8, dev);
}

void tgui_accel_write_l(uint32_t addr, uint32_t val, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	tgui_accel_write(addr, val, dev);
	tgui_accel_write(addr + 1, val >> 8, dev);
	tgui_accel_write(addr + 2, val >> 16, dev);
	tgui_accel_write(addr + 3, val >> 24, dev);
}

uint8_t tgui_accel_read(uint32_t addr, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	if ((addr & ~0xff) != 0xbff00)
		return 0xff;
	if ((addr & 0xff) != 0x20)
	       tgui_wait_fifo_idle(dev);
	switch (addr & 0xff) {
		case 0x20: /*Status*/
		        if (!FIFO_EMPTY)
		                return 1 << 5;
		        return 0;

		case 0x27: /*ROP*/
		        return dev->accel.rop;

		case 0x28: /*Flags*/
		        return dev->accel.flags & 0xff;
		case 0x29: /*Flags*/
		        return dev->accel.flags >> 8;

		case 0x2b:
		        return dev->accel.offset;

		case 0x2c: /*Background colour*/
		        return dev->accel.bg_col & 0xff;
		case 0x2d: /*Background colour*/
		        return dev->accel.bg_col >> 8;

		case 0x30: /*Foreground colour*/
		        return dev->accel.fg_col & 0xff;
		case 0x31: /*Foreground colour*/
		        return dev->accel.fg_col >> 8;

		case 0x38: /*Dest X*/
		        return dev->accel.dst_x & 0xff;
		case 0x39: /*Dest X*/
		        return dev->accel.dst_x >> 8;
		case 0x3a: /*Dest Y*/
		        return dev->accel.dst_y & 0xff;
		case 0x3b: /*Dest Y*/
		        return dev->accel.dst_y >> 8;

		case 0x3c: /*Src X*/
		        return dev->accel.src_x & 0xff;
		case 0x3d: /*Src X*/
		        return dev->accel.src_x >> 8;
		case 0x3e: /*Src Y*/
		        return dev->accel.src_y & 0xff;
		case 0x3f: /*Src Y*/
		        return dev->accel.src_y >> 8;

		case 0x40: /*Size X*/
		        return dev->accel.size_x & 0xff;
		case 0x41: /*Size X*/
		        return dev->accel.size_x >> 8;
		case 0x42: /*Size Y*/
		        return dev->accel.size_y & 0xff;
		case 0x43: /*Size Y*/
		        return dev->accel.size_y >> 8;

		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
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
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
		        return dev->accel.pattern[addr & 0x7f];
	}
	return 0xff;
}

uint16_t tgui_accel_read_w(uint32_t addr, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	return tgui_accel_read(addr, dev) | (tgui_accel_read(addr + 1, dev) << 8);
}

uint32_t tgui_accel_read_l(uint32_t addr, priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
	return tgui_accel_read_w(addr, dev) | (tgui_accel_read_w(addr + 2, dev) << 16);
}

void tgui_accel_write_fb_b(uint32_t addr, uint8_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;

        if (dev->write_blitter)
        	tgui_queue(dev, addr, val, FIFO_WRITE_FB_BYTE);
        else
                svga_write_linear(addr, val, svga);
}

void tgui_accel_write_fb_w(uint32_t addr, uint16_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;

        if (dev->write_blitter)
        	tgui_queue(dev, addr, val, FIFO_WRITE_FB_WORD);
        else
                svga_writew_linear(addr, val, svga);
}

void tgui_accel_write_fb_l(uint32_t addr, uint32_t val, priv_t priv)
{
        svga_t *svga = (svga_t *)priv;
        tgui_t *dev = (tgui_t *)svga->p;

        if (dev->write_blitter)
        	tgui_queue(dev, addr, val, FIFO_WRITE_FB_LONG);
        else
                svga_writel_linear(addr, val, svga);
}


static priv_t
tgui_init(const device_t *info, UNUSED(void *parent))
{
        tgui_t *dev = (tgui_t *)mem_alloc(sizeof(tgui_t));
        memset(dev, 0x00, sizeof(tgui_t));

        dev->type = info->local;
	dev->pci = !!(info->flags & DEVICE_PCI);

        dev->vram_size = device_get_config_int("memory") << 20;
        dev->vram_mask = dev->vram_size - 1;

        rom_init(&dev->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

        svga_init(&dev->svga, (priv_t)dev,
		  dev->vram_size, tgui_recalctimings,
		  tgui_in, tgui_out, tgui_hwcursor_draw, NULL);

        if (dev->type == TGUI_9400CXI)
		dev->svga.ramdac = device_add(&tkd8001_ramdac_device);

        mem_map_add(&dev->linear_mapping, 0, 0, 
                    svga_read_linear, svga_readw_linear, svga_readl_linear, 
                    tgui_accel_write_fb_b, tgui_accel_write_fb_w, tgui_accel_write_fb_l, 
                    NULL, 0, &dev->svga);
        mem_map_add(&dev->accel_mapping,  0xbc000, 0x4000, 
                    tgui_accel_read,  tgui_accel_read_w, tgui_accel_read_l, 
                    tgui_accel_write,  tgui_accel_write_w, tgui_accel_write_l, 
                    NULL, 0,  dev);
        mem_map_disable(&dev->accel_mapping);

        io_sethandler(0x03c0, 0x0020, tgui_in, NULL, NULL, tgui_out, NULL, NULL, dev);

        if (dev->type >= TGUI_9440) {
                io_sethandler(0x43c8, 0x0002, tgui_in, NULL, NULL, tgui_out, NULL, NULL, dev);

                if (info->flags & DEVICE_PCI)
                        pci_add_card(PCI_ADD_VIDEO, tgui_pci_read, tgui_pci_write, dev);
        
                dev->i2c = i2c_gpio_init("ddc_tgui9440");
                dev->ddc = ddc_init(i2c_gpio_get_bus(dev->i2c));
        }

        dev->wake_fifo_thread = thread_create_event();
        dev->fifo_not_full_event = thread_create_event();
        dev->fifo_thread = thread_create(fifo_thread, dev);

	video_inform(DEVICE_VIDEO_GET(info->flags), &tgui_timing);

        return ((priv_t)dev);
}


static void
tgui_close(priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
        
        svga_close(&dev->svga);

        if (dev->type >= TGUI_9440) {
                ddc_close(dev->ddc);
                i2c_gpio_close(dev->i2c);
        }
        
        thread_kill(dev->fifo_thread);
        thread_destroy_event(dev->wake_fifo_thread);
        thread_destroy_event(dev->fifo_not_full_event);

        free(dev);
}


static void
speed_changed(priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;
        
        svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
        tgui_t *dev = (tgui_t *)priv;

        dev->svga.fullchange = changeframecount;
}


static const device_config_t tgui9440_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",2,
                {
                        {
                                "1 MB",1
                        },
                        {
                                "2 MB",2
                        },
                        {
                                NULL
                        }
                },
        },
        {
                NULL
        }
};

const device_t tgui9400cxi_device = {
        "Trident TGUI 9400CXi",
        DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
        TGUI_9400CXI,
	ROM_TGUI_9400CXI,
        tgui_init, tgui_close, NULL,
        NULL,
        speed_changed,
        force_redraw,
        NULL,
        tgui9440_config
};

const device_t tgui9440_vlb_device = {
        "Trident TGUI 9440",
        DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
	TGUI_9440,
	ROM_TGUI_9440,
        tgui_init, tgui_close, NULL,
        NULL,
        speed_changed,
        force_redraw,
        NULL,
        tgui9440_config
};

const device_t tgui9440_pci_device = {
        "Trident TGUI 9440",
        DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
	TGUI_9440,
	ROM_TGUI_9440,
        tgui_init, tgui_close, NULL,
        NULL,
        speed_changed,
        force_redraw,
        NULL,
        tgui9440_config
};
