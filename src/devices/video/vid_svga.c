/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Generic SVGA handling.
 *
 *		This is intended to be used by another SVGA driver,
 *		and not as a card in it's own right.
 *
 * Version:	@(#)vid_svga.c	1.0.25	2020/01/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../system/clk.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"



extern int	cyc_total;
extern uint8_t	edatlookup[4][4];

uint8_t		svga_rotate[8][256];

/*Primary SVGA device. As multiple video cards are not yet supported this is the
  only SVGA device.*/
static svga_t	*svga_pri;


void svga_doblit(int y1, int y2, int wx, int wy, svga_t *svga);


svga_t *
svga_get_pri(void)
{
    return svga_pri;
}

void
svga_set_override(svga_t *svga, int val)
{
    if (svga->override && !val)
	svga->fullchange = changeframecount;
    svga->override = val;
}

void
svga_out(uint16_t addr, uint8_t val, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    uint8_t indx, o;
    int c;

    switch (addr) {
	case 0x3c0:
	case 0x3c1:
		if (!svga->attrff) {
			svga->attraddr = val & 31;
			if ((val & 0x20) != svga->attr_palette_enable) {
				svga->fullchange = 3;
				svga->attr_palette_enable = val & 0x20;
				svga_recalctimings(svga);
			}
		} else {
			if ((svga->attraddr == 0x13) && (svga->attrregs[0x13] != val))
				svga->fullchange = changeframecount;
			o = svga->attrregs[svga->attraddr & 31];
			svga->attrregs[svga->attraddr & 31] = val;
			if (svga->attraddr < 16) 
				svga->fullchange = changeframecount;
			if (svga->attraddr == 0x10 || svga->attraddr == 0x14 || svga->attraddr < 0x10) {
				for (c = 0; c < 16; c++) {
					if (svga->attrregs[0x10] & 0x80) {
						svga->egapal[c] = (svga->attrregs[c] &  0xf) |
								  ((svga->attrregs[0x14] & 0xf) << 4);
					} else {
						svga->egapal[c] = (svga->attrregs[c] & 0x3f) |
								  ((svga->attrregs[0x14] & 0xc) << 4);
					}
				}
			}
			/* Recalculate timings on change of attribute register 0x11
			   (overscan border color) too. */
			if (svga->attraddr == 0x10) {
				if (o != val)
					svga_recalctimings(svga);
			} else if (svga->attraddr == 0x11) {
				svga->overscan_color = svga->pallook[svga->attrregs[0x11] & 0x3f];
				if (o != val)
					svga_recalctimings(svga);
			} else if (svga->attraddr == 0x12) {
				if ((val & 0xf) != svga->plane_mask)
					svga->fullchange = changeframecount;
				svga->plane_mask = val & 0xf;
			}
		}
		svga->attrff ^= 1;
		break;

	case 0x3c2:
		svga->miscout = val;
		svga->vidclock = val & 4;
		io_removehandler(0x03a0, 0x0020, svga->video_in, NULL, NULL, svga->video_out, NULL, NULL, svga->p);
		if (!(val & 1))
			io_sethandler(0x03a0, 0x0020, svga->video_in, NULL, NULL, svga->video_out, NULL, NULL, svga->p);
		svga_recalctimings(svga);
		break;

	case 0x3c4: 
		svga->seqaddr = val; 
		break;

	case 0x3c5:
		if (svga->seqaddr > 0xf)
			return;
		o = svga->seqregs[svga->seqaddr & 0xf];
		svga->seqregs[svga->seqaddr & 0xf] = val;
		if (o != val && (svga->seqaddr & 0xf) == 1)
			svga_recalctimings(svga);
		switch (svga->seqaddr & 0xf) {
			case 1:
				if (svga->scrblank && !(val & 0x20)) 
					svga->fullchange = 3; 
				svga->scrblank = (svga->scrblank & ~0x20) | (val & 0x20); 
				svga_recalctimings(svga);
				break;

			case 2:
				svga->writemask = val & 0xf; 
				break;

			case 3:
				svga->charsetb = (((val >> 2) & 3) * 0x10000) + 2;
				svga->charseta = ((val & 3)  * 0x10000) + 2;
				if (val & 0x10)
					svga->charseta += 0x8000;
				if (val & 0x20)
					svga->charsetb += 0x8000;
				break;

			case 4: 
				svga->chain2_write = !(val & 4);
				svga->chain4 = val & 8;
				svga->fast = (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) &&
					      !svga->gdcreg[1]) && svga->chain4 && !(svga->adv_flags & FLAG_ADDR_BY8);
				break;
		}
		break;

	case 0x3c6: 
		svga->dac_mask = val; 
		break;

	case 0x3c7:
	case 0x3c8:
		svga->dac_pos = 0;
		svga->dac_status = addr & 0x03;
		svga->dac_addr = (val + (addr & 0x01)) & 255;
		break;

	case 0x3c9:
		svga->fullchange = changeframecount;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_r = val;
				svga->dac_pos++; 
				break;

			case 1:
				svga->dac_g = val;
				svga->dac_pos++; 
				break;

                        case 2:
				indx = svga->dac_addr & 255;
				svga->vgapal[indx].r = svga->dac_r;
				svga->vgapal[indx].g = svga->dac_g;
				svga->vgapal[indx].b = val; 
				if (svga->ramdac_type == RAMDAC_8BIT)
					svga->pallook[indx] = makecol32(svga->vgapal[indx].r, svga->vgapal[indx].g, svga->vgapal[indx].b);
				else
					svga->pallook[indx] = makecol32(video_6to8[svga->vgapal[indx].r & 0x3f], video_6to8[svga->vgapal[indx].g & 0x3f], video_6to8[svga->vgapal[indx].b & 0x3f]);
				svga->dac_pos = 0; 
				svga->dac_addr = (svga->dac_addr + 1) & 255; 
				break;
		}
		break;

	case 0x3ce:
		svga->gdcaddr = val; 
		break;

	case 0x3cf:
		o = svga->gdcreg[svga->gdcaddr & 15];
		switch (svga->gdcaddr & 15) {
			case 2:
				svga->colourcompare = val;
				break;

			case 4:
				svga->readplane = val & 3;
				break;

			case 5:
				svga->writemode = val & 3;
				svga->readmode = val & 8;
				svga->chain2_read = val & 0x10;
				break;

			case 6:
				if ((svga->gdcreg[6] & 0xc) != (val & 0xc)) {
					switch (val&0xC) {
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
				break;

			case 7:
				svga->colournocare=val;
				break;
		}
		svga->gdcreg[svga->gdcaddr & 15] = val;                
		svga->fast = (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) &&
			     !svga->gdcreg[1]) && svga->chain4;
		if (((svga->gdcaddr & 15) == 5  && (val ^ o) & 0x70) ||
		    ((svga->gdcaddr & 15) == 6 && (val ^ o) & 1))
			svga_recalctimings(svga);
		break;
    }
}


