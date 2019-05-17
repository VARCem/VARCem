/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Compaq's dual-mode VDU Controller card.
 *
 *		As per the service manual for the Portable, Portable Plus
 *		and Portable 286 machines, this controller was a merge of
 *		the existing MDA and CGA controllers, where the CGA part
 *		was also extended to higher resolutions. This was copied
 *		from Olivetti's version for their M24 system.
 *
 *		We also add a third mode here, the Plasma LCD used by the
 *		Compaq Portable III. This was a fixed-resolution amber gas
 *		plasma display. The code for this was taken from the code
 *		for the Toshiba 3100e machine, which used a similar display.
 *
 * Version:	@(#)m_compaq_vid.c	1.0.3	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../timer.h"
#include "../plat.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pit.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_mda.h"
#include "../devices/video/vid_cga.h"
#include "machine.h"
#include "m_compaq.h"


#define PLASMA_XSIZE 640
#define PLASMA_YSIZE 400


typedef struct {
    int		type;

    int		mode;
    uint8_t	options;

    uint8_t	attrmap; 		/* attribute mapping register */

    uint8_t	port_13c6;

    mda_t	mda;
    mem_map_t	mda_mapping;

    cga_t	cga;
    mem_map_t	cga_mapping;

    int64_t	dispontime,
		dispofftime;
    int64_t	vsynctime;

    int64_t	vidtime;

    int		linepos, displine;
    int		lineff;
    int		con, coff;
    int		vadj;
    int		vc;
    int		firstline,
		lastline;

    int		dispon;

    uint8_t	*vram;

    /* Mapping of attributes to colors. */
    uint32_t	amber,
		black;
    uint32_t	blinkcols[256][2];
    uint32_t	normcols[256][2];
} vid_t;


static uint8_t	st_video_options;
static int8_t	st_display_mode = -1;


static void
recalc_timings(vid_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    switch(dev->mode) {
	case 0:
		mda_recalctimings(&dev->mda);
		return;

	case 1:
		cga_recalctimings(&dev->cga);
		return;
    }
	
    disptime = 651;
    _dispontime = 640;
    _dispofftime = disptime - _dispontime;

    dev->dispontime  = (int64_t)(_dispontime  * (1LL << TIMER_SHIFT));
    dev->dispofftime = (int64_t)(_dispofftime * (1LL << TIMER_SHIFT));
}


static void
recalc_attrs(vid_t *dev)
{
    int n;

    /* val behaves as follows:
     *     Bit 0: Attributes 01-06, 08-0E are inverse video 
     *     Bit 1: Attributes 01-06, 08-0E are bold 
     *     Bit 2: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
     * 	      are inverse video 
     *     Bit 3: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
     * 	      are bold
     */

    /* Set up colors. */
    dev->amber = makecol(0xf7, 0x7c, 0x34);
    dev->black = makecol(0x17, 0x0c, 0x00);

    /*
     * Initialize the attribute mapping.
     * Start by defaulting everything to black on amber,
     * and with bold set by bit 3.
     */
    for (n = 0; n < 256; n++) {
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber; 
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
    }

    /*
     * Colors 0x11-0xff are controlled by bits 2 and 3 of the 
     * passed value. Exclude x0 and x8, which are always black
     * on amber.
     */
    for (n = 0x11; n <= 0xff; n++) {
	if ((n & 7) == 0) continue;

	if (dev->attrmap & 4) {
		/* Inverse */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
	} else {
		/* Normal */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
	}
    }

    /*
     * Set up the 01-0e range, controlled by bits 0 and 1 of the 
     * passed value. When blinking is enabled this also affects 81-8e.
     */
    for (n = 0x01; n <= 0x0e; n++) {
	if (n == 7) continue;

	if (dev->attrmap & 1) {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
		dev->blinkcols[n+128][0] = dev->amber;
		dev->blinkcols[n+128][1] = dev->black;
	} else {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
		dev->blinkcols[n+128][0] = dev->black;
		dev->blinkcols[n+128][1] = dev->amber;
	}
    }

    /*
     * Colors 07 and 0f are always amber on black.
     * If blinking is enabled so are 87 and 8f.
     */
    for (n = 0x07; n <= 0x0f; n += 8) {
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
	dev->blinkcols[n+128][0] = dev->black;
	dev->blinkcols[n+128][1] = dev->amber;
    }

    /* When not blinking, colors 81-8f are always amber on black. */
    for (n = 0x81; n <= 0x8f; n ++) {
	dev->normcols[n][0] = dev->black;
	dev->normcols[n][1] = dev->amber;
    }

    /*
     * Finally do the ones which are solid black.
     * These differ between the normal and blinking mappings.
     */
    for (n = 0; n <= 0xff; n += 0x11) {
	dev->normcols[n][0] = dev->normcols[n][1] = dev->black;
    }

    /* In the blinking range, 00 11 22 .. 77 and 80 91 a2 .. f7 are black. */
    for (n = 0; n <= 0x77; n += 0x11) {
	dev->blinkcols[n][0] = dev->blinkcols[n][1] = dev->black;
	dev->blinkcols[n+128][0] = dev->blinkcols[n+128][1] = dev->black;
    }
}


static void
vid_poll_plasma(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    cga_t *cga = &dev->cga;
    uint16_t ca = (cga->crtc[15] | (cga->crtc[14] << 8)) & 0x7fff;
    uint16_t ma = (cga->crtc[13] | (cga->crtc[12] << 8)) & 0x7fff;
    uint16_t addr;
    int drawcursor;
    int cursorline;
    int blink;
    int x, c;
    uint8_t chr, attr;
    uint32_t cols[2];
    uint8_t sc = (dev->displine) & 15;
    uint8_t dat, pattern;
    uint32_t ink0 = 0, ink1 = 0;
    uint32_t ink = 0;
    uint32_t fg = (cga->cgacol & 0x0f) ? dev->amber : dev->black;
    uint32_t bg = dev->black;

    if (! dev->linepos) {
	cga->vidtime += dev->dispofftime;
	cga->cgastat |= 1;
	dev->linepos = 1;

	if (dev->dispon) {
                if (cga->displine < cga->firstline) {
                        cga->firstline = cga->displine;
                        video_blit_wait_buffer();
                }
                cga->lastline = cga->displine;

		if (cga->cgamode & 0x02) {
			/* Graphics */
			if (cga->cgamode & 0x10) {
				if (dev->options & 1)
					addr = ((dev->displine) & 1) * 0x2000 + ((dev->displine >> 1) & 1) * 0x4000 + (dev->displine >> 2) * 80 + ((ma & ~1) << 1);
				else
					addr = ((dev->displine >> 1) & 1) * 0x2000 + (dev->displine >> 2) * 80 + ((ma & ~1) << 1);

				for (x = 0; x < 80; x++) {
					dat = dev->vram[(addr & 0x7fff)];
					addr++;

					for (c = 0; c < 8; c++) {
						ink = (dat & 0x80) ? fg : bg;
						if (! (cga->cgamode & 8))
							ink = dev->black;
						screen->line[dev->displine][x * 8 + c].val = ink;
						dat = dat << 1;
					}
				}					
			} else {
				addr = ((dev->displine >> 1) & 1) * 0x2000 + (dev->displine >> 2) * 80 + ((ma & ~1) << 1);

				for (x = 0; x < 80; x++) {
					dat = dev->vram[(addr & 0x7fff)];
					addr++;

					for (c = 0; c < 4; c++) {
						pattern = (dat & 0xc0) >> 6;
						if (! (cga->cgamode & 8))
							pattern = 0;

						switch (pattern & 3) {
							case 0:
								ink0 = ink1 = dev->black;
								break;

							case 1:
								if (dev->displine & 1) {
									ink0 = dev->black;
									ink1 = dev->black;
								} else {
									ink0 = dev->amber;
									ink1 = dev->black;
								}
								break;

							case 2:
								if (dev->displine & 1) {
									ink0 = dev->black;
									ink1 = dev->amber;
								} else {
									ink0 = dev->amber;
									ink1 = dev->black;
								}
								break;

							case 3:
								ink0 = ink1 = dev->amber;
								break;
						}

						screen->line[dev->displine][x*8 + 2*c].val = ink0;
						screen->line[dev->displine][x*8 + 2*c + 1].val = ink1;
						dat = dat << 2;
					}
				}					
			}
		} else if (cga->cgamode & 1) {
			/* High-res text */
			addr = ((ma & ~1) + (dev->displine >> 4) * 80) * 2;
			ma += (dev->displine >> 4) * 80;

			if ((cga->crtc[10] & 0x60) == 0x20) {
				cursorline = 0;
			} else {
				cursorline = ((cga->crtc[10] & 0x0f) <= sc) &&
					     ((cga->crtc[11] & 0x0f) >= sc);
			}

			for (x = 0; x < 80; x++) {
				chr  = dev->vram[((addr + 2 * x) & 0x7fff)];
				attr = dev->vram[((addr + 2 * x + 1) & 0x7fff)];

				drawcursor = ((ma == ca) && cursorline &&
					      (cga->cgamode & 8) &&
					      (dev->cga.cgablink & 16));

				blink = ((cga->cgablink & 16) &&
					 (cga->cgamode & 0x20) &&
					 (attr & 0x80) && !drawcursor);

				if (cga->cgamode & 0x20) {
					/* Blink */
					cols[1] = dev->blinkcols[attr][1];
					cols[0] = dev->blinkcols[attr][0];
					if (blink)
						cols[1] = cols[0];
				} else {
					cols[1] = dev->normcols[attr][1];
					cols[0] = dev->normcols[attr][0];
				}

				if (drawcursor) {
					for (c = 0; c < 8; c++) {
						screen->line[dev->displine][(x << 3) + c].val = cols[(fontdatm[chr][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->amber ^ dev->black);
					}
				} else {
					for (c = 0; c < 8; c++) {
						screen->line[dev->displine][(x << 3) + c].val = cols[(fontdatm[chr][sc] & (1 << (c ^ 7))) ? 1 : 0];
					}
				}

				ma++;
			}
		} else {
			addr = ((ma & ~1) + (dev->displine >> 4) * 40) * 2;
			ma += (dev->displine >> 4) * 40;

			if ((cga->crtc[10] & 0x60) == 0x20) {
				cursorline = 0;
			} else {
				cursorline = ((cga->crtc[10] & 0x0f) <= sc) &&
					     ((cga->crtc[11] & 0x0f) >= sc);
			}

			for (x = 0; x < 40; x++) {
				chr  = dev->vram[((addr + 2 * x) & 0x7fff)];
				attr = dev->vram[((addr + 2 * x + 1) & 0x7fff)];

				drawcursor = ((ma == ca) && cursorline &&
					      (cga->cgamode & 8) &&
					      (cga->cgablink & 16));

				blink = ((cga->cgablink & 16) &&
					 (cga->cgamode & 0x20) &&
					 (attr & 0x80) && !drawcursor);

				if (cga->cgamode & 0x20) {
					/* Blink */
					cols[1] = dev->blinkcols[attr][1]; 		
					cols[0] = dev->blinkcols[attr][0]; 		
					if (blink)
						cols[1] = cols[0];
				} else {
					cols[1] = dev->normcols[attr][1];
					cols[0] = dev->normcols[attr][0];
				}

				if (drawcursor) {
					for (c = 0; c < 8; c++) {
						screen->line[dev->displine][(x << 4) + c*2].val = screen->line[dev->displine][(x << 4) + c*2 + 1].val = cols[(fontdatm[chr][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->amber ^ dev->black);
					}
				} else {
					for (c = 0; c < 8; c++) {
						screen->line[dev->displine][(x << 4) + c*2].val = screen->line[dev->displine][(x << 4) + c*2+1].val = cols[(fontdatm[chr][sc] & (1 << (c ^ 7))) ? 1 : 0];
					}
				}

				ma++;
			}
		}
	}

	dev->displine++;

	/* Hardcode a fixed refresh rate and VSYNC timing */
	if (dev->displine == 400) {
		/* Start of VSYNC. */
		cga->cgastat |= 8;
		dev->dispon = 0;
	}

	if (dev->displine == 416) {
		/* End of VSYNC. */
		dev->displine = 0;
		cga->cgastat &= ~8;
		dev->dispon = 1;
	}
    } else {
	if (dev->dispon) 
		cga->cgastat &= ~1;

	cga->vidtime += dev->dispontime;

	dev->linepos = 0;
	if (dev->displine == 400) {
		/* Hardcode 640x400 window size. */
		if (PLASMA_XSIZE != xsize || PLASMA_YSIZE != ysize) {
			xsize = PLASMA_XSIZE;
			ysize = PLASMA_YSIZE;
			if (xsize < 64)
				xsize = 656;
			if (ysize < 32)
				ysize = 200;

			set_screen_size(xsize, ysize);
			if (video_force_resize_get())
				video_force_resize_set(0);
		}

		video_blit_start(0, 0, cga->firstline, 0, (cga->lastline - cga->firstline), xsize, (cga->lastline - cga->firstline));
		frames++;

		/* Fixed 640x400 resolution. */
		video_res_x = PLASMA_XSIZE;
		video_res_y = PLASMA_YSIZE;

		if (cga->cgamode & 0x02) {
			if (cga->cgamode & 0x10)
				video_bpp = 1;
			else	
				video_bpp = 2;
		} else	 
			video_bpp = 0;

		cga->cgablink++;
	}			
    }
}


static void
vid_poll(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

INFO("CPQ: poll (mode=%i)\n", dev->mode);
    switch(dev->mode) {
	case 1:		/* CGA */
		cga_poll(&dev->cga);
		break;

	case 2:		/* Plasma */
		vid_poll_plasma(dev);
		break;
    }
}


static void
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    mda_t *mda = &dev->mda;
    cga_t *cga = &dev->cga;
    uint8_t old;

    DEBUG("CPQ: vid_out(%04x, %02x)\n", addr, val);
if(addr>=0x3d0)INFO("CPQ: vid_out(%04x, %02x)\n", addr, val);

    switch (addr) {
	case 0x03b8:	/* MDA control register */
		INFO("CPQ: MDA control %02x", val);
		old = mda->ctrl;
		mda->ctrl = val;
		if (mda->ctrl ^ old) {
			INFO(":");
			if (mda->ctrl & 0x01) INFO(" [HIRES]");
			if (mda->ctrl & 0x08) INFO(" [ENABL]");
			if (mda->ctrl & 0x20) INFO(" [BLINK]");
		}
		INFO("\n");
		if ((mda->ctrl ^ old) & 3)
			mda_recalctimings(mda);
		return;

	case 0x03d1:	/* CGA CRTC, value */
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		/*
		 * Register 0x12 controls the attribute
		 * mappings for the plasma screen.
		 */
		if (dev->cga.crtcreg == 0x12) {
			dev->attrmap = val;	
			recalc_attrs(dev);
			return;
		}	
		cga_out(addr, val, &dev->cga);
		recalc_timings(dev);
		return;

	case 0x13c6:
INFO("CPQ: write(%04x, %02x)\n", addr, val);
		dev->port_13c6 = val;
		compaq_display_set(dev->port_13c6 ? 1 : 0);
		return;

	case 0x23c6:
INFO("CPQ: write(%04x, %02x)\n", addr, val);
		compaq_video_options_set(val);
		if (dev->options != st_video_options) {
			dev->options = st_video_options;

			/* Set the font used for the external display */
			dev->cga.fontbase = ((dev->options & 3) * 256);
			
			if (dev->options & 8) {
				/* Disable internal CGA. */
				mem_map_disable(&dev->cga_mapping);
			} else {
				mem_map_enable(&dev->cga_mapping);
			}
		}
		return;
    }

    if (addr >= 0x03b0 && addr <= 0x03bb)
	mda_out(addr, val, mda);

    if (addr >= 0x03d0 && addr <= 0x03df)
       	cga_out(addr, val, cga);
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    mda_t *mda = &dev->mda;
    cga_t *cga = &dev->cga;
    uint8_t ret = 0xff;

INFO("CPQ: vid_in(%04x) = ", addr);

    switch (addr) {
	case 0x03b8:
INFO("%02x\n", mda->ctrl);
		return(mda->ctrl);

	case 0x03d1:	/* CGA CRTC, value */
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		if (cga->crtcreg == 0x12) {
			ret = dev->attrmap & 0x0f;
			if (dev->mode == 2)
				ret |= 0x30;	/* Plasma / CRT */
INFO("%02x\n", ret);
			return(ret);
		}
		break;

	case 0x13c6:
		ret = dev->port_13c6;
		break;

	case 0x23c6:
		ret = 0x00;
		break;
    }

    if (addr >= 0x03b0 && addr <= 0x03bb)
	ret = mda_in(addr, mda);

    if (addr >= 0x03d0 && addr <= 0x03df)
       	ret = cga_in(addr, cga);

INFO("%02x\n", ret);
    return(ret);
}


static void
vid_write(uint32_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    dev->vram[addr & 0x7fff] = val;

    cycles -= 4;
}
	

static uint8_t
vid_read(uint32_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t ret;

    ret = dev->vram[addr & 0x7fff];

    cycles -= 4;

    return(ret);
}


static void
vid_close(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    if (dev->vram)
	free(dev->vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

INFO("CPQ: speed_changed(mode=%i)\n", dev->mode);
    mda_recalctimings(&dev->mda);

    cga_recalctimings(&dev->cga);

    recalc_timings(dev);
}


static priv_t
vid_init(const device_t *info, UNUSED(void *parent))
{
    vid_t *dev;

    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));
    dev->type = info->local;
INFO("CPQ: video_init(type=%i)\n", dev->type);

    dev->mode = 0;
    dev->options = 0xff;

    /* Default attribute mapping is 4. */
    dev->attrmap = 4;
    recalc_attrs(dev);

    /* 32K video RAM. */
    dev->vram = (uint8_t *)mem_alloc(32768);

    /* Initialize the MDA. */
    dev->mda.vram = dev->vram;
    mda_init(&dev->mda);
    io_sethandler(0x03b0, 0x000c,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    mem_map_add(&dev->mda_mapping, 0xb0000, 0x08000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, dev->vram, 0, dev);

    /* Initialize the CGA, start off in 80x25 text mode. */
    dev->cga.vram = dev->vram;
    cga_init(&dev->cga);
    dev->cga.cgastat = 0xf4;
    io_sethandler(0x03d0, 0x000c,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    mem_map_add(&dev->cga_mapping, 0xb8000, 0x08000,
	        vid_read,NULL,NULL, vid_write,NULL,NULL, dev->vram, 0, dev);
    mem_map_disable(&dev->cga_mapping);

    /* Respond to the Compaq ports. */
    if (dev->type == 1) {
	io_sethandler(0x13c6, 1, vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
	io_sethandler(0x23c6, 1, vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    }

    if (dev->mode != 0)
	timer_add(vid_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

#if 0
    /*
     * Flag whether the card is behaving like a CGA or an MDA,
     * so that DIP switches are reported correctly.
     */
    if (dev->mode == 0)
	video_inform(VID_TYPE_MDA, &cpq_timing);
    else
	video_inform(VID_TYPE_CGA, &cpq_timing);
#endif

    return((priv_t)dev);
}
		

const device_t compaq_video_device = {
    "Compaq VDU Controller",
    DEVICE_ISA,
    0,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};

const device_t compaq_video_plasma_device = {
    "Compaq VDU Plasma Controller",
    DEVICE_ISA,
    1,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};


void
compaq_video_options_set(uint8_t options)
{
    st_video_options = options;
}


void
compaq_display_set(uint8_t val)
{
    st_display_mode = val;
}
