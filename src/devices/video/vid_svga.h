/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the generic SVGA driver.
 *
 * Version:	@(#)vid_svga.h	1.0.10	2020/01/20
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
#ifndef VIDEO_SVGA_H
# define VIDEO_SVGA_H

#define FLAG_EXTRA_BANKS	1
#define FLAG_ADDR_BY8		2
#define FLAG_LATCH8		4

typedef struct {
    int ena,
	x, y, xoff, yoff, xsize, ysize,
	v_acc, h_acc;

    uint32_t addr, pitch;
} hwcursor_t;

typedef union {
    uint64_t	q;
    uint32_t	d[2];
    uint16_t	w[4];
    uint8_t	b[8];
} latch_t;

typedef struct svga_t
{
    mem_map_t mapping;

    int enabled, fast, vidclock, fb_only,
	dac_addr, dac_pos, dac_r, dac_g,
	ramdac_type, ext_overscan,
	readmode, writemode, readplane, extvram,
	chain4, chain2_write, chain2_read,
	oddeven_page, oddeven_chain,
	set_reset_disabled,
	vtotal, dispend, vsyncstart, split, vblankstart,
	hdisp,  hdisp_old, htotal,  hdisp_time, rowoffset,
	lowres, interlace, linedbl, rowcount, bpp,
	dispon, hdisp_on,
	vc, sc, linepos, vslines, linecountff, oddeven,
	con, cursoron, blink, scrollcache,
	firstline, lastline, firstline_draw, lastline_draw,
	displine, fullchange,
	video_res_x, video_res_y, video_bpp, frames, fps,
	vram_display_mask,
	hwcursor_on, dac_hwcursor_on, overlay_on,
	hwcursor_oddeven, dac_hwcursor_oddeven, overlay_oddeven;

    /*The three variables below allow us to implement memory maps like that seen on a 1MB Trio64 :
      0MB-1MB - VRAM
      1MB-2MB - VRAM mirror
      2MB-4MB - open bus
      4MB-xMB - mirror of above
      For the example memory map, decode_mask would be 4MB-1 (4MB address space), vram_max would be 2MB
      (present video memory only responds to first 2MB), vram_mask would be 1MB-1 (video memory wraps at 1MB)
    */
    uint32_t decode_mask, vram_max,
	     vram_mask,
	     charseta, charsetb,
	     adv_flags, ma_latch,
	     ma, maback,
	     write_bank, read_bank,
	     extra_banks[2],
	     banked_mask,
	     ca, overscan_color,
	     *map8, pallook[256];

    latch_t latch;
    
    PALETTE vgapal;

    tmrval_t dispontime, dispofftime,
	    vidtime;

    double clock;

    hwcursor_t hwcursor, hwcursor_latch,
	       dac_hwcursor, dac_hwcursor_latch,
	       overlay, overlay_latch;

    void (*render)(struct svga_t *svga);
    void (*recalctimings_ex)(struct svga_t *svga);
    void    (*video_out)(uint16_t addr, uint8_t val, priv_t);
    uint8_t (*video_in) (uint16_t addr, priv_t);
    void (*hwcursor_draw)(struct svga_t *svga, int displine);
    void (*dac_hwcursor_draw)(struct svga_t *svga, int displine);
    void (*overlay_draw)(struct svga_t *svga, int displine);
    void (*vblank_start)(struct svga_t *svga);
    void (*ven_write)(struct svga_t *svga, uint8_t val, uint32_t addr);
    float (*getclock)(int clock, priv_t);

    /*If set then another device is driving the monitor output and the SVGA
      card should not attempt to display anything */
    int		override;
    priv_t	p;

    uint8_t crtc[128], gdcreg[64], attrregs[32], seqregs[64],
	    egapal[16],
	    *vram, *changedvram;
    uint8_t crtcreg, gdcaddr,
	    attrff, attr_palette_enable, attraddr, seqaddr,
	    miscout, cgastat, scrblank,
	    plane_mask, writemask,
	    colourcompare, colournocare,
	    dac_mask, dac_status,
	    ksc5601_sbyte_mask;

    priv_t	ramdac,
		clock_gen;
} svga_t;


extern int	svga_init(svga_t *svga, priv_t, int memsize, 
			  void (*recalctimings_ex)(struct svga_t *svga),
			  uint8_t (*video_in) (uint16_t addr, priv_t),
			  void    (*video_out)(uint16_t addr, uint8_t val, priv_t),
			  void (*hwcursor_draw)(struct svga_t *svga, int displine),
			  void (*overlay_draw)(struct svga_t *svga, int displine));
extern void	svga_recalctimings(svga_t *svga);
extern void	svga_close(svga_t *svga);
uint8_t		svga_read(uint32_t addr, priv_t);
uint16_t	svga_readw(uint32_t addr, priv_t);
uint32_t	svga_readl(uint32_t addr, priv_t);
void		svga_write(uint32_t addr, uint8_t val, priv_t);
void		svga_writew(uint32_t addr, uint16_t val, priv_t);
void		svga_writel(uint32_t addr, uint32_t val, priv_t);
uint8_t		svga_read_linear(uint32_t addr, priv_t);
uint8_t		svga_readb_linear(uint32_t addr, priv_t);
uint16_t	svga_readw_linear(uint32_t addr, priv_t);
uint32_t	svga_readl_linear(uint32_t addr, priv_t);
void		svga_write_linear(uint32_t addr, uint8_t val, priv_t);
void		svga_writeb_linear(uint32_t addr, uint8_t val, priv_t);
void		svga_writew_linear(uint32_t addr, uint16_t val, priv_t);
void		svga_writel_linear(uint32_t addr, uint32_t val, priv_t);

extern uint8_t	svga_rotate[8][256];

extern void	svga_out(uint16_t addr, uint8_t val, priv_t);
extern uint8_t	svga_in(uint16_t addr, priv_t);

extern svga_t	*svga_get_pri(void);
extern void	svga_set_override(svga_t *svga, int val);

extern void	svga_set_ramdac_type(svga_t *svga, int type);
extern void	svga_close(svga_t *svga);

extern uint32_t	svga_mask_addr(uint32_t addr, svga_t *svga);
extern uint32_t	svga_mask_changedaddr(uint32_t addr, svga_t *svga);

extern void	svga_doblit(int y1, int y2, int wx, int wy, svga_t *svga);

enum {
    RAMDAC_6BIT = 0,
    RAMDAC_8BIT
};


#endif	/*VIDEO_SVGA_H*/