uint8_t
svga_in(uint16_t addr, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    uint8_t indx, ret = 0xff;

    switch (addr) {
	case 0x3c0:
		ret = svga->attraddr | svga->attr_palette_enable;
		break;

	case 0x3c1:
		ret = svga->attrregs[svga->attraddr];
		break;

	case 0x3c2:
		if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x4e)
			ret = 0;
		else
			ret = 0x10;
		break;

	case 0x3c4:
		ret = svga->seqaddr;
		break;

	case 0x3c5:
		ret = svga->seqregs[svga->seqaddr & 0x0f];
		break;

	case 0x3c6:
		ret = svga->dac_mask;
		break;

	case 0x3c7:
		ret = svga->dac_status;
		break;

	case 0x3c8:
		ret = svga->dac_addr;
		break;

	case 0x3c9:
		indx = (svga->dac_addr - 1) & 255;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_pos++; 
				if (svga->ramdac_type == RAMDAC_8BIT)
					ret = svga->vgapal[indx].r;
				else
					ret = svga->vgapal[indx].r & 0x3f;
				break;

			case 1:
				svga->dac_pos++; 
				if (svga->ramdac_type == RAMDAC_8BIT)
					ret = svga->vgapal[indx].g;
				else
					ret = svga->vgapal[indx].g & 0x3f;
				break;

			case 2:
				svga->dac_pos = 0; 
				svga->dac_addr = (svga->dac_addr + 1) & 255; 
				if (svga->ramdac_type == RAMDAC_8BIT)
					ret = svga->vgapal[indx].b;
				else
					ret = svga->vgapal[indx].b & 0x3f;
				break;
		}
		break;
	case 0x3cc:
		ret = svga->miscout;
		break;
	case 0x3ce:
		ret = svga->gdcaddr;
		break;
	case 0x3cf:
		/* The spec says GDC addresses 0xF8 to 0xFB return the latch. */
		switch(svga->gdcaddr) {
			case 0xf8:
				ret = svga->latch.b[0];
				break;
			case 0xf9:
				ret = svga->latch.b[1];
				break;
			case 0xfa:
				ret = svga->latch.b[2];
				break;
			case 0xfb:
				ret = svga->latch.b[3];
				break;
			default:
                		ret = svga->gdcreg[svga->gdcaddr & 0xf];
				break;
		}
		break;
	case 0x3da:
		svga->attrff = 0;

		if (svga->cgastat & 0x01)
			svga->cgastat &= ~0x30;
		else
			svga->cgastat ^= 0x30;
		ret = svga->cgastat;
		break;
    }

    return(ret);
}


void
svga_set_ramdac_type(svga_t *svga, int type)
{
    int c;

    if (svga->ramdac_type != type) {
	svga->ramdac_type = type;

	for (c = 0; c < 256; c++) {
		if (svga->ramdac_type == RAMDAC_8BIT)
			svga->pallook[c] = makecol32(svga->vgapal[c].r, svga->vgapal[c].g, svga->vgapal[c].b);
		else
			svga->pallook[c] = makecol32((svga->vgapal[c].r & 0x3f) * 4,
						     (svga->vgapal[c].g & 0x3f) * 4,
						     (svga->vgapal[c].b & 0x3f) * 4);
	}
    }
}


