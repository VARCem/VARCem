/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of Cirrus Logic cards.
 *
 * Version:	@(#)vid_cl54xx.c	1.0.33	2020/01/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Barry Rodewald,
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2018 Barry Rodewald.
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
#include <stdarg.h>
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
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"


#define BIOS_GD5420_PATH	L"video/cirruslogic/5420.vbi"
#define BIOS_GD5422_PATH	L"video/cirruslogic/cl5422.bin"
#define BIOS_GD5426_PATH	L"video/cirruslogic/diamond speedstar pro vlb v3.04.bin"
#define BIOS_GD5428_ISA_PATH	L"video/cirruslogic/gd5428.bin"
#define BIOS_GD5428_VLB_PATH	L"video/cirruslogic/vlbusjapan.bin"
#define BIOS_GD5429_PATH	L"video/cirruslogic/gd5429.vbi"
#define BIOS_GD5430_VLB_PATH	L"video/cirruslogic/diamondvlbus.bin"
#define BIOS_GD5430_PCI_PATH	L"video/cirruslogic/gd5430pci.bin"
#define BIOS_GD5434_PATH	L"video/cirruslogic/gd5434.bin"
#define BIOS_GD5436_PATH	L"video/cirruslogic/gd5436.vbi"
#define BIOS_GD5440_PATH	L"video/cirruslogic/gd5440.bin"
#define BIOS_GD5446_PATH	L"video/cirruslogic/gd5446bv.vbi"
#define BIOS_GD5446_STB_PATH	L"video/cirruslogic/stb_nitro64v.bin"
#define BIOS_GD5480_PATH	L"video/cirruslogic/gd5480.rom"


#define CIRRUS_ID_AVGA2			0x18
#define CIRRUS_ID_CLGD5402	  	0x89
#define CIRRUS_ID_CLGD5420	  	0x8a
#define CIRRUS_ID_CLGD5422  		0x8c
#define CIRRUS_ID_CLGD5424  		0x94
#define CIRRUS_ID_CLGD5426		0x90
#define CIRRUS_ID_CLGD5428		0x98
#define CIRRUS_ID_CLGD5429		0x9c
#define CIRRUS_ID_CLGD5430		0xa0
#define CIRRUS_ID_CLGD5434		0xa8
#define CIRRUS_ID_CLGD5436		0xac
#define CIRRUS_ID_CLGD5440		0xa0	/* 5430 + CLPX2070/'85 video accelerator */
#define CIRRUS_ID_CLGD5446		0xb8
#define CIRRUS_ID_CLGD5480		0xbc


/* sequencer 0x07 */
#define CIRRUS_SR7_BPP_VGA		0x00
#define CIRRUS_SR7_BPP_SVGA		0x01
#define CIRRUS_SR7_BPP_MASK		0x0e
#define CIRRUS_SR7_BPP_8		0x00
#define CIRRUS_SR7_BPP_16_DOUBLEVCLK	0x02
#define CIRRUS_SR7_BPP_24		0x04
#define CIRRUS_SR7_BPP_16		0x06
#define CIRRUS_SR7_BPP_32		0x08
#define CIRRUS_SR7_ISAADDR_MASK		0xe0

/* sequencer 0x12 */
#define CIRRUS_CURSOR_SHOW		0x01
#define CIRRUS_CURSOR_HIDDENPEL		0x02
#define CIRRUS_CURSOR_LARGE		0x04	/* 64x64 if set, else 32x32 */

/* sequencer 0x17 */
#define CIRRUS_BUSTYPE_VLBFAST		0x10
#define CIRRUS_BUSTYPE_PCI		0x20
#define CIRRUS_BUSTYPE_VLBSLOW		0x30
#define CIRRUS_BUSTYPE_ISA		0x38
#define CIRRUS_MMIO_ENABLE		0x04
#define CIRRUS_MMIO_USE_PCIADDR		0x40	/* 0xb8000 if cleared. */
#define CIRRUS_MEMSIZEEXT_DOUBLE	0x80

/* graphic controller 0x0b */
#define CIRRUS_BANKING_DUAL             0x01
#define CIRRUS_BANKING_GRANULARITY_16K  0x20	/* set:16k, clear:4k */

/* graphic controller 0x30 */
#define CIRRUS_BLTMODE_BACKWARDS	0x01
#define CIRRUS_BLTMODE_MEMSYSDEST	0x02
#define CIRRUS_BLTMODE_MEMSYSSRC	0x04
#define CIRRUS_BLTMODE_TRANSPARENTCOMP	0x08
#define CIRRUS_BLTMODE_PATTERNCOPY	0x40
#define CIRRUS_BLTMODE_COLOREXPAND	0x80
#define CIRRUS_BLTMODE_PIXELWIDTHMASK	0x30
#define CIRRUS_BLTMODE_PIXELWIDTH8	0x00
#define CIRRUS_BLTMODE_PIXELWIDTH16	0x10
#define CIRRUS_BLTMODE_PIXELWIDTH24	0x20
#define CIRRUS_BLTMODE_PIXELWIDTH32	0x30

/* graphic controller 0x31 */
#define CIRRUS_BLT_BUSY                 0x01
#define CIRRUS_BLT_START                0x02
#define CIRRUS_BLT_RESET                0x04
#define CIRRUS_BLT_FIFOUSED             0x10 /* 5436 only */
#define CIRRUS_BLT_PAUSED		0x20 /* 5436 only */
#define CIRRUS_BLT_APERTURE2		0x40
#define CIRRUS_BLT_AUTOSTART            0x80 /* 5436 only */

/* graphic controller 0x33 */
#define CIRRUS_BLTMODEEXT_BACKGROUNDONLY   0x08
#define CIRRUS_BLTMODEEXT_SOLIDFILL        0x04
#define CIRRUS_BLTMODEEXT_COLOREXPINV      0x02
#define CIRRUS_BLTMODEEXT_DWORDGRANULARITY 0x01

#define CL_GD5429_SYSTEM_BUS_VESA	5
#define CL_GD5429_SYSTEM_BUS_ISA	7

#define CL_GD543X_SYSTEM_BUS_PCI	4
#define CL_GD543X_SYSTEM_BUS_VESA	6
#define CL_GD543X_SYSTEM_BUS_ISA	7


typedef struct {
    mem_map_t		mmio_mapping;
    mem_map_t	 	linear_mapping;
    mem_map_t		aperture2_mapping;

    svga_t		svga;

    int			has_bios,
			rev;

    rom_t		bios_rom;

    uint32_t		vram_size;
    uint32_t		vram_mask;

    uint8_t		vclk_n[4];
    uint8_t		vclk_d[4];        

    struct {
	uint8_t	state;
	int	ctrl;
    }			ramdac;		

    struct {
	uint16_t	width, height;
	uint16_t	dst_pitch, src_pitch;               
	uint16_t	trans_col, trans_mask;
	uint16_t	height_internal;
	uint16_t	msd_buf_pos, msd_buf_cnt;
	
	uint8_t		status;
	uint8_t		mask, mode, rop, modeext;
	uint8_t		ms_is_dest, msd_buf[32];

	uint32_t	fg_col, bg_col;
	uint32_t	dst_addr_backup, src_addr_backup;
	uint32_t	dst_addr, src_addr;
	uint32_t	sys_src32, sys_cnt;

	/* Internal state */
	int		pixel_width, pattern_x;
	int		x_count, y_count;
	int		xx_count, dir;
	int		unlock_special;	
    }			blt;

    int			pci, vlb;
    int			countminusone;    

    uint8_t		pci_regs[256];
    uint8_t		int_line;

    /* FIXME: move to SVGA?  --FvK */
    uint8_t		fc;			/* Feature Connector */

    int			card;

    uint32_t		lfb_base;

    int			mmio_vram_overlap;
    
    uint32_t		extpallook[256];
    PALETTE		extpal;
} gd54xx_t;

static void	
gd543x_mmio_write(uint32_t addr, uint8_t val, priv_t);
static void	
gd543x_mmio_writeb(uint32_t addr, uint8_t val, priv_t);
static void	
gd543x_mmio_writew(uint32_t addr, uint16_t val, priv_t);
static void	
gd543x_mmio_writel(uint32_t addr, uint32_t val, priv_t);
static 
uint8_t	gd543x_mmio_read(uint32_t addr, priv_t);
static 
uint16_t	gd543x_mmio_readw(uint32_t addr, priv_t);
static 
uint32_t	gd543x_mmio_readl(uint32_t addr, priv_t);
static void	
reset_blit(gd54xx_t *gd54xx);
static void	
start_blit(uint32_t cpu_dat, uint32_t count,
				  gd54xx_t *gd54xx, svga_t *svga);

/* Returns 1 if the extension registers are locked */
static int
is_locked(svga_t *svga)
{
    if (svga->crtc[0x27] < CIRRUS_ID_CLGD5429) {
         if (svga->seqregs[6] == 0x12)
		return 0;
	else
		return 1;
    }

    return 0;
}
				  
/* Returns 1 if the card is a 5422+ */
static int
is_5422(svga_t *svga)
{
    if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5422)
	return 1;

    return 0;
}

/* Returns 1 if the card is a 5426+ */
static int
is_5426(svga_t *svga)
{
    if ((svga->crtc[0x27] >= CIRRUS_ID_CLGD5426) && !(svga->crtc[0x27] == CIRRUS_ID_CLGD5424))
	return 1;

    return 0;
}

/* Returns 1 if the card is a 5434, 5436/46, or 5480. */
static int
is_5434(svga_t *svga)
{
    if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5434)
	return 1;

    return 0;
}

/* Returns 1 if the card supports the 8-bpp/16-bpp transparency color or mask. */
static int
has_transp(svga_t *svga, int mask)
{
    if (((svga->crtc[0x27] == CIRRUS_ID_CLGD5446) || (svga->crtc[0x27] == CIRRUS_ID_CLGD5480)) &&
	!mask)
	return 1;	/* 5446 and 5480 have mask but not transparency. */
    if ((svga->crtc[0x27] == CIRRUS_ID_CLGD5426) || (svga->crtc[0x27] == CIRRUS_ID_CLGD5428))
	return 1;	/* 5426 and 5428 have both. */
    else
	return 0;	/* The rest have neither. */
}


static void
recalc_banking(gd54xx_t *dev)
{
    svga_t *svga = &dev->svga;

    if (! is_5422(svga)) {
	svga->extra_banks[0] = (svga->gdcreg[0x09] & 0x7f) << 12;

	if (svga->gdcreg[0x0b] & CIRRUS_BANKING_DUAL)
		svga->extra_banks[1] = (svga->gdcreg[0x0a] & 0x7f) << 12;
	else
		svga->extra_banks[1] = svga->extra_banks[0] + 0x8000;
    } else {
	if ((svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K) && (is_5426(svga)))
		svga->extra_banks[0] = svga->gdcreg[0x09] << 14;
	else
		svga->extra_banks[0] = svga->gdcreg[0x09] << 12;

	if (svga->gdcreg[0x0b] & CIRRUS_BANKING_DUAL) {
		if ((svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K) && (is_5426(svga)))
			svga->extra_banks[1] = svga->gdcreg[0x0a] << 14;
		else
			svga->extra_banks[1] = svga->gdcreg[0x0a] << 12;
	} else
		svga->extra_banks[1] = svga->extra_banks[0] + 0x8000;
    }
    
    svga->write_bank = svga->read_bank = svga->extra_banks[0];
}


