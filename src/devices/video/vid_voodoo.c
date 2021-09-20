/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the 3DFX Voodoo Graphics controller.
 *
 * Version:	@(#)vid_voodoo.c	1.0.26	2021/09/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		leilei,
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
 *		Copyright 2008-2018 leilei.
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
#include "../../device.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../system/pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_voodoo_common.h"
#include "vid_voodoo_blitter.h"
#include "vid_voodoo_display.h"
#include "vid_voodoo_dither.h"
#include "vid_voodoo_fb.h"
#include "vid_voodoo_reg.h"
#include "vid_voodoo_regs.h"
#include "vid_voodoo_render.h"
#include "vid_voodoo_texture.h"
#include "vid_voodoo_fifo.h"

#ifdef _MSC_VER
# include <malloc.h>
#endif


#ifdef ABS
#undef ABS
#endif

int tris = 0;

rgba8_t	*rgb332,
	*ai44,
	*rgb565,
	*argb1555,
	*argb4444,
	*ai88;

void voodoo_recalc(voodoo_t *voodoo)
{
        uint32_t buffer_offset = ((voodoo->fbiInit2 >> 11) & 511) * 4096;
        
        voodoo->params.front_offset = voodoo->disp_buffer*buffer_offset;
        voodoo->back_offset = voodoo->draw_buffer*buffer_offset;

        voodoo->buffer_cutoff = TRIPLE_BUFFER ? (buffer_offset * 4) : (buffer_offset * 3);
        if (TRIPLE_BUFFER)
                voodoo->params.aux_offset = buffer_offset * 3;
        else
                voodoo->params.aux_offset = buffer_offset * 2;

        switch (voodoo->lfbMode & LFB_WRITE_MASK) {
                case LFB_WRITE_FRONT:
                voodoo->fb_write_offset = voodoo->params.front_offset;
                voodoo->fb_write_buffer = voodoo->disp_buffer;
                break;
                case LFB_WRITE_BACK:
                voodoo->fb_write_offset = voodoo->back_offset;
                voodoo->fb_write_buffer = voodoo->draw_buffer;
                break;

                default:
                /*BreakNeck sets invalid LFB write buffer select*/
                voodoo->fb_write_offset = voodoo->params.front_offset;
                break;
        }

        switch (voodoo->lfbMode & LFB_READ_MASK) {
                case LFB_READ_FRONT:
                voodoo->fb_read_offset = voodoo->params.front_offset;
                break;
                case LFB_READ_BACK:
                voodoo->fb_read_offset = voodoo->back_offset;
                break;
                case LFB_READ_AUX:
                voodoo->fb_read_offset = voodoo->params.aux_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown lfb source\n");
        }
        
        switch (voodoo->params.fbzMode & FBZ_DRAW_MASK)
        {
                case FBZ_DRAW_FRONT:
                voodoo->params.draw_offset = voodoo->params.front_offset;
                voodoo->fb_draw_buffer = voodoo->disp_buffer;
                break;
                case FBZ_DRAW_BACK:
                voodoo->params.draw_offset = voodoo->back_offset;
                voodoo->fb_draw_buffer = voodoo->draw_buffer;
                break;

                default:
                fatal("voodoo_recalc : unknown draw buffer\n");
        }
        
        voodoo->block_width = ((voodoo->fbiInit1 >> 4) & 15) * 2;
        if (voodoo->fbiInit6 & (1 << 30))
                voodoo->block_width += 1;
        if (voodoo->fbiInit1 & (1 << 24))
                voodoo->block_width += 32;
        voodoo->row_width = voodoo->block_width * 32 * 2;

/*        DEBUG("voodoo_recalc : front_offset %08X  back_offset %08X  aux_offset %08X draw_offset %08x\n", voodoo->params.front_offset, voodoo->back_offset, voodoo->params.aux_offset, voodoo->params.draw_offset);
        DEBUG("                fb_read_offset %08X  fb_write_offset %08X  row_width %i  %08x %08x\n", voodoo->fb_read_offset, voodoo->fb_write_offset, voodoo->row_width, voodoo->lfbMode, voodoo->params.fbzMode);*/
}


static uint16_t voodoo_readw(uint32_t addr, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
        
        addr &= 0xffffff;

        cycles -= (int)voodoo->read_time;
        
        if ((addr & 0xc00000) == 0x400000) { /*Framebuffer*/
                if (SLI_ENABLED) {
                        voodoo_set_t *set = voodoo->set;
                        int y = (addr >> 11) & 0x3ff;
                
                        if (y & 1)
                                voodoo = set->voodoos[1];
                        else
                                voodoo = set->voodoos[0];
                }

                voodoo->flush = 1;
                while (!FIFO_EMPTY) {
                        voodoo_wake_fifo_thread_now(voodoo);
                        thread_wait_event(voodoo->fifo_not_full_event, 1);
                }
                voodoo_wait_for_render_thread_idle(voodoo);
                voodoo->flush = 0;
                
                return voodoo_fb_readw(addr, voodoo);
        }

        return 0xffff;
}


static uint32_t voodoo_readl(uint32_t addr, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
        uint32_t temp = 0;
        int fifo_size;
        voodoo->rd_count++;
        addr &= 0xffffff;
        
        cycles -= (int)voodoo->read_time;

        if (addr & 0x800000) /*Texture*/
        {
        }
        else if (addr & 0x400000) { /*Framebuffer*/
                if (SLI_ENABLED) {
                        voodoo_set_t *set = voodoo->set;
                        int y = (addr >> 11) & 0x3ff;
                
                        if (y & 1)
                                voodoo = set->voodoos[1];
                        else
                                voodoo = set->voodoos[0];
                }

                voodoo->flush = 1;
                while (!FIFO_EMPTY) {
                        voodoo_wake_fifo_thread_now(voodoo);
                        thread_wait_event(voodoo->fifo_not_full_event, 1);
                }
                voodoo_wait_for_render_thread_idle(voodoo);
                voodoo->flush = 0;
                
                temp = voodoo_fb_readl(addr, voodoo);
        }
        else switch (addr & 0x3fc) {
                case SST_status: {
                        int fifo_entries = FIFO_ENTRIES;
                        int swap_count = voodoo->swap_count;
                        int written = voodoo->cmd_written + voodoo->cmd_written_fifo;
                        int busy = (written - voodoo->cmd_read) || (voodoo->cmdfifo_depth_rd != voodoo->cmdfifo_depth_wr);

                        if (SLI_ENABLED && voodoo->type != VOODOO_2) {
                                voodoo_t *voodoo_other = (voodoo == voodoo->set->voodoos[0]) ? voodoo->set->voodoos[1] : voodoo->set->voodoos[0];
                                int other_written = voodoo_other->cmd_written + voodoo_other->cmd_written_fifo;
                                                        
                                if (voodoo_other->swap_count > swap_count)
                                        swap_count = voodoo_other->swap_count;
                                if ((voodoo_other->fifo_write_idx - voodoo_other->fifo_read_idx) > fifo_entries)
                                        fifo_entries = voodoo_other->fifo_write_idx - voodoo_other->fifo_read_idx;
                                if ((other_written - voodoo_other->cmd_read) ||
                                    (voodoo_other->cmdfifo_depth_rd != voodoo_other->cmdfifo_depth_wr))
                                        busy = 1;
                                if (!voodoo_other->voodoo_busy)
                                        voodoo_wake_fifo_thread(voodoo_other);
                        }
                        
                        fifo_size = 0xffff - fifo_entries;
                        temp = fifo_size << 12;
                        if (fifo_size < 0x40)
                                temp |= fifo_size;
                        else
                                temp |= 0x3f;
                        if (swap_count < 7)
                                temp |= (swap_count << 28);
                        else
                                temp |= (7 << 28);
                        if (!voodoo->v_retrace)
                                temp |= 0x40;

                        if (busy)
                                temp |= 0x380; /*Busy*/

                        if (!voodoo->voodoo_busy)
                                voodoo_wake_fifo_thread(voodoo);
                }
                break;

                case SST_fbzColorPath:
                voodoo_flush(voodoo);
                temp = voodoo->params.fbzColorPath;
                break;
                case SST_fogMode:
                voodoo_flush(voodoo);
                temp = voodoo->params.fogMode;
                break;
                case SST_alphaMode:
                voodoo_flush(voodoo);
                temp = voodoo->params.alphaMode;
                break;
                case SST_fbzMode:
                voodoo_flush(voodoo);
                temp = voodoo->params.fbzMode;
                break;                        
                case SST_lfbMode:
                voodoo_flush(voodoo);
                temp = voodoo->lfbMode;
                break;
                case SST_clipLeftRight:
                voodoo_flush(voodoo);
                temp = voodoo->params.clipRight | (voodoo->params.clipLeft << 16);
                break;
                case SST_clipLowYHighY:
                voodoo_flush(voodoo);
                temp = voodoo->params.clipHighY | (voodoo->params.clipLowY << 16);
                break;

                case SST_stipple:
                voodoo_flush(voodoo);
                temp = voodoo->params.stipple;
                break;
                case SST_color0:
                voodoo_flush(voodoo);
                temp = voodoo->params.color0;
                break;
                case SST_color1:
                voodoo_flush(voodoo);
                temp = voodoo->params.color1;
                break;
                
                case SST_fbiPixelsIn:
                temp = voodoo->fbiPixelsIn & 0xffffff;
                break;
                case SST_fbiChromaFail:
                temp = voodoo->fbiChromaFail & 0xffffff;
                break;
                case SST_fbiZFuncFail:
                temp = voodoo->fbiZFuncFail & 0xffffff;
                break;
                case SST_fbiAFuncFail:
                temp = voodoo->fbiAFuncFail & 0xffffff;
                break;
                case SST_fbiPixelsOut:
                temp = voodoo->fbiPixelsOut & 0xffffff;
                break;

                case SST_fbiInit4:
                temp = voodoo->fbiInit4;
                break;
                case SST_fbiInit0:
                temp = voodoo->fbiInit0;
                break;
                case SST_fbiInit1:
                temp = voodoo->fbiInit1;
                break;              
                case SST_fbiInit2:
                if (voodoo->initEnable & 0x04)
                        temp = voodoo->dac_readdata;
                else
                        temp = voodoo->fbiInit2;
                break;
                case SST_fbiInit3:
                temp = voodoo->fbiInit3 | (1 << 10) | (2 << 8);
                break;

                case SST_vRetrace:
                timer_clock();
                temp = voodoo->line & 0x1fff;
                break;
                case SST_hvRetrace:
                timer_clock();
                temp = voodoo->line & 0x1fff;
                temp |= ((((voodoo->line_time - voodoo->timer_count) * voodoo->h_total) / voodoo->timer_count) << 16) & 0x7ff0000;
                break;

                case SST_fbiInit5:
                temp = voodoo->fbiInit5 & ~0x1ff;
                break;
                case SST_fbiInit6:
                temp = voodoo->fbiInit6;
                break;
                case SST_fbiInit7:
                temp = voodoo->fbiInit7 & ~0xff;
                break;

                case SST_cmdFifoBaseAddr:
                temp = voodoo->cmdfifo_base >> 12;
                temp |= (voodoo->cmdfifo_end >> 12) << 16;
                break;
                
                case SST_cmdFifoRdPtr:
                temp = voodoo->cmdfifo_rp;
                break;
                case SST_cmdFifoAMin:
                temp = voodoo->cmdfifo_amin;
                break;
                case SST_cmdFifoAMax:
                temp = voodoo->cmdfifo_amax;
                break;
                case SST_cmdFifoDepth:
                temp = voodoo->cmdfifo_depth_wr - voodoo->cmdfifo_depth_rd;
                break;
                
                default:
                fatal("voodoo_readl  : bad addr %08X\n", addr);
                temp = 0xffffffff;
        }
        
        return temp;
}

static void voodoo_writew(uint32_t addr, uint16_t val, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
        voodoo->wr_count++;
        addr &= 0xffffff;

        cycles -= voodoo->write_time;

        if ((addr & 0xc00000) == 0x400000) /*Framebuffer*/
                voodoo_queue_command(voodoo, addr | FIFO_WRITEW_FB, val);
}

static void voodoo_writel(uint32_t addr, uint32_t val, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
	uint32_t pci_time;

        voodoo->wr_count++;

        addr &= 0xffffff;
        
        if (addr == voodoo->last_write_addr+4)
                cycles -= (int)voodoo->burst_time;
        else
                cycles -= (int)voodoo->write_time;
        voodoo->last_write_addr = addr;

        if (addr & 0x800000) { /*Texture*/
                voodoo->tex_count++;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_TEX, val);
        }
        else if (addr & 0x400000) { /*Framebuffer*/
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_FB, val);
        }
        else if ((addr & 0x200000) && (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)) {
//                DEBUG("Write CMDFIFO %08x(%08x) %08x  %08x\n", addr, voodoo->cmdfifo_base + (addr & 0x3fffc), val, (voodoo->cmdfifo_base + (addr & 0x3fffc)) & voodoo->fb_mask);
                *(uint32_t *)&voodoo->fb_mem[(voodoo->cmdfifo_base + (addr & 0x3fffc)) & voodoo->fb_mask] = val;
                voodoo->cmdfifo_depth_wr++;
                if ((voodoo->cmdfifo_depth_wr - voodoo->cmdfifo_depth_rd) < 20)
                        voodoo_wake_fifo_thread(voodoo);
        }
        else switch (addr & 0x3fc) {
                case SST_intrCtrl:
                fatal("intrCtrl write %08x\n", val);
                break;

                case SST_userIntrCMD:
                fatal("userIntrCMD write %08x\n", val);
                break;
                
                case SST_swapbufferCMD:
                voodoo->cmd_written++;
                voodoo->swap_count++;
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        return;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                if (!voodoo->voodoo_busy)
                        voodoo_wake_fifo_threads(voodoo->set, voodoo);
                break;
                case SST_triangleCMD:
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        return;
                voodoo->cmd_written++;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                if (!voodoo->voodoo_busy)
                        voodoo_wake_fifo_threads(voodoo->set, voodoo);
                break;
                case SST_ftriangleCMD:
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        return;
                voodoo->cmd_written++;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                if (!voodoo->voodoo_busy)
                        voodoo_wake_fifo_threads(voodoo->set, voodoo);
                break;
                case SST_fastfillCMD:
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        return;
                voodoo->cmd_written++;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                if (!voodoo->voodoo_busy)
                        voodoo_wake_fifo_threads(voodoo->set, voodoo);
                break;
                case SST_nopCMD:
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        return;
                voodoo->cmd_written++;
                voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                if (!voodoo->voodoo_busy)
                        voodoo_wake_fifo_threads(voodoo->set, voodoo);
                break;
                        
                case SST_fbiInit4:
                if (voodoo->initEnable & 0x01)
                {
			pci_time = pci_get_speed(0) + pci_get_speed(1);
                        voodoo->fbiInit4 = val;
                        voodoo->read_time = pci_time * ((voodoo->fbiInit4 & 1) ? 2 : 1);
//                        DEBUG("fbiInit4 write %08x - read_time=%i\n", val, voodoo->read_time);
                }
                break;
                case SST_backPorch:
                voodoo->backPorch = val;
                break;
                case SST_videoDimensions:
                voodoo->videoDimensions = val;
                voodoo->h_disp = (val & 0xfff) + 1;
                voodoo->v_disp = (val >> 16) & 0xfff;
                break;
                case SST_fbiInit0:
                if (voodoo->initEnable & 0x01) {
                        voodoo->fbiInit0 = val;
                        if (voodoo->set->nr_cards == 2)
                                svga_set_override(voodoo->svga, (voodoo->set->voodoos[0]->fbiInit0 | voodoo->set->voodoos[1]->fbiInit0) & 1);
                        else
                                svga_set_override(voodoo->svga, val & 1);
                        if (val & FBIINIT0_GRAPHICS_RESET) {
                                /*Reset display/draw buffer selection. This may not actually
                                  happen here on a real Voodoo*/
                                voodoo->disp_buffer = 0;
                                voodoo->draw_buffer = 1;
                                voodoo_recalc(voodoo);
                                voodoo->front_offset = voodoo->params.front_offset;
                        }
                }
                break;
                case SST_fbiInit1:
                if (voodoo->initEnable & 0x01) {
                        if ((voodoo->fbiInit1 & FBIINIT1_VIDEO_RESET) && !(val & FBIINIT1_VIDEO_RESET)) {
                                voodoo->line = 0;
                                voodoo->swap_count = 0;
                                voodoo->retrace_count = 0;
                        }
                        voodoo->fbiInit1 = (val & ~5) | (voodoo->fbiInit1 & 5);
			pci_time = pci_get_speed(1);
                        voodoo->burst_time = pci_time * ((voodoo->fbiInit1 & 2) ? 2 : 1);
			pci_time += pci_get_speed(0);
                        voodoo->write_time = pci_time * ((voodoo->fbiInit1 & 2) ? 1 : 0);
//                        DEBUG("fbiInit1 write %08x - write_time=%i burst_time=%i\n", val, voodoo->write_time, voodoo->burst_time);
                }
                break;
                case SST_fbiInit2:
                if (voodoo->initEnable & 0x01) {
                        voodoo->fbiInit2 = val;
                        voodoo_recalc(voodoo);
                }
                break;
                case SST_fbiInit3:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit3 = val;
                break;

                case SST_hSync:
                voodoo->hSync = val;
                voodoo->h_total = (val & 0xffff) + (val >> 16);
                voodoo_pixelclock_update(voodoo);
                break;
                case SST_vSync:
                voodoo->vSync = val;
                voodoo->v_total = (val & 0xffff) + (val >> 16);
                break;
                
                case SST_clutData:
                voodoo->clutData[(val >> 24) & 0x3f].b = val & 0xff;
                voodoo->clutData[(val >> 24) & 0x3f].g = (val >> 8) & 0xff;
                voodoo->clutData[(val >> 24) & 0x3f].r = (val >> 16) & 0xff;
                if (val & 0x20000000)
                {
                        voodoo->clutData[(val >> 24) & 0x3f].b = 255;
                        voodoo->clutData[(val >> 24) & 0x3f].g = 255;
                        voodoo->clutData[(val >> 24) & 0x3f].r = 255;
                }
                voodoo->clutData_dirty = 1;
                break;

                case SST_dacData:
                voodoo->dac_reg = (val >> 8) & 7;
                voodoo->dac_readdata = 0xff;
                if (val & 0x800) {
//                        DEBUG("  dacData read %i %02X\n", voodoo->dac_reg, voodoo->dac_data[7]);
                        if (voodoo->dac_reg == 5) {
                                switch (voodoo->dac_data[7]) {
        				case 0x01: voodoo->dac_readdata = 0x55; break;
        				case 0x07: voodoo->dac_readdata = 0x71; break;
        				case 0x0b: voodoo->dac_readdata = 0x79; break;
                                }
                        }
                        else
                                voodoo->dac_readdata = voodoo->dac_data[voodoo->dac_readdata & 7];
                } else {
                        if (voodoo->dac_reg == 5) {
                                if (!voodoo->dac_reg_ff)
                                        voodoo->dac_pll_regs[voodoo->dac_data[4] & 0xf] = (voodoo->dac_pll_regs[voodoo->dac_data[4] & 0xf] & 0xff00) | val;
                                else
                                        voodoo->dac_pll_regs[voodoo->dac_data[4] & 0xf] = (voodoo->dac_pll_regs[voodoo->dac_data[4] & 0xf] & 0xff) | (val << 8);
//                                DEBUG("Write PLL reg %x %04x\n", voodoo->dac_data[4] & 0xf, voodoo->dac_pll_regs[voodoo->dac_data[4] & 0xf]);
                                voodoo->dac_reg_ff = !voodoo->dac_reg_ff;
                                if (!voodoo->dac_reg_ff)
                                        voodoo->dac_data[4]++;

                        } else {
                                voodoo->dac_data[voodoo->dac_reg] = val & 0xff;
                                voodoo->dac_reg_ff = 0;
                        }
                        voodoo_pixelclock_update(voodoo);
                }
                break;

		case SST_scrFilter:
		if (voodoo->initEnable & 0x01) {
			voodoo->scrfilterEnabled = 1;
			voodoo->scrfilterThreshold = val; 	/* update the threshold values and generate a new lookup table if necessary */
		
			if (val < 1) 
				voodoo->scrfilterEnabled = 0;
			voodoo_threshold_check(voodoo);		
			DEBUG("Voodoo Filter: %06x\n", val);
		}
		break;

                case SST_fbiInit5:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit5 = (val & ~0x41e6) | (voodoo->fbiInit5 & 0x41e6);
                break;
                case SST_fbiInit6:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit6 = val;
                break;
                case SST_fbiInit7:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit7 = val;
                break;

                case SST_cmdFifoBaseAddr:
                voodoo->cmdfifo_base = (val & 0x3ff) << 12;
                voodoo->cmdfifo_end = ((val >> 16) & 0x3ff) << 12;
//                DEBUG("CMDFIFO base=%08x end=%08x\n", voodoo->cmdfifo_base, voodoo->cmdfifo_end);
                break;

                case SST_cmdFifoRdPtr:
                voodoo->cmdfifo_rp = val;
                break;
                case SST_cmdFifoAMin:
                voodoo->cmdfifo_amin = val;
                break;
                case SST_cmdFifoAMax:
                voodoo->cmdfifo_amax = val;
                break;
                case SST_cmdFifoDepth:
                voodoo->cmdfifo_depth_rd = 0;
                voodoo->cmdfifo_depth_wr = val & 0xffff;
                break;

                default:
                if (voodoo->fbiInit7 & FBIINIT7_CMDFIFO_ENABLE)
                        DEBUG("Unknown register write in CMDFIFO mode %08x %08x\n", addr, val);
		else
                        voodoo_queue_command(voodoo, addr | FIFO_WRITEL_REG, val);
                break;
        }
}

