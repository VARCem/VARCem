/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		SVGA renderers.
 *
 * Version:	@(#)vid_svga_render.c	1.0.20	2021/10/21
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_svga_render_remap.h"

void 
svga_render_null(svga_t *svga)
{
        if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
}

void
svga_render_blank(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int x, xx;

    if (svga->firstline_draw == 2000) 
	svga->firstline_draw = svga->displine;
    svga->lastline_draw = svga->displine;
	
    for (x = 0; x < svga->hdisp; x++) switch (svga->seqregs[1] & 9) {
	case 0:
		for (xx = 0; xx < 9; xx++)
			screen->line[svga->displine + y_add][(x * 9) + xx + 32 + x_add].val = 0;
		break;

	case 1:
		for (xx = 0; xx < 8; xx++)
			screen->line[svga->displine + y_add][(x * 8) + xx + 32 + x_add].val = 0;
		break;

	case 8:
		for (xx = 0; xx < 18; xx++)
			screen->line[svga->displine + y_add][(x * 18) + xx + 32 + x_add].val = 0;
		break;

	case 9:
		for (xx = 0; xx < 16; xx++)
			screen->line[svga->displine + y_add][(x * 16) + xx + 32 + x_add].val = 0;
		break;
    }
}


void
svga_render_text_40(svga_t *svga)
{     
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int xinc = (svga->seqregs[1] & 1) ? 16 : 18;
    uint8_t chr, attr, dat;
    uint32_t charaddr;
    int bg, fg, x, xx;
    int drawcursor;
    pel_t *p;

    if (svga->firstline_draw == 2000) 
	svga->firstline_draw = svga->displine;
    svga->lastline_draw = svga->displine;
	
    if (svga->fullchange) {
	p = &screen->line[svga->displine + y_add][32 + x_add];

	for (x = 0; x < svga->hdisp; x += xinc) {
		uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;
		drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
		chr  = svga->vram[addr];
		attr = svga->vram[addr+1];
		if (attr & 8)
			charaddr = svga->charsetb + (chr * 128);
		else
			charaddr = svga->charseta + (chr * 128);

		if (drawcursor) { 
			bg = svga->pallook[svga->egapal[attr & 15]]; 
			fg = svga->pallook[svga->egapal[attr >> 4]]; 
		} else {
			fg = svga->pallook[svga->egapal[attr & 15]];
			bg = svga->pallook[svga->egapal[attr >> 4]];
			if (attr & 0x80 && svga->attrregs[0x10] & 8) {
				bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
				if (svga->blink & 16) 
					fg = bg;
			}
		}

		dat = svga->vram[charaddr + (svga->sc << 2)];
		if (svga->seqregs[1] & 1) { 
			for (xx = 0; xx < 16; xx += 2) 
				p[xx].val = p[xx + 1].val = (dat & (0x80 >> (xx >> 1))) ? fg : bg;
		} else {
			for (xx = 0; xx < 16; xx += 2)
				p[xx].val = p[xx + 1].val = (dat & (0x80 >> (xx >> 1))) ? fg : bg;
			if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4))
				p[16].val = p[17].val = bg;
			else		  
				p[16].val = p[17].val = (dat & 1) ? fg : bg;
		}

		svga->ma += 4; 
		p += xinc;
	}

	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_text_80(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int xinc = (svga->seqregs[1] & 1) ? 8 : 9;
    uint8_t chr, attr, dat;
    uint32_t charaddr;
    int bg, fg, x, xx;
    int drawcursor;
    pel_t *p;

    if (svga->firstline_draw == 2000) 
	svga->firstline_draw = svga->displine;
    svga->lastline_draw = svga->displine;
	
    if (svga->fullchange) {
	p = &screen->line[svga->displine + y_add][32 + x_add];

	for (x = 0; x < svga->hdisp; x += xinc) {
		uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;
		drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
		chr  = svga->vram[addr];
		attr = svga->vram[addr+1];

		if (attr & 8)
			charaddr = svga->charsetb + (chr * 128);
		else
			charaddr = svga->charseta + (chr * 128);

		if (drawcursor) { 
			bg = svga->pallook[svga->egapal[attr & 15]]; 
			fg = svga->pallook[svga->egapal[attr >> 4]]; 
		} else {
			fg = svga->pallook[svga->egapal[attr & 15]];
			bg = svga->pallook[svga->egapal[attr >> 4]];

			if (attr & 0x80 && svga->attrregs[0x10] & 8) {
				bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
				if (svga->blink & 16) 
					fg = bg;
			}
		}

		dat = svga->vram[charaddr + (svga->sc << 2)];
		if (svga->seqregs[1] & 1) { 
			for (xx = 0; xx < 8; xx++) 
				p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
		} else {
			for (xx = 0; xx < 8; xx++) 
				p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
			if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4)) 
				p[8].val = bg;
			else		  
				p[8].val = (dat & 1) ? fg : bg;
		}

		svga->ma += 4; 
		p += xinc;
	}

	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_text_80_ksc5601(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int xinc = (svga->seqregs[1] & 1) ? 8 : 9;
    uint8_t chr, attr, dat, nextchr;
    uint32_t charaddr;
    int bg, fg, x, xx;
    int drawcursor;
    pel_t *p;

    if (svga->firstline_draw == 2000) 
	svga->firstline_draw = svga->displine;
    svga->lastline_draw = svga->displine;

    if (svga->fullchange) {
	p = &screen->line[svga->displine + y_add][32 + x_add];

	for (x = 0; x < svga->hdisp; x += xinc) {
		uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;
		drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
		chr  = svga->vram[addr];
		nextchr = svga->vram[addr + 8];
		if (svga->crtc[0x17] & 0x80)
			attr = svga->vram[addr + 1];

		if (drawcursor) { 
			bg = svga->pallook[svga->egapal[attr & 15]]; 
			fg = svga->pallook[svga->egapal[attr >> 4]]; 
		} else {
			fg = svga->pallook[svga->egapal[attr & 15]];
			bg = svga->pallook[svga->egapal[attr >> 4]];

			if (attr & 0x80 && svga->attrregs[0x10] & 8) {
				bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
				if (svga->blink & 16) 
					fg = bg;
			}
		}

		if(x + xinc < svga->hdisp && (chr & (nextchr | svga->ksc5601_sbyte_mask) & 0x80)) {
			if ((chr == 0xc9 || chr == 0xfe) && (nextchr > 0xa0 && nextchr < 0xff))
				dat = fontdatk_user[(chr == 0xfe ? 96 : 0) + (nextchr & 0x7F) - 0x20].chr[svga->sc];
			else if (nextchr & 0x80)
				dat = fontdatk[((chr & 0x7F) << 7) | (nextchr & 0x7F)].chr[svga->sc];
			else
				dat = 0xff;
		} else {
			if (attr & 8)
				charaddr = svga->charsetb + (chr * 128);
			else
				charaddr = svga->charseta + (chr * 128);

			dat = svga->vram[charaddr + (svga->sc << 2)];
		}

		if (svga->seqregs[1] & 1) { 
			for (xx = 0; xx < 8; xx++) 
				p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
		} else {
			for (xx = 0; xx < 8; xx++) 
				p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
			if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4)) 
				p[8].val = bg;
			else		  
				p[8].val = (dat & 1) ? fg : bg;
		}

		svga->ma += 4; 
		p += xinc;

		if (x + xinc < svga->hdisp && (chr & (nextchr | svga->ksc5601_sbyte_mask) & 0x80)) {
			attr = svga->vram[((svga->ma << 1) + 1) & svga->vram_display_mask];

			if (drawcursor) { 
				bg = svga->pallook[svga->egapal[attr & 15]]; 
				fg = svga->pallook[svga->egapal[attr >> 4]]; 
			} else {
				fg = svga->pallook[svga->egapal[attr & 15]];
				bg = svga->pallook[svga->egapal[attr >> 4]];

				if (attr & 0x80 && svga->attrregs[0x10] & 8) {
					bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
					if (svga->blink & 16) 
						fg = bg;
				}
			}

			if ((chr == 0xc9 || chr == 0xfe) && (nextchr > 0xa0 && nextchr < 0xff))
				dat = fontdatk_user[(chr == 0xfe ? 96 : 0) + (nextchr & 0x7F) - 0x20].chr[svga->sc + 16];
			else if (nextchr & 0x80)
				dat = fontdatk[((chr & 0x7F) << 7) | (nextchr & 0x7F)].chr[svga->sc + 16];
			else
				dat = 0xff;

			if (svga->seqregs[1] & 1) { 
				for (xx = 0; xx < 8; xx++) 
					p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
			} else {
				for (xx = 0; xx < 8; xx++) 
					p[xx].val = (dat & (0x80 >> xx)) ? fg : bg;
				if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4)) 
					p[8].val = bg;
				else		  
					p[8].val = (dat & 1) ? fg : bg;
			}

			svga->ma += 4; 
			p += xinc;
			x += xinc;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_2bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int x;
    int offset;
    uint8_t dat[2];
    pel_t *p;
    uint32_t changed_addr;

    changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = ((8 - svga->scrollcache) << 1) + 16;
	p = &screen->line[svga->displine + y_add][offset + x_add];
		
	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;
		       
	for (x = 0; x <= svga->hdisp; x += 16) {
		uint32_t addr = svga->remap_func(svga, svga->ma);
		dat[0] = svga->vram[addr];
		dat[1] = svga->vram[addr + 1];
		svga->ma += 4; 
		svga->ma &= svga->vram_display_mask;

		p[0].val  = p[1].val  = svga->pallook[svga->egapal[(dat[0] >> 6) & 3]];
		p[2].val  = p[3].val  = svga->pallook[svga->egapal[(dat[0] >> 4) & 3]];
		p[4].val  = p[5].val  = svga->pallook[svga->egapal[(dat[0] >> 2) & 3]];
		p[6].val  = p[7].val  = svga->pallook[svga->egapal[dat[0] & 3]];
		p[8].val  = p[9].val  = svga->pallook[svga->egapal[(dat[1] >> 6) & 3]];
		p[10].val = p[11].val = svga->pallook[svga->egapal[(dat[1] >> 4) & 3]];
		p[12].val = p[13].val = svga->pallook[svga->egapal[(dat[1] >> 2) & 3]];
		p[14].val = p[15].val = svga->pallook[svga->egapal[dat[1] & 3]];

		p += 16;
	}
    }
}


