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
 * NOTES:	OpenAT wip for 286-class machine with open BIOS.
 *		PS2_M80-486 wip, pending receipt of TRM's for machine.
 *
 * Version:	@(#)machine_table.c	1.0.23	2018/06/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../nvr.h"
#include "../rom.h"
#include "../device.h"
#include "machine.h"


const machine_t machines[] = {
    { "[8088] IBM PC",				ROM_IBMPC,		"ibm_pc",		L"ibm/pc",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  32,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] IBM PCjr",			ROM_IBMPCJR,		"ibm_pcjr",		L"ibm/pcjr",			{{"", cpus_pcjr},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										128,  640, 128,   0,		    machine_pcjr_init, &m_pcjr_device,		NULL			},
    { "[8088] IBM XT",				ROM_IBMXT,		"ibm_xt",		L"ibm/xt",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] OpenXT Generic Clone",		ROM_GENXT,		"open_xt",		L"generic/open_xt",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] AMI XT clone",			ROM_AMIXT,		"ami_xt",		L"generic/ami/xt",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] Award XT clone",			ROM_AWDXT,		"awd_xt",		L"generic/award/xt",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] Compaq Portable",			ROM_PORTABLE,		"compaq_portable",	L"compaq/portable",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO,										128,  640, 128,   0,	       machine_xt_compaq_init, NULL,			NULL			},
    { "[8088] DTK XT clone",			ROM_DTKXT,		"dtk_xt",		L"dtk/xt",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, &m_xt_device,		NULL			},
    { "[8088] Juko XT clone",			ROM_JUKOPC,		"juko_pc",		L"juko/pc",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, NULL,			NULL			},
    { "[8088] Phoenix XT clone",		ROM_PXXT,		"phoenix_xt",		L"generic/phoenix/xt",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												 64,  640,  64,   0,		      machine_xt_init, NULL,			NULL			},
    { "[8088] Schneider EuroPC",		ROM_EUROPC,		"schneider_europc",	L"schneider/europc",		{{"Siemens", cpus_europc},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE,						512,  640, 128,  16,		  machine_europc_init, NULL,			NULL			},
    { "[8088] Tandy 1000",			ROM_TANDY,		"tandy_1000",		L"tandy/t1000",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA,												128,  640, 128,   0,		 machine_tandy1k_init, &m_tandy1k_device,	NULL			},
    { "[8088] Tandy 1000 HX",			ROM_TANDY1000HX,	"tandy_1000hx",		L"tandy/t1000hx",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA,												256,  640, 128,   0,		 machine_tandy1k_init, &m_tandy1k_hx_device,	NULL			},
    { "[8088] Toshiba 1000",			ROM_T1000,		"toshiba_t1000",	L"toshiba/t1000",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,										512, 1280, 768,  64,		machine_xt_t1000_init, &m_xt_t1000_device,	machine_xt_t1x00_close	},
#if defined(DEV_BRANCH) && defined(USE_LASERXT)
    { "[8088] VTech Laser Turbo XT",		ROM_LTXT,		"vtech_ltxt",		L"vtech/ltxt",			{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												512,  512, 256,   0,	      machine_xt_laserxt_init, NULL,			NULL			},
#endif
    { "[8088] Xi8088",				ROM_XI8088,		"malinov_xi8088",	L"malinov/xi8088",		{{"", cpus_8088},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA /*| MACDHINE_AT*/ | MACHINE_PS2,								 64, 1024, 128, 128,	       machine_xt_xi8088_init, NULL,			NULL			},

    { "[8086] Amstrad PC1512",			ROM_PC1512,		"amstrad_pc1512",	L"amstrad/pc1512",		{{"", cpus_pc1512},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								512,  640, 128,  64,		 machine_amstrad_init, NULL,			NULL			},
    { "[8086] Amstrad PC1640",			ROM_PC1640,		"amstrad_pc1640",	L"amstrad/pc1640",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,		 machine_amstrad_init, NULL,			NULL			},
    { "[8086] Amstrad PC2086",			ROM_PC2086,		"amstrad_pc2086",	L"amstrad/pc2086",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,		 machine_amstrad_init, NULL,			NULL			},
    { "[8086] Amstrad PC3086",			ROM_PC3086,		"amstrad_pc3086",	L"amstrad/pc3086",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								640,  640,   0,  64,		 machine_amstrad_init, NULL,			NULL			},
    { "[8086] Amstrad PC20(0)",			ROM_PC200,		"amstrad_pc200",	L"amstrad/pc200",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								512,  640, 128,  64,		 machine_amstrad_init, NULL,			NULL			},
    { "[8086] Olivetti M24",			ROM_OLIM24,		"olivetti_m24",		L"olivetti/m24",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,								128,  640, 128,   0,		  machine_olim24_init, NULL,			NULL			},
    { "[8086] Tandy 1000 SL/2",			ROM_TANDY1000SL2,	"tandy_1000sl2",	L"tandy/t1000sl2",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA,												512,  768, 128,   0,	         machine_tandy1k_init, &m_tandy1k_sl2_device,	NULL			},
    { "[8086] Toshiba 1200",			ROM_T1200,		"toshiba_t1200",	L"toshiba/t1200",		{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_VIDEO,									       1024, 2048,1024,  64,		machine_xt_t1200_init, NULL,			machine_xt_t1x00_close	},
#if defined(DEV_BRANCH) && defined(USE_LASERXT)
    { "[8086] VTech Laser XT3",			ROM_LXT3,		"vtech_lxt3",		L"vtech/lxt3",			{{"", cpus_8086},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA,												256,  512, 256,   0,	      machine_xt_laserxt_init, NULL,			NULL			},
#endif

    { "[286 ISA] AMI 286 clone",		ROM_AMI286,		"ami_286",		L"generic/ami/286",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,	     machine_at_neat_ami_init, NULL,			NULL			},
    { "[286 ISA] Award 286 clone",		ROM_AWARD286,		"award_286",		L"generic/award/286",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,		 machine_at_scat_init, NULL,			NULL			},
    { "[286 ISA] Commodore PC 30 III",		ROM_CMDPC30,		"commodore_pc30",	L"commodore/pc30",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										640,16384, 128, 128,		machine_at_cmdpc_init, NULL,			NULL			},
    { "[286 ISA] Compaq Portable II",		ROM_PORTABLEII,		"compaq_portable2",	L"compaq/portable2",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										640,16384, 128, 128,	       machine_at_compaq_init, NULL,			NULL			},
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    { "[286 ISA] Compaq Portable III",		ROM_PORTABLEIII,	"compaq_portable3",	L"compaq/portable3",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_VIDEO,								640,16384, 128, 128,	       machine_at_compaq_init, NULL,			NULL			},
#endif
    { "[286 ISA] GW-286CT GEAR",		ROM_GW286CT,		"gw286ct",		L"unknown/gw286ct",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,		 machine_at_scat_init, NULL,			NULL			},
    { "[286 ISA] Hyundai Super-286TR",		ROM_SUPER286TR,		"hyundai_super286tr",	L"hyundai/super286tr",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 128, 128,		 machine_at_scat_init, NULL,			NULL			},
    { "[286 ISA] IBM AT",			ROM_IBMAT,		"ibm_at",		L"ibm/at",			{{"", cpus_ibmat},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										256,15872, 128,  64,		  machine_at_ibm_init, NULL,			NULL			},
    { "[286 ISA] IBM PS/1 model 2011",		ROM_IBMPS1_2011,	"ibm_ps1_2011",		L"ibm/ps1_2011",		{{"", cpus_ps1_m2011},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,				512, 6144, 512,  64,	       machine_ps1_m2011_init, &m_ps1_device,		NULL			},
    { "[286 ISA] IBM PS/2 model 30-286",	ROM_IBMPS2_M30_286,	"ibm_ps2_m30_286",	L"ibm/ps2_m30_286",		{{"", cpus_ps2_m30_286},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  1,   16,   1,  64,	     machine_ps2_m30_286_init, NULL,			NULL			},
    { "[286 ISA] IBM XT Model 286",		ROM_IBMXT286,		"ibm_xt286",		L"ibm/xt286",			{{"", cpus_ibmxt286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										256,15872, 128, 128,		  machine_at_ibm_init, NULL,			NULL			},
    { "[286 ISA] Samsung SPC-4200P",		ROM_SPC4200P,		"samsung_spc4200p",	L"samsung/spc4200p",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2,								512, 2048, 128, 128,		 machine_at_scat_init, NULL,			NULL			},
    { "[286 ISA] Samsung SPC-4216P",		ROM_SPC4216P,		"samsung_spc4216p",	L"samsung/spc4216p",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2,								  1,    5,   1, 128,		 machine_at_scat_init, NULL,			NULL			},
    { "[286 ISA] Toshiba 3100e",		ROM_T3100E,		"toshiba_t3100e",	L"toshiba/t3100e",		{{"", cpus_286},		{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,							       1024, 5120, 256,  64,	       machine_at_t3100e_init, NULL,			NULL			},

    { "[286 MCA] IBM PS/2 model 50",		ROM_IBMPS2_M50,		"ibm_ps2_m50",		L"ibm/ps2_m50",			{{"", cpus_ps2_m30_286},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  1,   10,   1,  64,	    machine_ps2_model_50_init, NULL,			NULL			},

    { "[386SX ISA] AMI 386SX clone",		ROM_AMI386SX,		"ami_386",		L"generic/ami/386",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								512,16384, 128, 128,	     machine_at_headland_init, NULL,			NULL			},
    { "[386SX ISA] Amstrad MegaPC",		ROM_MEGAPC,		"amstrad_megapc",	L"amstrad/megapc",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,				  1,   16,   1, 128,	      machine_at_wd76c10_init, NULL,			NULL			},
    { "[386SX ISA] Award 386SX clone",		ROM_AWARD386SX_OPTI495,	"award_386sx",		L"generic/award/opti495",	{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   64,   1, 128,	      machine_at_opti495_init, NULL,   			NULL			},
    { "[386SX ISA] DTK 386SX clone",		ROM_DTK386,		"dtk_386",		L"dtk/386",			{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								512,16384, 128, 128,		 machine_at_neat_init, NULL,			NULL			},
    { "[386SX ISA] IBM PS/1 model 2121",	ROM_IBMPS1_2121,	"ibm_ps1_2121",		L"ibm/ps1_2121",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,    6,   1,  64,	       machine_ps1_m2121_init, &m_ps1_device,		NULL			},
    { "[386SX ISA] IBM PS/1 m.2121+ISA",	ROM_IBMPS1_2121_ISA,	"ibm_ps1_2121_isa",	L"ibm/ps1_2121",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,    6,   1,  64,	       machine_ps1_m2121_init, &m_ps1_device,		NULL			},

    { "[386SX MCA] IBM PS/2 model 55SX",	ROM_IBMPS2_M55SX,	"ibm_ps2_m55sx",	L"ibm/ps2_m55sx",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  1,    8,   1,  64,	  machine_ps2_model_55sx_init, NULL,			NULL			},
    { "[386SX ISA] KMX-C-02",			ROM_KMXC02,		"kmxc02",		L"unknown/kmxc02",		{{"Intel", cpus_i386SX},	{"AMD", cpus_Am386SX},	{"Cyrix", cpus_486SLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT,										512,16384, 512, 128,	       machine_at_scatsx_init, NULL,			NULL			},

    { "[386DX ISA] AMI 386DX clone",		ROM_AMI386DX_OPTI495,	"ami_386dx",		L"generic/ami/386dx",		{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   64,   1, 128,	  machine_at_opti495_ami_init, NULL,			NULL			},
    { "[386DX ISA] Amstrad MegaPC 386DX",	ROM_MEGAPCDX,		"amstrad_megapc_dx",	L"amstrad/megapc",		{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,				  1,   32,   1, 128,	      machine_at_wd76c10_init, NULL,			NULL			},
    { "[386DX ISA] Award 386DX clone",		ROM_AWARD386DX_OPTI495,	"award_386dx",		L"generic/award/opti495",	{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   64,   1, 128,	      machine_at_opti495_init, NULL,			NULL			},
    { "[386DX ISA] MR 386DX clone",		ROM_MR386DX_OPTI495,	"microid_mr386dx",	L"microid/mr386dx",		{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_AT | MACHINE_HDC,								  1,   64,   1, 128,	  machine_at_opti495_ami_init, NULL,			NULL			},
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    { "[386DX ISA] Compaq Portable III (386)",  ROM_PORTABLEIII386,     "portable3_386",	L"compaq/deskpro386",		{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	1, MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,		                		  1,   14,   1, 128,           machine_at_compaq_init, NULL,			NULL			},
#endif

    { "[386DX MCA] IBM PS/2 model 70 (type 3)",	ROM_IBMPS2_M70_TYPE3,	"ibm_ps2_m70_type3",	L"ibm/ps2_m70_type3",		{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  2,   16,   2,  64,  machine_ps2_model_70_type3_init, NULL,			NULL			},
    { "[386DX MCA] IBM PS/2 model 80",		ROM_IBMPS2_M80,		"ibm_ps2_m80",		L"ibm/ps2_m80",			{{"Intel", cpus_i386DX},	{"AMD", cpus_Am386DX},	{"Cyrix", cpus_486DLC},	{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  1,   12,   1,  64,	    machine_ps2_model_80_init, NULL,			NULL			},

    { "[486 ISA] AMI 486 clone",		ROM_AMI486,		"ami_486",		L"generic/ami/486",		{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   64,   1, 128,	      machine_at_ali1429_init, NULL,			NULL			},
    { "[486 ISA] AMI WinBIOS 486",		ROM_WIN486,		"ami_win486",		L"generic/ami/win486",		{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   64,   1, 128,	      machine_at_ali1429_init, NULL,			NULL			},
    { "[486 ISA] Award 486 clone",		ROM_AWARD486_OPTI495,	"award_486",		L"generic/award/opti495",	{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,   64,   1, 128,	      machine_at_opti495_init, NULL,			NULL			},
    { "[486 ISA] DTK PKM-0038S E-2",		ROM_DTK486,		"dtk_486",		L"dtk/486",			{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,						  1,  128,   1, 128,	       machine_at_dtk486_init, NULL,			NULL			},
    { "[486 ISA] IBM PS/1 model 2133",		ROM_IBMPS1_2133,	"ibm_ps1_2133",		L"ibm/ps1_2133",		{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,					  1,   64,   1, 128,	       machine_ps1_m2133_init, NULL,			NULL			},
    { "[486 MCA] IBM PS/2 model 70 (type 4)",	ROM_IBMPS2_M70_TYPE4,	"ibm_ps2_m70_type4",	L"ibm/ps2_m70_type4",		{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	1, MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC_PS2,						  2,   16,   2,  64,  machine_ps2_model_70_type4_init, NULL,			NULL			},

    { "[486 PCI] Rise Computer R418",		ROM_R418,		"rise_r418",		L"rise/r418",			{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  1,  255,   1, 128,		 machine_at_r418_init, NULL,			NULL			},

#if defined(DEV_BRANCH) && defined(USE_GREENB)
    { "[486 VLB] Green-B 4GP V3.1",		ROM_4GPV31,		"addtech_4gpv31",	L"addtech/4gpv31",		{{"Intel", cpus_i486},		{"AMD", cpus_Am486},	{"Cyrix", cpus_Cx486},	{"", NULL},		{"", NULL}},	0, MACHINE_ISA | MACHINE_VLB | MACHINE_AT,							          1,  128,   1, 128,	       machine_at_4gpv31_init, NULL,			NULL			},
#endif

    { "[Socket 4 LX] Intel Premiere/PCI",	ROM_REVENGE,		"intel_revenge",	L"intel/revenge",		{{"Intel", cpus_Pentium5V},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  2,  128,   2, 128,	       machine_at_batman_init, NULL,			NULL			},

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
    { "[Socket 5 NX] Intel Premiere/PCI II",	ROM_PLATO,		"intel_plato",		L"intel/plato",			{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  2,  128,   2, 128,		machine_at_plato_init, NULL,			NULL			},

    { "[Socket 5 FX] ASUS P/I-P54TP4XE",	ROM_P54TP4XE,		"asus_p54tp4xe",	L"asus/p54tp4xe",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	     machine_at_p54tp4xe_init, NULL,			NULL			},
    { "[Socket 5 FX] Intel Advanced/EV",	ROM_ENDEAVOR,		"intel_endeavor",	L"intel/endeavor",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,	  8,  128,   8, 128,	     machine_at_endeavor_init, &s3_phoenix_trio64_onboard_pci_device,	NULL			},
    { "[Socket 5 FX] Intel Advanced/ZP",	ROM_ZAPPA,		"intel_zappa",		L"intel/zappa",			{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		machine_at_zappa_init, NULL,			NULL			},
    { "[Socket 5 FX] PC Partner MB500N",	ROM_MB500N,		"pcpartner_mb500n",	L"pcpartner/mb500n",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	       machine_at_mb500n_init, NULL,			NULL			},
    { "[Socket 5 FX] President Award 430FX PCI",ROM_PRESIDENT,		"president",		L"president/president",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"AMD", cpus_K5},	{"", NULL},		{"", NULL}}, 	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	    machine_at_president_init, NULL,			NULL			},

    { "[Socket 7 FX] Intel Advanced/ATX",	ROM_THOR,		"intel_thor",		L"intel/thor",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		 machine_at_thor_init, NULL,			NULL			},
    { "[Socket 7 FX] MR Intel Advanced/ATX",	ROM_MRTHOR,		"intel_mrthor",		L"intel/mrthor",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		 machine_at_thor_init, NULL,			NULL			},

    { "[Socket 7 HX] Acer M3a",			ROM_ACERM3A,		"acer_m3a",		L"acer/m3a",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	      machine_at_acerm3a_init, NULL,			NULL			},
    { "[Socket 7 HX] Acer V35n",		ROM_ACERV35N,		"acer_v35n",		L"acer/v35n",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	     machine_at_acerv35n_init, NULL,			NULL			},
    { "[Socket 7 HX] AOpen AP53",		ROM_AP53,		"aopen_ap53",		L"aopen/ap53",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,		 machine_at_ap53_init, NULL,			NULL			},
    { "[Socket 7 HX] ASUS P/I-P55T2P4",		ROM_P55T2P4,		"asus_p55t2p4",		L"asus/p55t2p4",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,	      machine_at_p55t2p4_init, NULL,			NULL			},
    { "[Socket 7 HX] SuperMicro Super P55T2S",	ROM_P55T2S,		"supermicro_p55t2s",	L"supermicro/p55t2s",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  768,   8, 128,	       machine_at_p55t2s_init, NULL,			NULL			},

    { "[Socket 7 VX] ASUS P/I-P55TVP4",		ROM_P55TVP4,		"asus_p55tvp4",		L"asus/p55tvp4",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	      machine_at_p55tvp4_init, NULL,			NULL			},
    { "[Socket 7 VX] Award 430VX PCI",		ROM_430VX,		"award_430vx",		L"generic/award/430vx",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	       machine_at_i430vx_init, NULL,			NULL			},
    { "[Socket 7 VX] Epox P55-VA",		ROM_P55VA,		"epox_p55va",		L"epox/p55va",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"AMD", cpus_K56},	{"Cyrix", cpus_6x86},	{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		machine_at_p55va_init, NULL,			NULL			},
#else 
    { "[Socket 5 NX] Intel Premiere/PCI II",	ROM_PLATO,		"intel_plato",		L"intel/plato",			{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  2,  128,   2, 128,		machine_at_plato_init, NULL,			NULL			},

    { "[Socket 5 FX] ASUS P/I-P54TP4XE",	ROM_P54TP4XE,		"asus_p54tp4xe",	L"asus/p54tp4xe",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	     machine_at_p54tp4xe_init, NULL,			NULL			},
    { "[Socket 5 FX] Intel Advanced/EV",	ROM_ENDEAVOR,		"intel_endeavor",	L"intel/endeavor",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,	  8,  128,   8, 128,	     machine_at_endeavor_init, &s3_phoenix_trio64_onboard_pci_device,	NULL			},
    { "[Socket 5 FX] Intel Advanced/ZP",	ROM_ZAPPA,		"intel_zappa",		L"intel/zappa",			{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		machine_at_zappa_init, NULL,			NULL			},
    { "[Socket 5 FX] PC Partner MB500N",	ROM_MB500N,		"pcpartner_mb500n",	L"pcpartner/mb500n",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	       machine_at_mb500n_init, NULL,			NULL			},
    { "[Socket 5 FX] President Award 430FX PCI",ROM_PRESIDENT,		"president",		L"president/president",		{{"Intel", cpus_PentiumS5},	{"IDT", cpus_WinChip},	{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,					  8,  128,   8, 128,	    machine_at_president_init, NULL,			NULL			},

    { "[Socket 7 FX] Intel Advanced/ATX",	ROM_THOR,		"intel_thor",		L"intel/thor",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		 machine_at_thor_init, NULL,			NULL			},
    { "[Socket 7 FX] MR Intel Advanced/ATX",	ROM_MRTHOR,		"intel_mrthor",		L"intel/mrthor",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		 machine_at_thor_init, NULL,			NULL			},

    { "[Socket 7 HX] Acer M3a",			ROM_ACERM3A,		"acer_m3a",		L"acer/m3a",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	      machine_at_acerm3a_init, NULL,			NULL			},
    { "[Socket 7 HX] Acer V35n",		ROM_ACERV35N,		"acer_v35n",		L"acer/v35n",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  192,   8, 128,	     machine_at_acerv35n_init, NULL,			NULL			},
    { "[Socket 7 HX] AOpen AP53",		ROM_AP53,		"aopen_ap53",		L"aopen/ap53",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,		 machine_at_ap53_init, NULL,			NULL			},
    { "[Socket 7 HX] ASUS P/I-P55T2P4",		ROM_P55T2P4,		"asus_p55t2p4",		L"asus/p55t2p4",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  512,   8, 128,	      machine_at_p55t2p4_init, NULL,			NULL			},
    { "[Socket 7 HX] SuperMicro Super P55T2S",	ROM_P55T2S,		"supermicro_p55t2s",	L"supermicro/p55t2s",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  768,   8, 128,	       machine_at_p55t2s_init, NULL,			NULL			},

    { "[Socket 7 VX] ASUS P/I-P55TVP4",		ROM_P55TVP4,		"asus_p55tvp4",		L"asus/p55tvp4",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	      machine_at_p55tvp4_init, NULL,			NULL			},
    { "[Socket 7 VX] Award 430VX PCI",		ROM_430VX,		"award_430vx",		L"generic/award/430vx",		{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,	       machine_at_i430vx_init, NULL,			NULL			},
    { "[Socket 7 VX] Epox P55-VA",		ROM_P55VA,		"epox_p55va",		L"epox/p55va",			{{"Intel", cpus_Pentium},	{"IDT", cpus_WinChip},	{"Cyrix", cpus_6x86},	{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,			  8,  128,   8, 128,		machine_at_p55va_init, NULL,			NULL			},
#endif

#if defined(DEV_BRANCH) && defined(USE_I686)
    { "[Socket 8 FX] Tyan Titan-Pro AT",	ROM_440FX,		"tyan_440fx",		L"tyan/440fx",			{{"Intel", cpus_PentiumPro},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,					  8, 1024,   8, 128,	       machine_at_i440fx_init, NULL,			NULL			},
    { "[Socket 8 FX] Tyan Titan-Pro ATX",	ROM_S1668,		"tyan_tpatx",		L"tyan/tpatx",			{{"Intel", cpus_PentiumPro},	{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,					  8, 1024,   8, 128,		machine_at_s1668_init, NULL,			NULL			},
#endif
    { NULL,					-1,			NULL,			NULL,				{{"", NULL},			{"", NULL},		{"", NULL},		{"", NULL},		{"", NULL}},	0, 0,													  0,    0,   0, 0,				 NULL, NULL,			NULL			}
};


int
machine_count(void)
{
    return((sizeof(machines) / sizeof(machine_t)) - 1);
}


int
machine_getromset(void)
{
    return(machines[machine].id);
}


int
machine_getromset_ex(int m)
{
    return(machines[m].id);
}


int
machine_getmachine(int rs)
{
    int c = 0;

    while (machines[c].id != -1) {
	if (machines[c].id == romset)
		return(c);
	c++;
    }

    /* Not found. */
    return(-1);
}


const char *
machine_getname(void)
{
    return(machines[machine].name);
}


const device_t *
machine_getdevice(int m)
{
    return(machines[m].device);
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
machine_get_machine_from_internal_name(const char *s)
{
    int c = 0;

    while (machines[c].id != -1) {
	if (!strcmp(machines[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(-1);
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