static void 
recalc_mapping(gd54xx_t *dev)
{
    svga_t *svga = &dev->svga;
    uint32_t base, size;
        
    if (! (dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM)) {
	mem_map_disable(&svga->mapping);
	mem_map_disable(&dev->linear_mapping);
	mem_map_disable(&dev->mmio_mapping);
	return;
    }

    dev->mmio_vram_overlap = 0;

    if (!is_5422(svga) || !(svga->seqregs[7] & 0xf0) || !(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	mem_map_disable(&dev->linear_mapping);
	mem_map_disable(&dev->aperture2_mapping);

	switch (svga->gdcreg[6] & 0x0c) {
		case 0x00:	/*128K at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x20000);
			svga->banked_mask = 0x1ffff;
			break;

		case 0x04:	/*64K at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
			svga->banked_mask = 0xffff;
			break;

		case 0x08:	/*32K at B0000*/
			mem_map_set_addr(&svga->mapping, 0xb0000, 0x08000);
			svga->banked_mask = 0x7fff;
			break;

		case 0x0c:	/*32K at B8000*/
			mem_map_set_addr(&svga->mapping, 0xb8000, 0x08000);
			svga->banked_mask = 0x7fff;
			dev->mmio_vram_overlap = 1;
			break;
	}

	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA) && (is_5426(svga))) {
		if (dev->mmio_vram_overlap) {
			mem_map_disable(&svga->mapping);
			mem_map_set_addr(&dev->mmio_mapping, 0xb8000, 0x08000);
		} else
			mem_map_set_addr(&dev->mmio_mapping, 0xb8000, 0x00100);
	}
	else
		mem_map_disable(&dev->mmio_mapping);
    } else {
	if (svga->crtc[0x27] <= CIRRUS_ID_CLGD5429 ||
	    (!dev->pci && !dev->vlb)) {
		if ((svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K) && (is_5426(svga))) {
			base = (svga->seqregs[7] & 0xe0) << 16;
			size = 2 * 1024 * 1024;
		} else {
			base = (svga->seqregs[7] & 0xf0) << 16;
			size = 1 * 1024 * 1024;
		}
	} else if (dev->pci) {
		base = dev->lfb_base;

		if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
			size = 16 * 1024 * 1024;
		else
			size = 4 * 1024 * 1024;
	} else { /*VLB*/
		base = 128 * 1024 * 1024;
		if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
			size = 16 * 1024 * 1024;
		else
			size = 4 * 1024 * 1024;
	}

	mem_map_disable(&svga->mapping);
	mem_map_set_addr(&dev->linear_mapping, base, size);

	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (is_5426(svga))) {
		if (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)
			mem_map_disable(&dev->mmio_mapping); /* MMIO is handled in the linear read/write functions */
		else
			mem_map_set_addr(&dev->mmio_mapping, 0xb8000, 0x00100);
	} else
		mem_map_disable(&dev->mmio_mapping);

	if ((svga->crtc[0x27] >= CIRRUS_ID_CLGD5436) && (dev->blt.status & CIRRUS_BLT_APERTURE2) &&
	    ((dev->blt.mode & (CIRRUS_BLTMODE_COLOREXPAND | CIRRUS_BLTMODE_MEMSYSSRC)) ==
	    (CIRRUS_BLTMODE_COLOREXPAND | CIRRUS_BLTMODE_MEMSYSSRC)))
		mem_map_set_addr(&dev->aperture2_mapping, 0xbc000, 0x04000);
	else
		mem_map_disable(&dev->aperture2_mapping);
	}
}


static void
recalc_timings(svga_t *svga)
{
    gd54xx_t *dev = (gd54xx_t *)svga->p;	
    uint8_t clocksel, rdmask;

    svga->rowoffset = (svga->crtc[0x13]) | ((svga->crtc[0x1b] & 0x10) << 4);

    svga->interlace = (svga->crtc[0x1a] & 0x01);
    
    svga->map8 = svga->pallook;

    if (svga->seqregs[7] & CIRRUS_SR7_BPP_SVGA)
	svga->render = svga_render_8bpp_highres;
    else if (svga->gdcreg[5] & 0x40)
	svga->render = svga_render_8bpp_lowres;

    svga->ma_latch |= ((svga->crtc[0x1b] & 0x01) << 16) | ((svga->crtc[0x1b] & 0xc) << 15);

    svga->bpp = 8;

    if (dev->ramdac.ctrl & 0x80)  { /* Extended Color Mode enabled */
	if (dev->ramdac.ctrl & 0x40) { /* Enable all Extended modes */
	
		if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5428)
			rdmask = 0xf;
		else
			rdmask = 0x7;
		
		switch (dev->ramdac.ctrl & rdmask) {
			case 0:
				svga->bpp = 15;
				if (dev->ramdac.ctrl & 0x10) /* Mixed Mode */
					svga->render = svga_render_mixed_highres;
				else				
					svga->render = svga_render_15bpp_highres;
				break;

			case 1:
				svga->bpp = 16;
				svga->render = svga_render_16bpp_highres;
				break;

			case 5:
				if (is_5434(svga) && (svga->seqregs[7] & CIRRUS_SR7_BPP_32)) {
					svga->bpp = 32;
					svga->render = svga_render_32bpp_highres;
					if (svga->crtc[0x27] < CIRRUS_ID_CLGD5436)
						svga->rowoffset *= 2;
				} else {
					svga->bpp = 24;
					svga->render = svga_render_24bpp_highres;
				}
				break;

			case 8:		
				svga->bpp = 8;
				svga->map8 = video_8togs;
				svga->render = svga_render_8bpp_gs_highres;
				break;
			
			case 9:		
				svga->bpp = 8;
				svga->map8 = video_8to32;
				svga->render = svga_render_8bpp_rgb_highres;
				break;
		}
	} else {
		/* Default to 5-5-5 Sierra Mode */
		svga->bpp = 15;
		if (dev->ramdac.ctrl & 0x10) /* Mixed Mode */
			svga->render = svga_render_mixed_highres;
		else				
			svga->render = svga_render_15bpp_highres;
	}
    }

    clocksel = (svga->miscout >> 2) & 3;

    if (!dev->vclk_n[clocksel] || !dev->vclk_d[clocksel])
	svga->clock = cpu_clock / ((svga->miscout & 0xc) ? 28322000.0 : 25175000.0);
	//svga->clock = (cpu_clock * (float)(1ull << 32)) / ((svga->miscout & 0xc) ? 28322000.0 : 25175000.0);
    else {
	int n = dev->vclk_n[clocksel] & 0x7f;
	int d = (dev->vclk_d[clocksel] & 0x3e) >> 1;
	int m = dev->vclk_d[clocksel] & 0x01 ? 2 : 1;
	float freq = ((float)14318184.0 * ((float)n / ((float)d * m)));
	if (is_5422(svga)) {
		switch (svga->seqregs[7] & (is_5434(svga) ? 0xe : 6)) {
			case 2:
				freq /= 2.0;
				break;

			case 4:
				if (! is_5434(svga))
					freq /= 3.0; 
				break;
		}
	}

	svga->clock = cpu_clock / freq;
	//svga->clock = (cpu_clock * (double)(1ull << 32)) / freq;
    }

    svga->vram_display_mask = (svga->crtc[0x1b] & 2) ? dev->vram_mask : 0x3ffff;
}