static uint16_t voodoo_snoop_readw(uint32_t addr, void *priv)
{
        voodoo_set_t *set = (voodoo_set_t *)priv;
        
        return voodoo_readw(addr, set->voodoos[0]);
}
static uint32_t voodoo_snoop_readl(uint32_t addr, void *priv)
{
        voodoo_set_t *set = (voodoo_set_t *)priv;
        
        return voodoo_readl(addr, set->voodoos[0]);
}

static void voodoo_snoop_writew(uint32_t addr, uint16_t val, void *priv)
{
        voodoo_set_t *set = (voodoo_set_t *)priv;

        voodoo_writew(addr, val, set->voodoos[0]);
        voodoo_writew(addr, val, set->voodoos[1]);
}
static void voodoo_snoop_writel(uint32_t addr, uint32_t val, void *priv)
{
        voodoo_set_t *set = (voodoo_set_t *)priv;

        voodoo_writel(addr, val, set->voodoos[0]);
        voodoo_writel(addr, val, set->voodoos[1]);
}

static void voodoo_recalcmapping(voodoo_set_t *set)
{
        if (set->nr_cards == 2) {
                if (set->voodoos[0]->pci_enable && set->voodoos[0]->memBaseAddr) {
                        if (set->voodoos[0]->type == VOODOO_2 && set->voodoos[1]->initEnable & (1 << 23))
                        {
                                DEBUG("voodoo_recalcmapping (pri) with snoop : memBaseAddr %08X\n", set->voodoos[0]->memBaseAddr);
                                mem_map_disable(&set->voodoos[0]->mapping);
                                mem_map_set_addr(&set->snoop_mapping, set->voodoos[0]->memBaseAddr, 0x01000000);
                        }
                        else if (set->voodoos[1]->pci_enable && (set->voodoos[0]->memBaseAddr == set->voodoos[1]->memBaseAddr))
                        {
                                DEBUG("voodoo_recalcmapping (pri) (sec) same addr : memBaseAddr %08X\n", set->voodoos[0]->memBaseAddr);
                                mem_map_disable(&set->voodoos[0]->mapping);
                                mem_map_disable(&set->voodoos[1]->mapping);
                                mem_map_set_addr(&set->snoop_mapping, set->voodoos[0]->memBaseAddr, 0x01000000);
                                return;
                        } else {
                                DEBUG("voodoo_recalcmapping (pri) : memBaseAddr %08X\n", set->voodoos[0]->memBaseAddr);
                                mem_map_disable(&set->snoop_mapping);
                                mem_map_set_addr(&set->voodoos[0]->mapping, set->voodoos[0]->memBaseAddr, 0x01000000);
                        }
                } else {
                        DEBUG("voodoo_recalcmapping (pri) : disabled\n");
                        mem_map_disable(&set->voodoos[0]->mapping);
                }

                if (set->voodoos[1]->pci_enable && set->voodoos[1]->memBaseAddr) {
                        DEBUG("voodoo_recalcmapping (sec) : memBaseAddr %08X\n", set->voodoos[1]->memBaseAddr);
                        mem_map_set_addr(&set->voodoos[1]->mapping, set->voodoos[1]->memBaseAddr, 0x01000000);
                } else {
                        DEBUG("voodoo_recalcmapping (sec) : disabled\n");
                        mem_map_disable(&set->voodoos[1]->mapping);
                }
        } else {
                voodoo_t *voodoo = set->voodoos[0];
                
                if (voodoo->pci_enable && voodoo->memBaseAddr) {
                        DEBUG("voodoo_recalcmapping : memBaseAddr %08X\n", voodoo->memBaseAddr);
                        mem_map_set_addr(&voodoo->mapping, voodoo->memBaseAddr, 0x01000000);
                } else {
                        DEBUG("voodoo_recalcmapping : disabled\n");
                        mem_map_disable(&voodoo->mapping);
                }
        }
}

