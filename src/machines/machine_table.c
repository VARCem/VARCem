/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of the emulated machines.
 *
 * Version:	@(#)machine_table.c	1.0.37	2019/03/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../nvr.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/video/video.h"
#include "machine.h"


/* To keep the table clean. */
#if defined(DEV_BRANCH) && defined(USE_AMD_K)
# define CPU_AMD_K5	{"AMD", cpus_K5}
# define CPU_AMD_K56	{"AMD", cpus_K56}
#else
# define CPU_AMD_K5	{"", NULL}
# define CPU_AMD_K56	{"", NULL}
#endif


const machine_t machines[] = {
    /* 8088 */
    { "[8088] IBM PC (1981)",			"ibm_pc",		L"ibm/pc",			-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 16,  256,  16,   0,	m_pc_init,			&m_pc_device,		NULL			},
    { "[8088] IBM PC (1982)",			"ibm_pc82",		L"ibm/pc82",			-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  256,  32,   0,	m_pc82_init,			&m_pc_device,		NULL			},
    { "[8088] IBM PCjr",			"ibm_pcjr",		L"ibm/pcjr",			-1,	{{"Intel", cpus_pcjr},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										128,  640, 128,   0,	m_pcjr_init,			&m_pcjr_device,		NULL			},
    { "[8088] IBM XT (1982)",			"ibm_xt",		L"ibm/xt",			-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			&m_xt_device,		NULL			},
    { "[8088] IBM XT (1986)",			"ibm_xt86",		L"ibm/xt86",			-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt86_init,			&m_xt_device,		NULL			},

    { "[8088] AMI XT (generic)",		"ami_xt",		L"generic/xt/ami",		0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			&m_xt_device,		NULL			},
    { "[8088] Award XT (generic)",		"awd_xt",		L"generic/xt/award",		0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			&m_xt_device,		NULL			},
    { "[8088] Phoenix XT (generic)",		"phoenix_xt",		L"generic/xt/phoenix",		0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			NULL,			NULL			},

    { "[8088] OpenXT (generic)",		"open_xt",		L"generic/xt/open_xt",		0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"Intel", cpus_186},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			&m_xt_device,		NULL			},

    { "[8088] Compaq Portable",			"compaq_portable",	L"compaq/portable",		-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO,										128,  640, 128,   0,	m_xt_compaq_p1_init,		NULL,			NULL			},
    { "[8088] DTK XT",				"dtk_xt",		L"dtk/xt",			0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			&m_xt_device,		NULL			},
    { "[8088] Juko XT",				"juko_pc",		L"juko/pc",			0,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,	m_xt_init,			NULL,			NULL			},
    { "[8088] Schneider EuroPC",		"schneider_europc",	L"schneider/europc",		0,	{{"Siemens", cpus_europc},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE,						512,  640, 128,  16,	m_europc_init,			NULL,			NULL			},
    { "[8088] Tandy 1000",			"tandy_1000",		L"tandy/t1000",			-1,	{{"Intel", cpus_europc},	{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										128,  640, 128,   0,	m_tandy1k_init,			&m_tandy1k_device,	NULL			},
    { "[8088] Tandy 1000 HX",			"tandy_1000hx",		L"tandy/t1000hx",		-1,	{{"Intel", cpus_europc},	{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										256,  640, 128,   0,	m_tandy1k_hx_init,		&m_tandy1k_hx_device,	NULL			},
    { "[8088] Toshiba T1000",			"toshiba_t1000",	L"toshiba/t1000",		-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										512, 1280, 768,  64,	m_xt_t1000_init,		&m_xt_t1000_device,	m_xt_t1x00_close	},
    { "[8088] VTech Laser Turbo XT",		"vtech_ltxt",		L"vtech/ltxt",			-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												256,  640, 256,   0,	m_xt_laserxt_init,		NULL,			NULL			},
    { "[8088] Xi8088",				"malinov_xi8088",	L"malinov/xi8088",		-1,	{{"Intel", cpus_8088},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA /*| MACDHINE_AT*/ | MACHINE_PS2,								 64, 1024, 128, 128,	m_xt_xi8088_init,		NULL,			NULL			},
    { "[8088] Zenith Data SupersPORT",		"zenith_supersport",	L"zenith/supersport",		-1,	{{"Intel", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												128,  640, 128,   0,	m_zenith_supersport_init,	NULL,			NULL			},

    /* 8086 */
    { "[8086] Amstrad PC1512",			"amstrad_pc1512",	L"amstrad/pc1512",		-1,	{{"Intel", cpus_pc1512},	{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								512,  640, 128,  64,	m_amstrad_1512_init,		&m_amstrad_1512_device,	NULL			},
    { "[8086] Amstrad PC1640",			"amstrad_pc1640",	L"amstrad/pc1640",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,	m_amstrad_1640_init,		NULL,			NULL			},
    { "[8086] Amstrad PC2086",			"amstrad_pc2086",	L"amstrad/pc2086",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,	m_amstrad_2086_init,		NULL,			NULL			},
    { "[8086] Amstrad PC3086",			"amstrad_pc3086",	L"amstrad/pc3086",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,	m_amstrad_3086_init,		NULL,			NULL			},
    { "[8086] Amstrad PC20(0)",			"amstrad_pc200",	L"amstrad/pc200",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								512,  640, 128,  64,	m_amstrad_200_init,		&m_amstrad_200_device,	NULL			},
    { "[8086] Olivetti M24",			"olivetti_m24",		L"olivetti/m24",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								128,  640, 128,   0,	m_olim24_init,			NULL,			NULL			},
    { "[8086] Tandy 1000 SL/2",			"tandy_1000sl2",	L"tandy/t1000sl2",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA,												512,  768, 128,   0,	m_tandy1k_sl2_init,		&m_tandy1k_sl2_device,	NULL			},
    { "[8086] Toshiba T1200",			"toshiba_t1200",	L"toshiba/t1200",		-1,	{{"Intel", cpus_8086},		{"NEC", cpus_nec},	{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_HDC,							       1024, 2048,1024,  64,	m_xt_t1200_init,		&t1200_video_device,	m_xt_t1x00_close	},
    { "[8086] VTech Laser XT3",			"vtech_lxt3",		L"vtech/lxt3",			-1,	{{"Intel", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												256,  640, 256,   0,	m_xt_lxt3_init,			NULL,			NULL			},

    /* 80286 */
    { "[286 ISA] IBM AT",			"ibm_at",		L"ibm/at",			-1,	{{"", cpus_ibmat},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										256,15872, 128,  64,	m_at_ibm_init,			NULL,			NULL			},
    { "[286 ISA] IBM XT Model 286",		"ibm_xt286",		L"ibm/xt286",			-1,	{{"", cpus_ibmxt286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										256,15872, 128, 128,	m_at_ibm_init,			NULL,			NULL			},
    { "[286 ISA] IBM PS/1 model 2011",		"ibm_ps1_2011",		L"ibm/ps1_2011",		-1,	{{"", cpus_ps1_m2011},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,				512, 6144, 512,  64,	m_ps1_m2011_init,		&m_ps1_device,		NULL			},
    { "[286 ISA] IBM PS/2 model 30-286",	"ibm_ps2_m30_286",	L"ibm/ps2_m30_286",		-1,	{{"", cpus_ps2_m30_286},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  1,   16,   1,  64,	m_ps2_m30_286_init,		NULL,			NULL			},

    { "[286 ISA] AMI 286 (generic)",		"ami_286",		L"generic/at/ami",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512, 8192, 128, 128,	m_at_neat_ami_init,		NULL,			NULL			},
    { "[286 ISA] Award 286 (generic)",		"award_286",		L"generic/at/award",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,	m_at_scat_init,			NULL,			NULL			},

    { "[286 ISA] Commodore PC 30 III",		"commodore_pc30",	L"commodore/pc30",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										640,16384, 128, 128,	m_at_cmdpc_init,		NULL,			NULL			},
    { "[286 ISA] Compaq Portable II",		"compaq_portable2",	L"compaq/portable2",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										640,16384, 128, 128,	m_at_compaq_p2_init,		NULL,			NULL			},
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    { "[286 ISA] Compaq Portable III",		"compaq_portable3",	L"compaq/portable3",		6,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_VIDEO,								640,16384, 128, 128,	m_at_compaq_p3_init,		NULL,			NULL			},
#endif
    { "[286 ISA] GW-286CT GEAR",		"gw286ct",		L"unknown/gw286ct",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,	m_at_scat_gw286ct_init,		NULL,			NULL			},
    { "[286 ISA] Hyundai Super-286TR",		"hyundai_super286tr",	L"hyundai/super286tr",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,	m_at_scat_init,			NULL,			NULL			},
    { "[286 ISA] Samsung SPC-4200P",		"samsung_spc4200p",	L"samsung/spc4200p",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2,								512, 2048, 128, 128,	m_at_scat_init,			NULL,			NULL			},
    { "[286 ISA] Samsung SPC-4216P",		"samsung_spc4216p",	L"samsung/spc4216p",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2,								  1,    5,   1, 128,	m_at_scat_spc4216p_init,	NULL,			NULL			},
    { "[286 ISA] Toshiba T3100e",		"toshiba_t3100e",	L"toshiba/t3100e",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_VIDEO | MACHINE_HDC,					       1024, 5120, 256,  64,	m_at_t3100e_init,		NULL,			NULL			},
    { "[286 ISA] Trigem 286M",			"tg286m",		L"trigem/tg286m",		8,	{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,							  			512, 8192, 128, 128,	m_at_tg286m_init,		NULL,			NULL			},
	
    { "[286 MCA] IBM PS/2 model 50",		"ibm_ps2_m50",		L"ibm/ps2_m50",			-1,	{{"", cpus_ps2_m30_286},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC_PS2,				  1,   10,   1,  64,	m_ps2_model_50_init,		NULL,			NULL			},

    /* 80386SX */
    { "[386SX ISA] IBM PS/1 model 2121",	"ibm_ps1_2121",		L"ibm/ps1_2121",		-1,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,    6,   1,  64,	m_ps1_m2121_init,		&m_ps1_device,		NULL			},
    { "[386SX ISA] IBM PS/1 m.2121+ISA",	"ibm_ps1_2121_isa",	L"ibm/ps1_2121",		-1,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,    6,   1,  64,	m_ps1_m2121_init,		&m_ps1_device,		NULL			},

    { "[386SX ISA] AMI 386SX",			"ami_386sx",		L"headland/386sx/ami",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								512,16384, 128, 128,	m_at_headland_init,		NULL,			NULL			},
    { "[386SX ISA] AMI 386SX (OPTi495)",	"ami_386sx_opti495",	L"opti495/ami",			16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   16,   1, 128,	m_at_opti495_ami_init,		NULL,			NULL			},
    { "[386SX ISA] Award 386SX (Opti495)",	"award_386sx_opti495",	L"opti495/award",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   16,   1, 128,	m_at_opti495_award_init,	NULL,   		NULL			},
    { "[386SX ISA] MR 386SX (OPTi495)",		"mr_386sx_opti495",	L"opti495/mr",			16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC}, {"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   16,   1, 128,	m_at_opti495_mr_init,		NULL,			NULL			},

    { "[386SX ISA] Amstrad MegaPC",		"amstrad_megapc",	L"amstrad/megapc",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,				  1,   16,   1, 128,	m_at_wd76c10_init,		NULL,			NULL			},
    { "[386SX ISA] Arche AMA-932J",		"arche_ama932j",	L"arche/ama932j",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,						512, 8192, 128, 128,	m_at_ama932j_init,		&oti067_onboard_device,	NULL			},
    { "[386SX ISA] DTK 386SX clone",		"dtk_386",		L"dtk/386",			16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								512, 8192, 128, 128,	m_at_neat_init,			NULL,			NULL			},
    { "[386SX ISA] KMX-C-02",			"kmxc02",		L"unknown/kmxc02",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 512, 128,	m_at_scatsx_init,		NULL,			NULL			},

    { "[386SX MCA] IBM PS/2 model 55SX",	"ibm_ps2_m55sx",	L"ibm/ps2_m55sx",		16,	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC_PS2,				  1,    8,   1,  64,	m_ps2_model_55sx_init,		NULL,			NULL			},

    /* 80386DX */
    { "[386DX ISA] AMI 386DX (Opti495)",	"ami_386dx_opti495",	L"opti495/ami",			0,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   32,   1, 128,	m_at_opti495_ami_init,		NULL,			NULL			},
    { "[386DX ISA] Award 386DX (Opti495)",	"award_386dx_opti495",	L"opti495/award",		0,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   32,   1, 128,	m_at_opti495_award_init,	NULL,			NULL			},
    { "[386DX ISA] MR 386DX (Opti495)",		"mr_386dx_opti495",	L"opti495/mr",			0,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   32,   1, 128,	m_at_opti495_mr_init,		NULL,			NULL			},

    { "[386DX ISA] Amstrad MegaPC 386DX",	"amstrad_megapc_dx",	L"amstrad/megapc",		0,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,   32,   1, 128,	m_at_wd76c10_init,		NULL,			NULL			},
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    { "[386DX ISA] Compaq Portable III (386)",  "portable3_386",	L"compaq/deskpro386",		-1,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,		                		  1,   14,   1, 128,	m_at_compaq_p3_386_init,	NULL,			NULL			},
#endif

    { "[386DX MCA] IBM PS/2 model 70 (type 3)",	"ibm_ps2_m70_type3",	L"ibm/ps2_m70_type3",		-1,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC_PS2,				  2,   16,   2,  64,	m_ps2_model_70_type3_init,	NULL,			NULL			},
    { "[386DX MCA] IBM PS/2 model 80",		"ibm_ps2_m80",		L"ibm/ps2_m80",			-1,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC_PS2,				  1,   12,   1,  64,	m_ps2_model_80_init,		NULL,			NULL			},

    /* 80486 */
    { "[486 ISA] IBM PS/1 model 2133",		"ibm_ps1_2133",		L"ibm/ps1_2133",		-1,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_NONMI,			  1,   64,   1, 128,	m_ps1_m2133_init,		NULL,			NULL			},

    { "[486 ISA] AMI 486 (ALi1429)",		"ami_486_ali1429",	L"ali1429/ami",			0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   64,   1, 128,	m_at_ali1429_init,		NULL,			NULL			},
#if defined(DEV_BRANCH) && defined(USE_SIS471)
    { "[486 ISA] AMI 486 (SiS471)",		"ami_486_sis471",	L"sis471/ami",			0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,  128,   1, 128,	m_at_sis471_ami_init,		NULL,			NULL			},
#endif
    { "[486 ISA] AMI WinBIOS486 (ALi1429)",	"ami_win486_ali1429",	L"ali1429/ami_win",		0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   64,   1, 128,	m_at_ali1429_init,		NULL,			NULL			},
    { "[486 ISA] Award 486 (Opti495)",		"award_486_opti495",	L"opti495/award",		0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   32,   1, 128,	m_at_opti495_award_init,	NULL,			NULL			},
    { "[486 ISA] MR 486 (Opti495)",		"mr_486dx_opti495",	L"opti495/mr",			0,	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   32,   1, 128,	m_at_opti495_mr_init,		NULL,			NULL			},

    { "[486 ISA] DTK PKM-0038S E-2",		"dtk_486",		L"dtk/486",			0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,  128,   1, 128,	m_at_dtk486_init,		NULL,			NULL			},

    { "[486 MCA] IBM PS/2 model 70 (type 4)",	"ibm_ps2_m70_type4",	L"ibm/ps2_m70_type4",		-1,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  2,   16,   2,  64,	m_ps2_model_70_type4_init,	NULL,			NULL			},

#if defined(DEV_BRANCH) && defined(USE_SIS496)
    { "[486 PCI] AMI 486 (SiS496)",		"ami_486_sis496",	L"sis496/ami",			0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,						  1,  255,   1, 128,	m_at_sis496_ami_init,		NULL,			NULL			},
#endif
    { "[486 PCI] Rise Computer R418",		"rise_r418",		L"rise/r418",			0,	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  1,  255,   1, 128,	m_at_r418_init,			NULL,			NULL			},

    /* Pentium, Socket4 (LX) */
    { "[Socket 4 LX] Intel Premiere/PCI",	"intel_revenge",	L"intel/revenge",		-1,	{{"Intel", cpus_Pentium5V},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  2,  128,   2, 128,	m_at_batman_init,		NULL,			NULL			},

    /* Pentium, Socket5 (NX) */
    { "[Socket 5 NX] Intel Premiere/PCI II",	"intel_plato",		L"intel/plato",			-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K5,		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  2,  128,   2, 128,	m_at_plato_init,		NULL,			NULL			},

    /* Pentium, Socket5 (FX) */
    { "[Socket 5 FX] ASUS P/I-P54TP4XE",	"asus_p54tp4xe",	L"asus/p54tp4xe",		-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K5,		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	m_at_p54tp4xe_init,		NULL,			NULL			},
    { "[Socket 5 FX] Intel Advanced/ZP",	"intel_zappa",		L"intel/zappa",			-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K5,		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_zappa_init,		NULL,			NULL			},
    { "[Socket 5 FX] PC Partner MB500N",	"pcpartner_mb500n",	L"pcpartner/mb500n",		-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K5,		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	m_at_mb500n_init,		NULL,			NULL			},
    { "[Socket 5 FX] President Award 430FX PCI","president",		L"president/president",		-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K5,		{"", NULL},		{"", NULL}}, 	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	m_at_president_init,		NULL,			NULL			},

    /* Pentium, Socket7 (FX) */
    { "[Socket 7 FX] Intel Advanced/ATX",	"intel_thor",		L"intel/thor",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_thor_init,			NULL,			NULL			},
    { "[Socket 7 FX] MR Intel Advanced/ATX",	"intel_thor_mr",	L"intel/thor_mr",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_thor_init,			NULL,			NULL			},
    { "[Socket 7 FX] Intel Advanced/EV",	"intel_endeavor",	L"intel/endeavor",		-1,	{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,	  8,  128,   8, 128,	m_at_endeavor_init,		&s3_phoenix_trio64_onboard_pci_device, NULL	},
    { "[Socket 7 FX] Packard Bell PB640",	"pbell_pb640",		L"pbell/pb640",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,	  8,  128,   8, 128,	m_at_pb640_init,		&gd5440_onboard_pci_device, NULL		},

    /* Pentium, Socket7 (HX) */
    { "[Socket 7 HX] Acer M3a",			"acer_m3a",		L"acer/m3a",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	m_at_acerm3a_init,		NULL,			NULL			},
    { "[Socket 7 HX] Acer V35n",		"acer_v35n",		L"acer/v35n",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	m_at_acerv35n_init,		NULL,			NULL			},
    { "[Socket 7 HX] AOpen AP53",		"aopen_ap53",		L"aopen/ap53",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,	m_at_ap53_init,			NULL,			NULL			},
    { "[Socket 7 HX] ASUS P/I-P55T2P4",		"asus_p55t2p4",		L"asus/p55t2p4",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,	m_at_p55t2p4_init,		NULL,			NULL			},
    { "[Socket 7 HX] SuperMicro Super P55T2S",	"supermicro_p55t2s",	L"supermicro/p55t2s",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  768,   8, 128,	m_at_p55t2s_init,		NULL,			NULL			},

    /* Pentium, Socket7 (VX) */
    { "[Socket 7 VX] ASUS P/I-P55TVP4",		"asus_p55tvp4",		L"asus/p55tvp4",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_p55tvp4_init,		NULL,			NULL			},
    { "[Socket 7 VX] Award 430VX PCI",		"award_430vx",		L"generic/430vx/award",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_i430vx_init,		NULL,			NULL			},
    { "[Socket 7 VX] Epox P55-VA",		"epox_p55va",		L"epox/p55va",			-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_p55va_init,		NULL,			NULL			},
    { "[Socket 7 VX] Jetway J656VXD",		"jetway_j656vxd",	L"jetway/j656vxd",		-1,	{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	CPU_AMD_K56,		{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	m_at_j656vxd_init,		NULL,			NULL			},

#if defined(DEV_BRANCH) && defined(USE_I686)
    /* Pentium, Socket5 */
    { "[Socket 8 FX] Tyan Titan-Pro AT",	"tyan_440fx",		L"tyan/440fx",			-1,	{{"Intel", cpus_PentiumPro},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,					  8, 1024,   8, 128,	m_at_i440fx_init,		NULL,			NULL			},
    { "[Socket 8 FX] Tyan Titan-Pro ATX",	"tyan_tpatx",		L"tyan/tpatx",			-1,	{{"Intel", cpus_PentiumPro},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,					  8, 1024,   8, 128,	m_at_s1668_init,		NULL,			NULL			},
#endif
    { NULL,					NULL,			NULL,				-1,	{{"", NULL},			{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, 0,													  0,    0,   0, 0,	NULL,				NULL,			NULL			}
};


const char *
machine_getname(void)
{
    return(machines[machine].name);
}


const char *
machine_getname_ex(int m)
{
    return(machines[m].name);
}


const char *
machine_get_internal_name(void)
{
    return(machines[machine].internal_name);
}


const char *
machine_get_internal_name_ex(int m)
{
    return(machines[m].internal_name);
}


int
machine_get_from_internal_name(const char *s)
{
    int c = 0;

    while (machines[c].internal_name != NULL) {
	if (! strcmp(machines[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(-1);
}


const device_t *
machine_getdevice(int m)
{
    return(machines[m].device);
}


int
machine_get_config_int(const char *s)
{
    const device_t *d = machine_getdevice(machine);
    const device_config_t *c;

    if (d == NULL) return(0);

    c = d->config;
    while (c && c->type != -1) {
	if (! strcmp(s, c->name))
		return(config_get_int(d->name, s, c->default_int));

	c++;
    }

    return(0);
}


const char *
machine_get_config_string(const char *s)
{
    const device_t *d = machine_getdevice(machine);
    const device_config_t *c;

    if (d == NULL) return(0);

    c = d->config;
    while (c && c->type != -1) {
	if (! strcmp(s, c->name))
		return(config_get_string(d->name, s, c->default_string));

	c++;
    }

    return(NULL);
}