static void
gd54xx_out(uint16_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t o, indx, old;
    uint32_t o32, mask;
    int c;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) &&
	!(svga->miscout & 1)) 
	addr ^= 0x60;

    switch (addr) {
	case 0x03ba:
	case 0x03da:
		dev->fc = val;		//FIXME: move to SVGA? --FvK
		break;

	case 0x03c0:
	case 0x03c1:
		if (! svga->attrff) {
			svga->attraddr = val & 0x3f;
			if ((val & 0x20) != svga->attr_palette_enable) {
				svga->fullchange = 3;
				svga->attr_palette_enable = val & 0x20;
				svga_recalctimings(svga);
			}
		} else {
			o = svga->attrregs[svga->attraddr & 0x3f];
			if ((svga->attraddr < 0x20) || (svga->attraddr >= 0x30))
				svga->attrregs[svga->attraddr & 0x3f] = val;
			if (svga->attraddr < 0x10) 
				svga->fullchange = changeframecount;
			if (svga->attraddr <= 0x10 || svga->attraddr == 0x14) {
				for (c = 0; c < 16; c++) {
					if (svga->attrregs[0x10] & 0x80) svga->egapal[c] = (svga->attrregs[c] &  0xf) | ((svga->attrregs[0x14] & 0xf) << 4);
					else                             svga->egapal[c] = (svga->attrregs[c] & 0x3f) | ((svga->attrregs[0x14] & 0xc) << 4);
				}
			}

			/* Recalculate timings on change of attribute register 0x11 (overscan border color) too. */
			if (svga->attraddr == 0x10) {
				if (o != val)
					svga_recalctimings(svga);
			} else if (svga->attraddr == 0x11) {
				if (! (svga->seqregs[0x12] & 0x80)) {
					svga->overscan_color = svga->pallook[svga->attrregs[0x11]];
					if (o != val)  svga_recalctimings(svga);
				}
			} else if (svga->attraddr == 0x12) {
				if ((val & 0xf) != svga->plane_mask)
					svga->fullchange = changeframecount;
				svga->plane_mask = val & 0xf;
			}
		}
		svga->attrff ^= 1;
                return;

	case 0x03c4:
		svga->seqaddr = val;
		break;

	case 0x03c5:
		if (is_locked(svga)) {
			if (svga->seqaddr == 2) {
				o = svga->seqregs[svga->seqaddr & 0x1f];
				svga_out(addr, val, svga);
				svga->seqregs[svga->seqaddr & 0x1f] = (o & 0xf0) | (val & 0x0f);
				return;
			}
			if (svga->seqaddr > 6)
				return;
		}
		if (svga->seqaddr > 5) {
			o = svga->seqregs[svga->seqaddr & 0x1f];
			svga->seqregs[svga->seqaddr & 0x1f] = val;
			switch (svga->seqaddr) {
				case 0x06:
					val &= 0x17;
					if (val == 0x12)
						svga->seqregs[6] = 0x12;
					    else
						svga->seqregs[6] = 0x0f;
					is_locked(svga);
					break;

				case 0x08:
					if ((dev->pci) || (dev->vlb))
						val &= 0xbf;
					svga->seqregs[8] = val;
					break;
											
				case 0x0a:
					if (is_5426(svga)) {
						svga->seqregs[0x0a] = val;
					} else {
						/* FIXME : Hack to force memory size on some GD-542x BIOSes*/
						val &= 0xe7;
						switch (dev->vram_size) {
							case 0x080000:
								svga->seqregs[0x0a] = val | 0x08;
								break;

							case 0x100000:
								svga->seqregs[0x0a] = val | 0x10;
								break;

							case 0x200000:
								svga->seqregs[0x0a] = (val | 0x18);
								break;
						}
					}
					break;

				case 0x0b: case 0x0c: case 0x0d: case 0x0e: /* VCLK stuff */
					dev->vclk_n[svga->seqaddr - 0x0b] = val;
					break;
					
				case 0x1b: case 0x1c: case 0x1d: case 0x1e: /* VCLK stuff */
					dev->vclk_d[svga->seqaddr-0x1b] = val;
					break;

				case 0x10: case 0x30: case 0x50: case 0x70:
				case 0x90: case 0xb0: case 0xd0: case 0xf0:
					svga->hwcursor.x = (val << 3) | (svga->seqaddr >> 5);
					break;

				case 0x11: case 0x31: case 0x51: case 0x71:
				case 0x91: case 0xb1: case 0xd1: case 0xf1:
					svga->hwcursor.y = (val << 3) | (svga->seqaddr >> 5);
					break;

				case 0x12:
					svga->ext_overscan = !!(val & 0x80);
					if (svga->ext_overscan && (is_5426(svga)))
						svga->overscan_color = dev->extpallook[2];
					else
						svga->overscan_color = svga->pallook[svga->attrregs[0x11]];
					svga_recalctimings(svga);
					svga->hwcursor.ena = val & CIRRUS_CURSOR_SHOW;
					if (is_5422(svga))
					        svga->hwcursor.xsize = svga->hwcursor.ysize = (val & CIRRUS_CURSOR_LARGE) ? 64 : 32;
					else
						svga->hwcursor.xsize = 32;
					svga->hwcursor.yoff = (svga->hwcursor.ysize == 32) ? 32 : 0;
					if ((! is_5426(svga)) || (svga->crtc[0x1b] & 2))
						mask = svga->vram_display_mask;
					else
						mask = svga->vram_mask;
					if ((svga->seqregs[0x12] & CIRRUS_CURSOR_LARGE) && (is_5422(svga)))
						svga->hwcursor.addr = ((dev->vram_size - 0x4000) + ((svga->seqregs[0x13] & 0x3c) * 256)) & mask;
					else
						svga->hwcursor.addr = ((dev->vram_size - 0x4000) + ((svga->seqregs[0x13] & 0x3f) * 256)) & mask;
					break;

				case 0x13:
					if ((! is_5426(svga)) || (svga->crtc[0x1b] & 2))
						mask = svga->vram_display_mask;
					else
						mask = svga->vram_mask;
					if ((svga->seqregs[0x12] & CIRRUS_CURSOR_LARGE) && (is_5422(svga)))
						svga->hwcursor.addr = ((dev->vram_size - 0x4000) + ((val & 0x3c) * 256)) & mask;
					else
						svga->hwcursor.addr = ((dev->vram_size - 0x4000) + ((val & 0x3f) * 256)) & mask;
					break;
			
				case 0x15:
					if (is_5426(svga)) {
						/* FIXME : Hack to force memory size on some GD-542x BIOSes*/
						val &= 0xf8;
						switch (dev->vram_size) {				
							case 0x100000:
								svga->seqregs[0x15] = val | 0x02;
								break;

							case 0x200000:
								svga->seqregs[0x15] = (val | 0x03);
								break;

							case 0x400000:
								svga->seqregs[0x15] = val | 0x04;
								break;
						}
					} else
						return;
					break;

				case 0x07:
					if (is_5422(svga)) 
						recalc_mapping(dev);
					else
						svga->seqregs[svga->seqaddr] &= 0x0f;
					if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5429)
						svga->set_reset_disabled = svga->seqregs[7] & 1;
					break;

				case 0x17:
					if (is_5422(svga))
						recalc_mapping(dev);
					else
						return;
					break;
			}
			return;
		}
		break;

	case 0x03c6:
		if (is_locked(svga))
			break;
		if (dev->ramdac.state == 4) {
			dev->ramdac.state = 0;
			dev->ramdac.ctrl = val;
			svga_recalctimings(svga);
			return;
		}
		dev->ramdac.state = 0;
		break;

	case 0x03c7: case 0x03c8:
		dev->ramdac.state = 0;
		break;
	
	case 0x03c9:
		dev->ramdac.state = 0;
		svga->dac_status = 0;
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
				indx = svga->dac_addr & 0xff;
				if (svga->seqregs[0x12] & 2) {
					indx &= 0x0f;
					dev->extpal[indx].r = svga->dac_r;
					dev->extpal[indx].g = svga->dac_g;
					dev->extpal[indx].b = val; 
					dev->extpallook[indx] = makecol32(video_6to8[dev->extpal[indx].r & 0x3f], video_6to8[dev->extpal[indx].g & 0x3f], video_6to8[dev->extpal[indx].b & 0x3f]);
					if (svga->ext_overscan && (indx == 2)) {
						o32 = svga->overscan_color;
						svga->overscan_color = dev->extpallook[2];
						if (o32 != svga->overscan_color)
							svga_recalctimings(svga);
					}
				} else {
					svga->vgapal[indx].r = svga->dac_r;
					svga->vgapal[indx].g = svga->dac_g;
					svga->vgapal[indx].b = val; 
					svga->pallook[indx] = makecol32(video_6to8[svga->vgapal[indx].r & 0x3f], video_6to8[svga->vgapal[indx].g & 0x3f], video_6to8[svga->vgapal[indx].b & 0x3f]);
				}
				svga->dac_addr = (svga->dac_addr + 1) & 255;
				svga->dac_pos = 0; 
                        	break;
                }
                return;

	case 0x03ce:
		svga->gdcaddr = val;
		return;
		
	case 0x03cf:
	
		if ((svga->gdcaddr > 0x1f) && (!is_5426(svga)))
			return;

		o = svga->gdcreg[svga->gdcaddr];

		if ((svga->gdcaddr < 2) && is_locked(svga))
			svga->gdcreg[svga->gdcaddr] = (svga->gdcreg[svga->gdcaddr] & 0xf0) | (val & 0x0f);
		else if ((svga->gdcaddr <= 8) || !is_locked(svga))
			svga->gdcreg[svga->gdcaddr] = val;
			
		if (svga->gdcaddr <= 8) {
			switch (svga->gdcaddr) {
				case 0x00:
					gd543x_mmio_write(0xb8000, val, dev);
					break;
					
				case 0x01:
					gd543x_mmio_write(0xb8004, val, dev);
					break;
					
				case 0x02:
					svga->colourcompare = val;
					break;
					
				case 0x04:
					svga->readplane = val & 3;
					break;
				
				case 0x05:
					if (svga->gdcreg[0xb] & 0x04)
						svga->writemode = val & 7;
					else
						svga->writemode = val & 3;
					svga->readmode = val & 8;
					svga->chain2_read = val & 0x10;
					break;
					
				case 0x06:
					if ((o ^ val) & 0x0c)
						recalc_mapping(dev);
					break;
					
				case 0x07:
					svga->colournocare = val;
					break;
			}
			
			svga->fast = (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) &&
				     !svga->gdcreg[1]) && svga->chain4;
			if (((svga->gdcaddr == 5) && ((val ^o) & 0x70)) ||
			    ((svga->gdcaddr == 6) && ((val ^o) & 1)))
			        svga_recalctimings(svga);
		} else {
			switch (svga->gdcaddr) {
				case 0x09: case 0x0a: case 0x0b:		
					if (svga->gdcreg[0xb] & 0x04)
						svga->writemode = svga->gdcreg[5] & 7;
					else
						svga->writemode = svga->gdcreg[5] & 3;
					svga->adv_flags = 0;
					if (svga->gdcreg[0xb] & 0x01)
						svga->adv_flags = FLAG_EXTRA_BANKS;
					if(svga->gdcreg[0xb] & 0x02)
						svga->adv_flags |= FLAG_ADDR_BY8;
					if (svga->gdcreg[0xb] & 0x08)
						svga->adv_flags |= FLAG_LATCH8;
					recalc_banking(dev);
					break;
					
				case 0x10:
					gd543x_mmio_write(0xb8001, val, dev);
					break;

				case 0x11:
					gd543x_mmio_write(0xb8005, val, dev);
					break;

				case 0x12:
					gd543x_mmio_write(0xb8002, val, dev);
					break;

				case 0x13:
					gd543x_mmio_write(0xb8006, val, dev);
					break;

				case 0x14:
					gd543x_mmio_write(0xb8003, val, dev);
					break;

				case 0x15:
					gd543x_mmio_write(0xb8007, val, dev);
					break;
					
				case 0x20:
					gd543x_mmio_write(0xb8008, val, dev);
					break;

				case 0x21:
					gd543x_mmio_write(0xb8009, val, dev);
					break;

				case 0x22:
					gd543x_mmio_write(0xb800a, val, dev);
					break;

				case 0x23:
					gd543x_mmio_write(0xb800b, val, dev);
					break;

				case 0x24:
					gd543x_mmio_write(0xb800c, val, dev);
					break;

				case 0x25:
					gd543x_mmio_write(0xb800d, val, dev);
					break;

				case 0x26:
					gd543x_mmio_write(0xb800e, val, dev);
					break;

				case 0x27:
					gd543x_mmio_write(0xb800f, val, dev);
					break;

				case 0x28:
					gd543x_mmio_write(0xb8010, val, dev);
					break;

				case 0x29:
					gd543x_mmio_write(0xb8011, val, dev);
					break;

				case 0x2a:
					gd543x_mmio_write(0xb8012, val, dev);
					break;

				case 0x2c:
					gd543x_mmio_write(0xb8014, val, dev);
					break;

				case 0x2d:
					gd543x_mmio_write(0xb8015, val, dev);
					break;

				case 0x2e:
					gd543x_mmio_write(0xb8016, val, dev);
					break;

				case 0x2f:
					gd543x_mmio_write(0xb8017, val, dev);
					break;

				case 0x30:
					gd543x_mmio_write(0xb8018, val, dev);
					break;
	
				case 0x32:
					gd543x_mmio_write(0xb801a, val, dev);
					break;

				case 0x33:
					gd543x_mmio_write(0xb801b, val, dev);
					break;
					
				case 0x31:
					gd543x_mmio_write(0xb8040, val, dev);
					break;
					
				case 0x34:
					gd543x_mmio_write(0xb801c, val, dev);
					break;
					
				case 0x35:
					gd543x_mmio_write(0xb801d, val, dev);
					break;

				case 0x38:
					gd543x_mmio_write(0xb8020, val, dev);
					break;

				case 0x39:
					gd543x_mmio_write(0xb8021, val, dev);
					break;
			}
		}
		return;

	case 0x03d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x03d5:

		if (((svga->crtcreg == 0x19) || (svga->crtcreg == 0x1a) ||
		     (svga->crtcreg == 0x1b) || (svga->crtcreg == 0X1d) ||
		     (svga->crtcreg == 0x25) || (svga->crtcreg == 0x27)) &&
		     is_locked(svga))
			return;

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
gd54xx_in(uint16_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t indx, temp = 0xff;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) &&
	!(svga->miscout & 1))
	addr ^= 0x60;

    switch (addr) {
	case 0x03c4:
		if (!is_locked(svga)) {
			temp = svga->seqaddr;
			if ((temp & 0x1e) == 0x10) {
				if (temp & 1)
					temp = ((svga->hwcursor.y & 7) << 5) | 0x11;
				else
					temp = ((svga->hwcursor.x & 7) << 5) | 0x10;
			}
			return temp;
		}
		return svga->seqaddr;
	
	case 0x03c5:
		if (is_locked(svga)) {
			if (svga->seqaddr == 2) 
				return(svga_in(addr, svga) & 0x0f);
			if (svga->seqaddr > 6)
				return 0xff;
		}
		else if (svga->seqaddr > 5) {
	
			switch (svga->seqaddr) {
				case 0x06:
					return ((svga->seqregs[6] & 0x17) == 0x12) ? 0x12 : 0x0f;

				case 0x0b: case 0x0c: case 0x0d: case 0x0e:
					return dev->vclk_n[svga->seqaddr-0x0b];

				case 0x17:
					if (is_5422(svga)) { 
						temp = svga->seqregs[0x17] & ~(7 << 3);
						if (svga->crtc[0x27] <= CIRRUS_ID_CLGD5429) {
							if (dev->vlb)
								temp |= (CL_GD5429_SYSTEM_BUS_VESA << 3);
							else
								temp |= (CL_GD5429_SYSTEM_BUS_ISA << 3);
						} else {
							if (dev->pci)
								temp |= (CL_GD543X_SYSTEM_BUS_PCI << 3);
							else if (dev->vlb)
								temp |= (CL_GD543X_SYSTEM_BUS_VESA << 3);
							else
								temp |= (CL_GD543X_SYSTEM_BUS_ISA << 3);
						}
						return temp;
					}
					else
						break;
				
				case 0x18: /* TODO Signature Generator */
					return svga->seqregs[0x18];

				case 0x1b: case 0x1c: case 0x1d: case 0x1e:
					return dev->vclk_d[svga->seqaddr-0x1b];
			}
			return svga->seqregs[svga->seqaddr & 0x3f];
		}
		break;

	case 0x03c6:
		if (is_locked(svga))
			return svga_in(addr, svga);
		if (dev->ramdac.state == 4) {
			if (svga->crtc[0x27] != CIRRUS_ID_CLGD5428)
				dev->ramdac.state = 0;
			return dev->ramdac.ctrl;
		} else {
			dev->ramdac.state++;
			if (dev->ramdac.state == 4)
				return dev->ramdac.ctrl;
			else
				return svga_in(addr, svga);
		}
		break;
	
	case 0x03c7: case 0x03c8:
		dev->ramdac.state = 0;
		return svga_in(addr, svga);
	break;
	
	case 0x03c9:
		dev->ramdac.state = 0;
		svga->dac_status = 3;
		indx = (svga->dac_addr - 1) & 0xff;
		if (svga->seqregs[0x12] & 2)
			indx &= 0x0f;

		switch (svga->dac_pos) {
			case 0:
				svga->dac_pos++;
				if (svga->seqregs[0x12] & 2)
					return dev->extpal[indx].r & 0x3f;
				return svga->vgapal[indx].r & 0x3f;

			case 1: 
				svga->dac_pos++;
				if (svga->seqregs[0x12] & 2)
					return dev->extpal[indx].g & 0x3f;
				return svga->vgapal[indx].g & 0x3f;

                        case 2: 
				svga->dac_pos = 0;
				svga->dac_addr = (svga->dac_addr + 1) & 0xff;
				if (svga->seqregs[0x12] & 2)
                        		return dev->extpal[indx].b & 0x3f;
                        	return svga->vgapal[indx].b & 0x3f;
                }
                return 0xff;
	
	case 0x03ca:
		return dev->fc;		//FIXME: move to SVGA? --FvK
		
	case 0x03ce:
		return svga->gdcaddr & 0x3f;

	case 0x03cf:
		if ((svga->gdcaddr > 8) && is_locked(svga))
			return 0xff;
		if (svga->gdcaddr >= 0x10) {
			if ((svga->gdcaddr > 0x1f) && !is_5426(svga))
				return 0xff;
			switch (svga->gdcaddr) {
				case 0x10:
					temp = gd543x_mmio_read(0xb8001, dev);
					break;
					
				case 0x11:
					temp = gd543x_mmio_read(0xb8005, dev);
					break;
					
				case 0x12:
					temp = gd543x_mmio_read(0xb8002, dev);
					break;
					
				case 0x13:
					temp = gd543x_mmio_read(0xb8006, dev);
					break;
					
				case 0x14:
					temp = gd543x_mmio_read(0xb8003, dev);
					break;
					
				case 0x15:
					temp = gd543x_mmio_read(0xb8007, dev);
					break;
					
				case 0x20:
					temp = gd543x_mmio_read(0xb8008, dev);
					break;
					
				case 0x21:
					temp = gd543x_mmio_read(0xb8009, dev);
					break;
					
				case 0x22:
					temp = gd543x_mmio_read(0xb800a, dev);
					break;
					
				case 0x23:
					temp = gd543x_mmio_read(0xb800b, dev);
					break;
					
				case 0x24:
					temp = gd543x_mmio_read(0xB800c, dev);
					break;
					
				case 0x25:
					temp = gd543x_mmio_read(0xb800d, dev);
					break;
					
				case 0x26:
					temp = gd543x_mmio_read(0xb800e, dev);
					break;
					
				case 0x27:
					temp = gd543x_mmio_read(0xb800f, dev);
					break;
					
				case 0x28:
					temp = gd543x_mmio_read(0xb8010, dev);
					break;
					
				case 0x29:
					temp = gd543x_mmio_read(0xb8011, dev);
					break;
					
				case 0x2a:
					temp = gd543x_mmio_read(0xb8012, dev);
					break;
					
				case 0x2c:
					temp = gd543x_mmio_read(0xb8014, dev);
					break;
					
				case 0x2d:
					temp = gd543x_mmio_read(0xb8015, dev);
					break;
					
				case 0x2e:
					temp = gd543x_mmio_read(0xb8016, dev);
					break;
					
				case 0x2f:
					temp = gd543x_mmio_read(0xb8017, dev);
					break;
					
				case 0x30:
					temp = gd543x_mmio_read(0xb8018, dev);
					break;
					
				case 0x32:
					temp = gd543x_mmio_read(0xb801a, dev);
					break;
					
				case 0x33:
					temp = gd543x_mmio_read(0xb801b, dev);
					break;
					
				case 0x31:
					temp = gd543x_mmio_read(0xb8040, dev);
					break;
					
				case 0x34:
					temp = gd543x_mmio_read(0xb801c, dev);
					break;
					
				case 0x35:
					temp = gd543x_mmio_read(0xb801d, dev);
					break;
					
				case 0x38:
					temp = gd543x_mmio_read(0xb8020, dev);
					break;
					
				case 0x39:
					temp = gd543x_mmio_read(0xb8021, dev);
					break;
					
				default:
					temp = 0xff;
					break;
			}
		} else {
			if ((svga->gdcaddr < 2) && is_locked(svga))
				temp = (svga->gdcreg[svga->gdcaddr] & 0x0f);
			else
				temp = svga->gdcreg[svga->gdcaddr];
		}
		return temp;

	case 0x03d4:
		return svga->crtcreg;

	case 0x03d5:
		if (((svga->crtcreg == 0x19) || (svga->crtcreg == 0x1a) ||
		     (svga->crtcreg == 0x1b) || (svga->crtcreg == 0x1d) ||
		     (svga->crtcreg == 0x25) || (svga->crtcreg == 0x27)) &&
		    is_locked(svga))
			return 0xff;
		switch (svga->crtcreg) {
		
			case 0x22: /*Graphis Data Latches Readback Register*/
				/*Should this be & 7 if 8 byte latch is enabled? */
				return (svga->latch.b[svga->gdcreg[4] & 3]);
			case 0x24: /*Attribute controller toggle readback (R)*/
				return svga->attrff << 7;

			case 0x26: /*Attribute controller index readback (R)*/
				return svga->attraddr & 0x3f;	

			case 0x27: /*ID*/
				return svga->crtc[0x27]; /*GD542x/GD543x*/

			case 0x28: /*Class ID*/
				if ((svga->crtc[0x27] == CIRRUS_ID_CLGD5430) || (svga->crtc[0x27] == CIRRUS_ID_CLGD5440))
					return 0xff; /*Standard CL-GD5430/40*/
		}
		return svga->crtc[svga->crtcreg];
    }

    return svga_in(addr, svga);
}