void
svga_render_2bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint8_t dat[2];
    pel_t *p;
    uint32_t changed_addr;

    changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - svga->scrollcache) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	for (x = 0; x <= svga->hdisp; x += 8) {
		uint32_t addr = svga->remap_func(svga, svga->ma);
		dat[0] = svga->vram[addr];
		dat[1] = svga->vram[addr + 1];
		svga->ma += 4; 
		svga->ma &= svga->vram_display_mask;

		p++->val = svga->pallook[svga->egapal[(dat[0] >> 6) & 3]];
		p++->val = svga->pallook[svga->egapal[(dat[0] >> 4) & 3]];
		p++->val = svga->pallook[svga->egapal[(dat[0] >> 2) & 3]];
		p++->val = svga->pallook[svga->egapal[dat[0] & 3]];
		p++->val = svga->pallook[svga->egapal[(dat[1] >> 6) & 3]];
		p++->val = svga->pallook[svga->egapal[(dat[1] >> 4) & 3]];
		p++->val = svga->pallook[svga->egapal[(dat[1] >> 2) & 3]];
		p++->val = svga->pallook[svga->egapal[dat[1] & 3]];
	}
    }
}


void
svga_render_4bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint8_t edat[4], dat;
    uint32_t *eptr = (uint32_t *)edat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = ((8 - svga->scrollcache) << 1) + 16;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	for (x = 0; x <= svga->hdisp; x += 16) {
		uint32_t addr = svga->remap_func(svga, svga->ma);
		*eptr = *(uint32_t *)(&svga->vram[addr]);			

		svga->ma += 4; 
		svga->ma &= svga->vram_display_mask;

		dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
		p[0].val  = p[1].val  = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p[2].val  = p[3].val  = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
		p[4].val  = p[5].val  = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p[6].val  = p[7].val  = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
		p[8].val  = p[9].val  = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p[10].val = p[11].val = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
		p[12].val = p[13].val = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p[14].val = p[15].val = svga->pallook[svga->egapal[dat & svga->plane_mask]];

		p += 16;
	}
    }
}