void
svga_recalctimings(svga_t *svga)
{
    double crtcconst, _dispontime, _dispofftime, disptime;

    svga->vtotal = svga->crtc[6];
    svga->dispend = svga->crtc[0x12];
    svga->vsyncstart = svga->crtc[0x10];
    svga->split = svga->crtc[0x18];
    svga->vblankstart = svga->crtc[0x15];

    if (svga->crtc[7] & 1)
	svga->vtotal |= 0x100;
    if (svga->crtc[7] & 32)
	svga->vtotal |= 0x200;
    svga->vtotal += 2;

    if (svga->crtc[7] & 2)
	svga->dispend |= 0x100;
    if (svga->crtc[7] & 64)
	svga->dispend |= 0x200;
    svga->dispend++;

    if (svga->crtc[7] & 4)
	svga->vsyncstart |= 0x100;
    if (svga->crtc[7] & 128)
	svga->vsyncstart |= 0x200;
    svga->vsyncstart++;

    if (svga->crtc[7] & 0x10)
	svga->split|=0x100;
    if (svga->crtc[9] & 0x40)
	svga->split|=0x200;
    svga->split++;

    if (svga->crtc[7] & 0x08)
	svga->vblankstart |= 0x100;
    if (svga->crtc[9] & 0x20)
	svga->vblankstart |= 0x200;
    svga->vblankstart++;

    svga->hdisp = svga->crtc[1];
    svga->hdisp++;

    svga->htotal = svga->crtc[0];
    svga->htotal += 6;	/*+6 is required for Tyrian*/

    svga->rowoffset = svga->crtc[0x13];

    svga->clock = (svga->vidclock) ? VGACONST2 : VGACONST1;

    svga->lowres = svga->attrregs[0x10] & 0x40;

    svga->interlace = 0;

    svga->ma_latch = (svga->crtc[0xc] << 8) | svga->crtc[0xd];

    svga->hdisp_time = svga->hdisp;
    svga->render = svga_render_blank;
    if (!svga->scrblank && svga->attr_palette_enable) {
	if (!(svga->gdcreg[6] & 1) && !(svga->attrregs[0x10] & 1)) { /*Text mode*/
		if (svga->seqregs[1] & 8) /*40 column*/ {
			svga->render = svga_render_text_40;
			svga->hdisp *= (svga->seqregs[1] & 1) ? 16 : 18;
		} else {
			svga->render = svga_render_text_80;
			svga->hdisp *= (svga->seqregs[1] & 1) ? 8 : 9;
		}
		svga->hdisp_old = svga->hdisp;
	} else {
		svga->hdisp *= (svga->seqregs[1] & 8) ? 16 : 8;
		svga->hdisp_old = svga->hdisp;                        

		switch (svga->gdcreg[5] & 0x60) {
			case 0x00: 
				if (svga->seqregs[1] & 8) /*Low res (320)*/
					svga->render = svga_render_4bpp_lowres;
				else
					svga->render = svga_render_4bpp_highres;
				break;
			case 0x20:		/*4 colours*/
				if (svga->seqregs[1] & 8) /*Low res (320)*/
					svga->render = svga_render_2bpp_lowres;
				else
					svga->render = svga_render_2bpp_highres;
				break;
			case 0x40: case 0x60:	/*256+ colours*/
				switch (svga->bpp) {
					case 8:
						svga->map8 = svga->pallook;
						if (svga->lowres)
							svga->render = svga_render_8bpp_lowres;
						else
							svga->render = svga_render_8bpp_highres;
						break;
					case 15:
						if (svga->lowres)
							svga->render = svga_render_15bpp_lowres;
						else
							svga->render = svga_render_15bpp_highres;
						break;
					case 16:
						if (svga->lowres)
							svga->render = svga_render_16bpp_lowres;
						else
							svga->render = svga_render_16bpp_highres;
						break;
					case 24:
						if (svga->lowres)
							svga->render = svga_render_24bpp_lowres;
						else
							svga->render = svga_render_24bpp_highres;
						break;
					case 32:
						if (svga->lowres)
							svga->render = svga_render_32bpp_lowres;
						else
							svga->render = svga_render_32bpp_highres;
						break;
				}
				break;
		}
	}
    }

    svga->linedbl = svga->crtc[9] & 0x80;
    svga->rowcount = svga->crtc[9] & 31;
    if (enable_overscan) {
	overscan_y = (svga->rowcount + 1) << 1;
	if (svga->seqregs[1] & 8)	/*Low res (320)*/
		overscan_y <<= 1;
	if (overscan_y < 16)
		overscan_y = 16;
    }
    if (svga->recalctimings_ex) 
	svga->recalctimings_ex(svga);

    if (svga->vblankstart < svga->dispend)
	svga->dispend = svga->vblankstart;

    crtcconst = (svga->seqregs[1] & 1) ? (svga->clock * 8.0) : (svga->clock * 9.0);

    disptime  = svga->htotal;
    _dispontime = svga->hdisp_time;

    if (svga->seqregs[1] & 8) {
	disptime *= 2;
	_dispontime *= 2;
    }
    _dispofftime = disptime - _dispontime;
    _dispontime *= crtcconst;
    _dispofftime *= crtcconst;

    svga->dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
    svga->dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
}