static void
hwcursor_draw(svga_t *svga, int displine)
{
    gd54xx_t *dev = (gd54xx_t *)svga->p;	
    int x, xx, comb, b0, b1;
    uint8_t dat[2];
    int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
    int y_add, x_add;
    int pitch = (svga->hwcursor.xsize == 64) ? 16 : 4;
    uint32_t bgcol = dev->extpallook[0x00];
    uint32_t fgcol = dev->extpallook[0x0f];

    y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
    x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;
    
    if (svga->interlace && svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;

    for (x = 0; x < svga->hwcursor.xsize; x += 8) {
	dat[0] = svga->vram[svga->hwcursor_latch.addr];
	if (svga->hwcursor.xsize == 64)
		dat[1] = svga->vram[svga->hwcursor_latch.addr + 0x08];
	else
		dat[1] = svga->vram[svga->hwcursor_latch.addr + 0x80];
	for (xx = 0; xx < 8; xx++) {
		b0 = (dat[0] >> (7 - xx)) & 1;
		b1 = (dat[1] >> (7 - xx)) & 1;
		comb = (b1 | (b0 << 1));

		if (offset >= svga->hwcursor_latch.x) {
			switch(comb) {
				case 0:
					/* The original screen pixel is shown (invisible cursor) */
					break;

				case 1:
					/* The pixel is shown in the cursor background color */
					screen->line[displine + y_add][offset + 32 + x_add].val = bgcol;
					break;

				case 2:
					/* The pixel is shown as the inverse of the original screen pixel
					   (XOR cursor) */
					screen->line[displine + y_add][offset + 32 + x_add].val ^= 0xffffff;
					break;

				case 3:
					/* The pixel is shown in the cursor foreground color */
					screen->line[displine + y_add][offset + 32 + x_add].val = fgcol;
					break;
			}
		}
		   
		offset++;
	}

	svga->hwcursor_latch.addr++;
    }

    if (svga->hwcursor.xsize == 64)
	svga->hwcursor_latch.addr += 8;

    if (svga->interlace && !svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;
}


static void
gd54xx_rop(gd54xx_t *dev, uint8_t *res, uint8_t *dst, const uint8_t *src)
{
    switch (dev->blt.rop) {
	case 0x00: *res =   0x00;          break;
	case 0x05: *res =   *src &  *dst;  break;
	case 0x06: *res =   *dst;          break;
	case 0x09: *res =   *src & ~*dst;  break;
	case 0x0b: *res =  ~*dst;          break;
	case 0x0d: *res =   *src;          break;
	case 0x0e: *res =   0xff;          break;
	case 0x50: *res =  ~*src &  *dst;  break;
	case 0x59: *res =   *src ^  *dst;  break;
	case 0x6d: *res =   *src |  *dst;  break;
	case 0x90: *res = ~(*src |  *dst); break;
	case 0x95: *res = ~(*src ^  *dst); break;
	case 0xad: *res =   *src | ~*dst;  break;
	case 0xd0: *res =  ~*src;          break;
	case 0xd6: *res =  ~*src |  *dst;  break;
	case 0xda: *res = ~(*src &  *dst); break;
    }
}

static uint8_t
mem_sys_dest_read(gd54xx_t *dev)
{
    uint8_t ret = 0xff;

    if (dev->blt.msd_buf_cnt != 0) {
	ret = dev->blt.msd_buf[dev->blt.msd_buf_pos++];
	dev->blt.msd_buf_cnt--;

	if (dev->blt.msd_buf_cnt == 0) {
		if (dev->countminusone == 1) {
			dev->blt.msd_buf_pos = 0;
			if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) &&
			    !(dev->blt.modeext & CIRRUS_BLTMODEEXT_DWORDGRANULARITY))
				start_blit(0xff, 8, dev, &dev->svga);
			else
				start_blit(0xffffffff, 32, dev, &dev->svga);
		} else
			reset_blit(dev);	/* End of blit, do no more. */
	}
    }

    return ret;
}

static void
mem_sys_src_write(gd54xx_t *dev, uint8_t val)
{
   int i;

    dev->blt.sys_src32 &= ~(0xff << (dev->blt.sys_cnt << 3));
    dev->blt.sys_src32 |= (val << (dev->blt.sys_cnt << 3));
    dev->blt.sys_cnt = (dev->blt.sys_cnt + 1) & 3;

    if (dev->blt.sys_cnt == 0) {
	if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) &&
	    !(dev->blt.modeext & CIRRUS_BLTMODEEXT_DWORDGRANULARITY)) {
		for (i = 0; i < 32; i += 8)
			start_blit((dev->blt.sys_src32 >> i) & 0xff, 8, dev, &dev->svga);
	} else
		start_blit(dev->blt.sys_src32, 32, dev, &dev->svga);
       }
}


static void
gd54xx_write(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;	

     if (dev->countminusone && !dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED)) {
        mem_sys_src_write(dev, val);
	return;
    }

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_write(addr, val, svga);
	return;
    }

    addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    svga_write_linear(addr, val, svga);
}


static void 
gd54xx_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
	
     if (dev->countminusone && !dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED)) {
        gd54xx_write(addr, val, dev);
	gd54xx_write(addr + 1, val >> 8, dev);
	return;
    }

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_writew(addr, val, svga);
	return;
    }

    addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    if (svga->writemode < 4)	
    	svga_writew_linear(addr, val, svga);
    else {
	svga_write_linear(addr, val, svga);
	svga_write_linear(addr + 1, val >> 8, svga);
    }
}


static void
gd54xx_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

     if (dev->countminusone && !dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	gd54xx_write(addr, val, dev);
	gd54xx_write(addr + 1, val >> 8, dev);
	gd54xx_write(addr + 2, val >> 16, dev);
	gd54xx_write(addr + 3, val >> 24, dev);
	return;
    }
    
    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_writel(addr, val, svga);
	return;
    }

     addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    if (svga->writemode < 4)
	svga_writel_linear(addr, val, svga);
    else {
	svga_write_linear(addr, val, svga);
	svga_write_linear(addr+1, val >> 8, svga);
	svga_write_linear(addr+2, val >> 16, svga);
	svga_write_linear(addr+3, val >> 24, svga);
    }
}


/* This adds write modes 4 and 5 to SVGA. */
static void
gd54xx_write_modes45(svga_t *svga, uint8_t val, uint32_t addr)
{
    uint32_t i, j;

    switch (svga->writemode) {
	case 4:
		if (svga->gdcreg[0xb] & 0x10) {
			addr <<= 2;

			for (i = 0; i < 8; i++) {
				if (val & svga->seqregs[2] & (0x80 >> i)) {
					svga->vram[addr + (i << 1)] = svga->gdcreg[1];
					svga->vram[addr + (i << 1) + 1] = svga->gdcreg[0x11];
				}
			}
		} else {
			addr <<= 1;

			for (i = 0; i < 8; i++) {
				if (val & svga->seqregs[2] & (0x80 >> i))
					svga->vram[addr + i] = svga->gdcreg[1];
			}
		}
		break;

	case 5:
		if (svga->gdcreg[0xb] & 0x10) {
			addr <<= 2;

			for (i = 0; i < 8; i++) {
				j = (0x80 >> i);
				if (svga->seqregs[2] & j) {
					svga->vram[addr + (i << 1)] = (val & j) ?
								      svga->gdcreg[1] : svga->gdcreg[0];
					svga->vram[addr + (i << 1) + 1] = (val & j) ?
									  svga->gdcreg[0x11] : svga->gdcreg[0x10];
				}
			}
		} else {
			addr <<= 1;

			for (i = 0; i < 8; i++) {
				j = (0x80 >> i);
				if (svga->seqregs[2] & j)
					svga->vram[addr + i] = (val & j) ? svga->gdcreg[1] : svga->gdcreg[0];
			}
		}
		break;
    }

    svga->changedvram[addr >> 12] = changeframecount;
}


static uint8_t
gd54xx_get_aperture(uint32_t addr)
{
    uint32_t ap = addr >> 22;
    return (uint8_t) (ap & 0x03);
}

static int
gd54xx_aperture2_enabled(gd54xx_t *dev)
{
    svga_t *svga = &dev->svga;

    if (svga->crtc[0x27] < CIRRUS_ID_CLGD5436)
	return 0;

    if (!(dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND))
	return 0;

    if (!(dev->blt.status & CIRRUS_BLT_APERTURE2))
	return 0;

    return 1;
}

static uint8_t
gd54xx_readb_linear(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    
    uint8_t ap = gd54xx_get_aperture(addr);
    addr &= 0x003fffff;	/* 4 MB mask */

    if(!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
       return svga_read_linear(addr, svga);
       
    if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
        if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR))
		return gd543x_mmio_read(addr & 0x000000ff, dev);
    }
    
     /* Do mem sys dest reads here if the blitter is neither paused, nor is there a second aperture. */
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED))
	return mem_sys_dest_read(dev);
    
    switch (ap) {
	case 0:
	default:		
		break;

	case 1:
		/* 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2 */
		addr ^= 0x00000001;
		break;

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000003;
		break;

	case 3:
		return 0xff;
    }

    return svga_read_linear(addr, svga);
}