uint8_t voodoo_pci_read(int func, int addr, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;

        if (func)
                return 0;

//        DEBUG("Voodoo PCI read %08X PC=%08x\n", addr, cpu_state.pc);

        switch (addr) {
                case 0x00: return 0x1a; /*3dfx*/
                case 0x01: return 0x12;
                
                case 0x02:
                if (voodoo->type == VOODOO_2)
                        return 0x02; /*Voodoo 2*/
                else
                        return 0x01; /*SST-1 (Voodoo Graphics)*/
                case 0x03: return 0x00;
                
                case 0x04: return voodoo->pci_enable ? 0x02 : 0x00; /*Respond to memory accesses*/

                case 0x08: return 2; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                case 0x0a: return 0;
                case 0x0b: return 0x04;
                
                case 0x10: return 0x00; /*memBaseAddr*/
                case 0x11: return 0x00;
                case 0x12: return 0x00;
                case 0x13: return voodoo->memBaseAddr >> 24;

                case 0x40:
                return voodoo->initEnable & 0xff;
                case 0x41:
                if (voodoo->type == VOODOO_2)
                        return 0x50 | ((voodoo->initEnable >> 8) & 0x0f);
                return (voodoo->initEnable >> 8) & 0x0f;
                case 0x42:
                return (voodoo->initEnable >> 16) & 0xff;
                case 0x43:
                return (voodoo->initEnable >> 24) & 0xff;
        }
        return 0;
}