void
svga_render_4bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint8_t edat[4], dat;
    uint32_t *eptr = (uint32_t *)edat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - svga->scrollcache) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	for (x = 0; x <= svga->hdisp; x += 8) {
		uint32_t addr = svga->remap_func(svga, svga->ma);
		*eptr = *(uint32_t *)(&svga->vram[addr]);
		svga->ma += 4;
		svga->ma &= svga->vram_display_mask;

		dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
		p++->val = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p++->val = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
		p++->val = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p++->val = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
		p++->val = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p++->val = svga->pallook[svga->egapal[dat & svga->plane_mask]];
		dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
		p++->val = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
		p++->val = svga->pallook[svga->egapal[dat & svga->plane_mask]];
	}
    }
}


void
svga_render_8bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);

			p[0].val = p[1].val = svga->pallook[dat & 0xff];
			p[2].val = p[3].val = svga->pallook[(dat >> 8) & 0xff];
			p[4].val = p[5].val = svga->pallook[(dat >> 16) & 0xff];
			p[6].val = p[7].val = svga->pallook[(dat >> 24) & 0xff];
		
			svga->ma += 4;
			p += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 8) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p[0].val = p[1].val = svga->pallook[dat & 0xff];
			p[2].val = p[3].val = svga->pallook[(dat >> 8) & 0xff];
			p[4].val = p[5].val = svga->pallook[(dat >> 16) & 0xff];
			p[6].val = p[7].val = svga->pallook[(dat >> 24) & 0xff];
		
			svga->ma += 4;
			p += 8;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_8bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000)
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);
			p++->val = svga->pallook[dat & 0xff];
			p++->val = svga->pallook[(dat >> 8) & 0xff];
			p++->val = svga->pallook[(dat >> 16) & 0xff];
			p++->val = svga->pallook[(dat >> 24) & 0xff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
			p++->val = svga->pallook[dat & 0xff];
			p++->val = svga->pallook[(dat >> 8) & 0xff];
			p++->val = svga->pallook[(dat >> 16) & 0xff];
			p++->val = svga->pallook[(dat >> 24) & 0xff];
		
			svga->ma += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 4) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			svga->ma += 4;

			p++->val = svga->pallook[dat & 0xff];
			p++->val = svga->pallook[(dat >> 8) & 0xff];
			p++->val = svga->pallook[(dat >> 16) & 0xff];
			p++->val = svga->pallook[(dat >> 24) & 0xff];
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void svga_render_8bpp_gs_lowres(svga_t *svga) /* FIXME */
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);

			p[0].val = p[1].val = video_8togs[dat & 0xff];
			p[2].val = p[3].val = video_8togs[(dat >> 8) & 0xff];
			p[4].val = p[5].val = video_8togs[(dat >> 16) & 0xff];
			p[6].val = p[7].val = video_8togs[(dat >> 24) & 0xff];

			svga->ma += 4;
			p += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 8) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p[0].val = p[1].val = video_8togs[dat & 0xff];
			p[2].val = p[3].val = video_8togs[(dat >> 8) & 0xff];
			p[4].val = p[5].val = video_8togs[(dat >> 16) & 0xff];
			p[6].val = p[7].val = video_8togs[(dat >> 24) & 0xff];

			svga->ma += 4;
			p += 8;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void svga_render_8bpp_gs_highres(svga_t *svga) /* FIXME */
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000)
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {	
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);
			p[0].val = video_8togs[dat & 0xff];
			p[1].val = video_8togs[(dat >> 8) & 0xff];
			p[2].val = video_8togs[(dat >> 16) & 0xff];
			p[3].val = video_8togs[(dat >> 24) & 0xff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
			p[4].val = video_8togs[dat & 0xff];
			p[5].val = video_8togs[(dat >> 8) & 0xff];
			p[6].val = video_8togs[(dat >> 16) & 0xff];
			p[7].val = video_8togs[(dat >> 24) & 0xff];

			svga->ma += 8;
			p += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 4) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			svga->ma += 4;

			p++->val = video_8togs[dat & 0xff];
			p++->val = video_8togs[(dat >> 8) & 0xff];
			p++->val = video_8togs[(dat >> 16) & 0xff];
			p++->val = video_8togs[(dat >> 24) & 0xff];
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void svga_render_8bpp_rgb_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

		if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);

			p[0].val = p[1].val = video_8to32[dat & 0xff];
			p[2].val = p[3].val = video_8to32[(dat >> 8) & 0xff];
			p[4].val = p[5].val = video_8to32[(dat >> 16) & 0xff];
			p[6].val = p[7].val = video_8to32[(dat >> 24) & 0xff];

			svga->ma += 4;
			p += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 8) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p[0].val = p[1].val = video_8to32[dat & 0xff];
			p[2].val = p[3].val = video_8to32[(dat >> 8) & 0xff];
			p[4].val = p[5].val = video_8to32[(dat >> 16) & 0xff];
			p[6].val = p[7].val = video_8to32[(dat >> 24) & 0xff];

			svga->ma += 4;
			p += 8;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void svga_render_8bpp_rgb_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {	
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);
			p[0].val = video_8to32[dat & 0xff];
			p[1].val = video_8to32[(dat >> 8) & 0xff];
			p[2].val = video_8to32[(dat >> 16) & 0xff];
			p[3].val = video_8to32[(dat >> 24) & 0xff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
			p[4].val = video_8to32[dat & 0xff];
			p[5].val = video_8to32[(dat >> 8) & 0xff];
			p[6].val = video_8to32[(dat >> 16) & 0xff];
			p[7].val = video_8to32[(dat >> 24) & 0xff];

			svga->ma += 8;
			p += 8;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 4) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			svga->ma += 4;

			p++->val = video_8to32[dat & 0xff];
			p++->val = video_8to32[(dat >> 8) & 0xff];
			p++->val = video_8to32[(dat >> 16) & 0xff];
			p++->val = video_8to32[(dat >> 24) & 0xff];
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}

