/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define all known video cards.
 *
 * Version:	@(#)vid_table.c	1.0.9	2018/03/15
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../emu.h"
#include "../machine/machine.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../timer.h"
#include "../plat.h"
#include "video.h"
#include "vid_svga.h"

#include "vid_ati18800.h"
#include "vid_ati28800.h"
#include "vid_ati_mach64.h"
#include "vid_cga.h"
#include "vid_cl54xx.h"
#include "vid_compaq_cga.h"
#include "vid_ega.h"
#include "vid_et4000.h"
#include "vid_et4000w32.h"
#include "vid_genius.h"
#include "vid_hercules.h"
#include "vid_herculesplus.h"
#include "vid_incolor.h"
#include "vid_colorplus.h"
#include "vid_mda.h"
#include "vid_oak_oti.h"
#include "vid_paradise.h"
#include "vid_s3.h"
#include "vid_s3_virge.h"
#include "vid_tgui9440.h"
#include "vid_ti_cf62011.h"
#include "vid_tvga.h"
#include "vid_vga.h"
#include "vid_voodoo.h"
#include "vid_wy700.h"


#define VIDEO_FLAG_TYPE_CGA     0x00
#define VIDEO_FLAG_TYPE_MDA     0x01
#define VIDEO_FLAG_TYPE_SPECIAL 0x02
#define VIDEO_FLAG_TYPE_MASK    0x03


enum {
    VIDEO_ISA = 0,
    VIDEO_BUS
};

typedef struct {
    const char	*name;
    const char	*internal_name;
    const device_t	*device;
    int		legacy_id;
    int		flags;
    video_timings_t	timing;
} VIDEO_CARD;


static VIDEO_CARD
video_cards[] = {
    {"None",						"none",			NULL,					VID_NONE,			VIDEO_FLAG_TYPE_SPECIAL,	{0,         0,  0,  0,   0,  0,  0}},
    {"Internal",					"internal",		NULL,					VID_INTERNAL,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] ATI Graphics Pro Turbo (Mach64 GX)",	"mach64gx_isa",		&mach64gx_isa_device,			VID_MACH64GX_ISA,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
    {"[ISA] ATI Korean VGA (ATI-28800-5)",		"ati28800k",		&ati28800k_device,			VID_ATIKOREANVGA,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
    {"[ISA] ATI VGA-88 (ATI-18800-1)",			"ati18800v",		&ati18800_vga88_device,			VID_VGA88,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] ATI VGA Charger (ATI-28800-5)",		"ati28800",		&ati28800_device,			VID_VGACHARGER,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
    {"[ISA] ATI VGA Edge-16 (ATI-18800-5)",		"ati18800",		&ati18800_device,			VID_VGAEDGE16,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] ATI VGA Wonder (ATI-18800)",		"ati18800w",		&ati18800_wonder_device,		VID_VGAWONDER,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
#if defined(DEV_BRANCH) && defined(USE_XL24)
    {"[ISA] ATI VGA Wonder XL24 (ATI-28800-6)",		"ati28800w",		&ati28800_wonderxl24_device,		VID_VGAWONDERXL24,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
#endif
    {"[ISA] CGA",					"cga",			&cga_device,				VID_CGA,			VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Chips & Technologies SuperEGA",		"superega",		&sega_device,				VID_SUPER_EGA,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Cirrus Logic GD5428",			"cl_gd5428_isa",	&gd5428_isa_device,			VID_CL_GD5428_ISA,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   8,  8, 12}},
    {"[ISA] Cirrus Logic GD5429",			"cl_gd5429_isa",	&gd5429_isa_device,			VID_CL_GD5429_ISA,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   8,  8, 12}},
    {"[ISA] Cirrus Logic GD5434",			"cl_gd5434_isa",	&gd5434_isa_device,			VID_CL_GD5434_ISA,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   8,  8, 12}},
    {"[ISA] Compaq ATI VGA Wonder XL (ATI-28800-5)",	"compaq_ati28800",	&compaq_ati28800_device,		VID_VGAWONDERXL,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
    {"[ISA] Compaq CGA",				"compaq_cga",		&compaq_cga_device,			VID_COMPAQ_CGA,			VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Compaq CGA 2",				"compaq_cga_2",		&compaq_cga_2_device,			VID_COMPAQ_CGA_2,		VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Compaq EGA",				"compaq_ega",		&cpqega_device,				VID_COMPAQ_EGA,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] EGA",					"ega",			&ega_device,				VID_EGA,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Hercules",					"hercules",		&hercules_device,			VID_HERCULES,			VIDEO_FLAG_TYPE_MDA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Hercules Plus",				"hercules_plus",	&herculesplus_device,			VID_HERCULESPLUS,		VIDEO_FLAG_TYPE_MDA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Hercules InColor",				"incolor",		&incolor_device,			VID_INCOLOR,			VIDEO_FLAG_TYPE_MDA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] MDA",					"mda",			&mda_device,				VID_MDA,			VIDEO_FLAG_TYPE_MDA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] MDSI Genius",				"genius",            	&genius_device,				VID_GENIUS,			VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] OAK OTI-037C",				"oti037c",		&oti037c_device,			VID_OTI037C,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 6,  8, 16,   6,  8, 16}},
    {"[ISA] OAK OTI-067",				"oti067",		&oti067_device,				VID_OTI067,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 6,  8, 16,   6,  8, 16}},
    {"[ISA] OAK OTI-077",				"oti077",		&oti077_device,				VID_OTI077,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 6,  8, 16,   6,  8, 16}},
    {"[ISA] Paradise PVGA1A",				"pvga1a",		&paradise_pvga1a_device,		VID_PVGA1A,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Paradise WD90C11-LR",			"wd90c11",		&paradise_wd90c11_device,		VID_WD90C11,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Paradise WD90C30-LR",			"wd90c30",		&paradise_wd90c30_device,		VID_WD90C30,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 6,  8, 16,   6,  8, 16}},
    {"[ISA] Plantronics ColorPlus",			"plantronics",		&colorplus_device,			VID_COLORPLUS,			VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
#if defined(DEV_BRANCH) && defined(USE_TI)
    {"[ISA] TI CF62011 SVGA",				"ti_cf62011",		&ti_cf62011_device,			VID_TICF62011,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
#endif
    {"[ISA] Trident TVGA8900D",				"tvga8900d",		&tvga8900d_device,			VID_TVGA,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   8,  8, 12}},
    {"[ISA] Tseng ET4000AX",				"et4000ax",		&et4000_device,				VID_ET4000,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 3,  3,  6,   5,  5, 10}},
    {"[ISA] VGA",					"vga",			&vga_device,				VID_VGA,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[ISA] Wyse 700",					"wy700",		&wy700_device,				VID_WY700,			VIDEO_FLAG_TYPE_CGA,		{VIDEO_ISA, 8, 16, 32,   8, 16, 32}},
    {"[PCI] ATI Graphics Pro Turbo (Mach64 GX)",	"mach64gx_pci",		&mach64gx_pci_device,			VID_MACH64GX_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  1,  20, 20, 21}},
    {"[PCI] ATI Video Xpression (Mach64 VT2)",		"mach64vt2",		&mach64vt2_device,			VID_MACH64VT2,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  1,  20, 20, 21}},
    {"[PCI] Cardex Tseng ET4000/w32p",			"et4000w32p_pci",	&et4000w32p_cardex_pci_device,		VID_ET4000W32_CARDEX_PCI,	VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  4,  10, 10, 10}},
    {"[PCI] Cirrus Logic GD5430",			"cl_gd5430_pci",	&gd5430_pci_device,			VID_CL_GD5430_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    {"[PCI] Cirrus Logic GD5434",			"cl_gd5434_pci",	&gd5434_pci_device,			VID_CL_GD5434_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    {"[PCI] Cirrus Logic GD5436",		    	"cl_gd5436_pci",	&gd5436_pci_device,			VID_CL_GD5436_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    #if defined(DEV_BRANCH) && defined(USE_STEALTH32)
    {"[PCI] Diamond Stealth 32 (Tseng ET4000/w32p)",	"stealth32_pci",	&et4000w32p_pci_device,			VID_ET4000W32_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  4,  10, 10, 10}},
#endif
    {"[PCI] Diamond Stealth 3D 2000 (S3 ViRGE)",	"stealth3d_2000_pci",	&s3_virge_pci_device,			VID_VIRGE_PCI,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[PCI] Diamond Stealth 3D 3000 (S3 ViRGE/VX)",	"stealth3d_3000_pci",	&s3_virge_988_pci_device,		VID_VIRGEVX_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  4,  26, 26, 42}},
    {"[PCI] Diamond Stealth 64 DRAM (S3 Trio64)",	"stealth64d_pci",	&s3_diamond_stealth64_pci_device,	VID_STEALTH64_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  4,  26, 26, 42}},
    {"[PCI] Number Nine 9FX (S3 Trio64)",		"n9_9fx_pci",		&s3_9fx_pci_device,			VID_N9_9FX_PCI,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[PCI] Paradise Bahamas 64 (S3 Vision864)",	"bahamas64_pci",	&s3_bahamas64_pci_device,		VID_BAHAMAS64_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  5,  20, 20, 35}},
    {"[PCI] Phoenix S3 Vision864",			"px_vision864_pci",	&s3_phoenix_vision864_pci_device,	VID_PHOENIX_VISION864_PCI,	VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  5,  20, 20, 35}},
    {"[PCI] Phoenix S3 Trio32",				"px_trio32_pci",	&s3_phoenix_trio32_pci_device,		VID_PHOENIX_TRIO32_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[PCI] Phoenix S3 Trio64",				"px_trio64_pci",	&s3_phoenix_trio64_pci_device,		VID_PHOENIX_TRIO64_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[PCI] S3 ViRGE/DX",				"virge375_pci",		&s3_virge_375_pci_device,		VID_VIRGEDX_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[PCI] S3 ViRGE/DX (VBE 2.0)",			"virge375_vbe20_pci",	&s3_virge_375_4_pci_device,		VID_VIRGEDX4_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[PCI] Trident TGUI9440",				"tgui9440_pci",		&tgui9440_pci_device,			VID_TGUI9440_PCI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  8, 16,   4,  8, 16}},
    {"[VLB] ATI Graphics Pro Turbo (Mach64 GX)",	"mach64gx_vlb",		&mach64gx_vlb_device,			VID_MACH64GX_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  1,  20, 20, 21}},
    {"[VLB] Cardex Tseng ET4000/w32p",			"et4000w32p_vlb",	&et4000w32p_cardex_vlb_device,		VID_ET4000W32_CARDEX_VLB,	VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  4,  10, 10, 10}},
    {"[VLB] Cirrus Logic GD5429",			"cl_gd5429_vlb",	&gd5429_vlb_device,			VID_CL_GD5429_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    {"[VLB] Cirrus Logic GD5434",			"cl_gd5434_vlb",	&gd5434_vlb_device,			VID_CL_GD5434_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    {"[VLB] Diamond SpeedStar PRO (CL GD5428)",		"cl_gd5428_vlb",	&gd5428_vlb_device,			VID_CL_GD5428_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
    {"[VLB] Diamond SpeedStar PRO SE (CL GD5430)",	"cl_gd5430_vlb",	&gd5430_vlb_device,			VID_CL_GD5430_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  8,  10, 10, 20}},
#if defined(DEV_BRANCH) && defined(USE_STEALTH32)
    {"[VLB] Diamond Stealth 32 (Tseng ET4000/w32p)",	"stealth32_vlb",	&et4000w32p_vlb_device,			VID_ET4000W32_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  4,  10, 10, 10}},
#endif
    {"[VLB] Diamond Stealth 3D 2000 (S3 ViRGE)",	"stealth3d_2000_vlb",	&s3_virge_vlb_device,			VID_VIRGE_VLB,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[VLB] Diamond Stealth 3D 3000 (S3 ViRGE/VX)",	"stealth3d_3000_vlb",	&s3_virge_988_vlb_device,		VID_VIRGEVX_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  4,  26, 26, 42}},
    {"[VLB] Diamond Stealth 64 DRAM (S3 Trio64)",	"stealth64d_vlb",	&s3_diamond_stealth64_vlb_device,	VID_STEALTH64_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  4,  26, 26, 42}},
    {"[VLB] Number Nine 9FX (S3 Trio64)",		"n9_9fx_vlb",		&s3_9fx_vlb_device,			VID_N9_9FX_VLB,			VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[VLB] Paradise Bahamas 64 (S3 Vision864)",	"bahamas64_vlb",	&s3_bahamas64_vlb_device,		VID_BAHAMAS64_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  5,  20, 20, 35}},
    {"[VLB] Phoenix S3 Vision864",			"px_vision864_vlb",	&s3_phoenix_vision864_vlb_device,	VID_PHOENIX_VISION864_VLB,	VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  4,  5,  20, 20, 35}},
    {"[VLB] Phoenix S3 Trio32",				"px_trio32_vlb",	&s3_phoenix_trio32_vlb_device,		VID_PHOENIX_TRIO32_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[VLB] Phoenix S3 Trio64",				"px_trio64_vlb",	&s3_phoenix_trio64_vlb_device,		VID_PHOENIX_TRIO64_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 3,  2,  4,  25, 25, 40}},
    {"[VLB] S3 ViRGE/DX",				"virge375_vlb",		&s3_virge_375_vlb_device,		VID_VIRGEDX_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[VLB] S3 ViRGE/DX (VBE 2.0)",			"virge375_vbe20_vlb",	&s3_virge_375_4_vlb_device,		VID_VIRGEDX4_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 2,  2,  3,  28, 28, 45}},
    {"[VLB] Trident TGUI9400CXi",			"tgui9400cxi_vlb",	&tgui9400cxi_device,			VID_TGUI9400CXI,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  8, 16,   4,  8, 16}},
    {"[VLB] Trident TGUI9440",				"tgui9440_vlb",		&tgui9440_vlb_device,			VID_TGUI9440_VLB,		VIDEO_FLAG_TYPE_SPECIAL,	{VIDEO_BUS, 4,  8, 16,   4,  8, 16}},
    {"",						"",			NULL,					-1,				0,				{0,	    0,  0,  0,   0,  0,  0}}
};


int
video_detect(void)
{
    int c, i;

    /* Make sure we have a usable video card. */
    c = 0;
    for (i=0; i<VID_MAX; i++) {
	vid_present[i] = video_card_available(video_old_to_new(i));
	c += vid_present[i];
    }

    return(c);
}


void
video_reset(int card)
{
    pclog("VIDEO: reset (romset=%d, vid_card=%d, internal=%d)\n",
       	romset, card, (machines[machine].flags & MACHINE_VIDEO)?1:0);

    /* Reset the CGA palette. */
    cga_palette = 0;
    cgapal_rebuild();

    /* Initialize the video font tables. */
    loadfont(L"roms/video/ibm/mda/mda.rom", 0);
    loadfont(L"roms/video/wyse/wyse700/wy700.rom", 3);
    loadfont(L"roms/video/mdsi/genius/8x12.bin", 4);
    loadfont(FONT_ATIKOR_PATH, 6);

    /* Do not initialize internal cards here. */
    if ((card == VID_NONE) || \
	(card == VID_INTERNAL) || machines[machine].fixed_vidcard) return;

    /* Initialize the video card. */
    device_add(video_cards[video_old_to_new(card)].device);

    /* Enable the Voodoo if configured. */
    if (voodoo_enabled)
       	device_add(&voodoo_device);
}


int
video_card_available(int card)
{
    if (video_cards[card].device)
	return(device_available(video_cards[card].device));

    return(1);
}


char *
video_card_getname(int card)
{
    return((char *)video_cards[card].name);
}


const device_t *
video_card_getdevice(int card)
{
    return(video_cards[card].device);
}


int
video_card_has_config(int card)
{
    if (video_cards[card].device == NULL) return(0);

    return(video_cards[card].device->config ? 1 : 0);
}


const video_timings_t *
video_card_gettiming(int card)
{
    return(&video_cards[card].timing);
}


int
video_card_getid(char *s)
{
    int c = 0;

    while (video_cards[c].legacy_id != -1) {
	if (! strcmp((char *)video_cards[c].name, s))
		return(c);
	c++;
    }

    return(0);
}


int
video_old_to_new(int card)
{
    int c = 0;

    while (video_cards[c].legacy_id != -1) {
	if (video_cards[c].legacy_id == card)
		return(c);
	c++;
    }

    return(0);
}


int
video_new_to_old(int card)
{
    return(video_cards[card].legacy_id);
}


char *
video_get_internal_name(int card)
{
    return((char *)video_cards[card].internal_name);
}


int
video_get_video_from_internal_name(char *s)
{
    int c = 0;

    while (video_cards[c].legacy_id != -1) {
	if (! strcmp((char *)video_cards[c].internal_name, s))
		return(video_cards[c].legacy_id);
	c++;
    }

    return(0);
}


int
video_is_mda(void)
{
    switch (romset) {
	case ROM_IBMPCJR:
	case ROM_TANDY:
	case ROM_TANDY1000HX:
	case ROM_TANDY1000SL2:
	case ROM_PC1512:
	case ROM_PC1640:
	case ROM_PC200:
	case ROM_OLIM24:
	case ROM_PC2086:
	case ROM_PC3086:
	case ROM_MEGAPC:
	case ROM_MEGAPCDX:
	case ROM_IBMPS1_2011:
	case ROM_IBMPS2_M30_286:
	case ROM_IBMPS2_M50:
	case ROM_IBMPS2_M55SX:
	case ROM_IBMPS2_M70_TYPE3:
	case ROM_IBMPS2_M70_TYPE4:
	case ROM_IBMPS2_M80:
	case ROM_IBMPS1_2121:
	case ROM_T3100E:
		return(0);
    }

    return((video_cards[video_old_to_new(vid_card)].flags & VIDEO_FLAG_TYPE_MASK) == VIDEO_FLAG_TYPE_MDA);
}


int
video_is_cga(void)
{
    switch (romset) {
	case ROM_IBMPCJR:
	case ROM_TANDY:
	case ROM_TANDY1000HX:
	case ROM_TANDY1000SL2:
	case ROM_PC1512:
	case ROM_PC200:
	case ROM_OLIM24:
	case ROM_T3100E:
		return(1);

	case ROM_PC1640:
	case ROM_PC2086:
	case ROM_PC3086:
	case ROM_MEGAPC:
	case ROM_MEGAPCDX:
	case ROM_IBMPS1_2011:
	case ROM_IBMPS2_M30_286:
	case ROM_IBMPS2_M50:
	case ROM_IBMPS2_M55SX:
	case ROM_IBMPS2_M70_TYPE3:
	case ROM_IBMPS2_M70_TYPE4:
	case ROM_IBMPS2_M80:
	case ROM_IBMPS1_2121:
		return(0);
    }

    return((video_cards[video_old_to_new(vid_card)].flags & VIDEO_FLAG_TYPE_MASK) == VIDEO_FLAG_TYPE_CGA);
}


int
video_is_ega_vga(void)
{
    switch (romset) {
	case ROM_IBMPCJR:
	case ROM_TANDY:
	case ROM_TANDY1000HX:
	case ROM_TANDY1000SL2:
	case ROM_PC1512:
	case ROM_PC200:
	case ROM_OLIM24:
	case ROM_T3100E:
		return(0);

	case ROM_PC1640:
	case ROM_PC2086:
	case ROM_PC3086:
	case ROM_MEGAPC:
	case ROM_MEGAPCDX:
	case ROM_IBMPS1_2011:
	case ROM_IBMPS2_M30_286:
	case ROM_IBMPS2_M50:
	case ROM_IBMPS2_M55SX:
	case ROM_IBMPS2_M70_TYPE3:
	case ROM_IBMPS2_M70_TYPE4:
	case ROM_IBMPS2_M80:
	case ROM_IBMPS1_2121:
	return(1);
    }

    return((video_cards[video_old_to_new(vid_card)].flags & VIDEO_FLAG_TYPE_MASK) == VIDEO_FLAG_TYPE_SPECIAL);
}