void
svga_poll(priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    uint32_t x;
    int wx, wy;

    if (!svga->linepos) {
	if (svga->displine == svga->hwcursor_latch.y && svga->hwcursor_latch.ena) {
		svga->hwcursor_on = 64 - svga->hwcursor_latch.yoff;
		svga->hwcursor_oddeven = 0;
	}

	if (svga->displine == (svga->hwcursor_latch.y + 1) && svga->hwcursor_latch.ena &&
			       svga->interlace) {
		svga->hwcursor_on = 64 - (svga->hwcursor_latch.yoff + 1);
		svga->hwcursor_oddeven = 1;
	}

	if (svga->displine == svga->dac_hwcursor_latch.y && svga->dac_hwcursor_latch.ena) {
		svga->dac_hwcursor_on = 64 - svga->dac_hwcursor_latch.yoff;
		svga->dac_hwcursor_oddeven = 0;
	}

	if (svga->displine == (svga->dac_hwcursor_latch.y + 1) && svga->dac_hwcursor_latch.ena &&
			       svga->interlace) {
		svga->dac_hwcursor_on = 64 - (svga->dac_hwcursor_latch.yoff + 1);
		svga->dac_hwcursor_oddeven = 1;
	}
	if (svga->displine == svga->overlay_latch.y && svga->overlay_latch.ena) {
		svga->overlay_on = svga->overlay_latch.ysize - svga->overlay_latch.yoff;
		svga->overlay_oddeven = 0;
	}

	if (svga->displine == svga->overlay_latch.y+1 && svga->overlay_latch.ena && svga->interlace) {
		svga->overlay_on = svga->overlay_latch.ysize - svga->overlay_latch.yoff;
		svga->overlay_oddeven = 1;
	}

	svga->vidtime += svga->dispofftime;
	svga->cgastat |= 1;
	svga->linepos = 1;

	if (svga->dispon) {
		svga->hdisp_on=1;

		svga->ma &= svga->vram_display_mask;
		if (svga->firstline == 2000) {
			svga->firstline = svga->displine;
			video_blit_wait_buffer();
		}

		if (svga->hwcursor_on || svga->dac_hwcursor_on  || svga->overlay_on) {
			svga->changedvram[svga->ma >> 12] = svga->changedvram[(svga->ma >> 12) + 1] =
							    svga->interlace ? 3 : 2;
		}

		if (!svga->override)
			svga->render(svga);

		if (svga->overlay_on) {
			if (!svga->override)
				svga->overlay_draw(svga, svga->displine);
			svga->overlay_on--;
			if (svga->overlay_on && svga->interlace)
				svga->overlay_on--;
		}

		if (svga->dac_hwcursor_on) {
			if (!svga->override && svga->dac_hwcursor_draw)
				svga->dac_hwcursor_draw(svga, svga->displine);
			svga->dac_hwcursor_on--;
			if (svga->dac_hwcursor_on && svga->interlace)
				svga->dac_hwcursor_on--;
		}
		if (svga->hwcursor_on) {
			if (!svga->override && svga->hwcursor_draw)
				svga->hwcursor_draw(svga, svga->displine);
			svga->hwcursor_on--;
			if (svga->hwcursor_on && svga->interlace)
				svga->hwcursor_on--;
		}

		if (svga->lastline < svga->displine) 
			svga->lastline = svga->displine;
	}

	svga->displine++;
	if (svga->interlace) 
		svga->displine++;
	if ((svga->cgastat & 8) && ((svga->displine & 15) == (svga->crtc[0x11] & 15)) && svga->vslines)
		svga->cgastat &= ~8;
	svga->vslines++;
	if (svga->displine > 1500)
		svga->displine = 0;
    } else {
	svga->vidtime += svga->dispontime;

	if (svga->dispon) 
		svga->cgastat &= ~1;
	svga->hdisp_on = 0;

	svga->linepos = 0;
	if ((svga->sc == (svga->crtc[11] & 31))  || (svga->sc == svga->rowcount))
		svga->con = 0;
	if (svga->dispon) {
		if (svga->linedbl && !svga->linecountff) {
			svga->linecountff = 1;
			svga->ma = svga->maback;
		} else if (svga->sc == svga->rowcount) {
			svga->linecountff = 0;
			svga->sc = 0;

			svga->maback += (svga->rowoffset << 3);
			if (svga->interlace)
				svga->maback += (svga->rowoffset << 3);
			svga->maback &= svga->vram_display_mask;
			svga->ma = svga->maback;
		} else {
			svga->linecountff = 0;
			svga->sc++;
			svga->sc &= 31;
			svga->ma = svga->maback;
		}
	}
	svga->vc++;
	svga->vc &= 2047;

	if (svga->vc == svga->split) {
		svga->ma = svga->maback = 0;
		svga->sc = 0;
		if (svga->attrregs[0x10] & 0x20) 
			svga->scrollcache = 0;
	}
	if (svga->vc == svga->dispend) {
		if (svga->vblank_start)
			svga->vblank_start(svga);
		svga->dispon=0;
		if (svga->crtc[10] & 0x20)
			svga->cursoron = 0;
		else
			svga->cursoron = svga->blink & 16;

		if (!(svga->gdcreg[6] & 1) && !(svga->blink & 15)) 
			svga->fullchange = 2;
		svga->blink++;

		for (x = 0; x < ((svga->vram_mask + 1) >> 12); x++) {
			if (svga->changedvram[x]) 
				svga->changedvram[x]--;
		}
		if (svga->fullchange) 
			svga->fullchange--;
	}
	if (svga->vc == svga->vsyncstart) {
		svga->dispon = 0;
		svga->cgastat |= 8;
		x = svga->hdisp;

		if (svga->interlace && !svga->oddeven)
			svga->lastline++;
		if (svga->interlace &&  svga->oddeven)
			svga->firstline--;

		wx = x;
		wy = svga->lastline - svga->firstline;

		if (!svga->override)
			svga_doblit(svga->firstline_draw, svga->lastline_draw + 1, wx, wy, svga);

		svga->firstline = 2000;
		svga->lastline = 0;

		svga->firstline_draw = 2000;
		svga->lastline_draw = 0;

		svga->oddeven ^= 1;

		changeframecount = svga->interlace ? 3 : 2;
		svga->vslines = 0;

		if (svga->interlace && svga->oddeven)
			svga->ma = svga->maback = svga->ma_latch + (svga->rowoffset << 1);
		else
			svga->ma = svga->maback = svga->ma_latch;
		svga->ca = (svga->crtc[0xe] << 8) | svga->crtc[0xf];

		svga->ma <<= 2;
		svga->maback <<= 2;
		svga->ca <<= 2;

		svga->video_res_x = wx;
		svga->video_res_y = wy + 1;
		if (!(svga->gdcreg[6] & 1) && !(svga->attrregs[0x10] & 1)) { /*Text mode*/
			svga->video_res_x /= (svga->seqregs[1] & 1) ? 8 : 9;
			svga->video_res_y /= (svga->crtc[9] & 31) + 1;
			svga->video_bpp = 0;
		} else {
			if (svga->crtc[9] & 0x80)
				svga->video_res_y /= 2;
			if (!(svga->crtc[0x17] & 2))
				svga->video_res_y *= 4;
			else if (!(svga->crtc[0x17] & 1))
				svga->video_res_y *= 2;
			svga->video_res_y /= (svga->crtc[9] & 31) + 1;                                   
			if (svga->lowres)
				svga->video_res_x /= 2;

			svga->video_bpp = svga->bpp;
		}
	}
	if (svga->vc == svga->vtotal) {
		svga->vc = 0;
		svga->sc = svga->crtc[8] & 0x1f;
		svga->dispon = 1;
		svga->displine = (svga->interlace && svga->oddeven) ? 1 : 0;
		svga->scrollcache = svga->attrregs[0x13] & 7;
		svga->linecountff = 0;

		svga->hwcursor_on = 0;
		svga->hwcursor_latch = svga->hwcursor;

		svga->dac_hwcursor_on = 0;
		svga->dac_hwcursor_latch = svga->dac_hwcursor;
		svga->overlay_on = 0;
		svga->overlay_latch = svga->overlay;
	}
	if (svga->sc == (svga->crtc[10] & 31)) 
		svga->con = 1;
    }
}