static uint16_t
gd54xx_readw_linear(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint16_t temp;

    addr &= 0x003fffff;	/* 4 MB mask */

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
        return svga_readw_linear(addr, svga);
	
    if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
        if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		temp = gd543x_mmio_readw(addr & 0x000000ff, dev);
	return temp;
	}
    }
    
     /* Do mem sys dest reads here if the blitter is neither paused, nor is there a second aperture. */
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	temp = gd54xx_readb_linear(addr, priv);
	temp |= gd54xx_readb_linear(addr + 1, priv) << 8;
	return temp;
    }

    
    switch (ap) {
	case 0:
	default:		
		return svga_readw_linear(addr, svga);

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000002;

	case 1:
		temp = svga_readb_linear(addr + 1, svga);
		temp |= (svga_readb_linear(addr, svga) << 8);

		if (svga->fast)
		        cycles -= video_timing_read_w;

		return temp;

	case 3:
		return 0xffff;
    }
}


static uint32_t
gd54xx_readl_linear(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint32_t temp;

    addr &= 0x003fffff;	/* 4 MB mask */

	if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
	    return svga_readl_linear(addr, svga);
	    
	if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
	    if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		temp = gd543x_mmio_readl(addr & 0x000000ff, dev);
		return temp;
	    }
        }
	
  /* Do mem sys dest reads here if the blitter is neither paused, nor is there a second aperture. */
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	temp = gd54xx_readb_linear(addr, priv);
	temp |= gd54xx_readb_linear(addr + 1, priv) << 8;
	temp |= gd54xx_readb_linear(addr + 2, priv) << 16;
	temp |= gd54xx_readb_linear(addr + 3, priv) << 24;
	return temp;
    }

    switch (ap) {
	case 0:
	default:		
		return svga_readl_linear(addr, svga);

	case 1:
		temp = svga_readb_linear(addr + 1, svga);
		temp |= (svga_readb_linear(addr, svga) << 8);
		temp |= (svga_readb_linear(addr + 3, svga) << 16);
		temp |= (svga_readb_linear(addr + 2, svga) << 24);

		if (svga->fast)
		        cycles -= video_timing_read_l;

		return temp;

	case 2:
		temp = svga_readb_linear(addr + 3, svga);
		temp |= (svga_readb_linear(addr + 2, svga) << 8);
		temp |= (svga_readb_linear(addr + 1, svga) << 16);
		temp |= (svga_readb_linear(addr, svga) << 24);

		if (svga->fast)
		        cycles -= video_timing_read_l;

		return temp;

	case 3:
		return 0xffffffff;
    }
}

static uint8_t
gd5436_aperture2_readb(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if (dev->countminusone && dev->blt.ms_is_dest &&
	gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED))
	return mem_sys_dest_read(dev);
    return 0xff;
}


static uint16_t
gd5436_aperture2_readw(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    uint16_t ret = 0xffff;

    if (dev->countminusone && dev->blt.ms_is_dest &&
	gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd5436_aperture2_readb(addr, priv);
	ret |= gd5436_aperture2_readb(addr + 1, priv) << 8;
	return ret;
    }

    return ret;
}


static uint32_t
gd5436_aperture2_readl(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    uint32_t ret = 0xffffffff;

    if (dev->countminusone && dev->blt.ms_is_dest &&
	gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd5436_aperture2_readb(addr, priv);
	ret |= gd5436_aperture2_readb(addr + 1, priv) << 8;
	ret |= gd5436_aperture2_readb(addr + 2, priv) << 16;
	ret |= gd5436_aperture2_readb(addr + 3, priv) << 24;
	return ret;
    }

    return ret;
}

static void
gd5436_aperture2_writeb(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if (dev->countminusone && !dev->blt.ms_is_dest
	&& gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED))
	mem_sys_src_write(dev, val);
}


static void
gd5436_aperture2_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if (dev->countminusone && !dev->blt.ms_is_dest
	&& gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	gd5436_aperture2_writeb(addr, val, dev);
	gd5436_aperture2_writeb(addr + 1, val >> 8, dev);
    }
}


static void
gd5436_aperture2_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if (dev->countminusone && !dev->blt.ms_is_dest
	&& gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	gd5436_aperture2_writeb(addr, val, dev);
	gd5436_aperture2_writeb(addr + 1, val >> 8, dev);
	gd5436_aperture2_writeb(addr + 2, val >> 16, dev);
	gd5436_aperture2_writeb(addr + 3, val >> 24, dev);
    }
}

static void
gd54xx_writeb_linear(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    uint8_t ap = gd54xx_get_aperture(addr);
    
    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_write_linear(addr, val, svga);
	return;
    }
    
    addr &= 0x003fffff;	/* 4 MB mask */
    
    if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		gd543x_mmio_write(addr & 0x000000ff, val, dev);
		return;
	}
    }

    /* Do mem sys src writes here if the blitter is neither paused, nor there is a second aperture. */
    if (dev->countminusone && !dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	mem_sys_src_write(dev, val);
	return;
    }

    switch (ap) {
	case 0:
	default:
		break;

	case 1:
		/* 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2 */
		addr ^= 0x00000001;
		break;

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000003;
		break;

	case 3:
		return;
    }

    svga_write_linear(addr, val, svga);
}


static void 
gd54xx_writew_linear(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    uint8_t ap = gd54xx_get_aperture(addr);

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_writew_linear(addr, val, svga);
	return;
    }

    addr &= 0x003fffff;	/* 4 MB mask */

    if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		gd543x_mmio_writew(addr & 0x000000ff, val, dev);
		return;
	}
    }

    /* Do mem sys src writes here if the blitter is neither paused, nor there is a second aperture. */
    if (dev->countminusone && !dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	gd54xx_writeb_linear(addr, val, dev);
	gd54xx_writeb_linear(addr + 1, val >> 8, dev);
	return;
    }
    
    if (svga->writemode < 4) {
	switch(ap) {
		case 0:
		default:
			svga_writew_linear(addr, val, svga);
			return;

		case 2:
			addr ^= 0x00000002;
			/*FALLTHROUGH*/
			return;

		case 1:
			svga_writeb_linear(addr + 1, val & 0xff, svga);
			svga_writeb_linear(addr, val >> 8, svga);

			if (svga->fast)
		        	cycles -= video_timing_write_w;
			return;

		case 3:
			return;
	}
    } else {
	switch(ap) {
		case 0:
		default:
			svga_write_linear(addr, val & 0xff, svga);
			svga_write_linear(addr + 1, val >> 8, svga);
			return;

		case 2:
			addr ^= 0x00000002;
			/*FALLTHROUGH*/

		case 1:
			svga_write_linear(addr + 1, val & 0xff, svga);
			svga_write_linear(addr, val >> 8, svga);
			return;

		case 3:
			return;
	}
    }
}


static void 
gd54xx_writel_linear(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    uint8_t ap = gd54xx_get_aperture(addr);


    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA)) {
	svga_writel_linear(addr, val, svga);
	return;
    }
    
    addr &= 0x003fffff;	/* 4 MB mask */

    if ((addr >= (svga->vram_max - 256)) && (addr < svga->vram_max)) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		gd543x_mmio_writel(addr & 0x000000ff, val, dev);
		return;
	}
    }

    /* Do mem sys src writes here if the blitter is neither paused, nor there is a second aperture. */
    if (dev->countminusone && !dev->blt.ms_is_dest &&
	!gd54xx_aperture2_enabled(dev) && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	gd54xx_writeb_linear(addr, val, dev);
	gd54xx_writeb_linear(addr + 1, val >> 8, dev);
	gd54xx_writeb_linear(addr + 2, val >> 16, dev);
	gd54xx_writeb_linear(addr + 3, val >> 24, dev);
	return;
    }
    
    if (svga->writemode < 4) {
	switch(ap) {
		case 0:
		default:
			svga_writel_linear(addr, val, svga);
			return;

		case 1:
			svga_writeb_linear(addr + 1, val & 0xff, svga);
			svga_writeb_linear(addr, val >> 8, svga);
			svga_writeb_linear(addr + 3, val >> 16, svga);
			svga_writeb_linear(addr + 2, val >> 24, svga);
			return;

		case 2:
			svga_writeb_linear(addr + 3, val & 0xff, svga);
			svga_writeb_linear(addr + 2, val >> 8, svga);
			svga_writeb_linear(addr + 1, val >> 16, svga);
			svga_writeb_linear(addr, val >> 24, svga);
			return;

		case 3:
			return;
	}
    } else {
	switch(ap) {
		case 0:
		default:
			svga_write_linear(addr, val & 0xff, svga);
			svga_write_linear(addr+1, val >> 8, svga);
			svga_write_linear(addr+2, val >> 16, svga);
			svga_write_linear(addr+3, val >> 24, svga);
			return;

		case 1:
			svga_write_linear(addr + 1, val & 0xff, svga);
			svga_write_linear(addr, val >> 8, svga);
			svga_write_linear(addr + 3, val >> 16, svga);
			svga_write_linear(addr + 2, val >> 24, svga);
			return;

		case 2:
			svga_write_linear(addr + 3, val & 0xff, svga);
			svga_write_linear(addr + 2, val >> 8, svga);
			svga_write_linear(addr + 1, val >> 16, svga);
			svga_write_linear(addr, val >> 24, svga);
			return;

		case 3:
			return;
	}
    }
}


static uint8_t
gd54xx_read(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
	return svga_read(addr, svga);
	
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED))
	return mem_sys_dest_read(dev);

    addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    return svga_read_linear(addr, svga);
}


static uint16_t
gd54xx_readw(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint16_t ret;

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
	return svga_readw(addr, svga);
	
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd54xx_read(addr, priv);
	ret |= gd54xx_read(addr + 1, priv) << 8;
	return ret;
    }

    addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    return svga_readw_linear(addr, svga);
}


static uint32_t
gd54xx_readl(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint32_t ret;

    if (!(svga->seqregs[0x07] & CIRRUS_SR7_BPP_SVGA))
	return svga_readl(addr, svga);
	
    if (dev->countminusone && dev->blt.ms_is_dest &&
	!(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd54xx_read(addr, priv);
	ret |= gd54xx_read(addr + 1, priv) << 8;
	ret |= gd54xx_read(addr + 2, priv) << 16;
	ret |= gd54xx_read(addr + 3, priv) << 24;
	return ret;
    }

    addr = (addr & 0x7fff) + svga->extra_banks[(addr >> 15) & 1];

    return svga_readl_linear(addr, svga);
}


static int
gd543x_do_mmio(svga_t *svga, uint32_t addr)
{
    if (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)
	return 1;

    return ((addr & ~0xff) == 0xb8000);
}


static void
gd543x_mmio_write(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    
    uint8_t old;
	
    if (gd543x_do_mmio(svga, addr)) {
	switch (addr & 0xff) {
		case 0x00:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xffffff00) | val;
			else
				dev->blt.bg_col = (dev->blt.bg_col & 0xff00) | val;
			break;

		case 0x01:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xffff00ff) | (val << 8);
			else
				dev->blt.bg_col = (dev->blt.bg_col & 0x00ff) | (val << 8);
			break;

		case 0x02:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xff00ffff) | (val << 16);
			break;

		case 0x03:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0x00ffffff) | (val << 24);
			break;

		case 0x04:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xffffff00) | val;
			else
				dev->blt.fg_col = (dev->blt.fg_col & 0xff00) | val;
			break;

		case 0x05:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xffff00ff) | (val << 8);
			else
				dev->blt.fg_col = (dev->blt.fg_col & 0x00ff) | (val << 8);
			break;

		case 0x06:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xff00ffff) | (val << 16);
			break;

		case 0x07:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0x00ffffff) | (val << 24);
			break;

		case 0x08:
			dev->blt.width = (dev->blt.width & 0xff00) | val;
			break;

		case 0x09:
			dev->blt.width = (dev->blt.width & 0x00ff) | (val << 8);
			if (is_5434(svga))
				dev->blt.width &= 0x1fff;
			else
				dev->blt.width &= 0x07ff;
			break;

		case 0x0a:
			dev->blt.height = (dev->blt.height & 0xff00) | val;
			break;

		case 0x0b:
			dev->blt.height = (dev->blt.height & 0x00ff) | (val << 8);
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
				dev->blt.height &= 0x07ff;
			else
				dev->blt.height &= 0x03ff;
			break;

		case 0x0c:
			dev->blt.dst_pitch = (dev->blt.dst_pitch & 0xff00) | val;
			break;

		case 0x0d:
			dev->blt.dst_pitch = (dev->blt.dst_pitch & 0x00ff) | (val << 8);
			dev->blt.dst_pitch &= 0x1fff;
			break;

		case 0x0e:
			dev->blt.src_pitch = (dev->blt.src_pitch & 0xff00) | val;
			break;

		case 0x0f:
			dev->blt.src_pitch = (dev->blt.src_pitch & 0x00ff) | (val << 8);
			dev->blt.src_pitch &= 0x1fff;
			break;

		case 0x10:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0xffff00) | val;
			break;
			case 0x11:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0xff00ff) | (val << 8);
			break;

		case 0x12:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0x00ffff) | (val << 16);
			if (is_5434(svga))
				dev->blt.dst_addr &= 0x3fffff;
			else
				dev->blt.dst_addr &= 0x1fffff;

			if ((svga->crtc[0x27] >= CIRRUS_ID_CLGD5436) && (dev->blt.status & CIRRUS_BLT_AUTOSTART) &&
			    !(dev->blt.status & CIRRUS_BLT_BUSY)) {
				dev->blt.status |= CIRRUS_BLT_BUSY;
				start_blit(0, 0xffffffff, dev, svga);
			}
			break;

		case 0x14:
			dev->blt.src_addr = (dev->blt.src_addr & 0xffff00) | val;
			break;

		case 0x15:
			dev->blt.src_addr = (dev->blt.src_addr & 0xff00ff) | (val << 8);
			break;

		case 0x16:
			dev->blt.src_addr = (dev->blt.src_addr & 0x00ffff) | (val << 16);
			if (is_5434(svga))
				dev->blt.src_addr &= 0x3fffff;
			else
				dev->blt.src_addr &= 0x1fffff;
			break;

		case 0x17:
			dev->blt.mask = val;
			break;

		case 0x18:
			dev->blt.mode = val;
			recalc_mapping(dev);
			break;

		case 0x1a:
			dev->blt.rop = val;
			break;

		case 0x1b:
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)	
				dev->blt.modeext = val;
			break;		
		
		case 0x1c:
			dev->blt.trans_col = (dev->blt.trans_col & 0xff00) | val;
			break;
		
		case 0x1d:	
			dev->blt.trans_col = (dev->blt.trans_col & 0x00ff) | (val << 8);
			break;
		
		case 0x20:
			dev->blt.trans_mask = (dev->blt.trans_mask & 0xff00) | val;
			break;
		
		case 0x21:
			dev->blt.trans_mask = (dev->blt.trans_mask & 0x00ff) | (val << 8);
			break;
		
		case 0x40:
			old = dev->blt.status;
			dev->blt.status = val;
			recalc_mapping(dev);
			if (!(old & CIRRUS_BLT_RESET) && (dev->blt.status & CIRRUS_BLT_RESET))
				reset_blit(dev);
			else if (!(old & CIRRUS_BLT_START) && (dev->blt.status & CIRRUS_BLT_START)) {
				dev->blt.status |= CIRRUS_BLT_BUSY;
				start_blit(0, 0xffffffff, dev, svga);
			}
			break;
		}		
    } else if (dev->mmio_vram_overlap)
	gd54xx_write(addr, val, dev);
}