void
svga_render_15bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];
	
	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;
 
	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 4) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);

			p[x].val     = video_15to32[dat & 0xffff];
			p[x + 1].val = video_15to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);

			p[x + 2].val = video_15to32[dat & 0xffff];
			p[x + 3].val = video_15to32[dat >> 16];
		}

		svga->ma += x << 1; 
	} else {
		for (x = 0; x <= svga->hdisp; x += 2) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p[x].val     = video_15to32[dat & 0xffff];
			p[x + 1].val = video_15to32[dat >> 16];

			svga->ma += 4;
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_15bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat, addr;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
			p++->val = video_15to32[dat & 0xffff];
			p++->val = video_15to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);
			p++->val = video_15to32[dat & 0xffff];
			p++->val = video_15to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 8) & svga->vram_display_mask]);
			p++->val = video_15to32[dat & 0xffff];
			p++->val = video_15to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 12) & svga->vram_display_mask]);
			p++->val = video_15to32[dat & 0xffff];
			p++->val = video_15to32[dat >> 16];
		}

		svga->ma += x << 1;
	} else {
		for (x = 0; x <= svga->hdisp; x += 2) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p++->val = video_15to32[dat & 0xffff];
			p++->val = video_15to32[dat >> 16];

			svga->ma += 4;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void 
svga_render_mixed_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 4) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];

			dat >>= 16;
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];

			dat >>= 16;
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];
		}
		svga->ma += x << 1;
	} else {
		for (x = 0; x <= svga->hdisp; x += 2) {
			uint32_t addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];

			dat >>= 16;
			p++->val = (dat & 0x00008000) ? svga->pallook[dat & 0xff] : video_15to32[dat & 0x7fff];

			svga->ma += 4;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}