void voodoo_pci_write(int func, int addr, uint8_t val, void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
        
        if (func)
                return;

//        DEBUG("Voodoo PCI write %04X %02X PC=%08x\n", addr, val, cpu_state.pc);

        switch (addr) {
                case 0x04:
                voodoo->pci_enable = val & 2;
                voodoo_recalcmapping(voodoo->set);
                break;
                
                case 0x13:
                voodoo->memBaseAddr = val << 24;
                voodoo_recalcmapping(voodoo->set);
                break;
                
                case 0x40:
                voodoo->initEnable = (voodoo->initEnable & ~0x000000ff) | val;
                break;
                case 0x41:
                voodoo->initEnable = (voodoo->initEnable & ~0x0000ff00) | (val << 8);
                break;
                case 0x42:
                voodoo->initEnable = (voodoo->initEnable & ~0x00ff0000) | (val << 16);
                voodoo_recalcmapping(voodoo->set);
                break;
                case 0x43:
                voodoo->initEnable = (voodoo->initEnable & ~0xff000000) | (val << 24);
                voodoo_recalcmapping(voodoo->set);
                break;
        }
}


static void voodoo_speed_changed(void *priv)
{
        voodoo_set_t *voodoo_set = (voodoo_set_t *)priv;
	uint32_t pci_time = pci_get_speed(1);
        
        voodoo_pixelclock_update(voodoo_set->voodoos[0]);
        voodoo_set->voodoos[0]->burst_time = pci_time * ((voodoo_set->voodoos[0]->fbiInit1 & 2) ? 2 : 1);
	pci_time += pci_get_speed(0);
        voodoo_set->voodoos[0]->read_time = pci_time * ((voodoo_set->voodoos[0]->fbiInit4 & 1) ? 2 : 1);
        voodoo_set->voodoos[0]->write_time = pci_time * ((voodoo_set->voodoos[0]->fbiInit1 & 2) ? 1 : 0);
        if (voodoo_set->nr_cards == 2) {
                voodoo_pixelclock_update(voodoo_set->voodoos[1]);
                voodoo_set->voodoos[1]->read_time = pci_time * ((voodoo_set->voodoos[1]->fbiInit4 & 1) ? 2 : 1);
                voodoo_set->voodoos[1]->write_time = pci_time * ((voodoo_set->voodoos[1]->fbiInit1 & 2) ? 1 : 0);
		pci_time = pci_get_speed(1);
                voodoo_set->voodoos[1]->burst_time = pci_time * ((voodoo_set->voodoos[1]->fbiInit1 & 2) ? 2 : 1);
        }
//        DEBUG("Voodoo read_time=%i write_time=%i burst_time=%i %08x %08x\n", voodoo->read_time, voodoo->write_time, voodoo->burst_time, voodoo->fbiInit1, voodoo->fbiInit4);
}

