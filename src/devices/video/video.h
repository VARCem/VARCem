/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the video controller module.
 *
 * Version:	@(#)video.h	1.0.27	2019/03/03
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
#ifndef EMU_VIDEO_H
# define EMU_VIDEO_H


#define makecol(r, g, b)    ((b) | ((g) << 8) | ((r) << 16))
#define makecol32(r, g, b)  ((b) | ((g) << 8) | ((r) << 16))


enum {
    VID_NONE = 0,
    VID_INTERNAL,
    VID_MDA,			/* IBM MDA */
    VID_CGA,			/* IBM CGA */
    VID_EGA,			/* IBM EGA */
    VID_VGA,        		/* IBM VGA */
    VID_HERCULES		/* Hercules */
};

enum {
    VID_TYPE_DFLT = 0,
    VID_TYPE_CGA,
    VID_TYPE_MDA,
    VID_TYPE_SPEC
};
#define VID_TYPE_MASK 0x03

enum {
    VID_ISA = 0,
    VID_MCA,
    VID_BUS
};

enum {
    FULLSCR_SCALE_FULL = 0,
    FULLSCR_SCALE_43,
    FULLSCR_SCALE_SQ,
    FULLSCR_SCALE_INT,
    FULLSCR_SCALE_KEEPRATIO
};


#ifdef __cplusplus
extern "C" {
#endif


typedef struct video_timings {
    int		type;
    int		write_b, write_w, write_l;
    int		read_b, read_w, read_l;
} video_timings_t;

typedef struct {
    int		w, h;
    uint8_t	*dat;
#if 1
    /* Most compilers are OK with this, as long as its at end of struct. */
    uint8_t	*line[];
#else
    /* However, just in case we need it... */
    uint8_t	*line[2048];
#endif
} bitmap_t;

typedef struct {
    uint8_t	r, g, b;
} rgb_t;

typedef struct {
    uint8_t	chr[32];
} dbcs_font_t;

typedef rgb_t PALETTE[256];


extern int		changeframecount;

/* These will go away soon. */
extern uint8_t		fontdat[256][8];
extern uint8_t		fontdatm[256][16];
extern dbcs_font_t	*fontdatk,
			*fontdatk_user;

extern bitmap_t		*buffer,
			*buffer32;
extern PALETTE		cgapal,
			cgapal_mono[6];
extern uint32_t		pal_lookup[256];
extern uint32_t		*video_6to8,
			*video_15to32,
			*video_16to32;
extern int		fullchange;
extern int		xsize,ysize;
extern int		update_overscan;
extern int		overscan_x,
			overscan_y;
extern int		video_timing_read_b,
			video_timing_read_w,
			video_timing_read_l;
extern int		video_timing_write_b,
			video_timing_write_w,
			video_timing_write_l;
extern int		video_res_x,
			video_res_y,
			video_bpp;
extern int		cga_palette;

extern float		cpuclock;
extern int		frames;


#ifdef EMU_DEVICE_H
/* ATi 18800 series cards. */
extern const device_t	ati18800_wonder_device;
extern const device_t	ati18800_vga88_device;
extern const device_t	ati18800_device;

/* ATi 28800 series cards. */
extern const device_t	ati28800_device;
extern const device_t	ati28800k_device;
extern const device_t	ati28800_compaq_device;
#if defined(DEV_BRANCH) && defined(USE_XL24)
extern const device_t	ati28800_wonderxl24_device;
#endif

/* ATi Mach64 series cards. */
extern const device_t	mach64gx_isa_device;
extern const device_t	mach64gx_vlb_device;
extern const device_t	mach64gx_pci_device;
extern const device_t	mach64vt2_device;

/* IBM CGA and compatibles. */
extern const device_t	cga_device;

/* IBM PGC. */
extern const device_t	pgc_device;

/* Cirrus Logic GD-series cards. */
#if defined(DEV_BRANCH)
extern const device_t	gd5402_isa_device;
extern const device_t	gd5420_isa_device;
extern const device_t	gd5422_isa_device;
extern const device_t	gd5424_vlb_device;
#endif
extern const device_t	gd5426_vlb_device;
extern const device_t	gd5428_isa_device;
extern const device_t	gd5428_vlb_device;
extern const device_t	gd5429_isa_device;
extern const device_t	gd5429_vlb_device;
extern const device_t	gd5430_vlb_device;
extern const device_t	gd5430_pci_device;
extern const device_t	gd5434_isa_device;
extern const device_t	gd5434_vlb_device;
extern const device_t	gd5434_pci_device;
extern const device_t	gd5436_pci_device;
extern const device_t	gd5446_pci_device;
extern const device_t	gd5440_onboard_pci_device;
extern const device_t	gd5440_pci_device;
extern const device_t	gd5446_stb_pci_device;
extern const device_t	gd5480_pci_device;

/* COMPAQ CGA-derived cards. */
extern const device_t	cga_compaq_device;
extern const device_t	cga_compaq2_device;

/* IBM EGA and compatibles. */
extern const device_t	ega_device;
extern const device_t	ega_compaq_device;
extern const device_t	sega_device;

/* Tseng Labs ET4000 series cards. */
extern const device_t	et4000_isa_device;
extern const device_t	et4000_mca_device;
extern const device_t	et4000k_isa_device;
extern const device_t	et4000k_tg286_isa_device;

/* Tseng Labs ET4000-W32 series cards. */
extern const device_t	et4000w32p_vlb_device;
extern const device_t	et4000w32p_pci_device;
extern const device_t	et4000w32p_cardex_vlb_device;
extern const device_t	et4000w32p_cardex_pci_device;

/* MDSI Genius VHR card. */
extern const device_t	genius_device;

/* ImageManager 1024 card. */
extern const device_t	im1024_device;

/* Hercules series cards and compatibles. */
extern const device_t	hercules_device;

/* HerculesPlus series cards and compatibles. */
extern const device_t	herculesplus_device;

/* Hercules InColor series cards and compatibles. */
extern const device_t	incolor_device;

/* Hercules ColorPlus series cards and compatibles. */
extern const device_t	colorplus_device;

/* IBM MDA and compatibles. */
extern const device_t	mda_device;

/* Oak Technology OTI series cards. */
extern const device_t	oti037c_device;
extern const device_t	oti067_device;
extern const device_t	oti067_acer386_device;
extern const device_t	oti067_onboard_device;
extern const device_t	oti077_device;

/* Paradise PVGA series cards and compatibles. */
extern const device_t	paradise_pvga1a_pc2086_device;
extern const device_t	paradise_pvga1a_pc3086_device;
extern const device_t	paradise_pvga1a_device;
extern const device_t	paradise_wd90c11_megapc_device;
extern const device_t	paradise_wd90c11_device;
extern const device_t	paradise_wd90c30_device;

/* S3, Inc standard series cards. */
extern const device_t	s3_v7mirage_86c801_isa_device;
extern const device_t	s3_phoenix_86c805_vlb_device;
extern const device_t	s3_bahamas64_vlb_device;
extern const device_t	s3_bahamas64_pci_device;
extern const device_t	s3_9fx_vlb_device;
extern const device_t	s3_9fx_pci_device;
extern const device_t	s3_phoenix_trio32_vlb_device;
extern const device_t	s3_phoenix_trio32_pci_device;
extern const device_t	s3_phoenix_trio64_vlb_device;
extern const device_t	s3_phoenix_trio64_pci_device;
extern const device_t	s3_phoenix_trio64_onboard_pci_device;
extern const device_t	s3_phoenix_vision864_vlb_device;
extern const device_t	s3_phoenix_vision864_pci_device;
extern const device_t	s3_diamond_stealth64_vlb_device;
extern const device_t	s3_diamond_stealth64_pci_device;
extern const device_t	s3_diamond_stealth64_964_pci_device;
extern const device_t	s3_diamond_stealth64_964_vlb_device;

/* S3, Inc ViRGE series cards. */
extern const device_t	s3_virge_vlb_device;
extern const device_t	s3_virge_pci_device;
extern const device_t	s3_virge_988_vlb_device;
extern const device_t	s3_virge_988_pci_device;
extern const device_t	s3_virge_375_vlb_device;
extern const device_t	s3_virge_375_pci_device;
extern const device_t	s3_virge_375_4_vlb_device;
extern const device_t	s3_virge_375_4_pci_device;

/* Sigma Designs cards. */
extern const device_t	sigma_device;

/* Trident 8900 series cards. */
extern const device_t	tvga8900b_device;
extern const device_t	tvga8900cx_device;
extern const device_t	tvga8900d_device;

/* Trident 9400 series cards. */
extern const device_t	tgui9400cxi_device;
extern const device_t	tgui9440_vlb_device;
extern const device_t	tgui9440_pci_device;

/* TI-derived cards. */
#if defined(DEV_BRANCH) && defined(USE_TI)
extern const device_t	ti_cf62011_device;
#endif
extern const device_t	ibm_ps1_2121_device;

/* IBM VGA and compatibles. */
extern const device_t	vga_device;
extern const device_t	ps1vga_device;
extern const device_t	ps1vga_mca_device;

/* 3Dfx VooDoo-series cards. */
extern const device_t	voodoo_device;

/* Wyse700 cards. */
extern const device_t	wy700_device;
#endif


/* Table functions. */
extern void		video_card_log(int level, const char *fmt, ...);
extern int		video_card_available(int card);
extern const char	*video_card_getname(int card);
extern const char	*video_get_internal_name(int card);
extern int		video_get_video_from_internal_name(const char *s);
extern int		video_card_has_config(int card);
#ifdef EMU_DEVICE_H
extern const device_t	*video_card_getdevice(int card);
#endif

extern void		video_setblit(void(*blit)(int,int,int,int,int,int));
extern void		video_blend(int x, int y);
extern void		video_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h);
extern void		video_blit_memtoscreen_8(int x, int y, int y1, int y2, int w, int h);
extern void		video_blit_complete(void);
extern void		video_wait_for_blit(void);
extern void		video_wait_for_buffer(void);

extern void		cgapal_rebuild(void);

#ifdef VIDEO_SVGA_H
extern int		ati28800k_load_font(svga_t *, const wchar_t *);
#endif

extern void		video_log(int level, const char *fmt, ...);
extern void		video_init(void);
extern void		video_close(void);
extern void		video_reset(void);
extern void		video_update_timing(void);
extern void		video_inform(int type, const video_timings_t *ptr);
extern int		video_type(void);
extern void		video_reset_font(void);
extern void		video_load_font(const wchar_t *s, int format);
extern uint8_t		video_force_resize_get(void);
extern void		video_force_resize_set(uint8_t res);
extern uint32_t		video_color_transform(uint32_t color);
extern void		video_transform_copy(uint32_t *dst, uint32_t *src, int len);

extern void		updatewindowsize(int x, int y);
extern int		get_actual_size_x(void);
extern int		get_actual_size_y(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_VIDEO_H*/