int
svga_init(svga_t *svga, priv_t priv, int vramsize, 
	  void (*recalctimings_ex)(struct svga_t *svga),
	  uint8_t (*video_in) (uint16_t addr, priv_t),
	  void    (*video_out)(uint16_t addr, uint8_t val, priv_t),
	  void (*hwcursor_draw)(struct svga_t *svga, int displine),
	  void (*overlay_draw)(struct svga_t *svga, int displine))
{
    int c, d, e;

    svga->p = priv;

    for (c = 0; c < 256; c++) {
	e = c;
	for (d = 0; d < 8; d++) {
		svga_rotate[d][c] = e;
		e = (e >> 1) | ((e & 1) ? 0x80 : 0);
	}
    }
    svga->readmode = 0;

    svga->attrregs[0x11] = 0;
    svga->overscan_color = 0x000000;

    overscan_x = 16;
    overscan_y = 32;

    svga->crtc[0] = 63;
    svga->crtc[6] = 255;
    svga->dispontime = svga->dispofftime = 1000 * (1 << TIMER_SHIFT);
    svga->bpp = 8;
    svga->vram = (uint8_t *)mem_alloc(vramsize);
    svga->vram_max = vramsize;
    svga->vram_display_mask = svga->vram_mask = vramsize - 1;
    svga->decode_mask = 0x7fffff;
    svga->changedvram = (uint8_t *)mem_alloc(vramsize >> 12);
    svga->recalctimings_ex = recalctimings_ex;
    svga->video_in  = video_in;
    svga->video_out = video_out;
    svga->hwcursor_draw = hwcursor_draw;
    svga->overlay_draw = overlay_draw;

    svga->hwcursor.xsize = svga->hwcursor.ysize = 32;
    svga->hwcursor.yoff = 32;

    svga->dac_hwcursor.xsize = svga->dac_hwcursor.ysize = 32;
    svga->dac_hwcursor.yoff = 32;
    mem_map_add(&svga->mapping, 0xa0000, 0x20000,
		svga_read, svga_readw, svga_readl,
		svga_write, svga_writew, svga_writel,
		NULL, MEM_MAPPING_EXTERNAL, svga);

    timer_add(svga_poll, svga, &svga->vidtime, TIMER_ALWAYS_ENABLED);

    svga_pri = svga;

    svga->ramdac_type = RAMDAC_6BIT;

    svga->map8 = svga->pallook;
    return 0;
}