static void
gd543x_mmio_writeb(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (!gd543x_do_mmio(svga, addr) && !dev->blt.ms_is_dest &&
	dev->countminusone && !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	mem_sys_src_write(dev, val);
	return;
    }

    gd543x_mmio_write(addr, val, priv);
}

static void
gd543x_mmio_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr)) {
	gd543x_mmio_write(addr, val & 0xff, dev);
	gd543x_mmio_write(addr+1, val >> 8, dev);
    } else if (dev->mmio_vram_overlap) {
	if (dev->countminusone && !dev->blt.ms_is_dest &&
	    !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
		gd543x_mmio_write(addr, val & 0xff, dev);
		gd543x_mmio_write(addr + 1, val >> 8, dev);
	} else {
		gd54xx_write(addr, val, dev);
		gd54xx_write(addr + 1, val >> 8, dev);
	}
    }
}


static void
gd543x_mmio_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr)) {
	gd543x_mmio_write(addr, val & 0xff, dev);
	gd543x_mmio_write(addr+1, val >> 8, dev);
	gd543x_mmio_write(addr+2, val >> 16, dev);
	gd543x_mmio_write(addr+3, val >> 24, dev);
    } else if (dev->mmio_vram_overlap) {
        if (dev->countminusone && !dev->blt.ms_is_dest &&
	    !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	    gd543x_mmio_write(addr, val & 0xff, dev);
	    gd543x_mmio_write(addr+1, val >> 8, dev);
	    gd543x_mmio_write(addr+2, val >> 16, dev);
	    gd543x_mmio_write(addr+3, val >> 24, dev);
        } else {
	    gd54xx_write(addr, val, dev);
	    gd54xx_write(addr+1, val >> 8, dev);
	    gd54xx_write(addr+2, val >> 16, dev);
	    gd54xx_write(addr+3, val >> 24, dev);
        }
      }
}

static uint8_t
gd543x_mmio_read(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t ret = 0xff;

    if (gd543x_do_mmio(svga, addr)) {
	switch (addr & 0xff) {
		case 0x00:
			ret = dev->blt.bg_col & 0xff;
			break;
		
		case 0x01:
			ret = (dev->blt.bg_col >> 8) & 0xff;
			break;
		
		case 0x02:
			if (is_5434(svga))
				ret = (dev->blt.bg_col >> 16) & 0xff;
			break;
		
		case 0x03:
			if (is_5434(svga))
				ret = (dev->blt.bg_col >> 24) & 0xff;
			break;

		case 0x04:
			ret = dev->blt.fg_col & 0xff;
			break;
		
		case 0x05:
			ret = (dev->blt.fg_col >> 8) & 0xff;
			break;
		
		case 0x06:
			if (is_5434(svga))
				ret = (dev->blt.fg_col >> 16) & 0xff;
			break;
		
		case 0x07:
			if (is_5434(svga))
				ret = (dev->blt.fg_col >> 24) & 0xff;
			break;

		case 0x08:
			ret = dev->blt.width & 0xff;
			break;
		
		case 0x09:
			if (is_5434(svga))
				ret = (dev->blt.width >> 8) & 0x1f;
			else
				ret = (dev->blt.width >> 8) & 0x07;
			break;
		
		case 0x0a:
			ret = dev->blt.height & 0xff;
			break;
		
		case 0x0b:
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
				ret = (dev->blt.height >> 8) & 0x07;
			else
				ret = (dev->blt.height >> 8) & 0x03;
			break;
		
		case 0x0c:
			ret = dev->blt.dst_pitch & 0xff;
			break;
		
		case 0x0d:
			ret = (dev->blt.dst_pitch >> 8) & 0x1f;
			break;
		
		case 0x0e:
			ret = dev->blt.src_pitch & 0xff;
			break;
		
		case 0x0f:
			ret = (dev->blt.src_pitch >> 8) & 0x1f;
			break;

		case 0x10:
			ret = dev->blt.dst_addr & 0xff;
			break;
		
		case 0x11:
			ret = (dev->blt.dst_addr >> 8) & 0xff;
			break;
		
		case 0x12:
			if (is_5434(svga))
				ret = (dev->blt.dst_addr >> 16) & 0x3f;
			else
				ret = (dev->blt.dst_addr >> 16) & 0x1f;
			break;

		case 0x14:
			ret = dev->blt.src_addr & 0xff;
			break;
		
		case 0x15:
			ret = (dev->blt.src_addr >> 8) & 0xff;
			break;
		
		case 0x16:
			if (is_5434(svga))
				ret = (dev->blt.src_addr >> 16) & 0x3f;
			else
				ret = (dev->blt.src_addr >> 16) & 0x1f;
			break;

		case 0x17:
			ret = dev->blt.mask;
			break;
		
		case 0x18:
			ret = dev->blt.mode;
			break;

		case 0x1a:
			ret = dev->blt.rop;
			break;

		case 0x1b:
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)	
				ret = dev->blt.modeext;
			break;

		case 0x1c:
			ret = dev->blt.trans_col & 0xff;
			break;
		
		case 0x1d:	
			ret = (dev->blt.trans_col >> 8) & 0xff;
			break;

		case 0x20:
			ret = dev->blt.trans_mask & 0xff;
			break;
		
		case 0x21:	
			ret = (dev->blt.trans_mask >> 8) & 0xff;
			break;

		case 0x40:
			ret = dev->blt.status;
			break;
	}
    }
    else if (dev->mmio_vram_overlap)
	ret = gd54xx_read(addr, dev);
    else if (dev->countminusone && dev->blt.ms_is_dest &&
	     !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	     ret = mem_sys_dest_read(dev);
    }

    return ret;
}


static uint16_t
gd543x_mmio_readw(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint16_t ret = 0xffff;

    if (gd543x_do_mmio(svga, addr))
	ret = gd543x_mmio_read(addr, dev) | (gd543x_mmio_read(addr+1, dev) << 8);
    else if (dev->mmio_vram_overlap)
	ret = gd54xx_read(addr, dev) | (gd54xx_read(addr+1, dev) << 8);
    else if (dev->countminusone && dev->blt.ms_is_dest &&
	     !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd543x_mmio_read(addr, priv);
	ret |= gd543x_mmio_read(addr + 1, priv) << 8;
	return ret;
    }

    return ret;
}


static uint32_t
gd543x_mmio_readl(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint32_t ret = 0xffffffff;

    if (gd543x_do_mmio(svga, addr))
	ret = gd543x_mmio_read(addr, dev) | (gd543x_mmio_read(addr+1, dev) << 8) | (gd543x_mmio_read(addr+2, dev) << 16) | (gd543x_mmio_read(addr+3, dev) << 24);
    else if (dev->mmio_vram_overlap)
	ret = gd54xx_read(addr, dev) | (gd54xx_read(addr+1, dev) << 8) | (gd54xx_read(addr+2, dev) << 16) | (gd54xx_read(addr+3, dev) << 24);
    else if (dev->countminusone && dev->blt.ms_is_dest &&
	     !(dev->blt.status & CIRRUS_BLT_PAUSED)) {
	ret = gd543x_mmio_read(addr, priv);
	ret |= gd543x_mmio_read(addr + 1, priv) << 8;
	ret |= gd543x_mmio_read(addr + 2, priv) << 16;
	ret |= gd543x_mmio_read(addr + 3, priv) << 24;
	return ret;
    }

    return ret;
}


static uint8_t
gd54xx_color_expand(gd54xx_t *dev, int mask, int shift)
{
    uint8_t ret;

    if (dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP)
	ret = dev->blt.fg_col >> (shift << 3);
    else
	ret = mask ? (dev->blt.fg_col >> (shift << 3))
		   : (dev->blt.bg_col >> (shift << 3));

    return ret;
}

static int
get_pixel_width(gd54xx_t *dev)
{
    int ret = 1;

    switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
	case CIRRUS_BLTMODE_PIXELWIDTH8:
		ret = 1;
		break;
	case CIRRUS_BLTMODE_PIXELWIDTH16:
		ret = 2;
		break;
	case CIRRUS_BLTMODE_PIXELWIDTH24:
		ret = 3;
		break;
	case CIRRUS_BLTMODE_PIXELWIDTH32:
		ret = 4;
		break;
    }

    return ret;
}

static void
gd54xx_blit(gd54xx_t *dev, uint8_t mask, uint8_t *dst, uint8_t target, int skip)
{
    int is_transp, is_bgonly;

    /* skip indicates whether or not it is a pixel to be skipped (used for left skip);
       mask indicates transparency or not (only when transparent comparison is enabled):
	color expand: direct pattern bit; 1 = write, 0 = do not write
		      (the other way around in inverse mode);
	normal 8-bpp or 16-bpp: does not match transparent color = write,
				matches transparent color = do not write */

    /* Make sure to always ignore transparency and skip in case of mem sys dest. */
    is_transp = (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSDEST) ? 0 : (dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP);
    is_bgonly = (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSDEST) ? 0 : (dev->blt.modeext & CIRRUS_BLTMODEEXT_BACKGROUNDONLY);
    skip = (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSDEST) ? 0 : skip;

    if (is_transp) {
	if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) &&
	    (dev->blt.modeext & CIRRUS_BLTMODEEXT_COLOREXPINV))
		mask = !mask;

	/* If mask is 1 and it is not a pixel to be skipped, write it. */
	if (mask && !skip)
		*dst = target;
    } else if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) && is_bgonly) {
	/* If mask is 1 or it is not a pixel to be skipped, write it.
	   (Skip only background pixels.) */
	if (mask || !skip)
		*dst = target;
    } else {
	/* If if it is not a pixel to be skipped, write it. */
	if (!skip)
		*dst = target;
    }
}