void
svga_render_mixed_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
			if (dat & 0x8000)
				p[x].val = svga->pallook[dat & 0xff];
			else
				p[x].val     = video_15to32[dat & 0x7fff];

			if ((dat >> 16) & 0x8000)
				p[x + 1].val = svga->pallook[(dat >> 16) & 0xff];
			else
				p[x + 1].val = video_15to32[(dat >> 16) & 0x7fff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);

	    		if (dat & 0x8000)
				p[x + 2].val = svga->pallook[dat & 0xff];
			else
				p[x + 2].val     = video_15to32[dat & 0x7fff];

			if ((dat >> 16) & 0x8000)
				p[x + 3].val = svga->pallook[(dat >> 16) & 0xff];
			else
				p[x + 3].val = video_15to32[(dat >> 16) & 0x7fff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 8) & svga->vram_display_mask]);
			if (dat & 0x8000)
				p[x + 4].val = svga->pallook[dat & 0xff];
			else
				p[x + 4].val     = video_15to32[dat & 0x7fff];

			if ((dat >> 16) & 0x8000)
				p[x + 5].val = svga->pallook[(dat >> 16) & 0xff];
			else
				p[x + 5].val = video_15to32[(dat >> 16) & 0x7fff];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 12) & svga->vram_display_mask]);
			if (dat & 0x8000)
				p[x + 6].val = svga->pallook[dat & 0xff];
			else
				p[x + 6].val     = video_15to32[dat & 0x7fff];

			if ((dat >> 16) & 0x8000)
				p[x + 7].val = svga->pallook[(dat >> 16) & 0xff];
			else
				p[x + 7].val = video_15to32[(dat >> 16) & 0x7fff];
		}
		svga->ma += x << 1; 
	} else {
		for (x = 0; x <= svga->hdisp; x += 2) {
			uint32_t addr = svga->remap_func(svga, svga->ma);

			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			if (dat & 0x8000)
				p[x].val = svga->pallook[dat & 0xff];
			else
				p[x].val = video_15to32[dat & 0x7fff];

			if ((dat >> 16) & 0x8000)
				p[x + 1].val = svga->pallook[(dat >> 16) & 0xff];
			else
				p[x + 1].val = video_15to32[(dat >> 16) & 0x7fff];

			svga->ma += 4;
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_16bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat, addr;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required){
		for (x = 0; x <= svga->hdisp; x += 4) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);

			p++->val     = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);

			p++->val = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];
		}

		svga->ma += x << 1;
	} else {
		for (x = 0; x <= svga->hdisp; x += 2) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p++->val     = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];

			svga->ma += 4;
		}
	} 
	svga->ma += x << 1;
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_16bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 8) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
			p++->val     = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);
			p++->val = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 8) & svga->vram_display_mask]);
			p++->val = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];

			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 1) + 12) & svga->vram_display_mask]);
			p++->val = video_16to32[dat & 0xffff];
			p++->val = video_16to32[dat >> 16];
		}
		svga->ma += x << 1;
	} 
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_24bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat0, dat1, dat2, addr;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x++) {
			dat0 = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);
			dat1 = *(uint32_t *)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
			dat2 = *(uint32_t *)(&svga->vram[(svga->ma + 8) & svga->vram_display_mask]);

			p[0].val = p[1].val = dat0 & 0xffffff;
			p[2].val = p[3].val = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
			p[4].val = p[5].val = (dat1 >> 16) | ((dat2 & 0xff) << 16);
			p[6].val = p[7].val = dat2 >> 8;

			svga->ma += 12;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 4) {
			addr = svga->remap_func(svga, svga->ma);
			dat0 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			addr = svga->remap_func(svga, svga->ma + 4);
			dat1 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			addr = svga->remap_func(svga, svga->ma + 8);
			dat2 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p[0].val = p[1].val = dat0 & 0xffffff;
			p[2].val = p[3].val = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
			p[4].val = p[5].val = (dat1 >> 16) | ((dat2 & 0xff) << 16);
			p[6].val = p[7].val = dat2 >> 8;

			svga->ma += 12;
		}
	}
    }
}