void *voodoo_card_init()
{
        int c;
        voodoo_t *voodoo = (voodoo_t *)mem_alloc(sizeof(voodoo_t));
        memset(voodoo, 0, sizeof(voodoo_t));

        voodoo->bilinear_enabled = device_get_config_int("bilinear");
        voodoo->scrfilter = device_get_config_int("dacfilter");
        voodoo->texture_size = device_get_config_int("texture_memory");
        voodoo->texture_mask = (voodoo->texture_size << 20) - 1;
        voodoo->fb_size = device_get_config_int("framebuffer_memory");
        voodoo->fb_mask = (voodoo->fb_size << 20) - 1;
        voodoo->render_threads = device_get_config_int("render_threads");
        voodoo->odd_even_mask = voodoo->render_threads - 1;
#ifndef NO_CODEGEN
        voodoo->use_recompiler = device_get_config_int("recompiler");
#endif                        
        voodoo->type = device_get_config_int("type");
        switch (voodoo->type) {
                case VOODOO_1:
                voodoo->dual_tmus = 0;
                break;
                case VOODOO_SB50:
                voodoo->dual_tmus = 1;
                break;
                case VOODOO_2:
                voodoo->dual_tmus = 1;
                break;
        }
        
	if (voodoo->type == VOODOO_2) /*generate filter lookup tables*/
		voodoo_generate_filter_v2(voodoo);
	else
		voodoo_generate_filter_v1(voodoo);
        
        pci_add_card(PCI_ADD_NORMAL, voodoo_pci_read, voodoo_pci_write, voodoo);

        mem_map_add(&voodoo->mapping, 0, 0, NULL, voodoo_readw, voodoo_readl, NULL, voodoo_writew, voodoo_writel,     NULL, MEM_MAPPING_EXTERNAL, voodoo);

        voodoo->fb_mem = (uint8_t *)mem_alloc(4 * 1024 * 1024);
        voodoo->tex_mem[0] = (uint8_t *)mem_alloc(voodoo->texture_size * 1024 * 1024);
        if (voodoo->dual_tmus)
                voodoo->tex_mem[1] = (uint8_t *)mem_alloc(voodoo->texture_size * 1024 * 1024);
        voodoo->tex_mem_w[0] = (uint16_t *)voodoo->tex_mem[0];
        voodoo->tex_mem_w[1] = (uint16_t *)voodoo->tex_mem[1];
        
        for (c = 0; c < TEX_CACHE_MAX; c++) {
                voodoo->texture_cache[0][c].data = (uint32_t *)mem_alloc((256*256 + 256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2) * 4);
                voodoo->texture_cache[0][c].base = -1; /*invalid*/
                voodoo->texture_cache[0][c].refcount = 0;
                if (voodoo->dual_tmus) {
                        voodoo->texture_cache[1][c].data = (uint32_t *)mem_alloc((256*256 + 256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2) * 4);
                        voodoo->texture_cache[1][c].base = -1; /*invalid*/
                        voodoo->texture_cache[1][c].refcount = 0;
                }
        }

        timer_add(voodoo_callback, voodoo,
		  &voodoo->timer_count, TIMER_ALWAYS_ENABLED);
        
        voodoo->svga = svga_get_pri();
        voodoo->fbiInit0 = 0;

        voodoo->wake_fifo_thread = thread_create_event();
        voodoo->wake_render_thread[0] = thread_create_event();
        voodoo->wake_render_thread[1] = thread_create_event();
        voodoo->wake_main_thread = thread_create_event();
        voodoo->fifo_not_full_event = thread_create_event();
        voodoo->render_not_full_event[0] = thread_create_event();
        voodoo->render_not_full_event[1] = thread_create_event();
        voodoo->fifo_thread = thread_create(voodoo_fifo_thread, voodoo);
        voodoo->render_thread[0] = thread_create(voodoo_render_thread_1, voodoo);
        if (voodoo->render_threads == 2)
                voodoo->render_thread[1] = thread_create(voodoo_render_thread_2, voodoo);

        timer_add(voodoo_wake_timer, voodoo,
		  &voodoo->wake_timer, &voodoo->wake_timer);
        
        for (c = 0; c < 0x100; c++) {
                rgb332[c].r = c & 0xe0;
                rgb332[c].g = (c << 3) & 0xe0;
                rgb332[c].b = (c << 6) & 0xc0;
                rgb332[c].r = rgb332[c].r | (rgb332[c].r >> 3) | (rgb332[c].r >> 6);
                rgb332[c].g = rgb332[c].g | (rgb332[c].g >> 3) | (rgb332[c].g >> 6);
                rgb332[c].b = rgb332[c].b | (rgb332[c].b >> 2);
                rgb332[c].b = rgb332[c].b | (rgb332[c].b >> 4);
                rgb332[c].a = 0xff;
                
                ai44[c].a = (c & 0xf0) | ((c & 0xf0) >> 4);
                ai44[c].r = (c & 0x0f) | ((c & 0x0f) << 4);
                ai44[c].g = ai44[c].b = ai44[c].r;
        }
                
        for (c = 0; c < 0x10000; c++) {
                rgb565[c].r = (c >> 8) & 0xf8;
                rgb565[c].g = (c >> 3) & 0xfc;
                rgb565[c].b = (c << 3) & 0xf8;
                rgb565[c].r |= (rgb565[c].r >> 5);
                rgb565[c].g |= (rgb565[c].g >> 6);
                rgb565[c].b |= (rgb565[c].b >> 5);
                rgb565[c].a = 0xff;

                argb1555[c].r = (c >> 7) & 0xf8;
                argb1555[c].g = (c >> 2) & 0xf8;
                argb1555[c].b = (c << 3) & 0xf8;
                argb1555[c].r |= (argb1555[c].r >> 5);
                argb1555[c].g |= (argb1555[c].g >> 5);
                argb1555[c].b |= (argb1555[c].b >> 5);
                argb1555[c].a = (c & 0x8000) ? 0xff : 0;

                argb4444[c].a = (c >> 8) & 0xf0;
                argb4444[c].r = (c >> 4) & 0xf0;
                argb4444[c].g = c & 0xf0;
                argb4444[c].b = (c << 4) & 0xf0;
                argb4444[c].a |= (argb4444[c].a >> 4);
                argb4444[c].r |= (argb4444[c].r >> 4);
                argb4444[c].g |= (argb4444[c].g >> 4);
                argb4444[c].b |= (argb4444[c].b >> 4);
                
                ai88[c].a = (c >> 8);
                ai88[c].r = c & 0xff;
                ai88[c].g = c & 0xff;
                ai88[c].b = c & 0xff;
        }
#ifndef NO_CODEGEN
        voodoo_codegen_init(voodoo);
#endif

        voodoo->disp_buffer = 0;
        voodoo->draw_buffer = 1;
        
        return voodoo;
}