static int
gd54xx_transparent_comp(gd54xx_t *dev, uint32_t xx, uint8_t src)
{
    svga_t *svga = &dev->svga;
    int ret = 1;

    if ((dev->blt.pixel_width <= 2) && has_transp(svga, 0)) {
	ret = src ^ ((uint8_t *) &(dev->blt.trans_col))[xx];
	if (has_transp(svga, 1))
		ret &= ~(((uint8_t *) &(dev->blt.trans_mask))[xx]);
	ret = !ret;
    }


    return ret;
}


static void
gd54xx_pattern_copy(gd54xx_t *dev)
{
    uint8_t target, src, *dst;
    int x, y, pattern_y, pattern_pitch;
    uint32_t bitmask = 0, xx, pixel;
    uint32_t srca, srca2, dsta;
    svga_t *svga = &dev->svga;

    pattern_pitch = dev->blt.pixel_width << 3;

    if (dev->blt.pixel_width == 3)
	pattern_pitch = 32;

    if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
	pattern_pitch = 1;

    dsta = dev->blt.dst_addr & svga->vram_mask;
    /* The vertical offset is in the three low-order bits of the Source Address register. */
    pattern_y = dev->blt.src_addr & 0x07;

    /* Mode		Pattern bytes	Pattern line bytes
       ---------------------------------------------------
       Color Expansion	8		1
       8-bpp		64		8
       16-bpp		128		16
       24-bpp		256		32
       32-bpp		256		32
     */

    /* The boundary has to be equal to the size of the pattern. */
    srca = (dev->blt.src_addr & ~0x07) & svga->vram_mask;

    for (y = 0; y <= dev->blt.height; y++) {
	/* Go to the correct pattern line. */
	srca2 = srca + (pattern_y * pattern_pitch);
	pixel = 0;
	for (x = 0; x <= dev->blt.width; x += dev->blt.pixel_width) {
		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {
			if (dev->blt.modeext & CIRRUS_BLTMODEEXT_SOLIDFILL)
				bitmask = 1;
			else
				bitmask = svga->vram[srca2 & svga->vram_mask] & (0x80 >> pixel);
		}
		for (xx = 0; xx < dev->blt.pixel_width; xx++) {
			if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
				src = gd54xx_color_expand(dev, bitmask, xx);
			else {
				src = svga->vram[(srca2 + (x % (dev->blt.pixel_width << 3)) + xx) & svga->vram_mask];
				bitmask = gd54xx_transparent_comp(dev, xx, src);
			}
			dst = &(svga->vram[(dsta + x + xx) & svga->vram_mask]);
			target = *dst;
			gd54xx_rop(dev, &target, &target, &src);
			if (dev->blt.pixel_width == 3)
				gd54xx_blit(dev, bitmask, dst, target, ((x + xx) < dev->blt.pattern_x));
			else
				gd54xx_blit(dev, bitmask, dst, target, (x < dev->blt.pattern_x));
		}
		pixel = (pixel + 1) & 7;
		svga->changedvram[((dsta + x) & svga->vram_mask) >> 12] = changeframecount;
	}
	pattern_y = (pattern_y + 1) & 7;
	dsta += dev->blt.dst_pitch;
    }
}



static void
reset_blit(gd54xx_t *dev)
{
    dev->countminusone = 0;
    dev->blt.status &= ~(CIRRUS_BLT_START|CIRRUS_BLT_BUSY|CIRRUS_BLT_FIFOUSED);
}

/* Each blit is either 1 byte -> 1 byte (non-color expand blit)
   or 1 byte -> 8/16/24/32 bytes (color expand blit). */
static void
mem_sys_src(gd54xx_t *dev, uint32_t cpu_dat, uint32_t count)
{
    uint8_t *dst, exp, target;
    int mask_shift;
    uint32_t byte_pos, bitmask = 0;
    svga_t *svga = &dev->svga;

    dev->blt.ms_is_dest = 0;

    if (dev->blt.mode & (CIRRUS_BLTMODE_MEMSYSDEST | CIRRUS_BLTMODE_PATTERNCOPY))
	reset_blit(dev);
    else if (count == 0xffffffff) {
	dev->blt.dst_addr_backup = dev->blt.dst_addr;
	dev->blt.src_addr_backup = dev->blt.src_addr;
	dev->blt.x_count = dev->blt.xx_count = 0;
	dev->blt.y_count = 0;
	dev->countminusone = 1;
	dev->blt.sys_src32 = 0x00000000;
	dev->blt.sys_cnt = 0;
	return;
    } else if (dev->countminusone) {
	if (!(dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) || (dev->blt.modeext & CIRRUS_BLTMODEEXT_DWORDGRANULARITY)) {
		if (!dev->blt.xx_count && !dev->blt.x_count)
			byte_pos = (((dev->blt.mask >> 5) & 3) << 3);
		else
			byte_pos = 0;
		mask_shift = 31 - byte_pos;
		if (!(dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND))
			cpu_dat >>= byte_pos;
	} else
		mask_shift = 7;

	while (mask_shift > -1) {
		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {
			bitmask = (cpu_dat >> mask_shift) & 0x01;
			exp = gd54xx_color_expand(dev, bitmask, dev->blt.xx_count);
		} else {
			exp = cpu_dat & 0xff;
			bitmask = gd54xx_transparent_comp(dev, dev->blt.xx_count, exp);
		}

		dst = &(svga->vram[dev->blt.dst_addr_backup & svga->vram_mask]);
		target = *dst;
		gd54xx_rop(dev, &target, &target, &exp);
		if ((dev->blt.pixel_width == 3) && (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND))
			gd54xx_blit(dev, bitmask, dst, target, ((dev->blt.x_count + dev->blt.xx_count) < dev->blt.pattern_x));
		else
			gd54xx_blit(dev, bitmask, dst, target, (dev->blt.x_count < dev->blt.pattern_x));

		dev->blt.dst_addr_backup += dev->blt.dir;

		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
			dev->blt.xx_count = (dev->blt.xx_count + 1) % dev->blt.pixel_width;

		svga->changedvram[(dev->blt.dst_addr_backup & svga->vram_mask) >> 12] = changeframecount;

		if (!dev->blt.xx_count) {
			/* 1 mask bit = 1 blitted pixel */
			if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
				mask_shift--;
			else {
				cpu_dat >>= 8;
				mask_shift -= 8;
			}

			if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
				dev->blt.x_count = (dev->blt.x_count + dev->blt.pixel_width) % (dev->blt.width + 1);
			else
				dev->blt.x_count = (dev->blt.x_count + 1) % (dev->blt.width + 1);

			if (!dev->blt.x_count) {
				dev->blt.y_count = (dev->blt.y_count + 1) % (dev->blt.height + 1);
				if (dev->blt.y_count)
					dev->blt.dst_addr_backup = dev->blt.dst_addr + (dev->blt.dst_pitch * dev->blt.y_count * dev->blt.dir);
				else {
				    /* If we're here, the blit is over, reset. */
				    reset_blit(dev);
				}
				/* Stop blitting and request new data if end of line reached. */
				return;
			}
		}
	}
    }
}

static void
gd54xx_normal_blit(uint32_t count, gd54xx_t *dev, svga_t *svga)
{
    uint8_t src = 0, dst;
    uint16_t width = dev->blt.width;
    int x_max = 0, shift = 0, mask = 0;
    uint32_t src_addr = dev->blt.src_addr;
    uint32_t dst_addr = dev->blt.dst_addr;

    x_max = dev->blt.pixel_width << 3;

    dev->blt.dst_addr_backup = dev->blt.dst_addr;
    dev->blt.src_addr_backup = dev->blt.src_addr;
    dev->blt.height_internal = dev->blt.height;
    dev->blt.x_count = 0;
    dev->blt.y_count = 0;

    while (count) {
	src = 0;
	mask = 0;

	if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {
		mask = svga->vram[src_addr & svga->vram_mask] & (0x80 >> (dev->blt.x_count / dev->blt.pixel_width));
		shift = (dev->blt.x_count % dev->blt.pixel_width);
		src = gd54xx_color_expand(dev, mask, shift);
	} else {
		src = svga->vram[src_addr & svga->vram_mask];
		src_addr += dev->blt.dir;
		mask = 1;
	}
	count--;

	dst = svga->vram[dst_addr & svga->vram_mask];
	svga->changedvram[(dst_addr & svga->vram_mask) >> 12] = changeframecount;

	gd54xx_rop(dev, (uint8_t *) &dst, (uint8_t *) &dst, (const uint8_t *) &src);

	if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) && (dev->blt.modeext & CIRRUS_BLTMODEEXT_COLOREXPINV))
		mask = !mask;

	/* This handles 8bpp and 16bpp non-color-expanding transparent comparisons. */
	if ((dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP) && !(dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) &&
	    ((dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) <= CIRRUS_BLTMODE_PIXELWIDTH16) &&
	    (src != ((dev->blt.trans_mask >> (shift << 3)) & 0xff)))
		mask = 0;

	if (((dev->blt.width - width) >= dev->blt.pattern_x) &&
		!((dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP) && !mask)) {
		svga->vram[dst_addr & svga->vram_mask] = dst;
       }

	dst_addr += dev->blt.dir;
	dev->blt.x_count++;

	if (dev->blt.x_count == x_max) {
		dev->blt.x_count = 0;
		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND)
			src_addr++;
	}

	width--;
	if (width == 0xffff) {
		width = dev->blt.width;
		dst_addr = dev->blt.dst_addr_backup = dev->blt.dst_addr_backup + (dev->blt.dst_pitch * dev->blt.dir);
		dev->blt.y_count = (dev->blt.y_count + dev->blt.dir) & 7;

		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {
			if (dev->blt.x_count != 0)
				src_addr++;
		} else
			src_addr = dev->blt.src_addr_backup = dev->blt.src_addr_backup + (dev->blt.src_pitch * dev->blt.dir);

		dst_addr &= svga->vram_mask;
		dev->blt.dst_addr_backup &= svga->vram_mask;
		src_addr &= svga->vram_mask;
		dev->blt.src_addr_backup &= svga->vram_mask;

		dev->blt.x_count = 0;

		dev->blt.height_internal--;
		if (dev->blt.height_internal == 0xffff) {
			reset_blit(dev);
			return;
		}
	}
    }

    /* Count exhausted, stuff still left to blit. */
    reset_blit(dev);
}


static void
mem_sys_dest(uint32_t count, gd54xx_t *dev, svga_t *svga)
{
    dev->blt.ms_is_dest = 1;

    if (dev->blt.mode & CIRRUS_BLTMODE_PATTERNCOPY) {
	fatal("mem sys dest pattern copy not allowed (see 1994 manual)\n");
	reset_blit(dev);
    } else if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {
	fatal("mem sys dest color expand not allowed (see 1994 manual)\n");
	reset_blit(dev);
    } else {
	if (count == 0xffffffff) {
		dev->blt.dst_addr_backup = dev->blt.dst_addr;
		dev->blt.msd_buf_cnt = 0;
		dev->blt.src_addr_backup = dev->blt.src_addr;
		dev->blt.x_count = dev->blt.xx_count = 0;
		dev->blt.y_count = 0;
		dev->countminusone = 1;
		count = 32;
	}

	dev->blt.msd_buf_pos = 0;

	while (dev->blt.msd_buf_pos < 32) {
		dev->blt.msd_buf[dev->blt.msd_buf_pos & 0x1f] = svga->vram[dev->blt.src_addr_backup & svga->vram_mask];
		dev->blt.src_addr_backup += dev->blt.dir;
		dev->blt.msd_buf_pos++;

		dev->blt.x_count = (dev->blt.x_count + 1) % (dev->blt.width + 1);

		if (!dev->blt.x_count) {
			dev->blt.y_count = (dev->blt.y_count + 1) % (dev->blt.height + 1);

			if (dev->blt.y_count)
				dev->blt.src_addr_backup = dev->blt.src_addr + (dev->blt.src_pitch * dev->blt.y_count * dev->blt.dir);
			else
				dev->countminusone = 2;	/* Signal end of blit. */
			/* End of line reached, stop and notify regardless of how much we already transferred. */
			goto request_more_data;
		}
	}

	/* End of while. */
request_more_data:
	/* If the byte count we have blitted are not divisible by 4, round them up. */
	if (dev->blt.msd_buf_pos & 3)
		dev->blt.msd_buf_cnt = (dev->blt.msd_buf_pos & ~3) + 4;
	else
		dev->blt.msd_buf_cnt = dev->blt.msd_buf_pos;
	dev->blt.msd_buf_pos = 0;
	return;
    }
}