void
svga_render_24bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat0, dat1, dat2, addr;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x += 4) {
			dat0 = *(uint32_t *)(&svga->vram[svga->ma & svga->vram_display_mask]);
			dat1 = *(uint32_t *)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
			dat2 = *(uint32_t *)(&svga->vram[(svga->ma + 8) & svga->vram_display_mask]);
			
			p++->val = dat0 & 0xffffff;
			p++->val = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
			p++->val = (dat1 >> 16) | ((dat2 & 0xff) << 16);
			p++->val = dat2 >> 8;

			svga->ma += 12;
		}
	} else {
		for (x = 0; x <= svga->hdisp; x += 4) {
			addr = svga->remap_func(svga, svga->ma);
			dat0 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			addr = svga->remap_func(svga, svga->ma + 4);
			dat1 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			addr = svga->remap_func(svga, svga->ma + 8);
			dat2 = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);

			p++->val = dat0 & 0xffffff;
			p++->val = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
			p++->val = (dat1 >> 16) | ((dat2 & 0xff) << 16);
			p++->val = dat2 >> 8;

			svga->ma += 12;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_32bpp_lowres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int x, offset;
    uint32_t addr, dat;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);
    pel_t *p;

    if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange) {
	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	offset = (8 - (svga->scrollcache & 6)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x++) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
			p++->val = dat & 0xffffff;
			p++->val = dat & 0xffffff;
		}
		svga->ma += x*4;
	} else {
		 for (x = 0; x <= svga->hdisp; x++) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr]);
			p++->val = dat & 0xffffff;
			p++->val = dat & 0xffffff;
			svga->ma += 4;
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}