void *
voodoo_init(const device_t *info, UNUSED(void *parent))
{
        voodoo_set_t *voodoo_set = (voodoo_set_t *)mem_alloc(sizeof(voodoo_set_t));
        uint32_t tmuConfig = 1;
        int type;
        memset(voodoo_set, 0, sizeof(voodoo_set_t));
        
        rgb332 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x100);
        ai44 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x100);
        rgb565 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x10000);
        argb1555 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x10000);
        argb4444 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x10000);
        ai88 = (rgba8_t *)mem_alloc(sizeof(rgba8_t)*0x10000);

        type = device_get_config_int("type");
        
        voodoo_set->nr_cards = device_get_config_int("sli") ? 2 : 1;
        voodoo_set->voodoos[0] = (voodoo_t *)voodoo_card_init();
        voodoo_set->voodoos[0]->set = voodoo_set;
        if (voodoo_set->nr_cards == 2) {
                voodoo_set->voodoos[1] = (voodoo_t *)voodoo_card_init();
                                
                voodoo_set->voodoos[1]->set = voodoo_set;

                if (type == VOODOO_2) {
                        voodoo_set->voodoos[0]->fbiInit5 |= FBIINIT5_MULTI_CVG;
                        voodoo_set->voodoos[1]->fbiInit5 |= FBIINIT5_MULTI_CVG;
                } else {
                        voodoo_set->voodoos[0]->fbiInit1 |= FBIINIT1_MULTI_SST;
                        voodoo_set->voodoos[1]->fbiInit1 |= FBIINIT1_MULTI_SST;
                }
        }

        switch (type) {
                case VOODOO_1:
                if (voodoo_set->nr_cards == 2)
                        tmuConfig = 1 | (3 << 3);
                else
                        tmuConfig = 1;
                break;
                case VOODOO_SB50:
                if (voodoo_set->nr_cards == 2)
                        tmuConfig = 1 | (3 << 3) | (3 << 6) | (2 << 9);
                else
                        tmuConfig = 1 | (3 << 6);
                break;
                case VOODOO_2:
                tmuConfig = 1 | (3 << 6);
                break;
        }
        
        voodoo_set->voodoos[0]->tmuConfig = tmuConfig;
        if (voodoo_set->nr_cards == 2)
                voodoo_set->voodoos[1]->tmuConfig = tmuConfig;

        mem_map_add(&voodoo_set->snoop_mapping, 0, 0, NULL, voodoo_snoop_readw, voodoo_snoop_readl, NULL, voodoo_snoop_writew, voodoo_snoop_writel,     NULL, MEM_MAPPING_EXTERNAL, voodoo_set);
                
        return voodoo_set;
}