void
svga_close(svga_t *svga)
{
    free(svga->changedvram);
    free(svga->vram);

    svga_pri = NULL;
}

static uint32_t
svga_decode_addr(svga_t *svga, uint32_t addr, int write)
{
    int memory_map_mode = (svga->gdcreg[6] >> 2) & 3;

    addr &= 0x1ffff;

    switch (memory_map_mode) {
	case 0:
		break;
	case 1:
		if (addr >= 0x10000)
			return 0xffffffff;
		break;
	case 2:
		addr -= 0x10000;
		if (addr >= 0x8000)
			return 0xffffffff;
		break;
	default:
	case 3:
		addr -= 0x18000;
		if (addr >= 0x8000)
			return 0xffffffff;
		break;
    }

    if (memory_map_mode <= 1) {
	if (svga->adv_flags & FLAG_EXTRA_BANKS)
		addr = (addr & 0x17fff) + svga->extra_banks[(addr >> 15) & 1];
	else {
		if (write)
			addr += svga->write_bank;
		else
			addr += svga->read_bank;
	}
    }

    return addr;
}

void
svga_write_common(uint32_t addr, uint8_t val, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    int writemask2 = svga->writemask, reset_wm = 0;
    latch_t vall;
    uint8_t wm = svga->writemask;
    uint8_t count, i;
    
    if (svga->adv_flags & FLAG_ADDR_BY8)
	writemask2 = svga->seqregs[2];
    
    cycles -= video_timing_write_b;


    if (! linear) {
	addr = svga_decode_addr(svga, addr, 1);
	
	if (addr == 0xffffffff)
		return;
    }

    if (!(svga->gdcreg[6] & 1))
	svga->fullchange = 2;

    if ((svga->adv_flags & FLAG_ADDR_BY8) && (svga->writemode < 4))
	addr <<= 3;
    else if ((svga->chain4 || svga->fb_only) && (svga->writemode < 4)) {
	writemask2 = 1 << (addr & 3);
	addr &= ~3;
    } else if (svga->chain2_write) {
	writemask2 &= ~0xa;
	if (addr & 1)
		writemask2 <<= 1;
	addr &= ~1;
	addr <<= 2;
    } else
	addr <<= 2;
	
    addr &= svga->decode_mask;

    if (addr >= svga->vram_max)
	return;

    addr &= svga->vram_mask;

    svga->changedvram[addr >> 12] = changeframecount;

    /* standard VGA latched access */
    count =4;
    if (svga->adv_flags & FLAG_LATCH8)
	count = 8;

    switch (svga->writemode) {
	case 0:
		if (svga->gdcreg[3] & 7)
			val = svga_rotate[svga->gdcreg[3] & 7][val];
		if ((svga->gdcreg[8] == 0xff) && !(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
			for (i = 0; i < count; i++) {
				if (writemask2 & (1 <<i))
					svga->vram[addr | i] = val;
			}
			return;
		} else {
			for (i = 0; i < count; i++) {
				if (svga->gdcreg[1] & (1 << i))
					vall.b[i] = !!(svga->gdcreg[0] & (1 << i)) * 0xff;
				else
					vall.b[i] = val;
			}
		}

		break;
	case 1:
		for (i = 0; i < count; i++) {
			if (writemask2 & (1 << i))
				svga->vram[addr | i] = svga->latch.b[i];
		}
		return;
	case 2:
	for (i = 0; i < count; i++)
			vall.b[i] = !!(val & (1 << i)) * 0xff;

		if (!(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
			for (i = 0; i < count; i++) {
				if (writemask2 & (1 << i))
					svga->vram[addr | i] = (vall.b[i] & svga->gdcreg[8]) | (svga->latch.b[i] & ~svga->gdcreg[8]);
			}
			return;
		}
		break;
	case 3:
		if (svga->gdcreg[3] & 7) 
                        val = svga_rotate[svga->gdcreg[3] & 7][val];
                wm = svga->gdcreg[8];
                svga->gdcreg[8] &= val;

		for (i = 0; i < count; i++)
			vall.b[i] = !!(svga->gdcreg[0] & (1 << i)) * 0xff;

                reset_wm = 1;
                break;
	default:
		if (svga->ven_write)
			svga->ven_write(svga, val, addr);
		return;
    }

    switch (svga->gdcreg[3] & 0x18) {
	case 0x00:	/* Set */
		for (i = 0; i < count; i++) {
			if (writemask2 & (1 << i))
				svga->vram[addr | i] = (vall.b[i] & svga->gdcreg[8]) | (svga->latch.b[i] & ~svga->gdcreg[8]);
		}    
		break;
	case 0x08:	/* AND */
		for (i = 0; i < count; i++) {
			if (writemask2 & (1 << i))
				svga->vram[addr | i] = (vall.b[i] | ~svga->gdcreg[8]) & svga->latch.b[i];
		}
		break;
	case 0x10:	/* OR */
		for (i = 0; i < count; i++) {
			if (writemask2 & (1 << i))
				svga->vram[addr | i] = (vall.b[i] & svga->gdcreg[8]) | svga->latch.b[i];
		}
		break;
	case 0x18:	/* XOR */
		for (i = 0; i < count; i++) {
			if (writemask2 & (1 << i))
				svga->vram[addr | i] = (vall.b[i] & svga->gdcreg[8]) ^ svga->latch.b[i];
		}
		break;
    }

   if (reset_wm)
	svga->gdcreg[8] = wm;
}


uint8_t
svga_read_common(uint32_t addr, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    uint32_t latch_addr = 0;
    int readplane = svga->readplane;
    uint8_t count, i;
    uint8_t plane, pixel;
    uint8_t temp, ret;

    if (svga->adv_flags & FLAG_ADDR_BY8)
	readplane = svga->gdcreg[4] & 7;

    cycles -= video_timing_read_b;

    
    if (! linear) {
	addr = svga_decode_addr(svga, addr, 0);

    if (addr == 0xffffffff)
		return 0xff;
    }

    count = 2;
    if (svga->adv_flags & FLAG_LATCH8)
	count = 3;

    latch_addr = (addr << count) & svga->decode_mask;
    count = (1 << count);

    if (svga->adv_flags & FLAG_ADDR_BY8)
	addr <<= 3;
    else if (svga->chain4 || svga->fb_only) {
	addr &= svga->decode_mask;
	if (addr >= svga->vram_max)
		return 0xff;
	return svga->vram[addr & svga->vram_mask];
    } else if (svga->chain2_read) {
	readplane = (readplane & 2) | (addr & 1);
	addr &= ~1;
	addr <<= 2;
	addr |= readplane;
	addr &= svga->decode_mask;
	if (addr >= svga->vram_max)
		return 0xff;
	addr &= svga->vram_mask;
	return svga->vram[addr];
    } else
	addr <<= 2;

    addr &= svga->decode_mask;

    /* standard VGA latched access */
    if (linear) {
	if (addr >= svga->vram_max)
		return 0xff;

	addr &= svga->vram_mask;

	for (i = 0; i < count; i++)
		svga->latch.b[i] = svga->vram[addr | i];
    } else {
	if (latch_addr >= svga->vram_max) {
		for (i = 0; i < count; i++)
			svga->latch.b[i] = 0xff;
	} else {
		latch_addr &= svga->vram_mask;
		for (i = 0; i < count; i++)
			svga->latch.b[i] = svga->vram[latch_addr | i];
	}

    	if (addr >= svga->vram_max)
		return 0xff;

	    addr &= svga->vram_mask;
    }

    if (svga->readmode) {
	temp = 0xff;

	for (pixel = 0; pixel < 8; pixel++) {
		for (plane = 0; plane < count; plane++) {
			if (svga->colournocare & (1 << plane)) {
				/* If we care about a plane, and the pixel has a mismatch on it, clear its bit. */
				if (((svga->latch.b[plane] >> pixel) & 1) != ((svga->colourcompare >> plane) & 1))
					temp &= ~(1 << pixel);
			}
		}
	}

	ret = temp;
    } else
	ret = svga->vram[addr | readplane];

    return ret;
}


void
svga_write(uint32_t addr, uint8_t val, priv_t priv)
{
    svga_write_common(addr, val, 0, priv);
}


void
svga_write_linear(uint32_t addr, uint8_t val, priv_t priv)
{
    svga_write_common(addr, val, 1, priv);
}


uint8_t
svga_read(uint32_t addr, priv_t priv)
{
    return svga_read_common(addr, 0, priv);
}


uint8_t
svga_read_linear(uint32_t addr, priv_t priv)
{
    return svga_read_common(addr, 1, priv);
}


void
svga_doblit(int y1, int y2, int wx, int wy, svga_t *svga)
{
    int y_add = (enable_overscan) ? overscan_y : 0;
    int x_add = (enable_overscan) ? 16 : 0;
    int i, j;

    svga->frames++;

    if ((xsize > 2032) || (ysize > 2032)) {
	x_add = 0;
	y_add = 0;
	suppress_overscan = 1;
    } else
	suppress_overscan = 0;

    if (y1 > y2) {
	video_blit_start(0, 32, 0, 0, 0, xsize + x_add, ysize + y_add);
	return;
    }

    if ((wx != xsize) || ((wy + 1) != ysize) || video_force_resize_get()) {
	/* Screen res has changed.. fix up, and let them know. */
	xsize = wx;
	ysize = wy + 1;
	if (xsize < 64)
		xsize = 640;
	if (ysize < 32)
		ysize = 200;

	set_screen_size(xsize+x_add,ysize+y_add);

	if (video_force_resize_get())
		video_force_resize_set(0);
    }

    if (enable_overscan && !suppress_overscan) {
	if ((wx >= 160) && ((wy + 1) >= 120)) {
		/* Draw (overscan_size - scroll size) lines of overscan on top. */
		for (i  = 0; i < (y_add >> 1); i++) {
			for (j = 0; j < (xsize + x_add); j++)
				screen->line[i & 0x7ff][32 + j].val = svga->overscan_color;
		}

		/* Draw (overscan_size + scroll size) lines of overscan on the bottom. */
		for (i  = 0; i < (y_add >> 1); i++) {
			for (j = 0; j < (xsize + x_add); j++)
				screen->line[(ysize + (y_add >> 1) + i) & 0x7ff][32 + j].val = svga->overscan_color;
		}

		for (i = (y_add >> 1); i < (ysize + (y_add >> 1)); i ++) {
			for (j = 0; j < 8; j++) {
				screen->line[i & 0x7ff][32 + j].val = svga->overscan_color;
				screen->line[i & 0x7ff][32 + xsize + (x_add >> 1) + j].val = svga->overscan_color;
			}
		}
	}
    }

    video_blit_start(0, 32, 0, y1, y2 + y_add, xsize + x_add, ysize + y_add);
}


void
svga_writeb_linear(uint32_t addr, uint8_t val, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast) {
	svga_write_linear(addr, val, priv);
	return;
    }

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return;
    addr &= svga->vram_mask;
    svga->changedvram[addr >> 12] = changeframecount;
    *(uint8_t *)&svga->vram[addr] = val;
}


void
svga_writew_common(uint32_t addr, uint16_t val, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast) {
	svga_write_common(addr, val & 0xff, linear, priv);
	svga_write_common(addr + 1, val >> 8, linear, priv);
	return;
    }

    cycles -= video_timing_write_w;


    if (!linear) {
	addr = svga_decode_addr(svga, addr, 1);
	if (addr == 0xffffffff)
		return;
    }

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return;
    addr &= svga->vram_mask;
    svga->changedvram[addr >> 12] = changeframecount;
    *(uint16_t *)&svga->vram[addr] = val;
}