//
static void
start_blit(uint32_t cpu_dat, uint32_t count, gd54xx_t *dev, svga_t *svga)
{
    if ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) &&
	!(dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) &&
	!(dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP))
	dev->blt.dir = -1;
    else
	dev->blt.dir = 1;

    dev->blt.pixel_width = get_pixel_width(dev);

    if (dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) {
	if (dev->blt.pixel_width == 3)
		dev->blt.pattern_x = dev->blt.mask & 0x1f;				/* (Mask & 0x1f) bytes. */
	else
		dev->blt.pattern_x = (dev->blt.mask & 0x07) * dev->blt.pixel_width;	/* (Mask & 0x07) pixels. */
    } else
	dev->blt.pattern_x = 0;								/* No skip in normal blit mode. */

    if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSSRC)
	mem_sys_src(dev, cpu_dat, count);
    else if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSDEST)
	mem_sys_dest(count, dev, svga);
    else if (dev->blt.mode & CIRRUS_BLTMODE_PATTERNCOPY) {
	gd54xx_pattern_copy(dev);
	reset_blit(dev);
    } else
	gd54xx_normal_blit(count, dev, svga);
}

static uint8_t 
cl_pci_read(int func, int addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((addr >= 0x30) && (addr <= 0x33) && (!dev->has_bios))
	return 0;

    switch (addr) {
	case 0x00: return 0x13; /*Cirrus Logic*/
	case 0x01: return 0x10;

	case 0x02:
		return svga->crtc[0x27];
	case 0x03: return 0x00;
	
	case PCI_REG_COMMAND:
		return dev->pci_regs[PCI_REG_COMMAND]; /*Respond to IO and memory accesses*/

	case 0x07: return 0x02; /*Fast DEVSEL timing*/
        
	case 0x08: return dev->rev; /*Revision ID*/
	case 0x09: return 0x00; /*Programming interface*/
        
	case 0x0a: return 0x00; /*Supports VGA interface*/
	case 0x0b: return 0x03;

	case 0x10: return 0x08; /*Linear frame buffer address*/
	case 0x11: return 0x00;
	case 0x12: return 0x00;
	case 0x13: return dev->lfb_base >> 24;

	case 0x30: return (dev->pci_regs[0x30] & 0x01); /*BIOS ROM address*/
	case 0x31: return 0x00;
	case 0x32: return dev->pci_regs[0x32];
	case 0x33: return dev->pci_regs[0x33];

	case 0x3c: return dev->int_line;
	case 0x3d: return PCI_INTA;
    }

    return 0;
}


static void 
cl_pci_write(int func, int addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if ((addr >= 0x30) && (addr <= 0x33) && (!dev->has_bios))
	return;

    switch (addr) {
	case PCI_REG_COMMAND:
		dev->pci_regs[PCI_REG_COMMAND] = val & 0x23;
		io_removehandler(0x03c0, 0x0020, gd54xx_in, NULL, NULL, gd54xx_out, NULL, NULL, dev);
		if (val & PCI_COMMAND_IO)
			io_sethandler(0x03c0, 0x0020, gd54xx_in, NULL, NULL, gd54xx_out, NULL, NULL, dev);
		recalc_mapping(dev);
		break;

	case 0x13: 
		dev->lfb_base = val << 24;
		recalc_mapping(dev); 
		break;                

	case 0x30: case 0x32: case 0x33:
		dev->pci_regs[addr] = val;
		if (dev->pci_regs[0x30] & 0x01) {
			uint32_t addr = (dev->pci_regs[0x32] << 16) | (dev->pci_regs[0x33] << 24);
			mem_map_set_addr(&dev->bios_rom.mapping, addr, 0x8000);
		} else
			mem_map_disable(&dev->bios_rom.mapping);
		return;

	case 0x3c:
		dev->int_line = val;
		return;
    }
}


static priv_t
gd54xx_init(const device_t *info, UNUSED(void *parent))
{
    gd54xx_t *dev = (gd54xx_t *)mem_alloc(sizeof(gd54xx_t));
    svga_t *svga = &dev->svga;
    int id = info->local & 0xff;
    const wchar_t *romfn = info->path;
    int vram;

    memset(dev, 0x00, sizeof(gd54xx_t));
    dev->pci = !!(info->flags & DEVICE_PCI);
    dev->vlb = !!(info->flags & DEVICE_VLB);
    dev->rev = 0;
    dev->has_bios = 1;

    switch (id) {
	case CIRRUS_ID_CLGD5402:
	case CIRRUS_ID_CLGD5420:
		break;

	case CIRRUS_ID_CLGD5422:
	case CIRRUS_ID_CLGD5424:
		break;

	case CIRRUS_ID_CLGD5426:
		break;

	case CIRRUS_ID_CLGD5428:
		if (dev->vlb)
			romfn = BIOS_GD5428_VLB_PATH;
		else
			romfn = BIOS_GD5428_ISA_PATH;
		break;

	case CIRRUS_ID_CLGD5429:
		break;

	case CIRRUS_ID_CLGD5434:
		break;
		
	case CIRRUS_ID_CLGD5436:
		break;

	case CIRRUS_ID_CLGD5430:
		if (info->local & 0x400) {
			/* CL-GD 5440 */
			dev->rev = 0x47;
			if (info->local & 0x200) {
				romfn = NULL;
				dev->has_bios = 0;
			} else
				romfn = BIOS_GD5440_PATH;
		} else {
			/* CL-GD 5430 */
			if (dev->pci)
				romfn = BIOS_GD5430_PCI_PATH;
			else
				romfn = BIOS_GD5430_VLB_PATH;
		}
		break;

	case CIRRUS_ID_CLGD5446:
		if (info->local & 0x100)
			romfn = BIOS_GD5446_STB_PATH;
		else
			romfn = BIOS_GD5446_PATH;
		break;

	case CIRRUS_ID_CLGD5480:
		break;
    }

    if (id >= CIRRUS_ID_CLGD5420)
	vram = device_get_config_int("memory");
    else
	vram = 0;

    if (vram)
	dev->vram_size = vram << 20;
    else
	dev->vram_size = 1 << 19;
    dev->vram_mask = dev->vram_size - 1;

    if (vram <= 1)
	svga->decode_mask = dev->vram_mask;
    
    if (romfn)
	rom_init(&dev->bios_rom, romfn, 0xc0000, 0x8000, 0x7fff,
		 0, MEM_MAPPING_EXTERNAL);

    svga_init(&dev->svga, (priv_t)dev, dev->vram_size,
	      recalc_timings, gd54xx_in, gd54xx_out,
	      hwcursor_draw, NULL);
    svga->ven_write = gd54xx_write_modes45;

    mem_map_set_handler(&svga->mapping,
			gd54xx_read, gd54xx_readw, gd54xx_readl,
			gd54xx_write, gd54xx_writew, gd54xx_writel);
    mem_map_set_p(&svga->mapping, dev);

    mem_map_add(&dev->mmio_mapping, 0, 0,
		gd543x_mmio_read, gd543x_mmio_readw, gd543x_mmio_readl,
		gd543x_mmio_writeb, gd543x_mmio_writew, gd543x_mmio_writel,
		NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_map_disable(&dev->mmio_mapping);
    
    mem_map_add(&dev->linear_mapping, 0, 0,
		gd54xx_readb_linear, gd54xx_readw_linear, gd54xx_readl_linear,
		gd54xx_writeb_linear, gd54xx_writew_linear, gd54xx_writel_linear,
		NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_map_disable(&dev->linear_mapping);
   
    mem_map_add(&dev->aperture2_mapping, 0, 0,
		    gd5436_aperture2_readb, gd5436_aperture2_readw, gd5436_aperture2_readl,
		    gd5436_aperture2_writeb, gd5436_aperture2_writew, gd5436_aperture2_writel,
		    NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_map_disable(&dev->aperture2_mapping);
    
    io_sethandler(0x03c0, 32,
		  gd54xx_in,NULL,NULL, gd54xx_out,NULL,NULL, (priv_t)dev);

    svga->hwcursor.yoff = 32;
    svga->hwcursor.xoff = 0;

    if (id >= CIRRUS_ID_CLGD5420) {
	dev->vclk_n[0] = 0x4a;
	dev->vclk_d[0] = 0x2b;
	dev->vclk_n[1] = 0x5b;
	dev->vclk_d[1] = 0x2f;
	dev->vclk_n[2] = 0x45;
	dev->vclk_d[2] = 0x30;
	dev->vclk_n[3] = 0x7e;
	dev->vclk_d[3] = 0x33;
    } else {
	dev->vclk_n[0] = 0x66;
	dev->vclk_d[0] = 0x3b;
	dev->vclk_n[1] = 0x5b;
	dev->vclk_d[1] = 0x2f;
	dev->vclk_n[1] = 0x45;
	dev->vclk_d[1] = 0x2c;
	dev->vclk_n[1] = 0x7e;
	dev->vclk_d[1] = 0x33;
    }

    svga->extra_banks[1] = 0x8000;

    if (dev->pci && id >= CIRRUS_ID_CLGD5430)
	pci_add_card(PCI_ADD_VIDEO, cl_pci_read, cl_pci_write, dev);

    dev->pci_regs[PCI_REG_COMMAND] = 7;

    dev->pci_regs[0x30] = 0x00;
    dev->pci_regs[0x32] = 0x0c;
    dev->pci_regs[0x33] = 0x00;
	
    svga->crtc[0x27] = id;
    
    svga->seqregs[0x06] = 0x0f;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


static void
gd54xx_close(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
	
    svga_close(&dev->svga);
    
    free(dev);
}


static void
gd54xx_speed_changed(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    
    svga_recalctimings(&dev->svga);
}


static void
gd54xx_force_redraw(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    dev->svga.fullchange = changeframecount;
}

static const device_config_t gd5422_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",1,
                {
                        {
                                "512 KB",0
                        },
                        {
                                "1 MB",1
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

static const device_config_t gd5428_config[] =
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

static const device_config_t gd5440_onboard_config[] =
{
        {
                "memory","Video memory size",CONFIG_SELECTION,"",2,
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

static const device_config_t gd5434_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",4,
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
                NULL
        }
};

static const video_timings_t cl_gd_avga_timing = {VID_ISA,3,3,6,5,5,10};
static const video_timings_t cl_gd_isa_timing = {VID_ISA,3,3,6,8,8,12};
static const video_timings_t cl_gd_vlb_timing = {VID_BUS,4,4,8,10,10,20};
static const video_timings_t cl_gd_pci_timing = {VID_BUS,4,4,8,10,10,20};

const device_t gd5402_isa_device = {
    "Cirrus Logic GD-5402 (ACUMOS AVGA2)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5402,
    BIOS_GD5420_PATH,		/* Common BIOS between 5402 and 5420 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_avga_timing,
    NULL,
};

const device_t gd5420_isa_device = {
    "Cirrus Logic GD-5420",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5420,
    BIOS_GD5420_PATH,		/* Common BIOS between 5402 and 5420 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5422_config,
};

const device_t gd5422_isa_device = {
    "Cirrus Logic GD-5422",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5422,
    BIOS_GD5422_PATH,		/* Common BIOS between 5422 and 5424 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5422_config,
};

const device_t gd5424_vlb_device = {
    "Cirrus Logic GD-5424",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5424,
    BIOS_GD5422_PATH,		/* Common BIOS between 5422 and 5424 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5422_config,
};

const device_t gd5426_vlb_device = {
    "Cirrus Logic GD-5426",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5426,
    BIOS_GD5426_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5428_isa_device = {
    "Cirrus Logic GD-5428",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5428,
    BIOS_GD5428_ISA_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5428_config
};

const device_t gd5428_vlb_device = {
    "Cirrus Logic GD-5428",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5428,
    BIOS_GD5428_VLB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5429_isa_device = {
    "Cirrus Logic GD-5429",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5429,
    BIOS_GD5429_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5428_config
};

const device_t gd5429_vlb_device = {
    "Cirrus Logic GD-5429",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5429,
    BIOS_GD5429_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5430_vlb_device = {
    "Cirrus Logic GD-5430",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5430,
    BIOS_GD5430_VLB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5430_pci_device = {
    "Cirrus Logic GD-5430",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5430,
    BIOS_GD5430_PCI_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5428_config
};

const device_t gd5434_isa_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5434_config
};

const device_t gd5434_vlb_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5434_config
};

const device_t gd5434_pci_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5436_pci_device = {
    "Cirrus Logic GD-5436",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5436,
    BIOS_GD5436_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5440_pci_device = {
    "Cirrus Logic GD-5440",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5440 | 0x400,
    BIOS_GD5440_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5428_config
};

const device_t gd5440_onboard_pci_device = {
    "Onboard Cirrus Logic GD-5440",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5440 | 0x600,
    NULL,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5440_onboard_config
};

const device_t gd5446_pci_device = {
    "Cirrus Logic GD-5446",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5446,
    BIOS_GD5446_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5446_stb_pci_device = {
    "STB Nitro 64V",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5446 | 0x100,
    BIOS_GD5446_STB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5480_pci_device = {
    "Cirrus Logic GD-5480",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5480,
    BIOS_GD5480_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};