void
voodoo_card_close(voodoo_t *voodoo)
{
#ifndef RELEASE_BUILD
        FILE *f;
#endif
        int c;
        
#ifndef RELEASE_BUILD        
        f = plat_fopen(nvr_path(L"texram.dmp"), L"wb");
	if (f != NULL)
	{
        	(void)fwrite(voodoo->tex_mem[0], voodoo->texture_size*1024*1024, 1, f);
        	fclose(f);
	}
        if (voodoo->dual_tmus)
        {
                f = plat_fopen(nvr_path(L"texram2.dmp"), L"wb");
		if (f != NULL)
		{
                	(void)fwrite(voodoo->tex_mem[1], voodoo->texture_size*1024*1024, 1, f);
                	fclose(f);
		}
        }
#endif

        thread_kill(voodoo->fifo_thread);
        thread_kill(voodoo->render_thread[0]);
        if (voodoo->render_threads == 2)
                thread_kill(voodoo->render_thread[1]);
        thread_destroy_event(voodoo->fifo_not_full_event);
        thread_destroy_event(voodoo->wake_main_thread);
        thread_destroy_event(voodoo->wake_fifo_thread);
        thread_destroy_event(voodoo->wake_render_thread[0]);
        thread_destroy_event(voodoo->wake_render_thread[1]);
        thread_destroy_event(voodoo->render_not_full_event[0]);
        thread_destroy_event(voodoo->render_not_full_event[1]);

        for (c = 0; c < TEX_CACHE_MAX; c++) {
                if (voodoo->dual_tmus)
                        free(voodoo->texture_cache[1][c].data);
                free(voodoo->texture_cache[0][c].data);
        }
#ifndef NO_CODEGEN
        voodoo_codegen_close(voodoo);
#endif
        free(voodoo->fb_mem);
        if (voodoo->dual_tmus)
                free(voodoo->tex_mem[1]);
        free(voodoo->tex_mem[0]);
        free(voodoo);
}