void
svga_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    svga_writew_common(addr, val, 0, priv);
}


void
svga_writew_linear(uint32_t addr, uint16_t val, priv_t priv)
{
    svga_writew_common(addr, val, 1, priv);
}


void
svga_writel_common(uint32_t addr, uint32_t val, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast) {
	svga_write_common(addr, val, linear, priv);
	svga_write_common(addr + 1, val >> 8, linear, priv);
	svga_write_common(addr + 2, val >> 16, linear, priv);
	svga_write_common(addr + 3, val >> 24, linear, priv);
	return;
    }

    cycles -= video_timing_write_l;


     if (!linear) {
	addr = svga_decode_addr(svga, addr, 1);

	if (addr == 0xffffffff)
		return;
    }
    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return;
    addr &= svga->vram_mask;

    svga->changedvram[addr >> 12] = changeframecount;
    *(uint32_t *)&svga->vram[addr] = val;
}


void
svga_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    svga_writel_common(addr, val, 0, priv);
}


void
svga_writel_linear(uint32_t addr, uint32_t val, priv_t priv)
{
    svga_writel_common(addr, val, 1, priv);
}


uint8_t
svga_readb_linear(uint32_t addr, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast)
	return svga_read_linear(addr, priv);

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return 0xff;

    return *(uint8_t *)&svga->vram[addr & svga->vram_mask];
}