/*72%
  91%*/
void
svga_render_32bpp_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t addr, dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] ||  svga->changedvram[(changed_addr >> 12) + 1] || svga->changedvram[(svga->ma >> 12) + 2] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x++) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
			p++->val = dat & 0xffffff;
		}
		svga->ma += 4;
	} else {
		for (x = 0; x <= svga->hdisp; x++) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			p++->val = dat & 0xffffff;
			svga->ma += 4;
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_ABGR8888_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t dat, addr;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] ||  svga->changedvram[(changed_addr >> 12) + 1] || svga->changedvram[(svga->ma >> 12) + 2] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000)
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x++) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
			p++->val = ((dat & 0xff0000) >> 16) | (dat & 0x00ff00) | ((dat & 0x0000ff) << 16);
		}
		svga->ma += 4;
	} else {
		for (x = 0; x <= svga->hdisp; x++) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			p++->val = ((dat & 0xff0000) >> 16) | (dat & 0x00ff00) | ((dat & 0x0000ff) << 16);
			svga->ma += 4;
		}
	}
	svga->ma &= svga->vram_display_mask;
    }
}


void
svga_render_RGBA8888_highres(svga_t *svga)
{
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int offset, x;
    uint32_t addr, dat;
    pel_t *p;
    uint32_t changed_addr = svga->remap_func(svga, svga->ma);

    if (svga->changedvram[changed_addr >> 12] ||  svga->changedvram[(changed_addr >> 12) + 1] || svga->changedvram[(svga->ma >> 12) + 2] || svga->fullchange) {
	offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
	p = &screen->line[svga->displine + y_add][offset + x_add];

	if (svga->firstline_draw == 2000) 
		svga->firstline_draw = svga->displine;
	svga->lastline_draw = svga->displine;

	if (!svga->remap_required) {
		for (x = 0; x <= svga->hdisp; x++) {
			dat = *(uint32_t *)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
			p++->val = dat >> 8;
		}
		svga->ma += x*4;
	} else { 
		for (x = 0; x <= svga->hdisp; x++) {
			addr = svga->remap_func(svga, svga->ma);
			dat = *(uint32_t *)(&svga->vram[addr & svga->vram_display_mask]);
			p++->val = dat >> 8;
			svga->ma += 4;
		}
	}

	svga->ma &= svga->vram_display_mask;
    }
}