void
voodoo_close(void *priv)
{
        voodoo_set_t *voodoo_set = (voodoo_set_t *)priv;
        
        if (voodoo_set->nr_cards == 2)
                voodoo_card_close(voodoo_set->voodoos[1]);
        voodoo_card_close(voodoo_set->voodoos[0]);
        
        free(rgb332);
        free(ai44);
        free(rgb565);
        free(argb1555);
        free(argb4444);
        free(ai88);
        
        free(voodoo_set);
}


static const device_config_t voodoo_config[] =
{
        {
                "type","Voodoo type",CONFIG_SELECTION,"",VOODOO_1,
                {
                        {
                                "Voodoo Graphics",VOODOO_1
                        },
                        {
                                "Obsidian SB50 + Amethyst (2 TMUs)",VOODOO_SB50
                        },
                        {
                                "Voodoo 2",VOODOO_2
                        },
                        {
                                NULL
                        }
                },
        },
        {
                "framebuffer_memory","Framebuffer memory size",CONFIG_SELECTION,"",2,
                {
                        {
                                "2 MB",2
                        },
                        {
                                "4 MB",4
                        },
                        {
                                NULL
                        }
                },
        },
        {
                "texture_memory","Texture memory size",CONFIG_SELECTION,"",2,
                {
                        {
                                "2 MB",2
                        },
                        {
                                "4 MB",4
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
                "dacfilter","Screen Filter",CONFIG_BINARY,"",0
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
        {
                "sli","SLI",CONFIG_BINARY,"",0
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


const device_t voodoo_device = {
    "3DFX Voodoo Graphics",
    DEVICE_PCI,
    0,
    NULL,
    voodoo_init, voodoo_close, NULL,
    NULL,
    voodoo_speed_changed,
    NULL,
    NULL,
    voodoo_config
};