uint16_t
svga_readw_common(uint32_t addr, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast)
	return svga_read_common(addr, linear, priv) | (svga_read_common(addr + 1, linear, priv) << 8);

    cycles -= video_timing_read_w;


    if (!linear) {
	addr = svga_decode_addr(svga, addr, 0);

	if (addr == 0xffffffff)
		return 0xffff;
    }
    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return 0xffff;

    return *(uint16_t *)&svga->vram[addr & svga->vram_mask];
}


uint16_t
svga_readw(uint32_t addr, priv_t priv)
{
    return svga_readw_common(addr, 0, priv);
}


uint16_t
svga_readw_linear(uint32_t addr, priv_t priv)
{
    return svga_readw_common(addr, 1, priv);
}


uint32_t
svga_readl_common(uint32_t addr, uint8_t linear, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;

    if (!svga->fast) {
	return svga_read_common(addr, linear, priv) | (svga_read_common(addr + 1, linear, priv) << 8) |
	       (svga_read_common(addr + 2, linear, priv) << 16) | (svga_read_common(addr + 3, linear, priv) << 24);
    }

    cycles -= video_timing_read_l;

    if (!linear) {
	addr = svga_decode_addr(svga, addr, 0);

	if (addr == 0xffffffff)
		return 0xffffffff;
    }
    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
	return 0xffffffff;

    return *(uint32_t *)&svga->vram[addr & svga->vram_mask];
}


uint32_t
svga_readl(uint32_t addr, priv_t priv)
{
    return svga_readl_common(addr, 0, priv);
}


uint32_t
svga_readl_linear(uint32_t addr, priv_t priv)
{
    return svga_readl_common(addr, 1, priv);
}
