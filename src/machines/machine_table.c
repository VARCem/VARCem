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
 * Version:	@(#)machine_table.c	1.0.46	2020/06/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../nvr.h"
#include "../rom.h"
#include "../device.h"
#include "machine.h"


static const struct {
    const char		*name;
    const char		*internal_name;
    const device_t	*device;
} machines[] = {
    /* 8088 */
    { "[8088] IBM PC (1981)",			"ibm_pc",		&m_pc81			},
    { "[8088] IBM PC (1982)",			"ibm_pc82",		&m_pc82			},
    { "[8088] IBM PCjr",			"ibm_pcjr",		&m_pcjr			},
    { "[8088] IBM XT (1982)",			"ibm_xt",		&m_xt82			},
    { "[8088] IBM XT (1986)",			"ibm_xt86",		&m_xt86			},

    { "[8088] AMI XT (generic)",		"ami_xt",		&m_xt_ami		},
    { "[8088] Award XT (generic)",		"awd_xt",		&m_xt_award		},
    { "[8088] Phoenix XT (generic)",		"phoenix_xt",		&m_xt_phoenix		},

    { "[8088] OpenXT (generic)",		"open_xt",		&m_xt_openxt		},

#if defined(DEV_BRANCH) && defined(USE_COMPAQ)
    { "[8088] Compaq Portable",			"compaq_portable",	&m_cpq_p1		},
#endif
    { "[8088] DTK XT",				"dtk_xt",		&m_xt_dtk		},
    { "[8088] Juko XT",				"juko_pc",		&m_xt_juko		},
    { "[8088] Schneider EuroPC",		"schneider_europc",	&m_europc		},
    { "[8088] Tandy 1000",			"tandy_1000",		&m_tandy_1k		},
    { "[8088] Tandy 1000 HX",			"tandy_1000hx",		&m_tandy_1k_hx		},
    { "[8088] Toshiba T1000",			"toshiba_t1000",	&m_tosh_1000		},
    { "[8088] VTech Laser Turbo XT",		"vtech_ltxt",		&m_laser_xt		},
    { "[8088] Xi8088",				"malinov_xi8088",	&m_xi8088		},
#if defined(DEV_BRANCH) && defined(USE_SUPERSPORT)
    { "[8088] Zenith SupersPORT",		"zenith_supersport",	&m_zenith_ss		},
#endif

    /* 8086 */
    { "[8086] Amstrad PC1512",			"amstrad_pc1512",	&m_amstrad_1512		},
    { "[8086] Amstrad PC1640",			"amstrad_pc1640",	&m_amstrad_1640		},
    { "[8086] Amstrad PC2086",			"amstrad_pc2086",	&m_amstrad_2086		},
    { "[8086] Amstrad PC3086",			"amstrad_pc3086",	&m_amstrad_3086		},
    { "[8086] Amstrad PC20(0)",			"amstrad_pc200",	&m_amstrad_200		},
    { "[8086] Amstrad PPC512/640",		"amstrad_ppc512",	&m_amstrad_ppc		},
    { "[8086] AT&T PC6300",			"att_6300",		&m_att_6300		},
    { "[8086] Olivetti M24",			"olivetti_m24",		&m_oli_m24		},
    { "[8086] Olivetti M240",			"olivetti_m240",	&m_oli_m240		},
    { "[8086] Tandy 1000 SL/2",			"tandy_1000sl2",	&m_tandy_1k_sl2		},
    { "[8086] Toshiba T1200",			"toshiba_t1200",	&m_tosh_1200		},
    { "[8086] VTech Laser XT/3",		"vtech_lxt3",		&m_laser_xt3		},

    /* 80286 */
    { "[286 ISA] IBM AT",			"ibm_at",		&m_at			},
    { "[286 ISA] IBM XT Model 286",		"ibm_xt286",		&m_xt286		},
    { "[286 ISA] IBM PS/1 model 2011",		"ibm_ps1_2011",		&m_ps1_2011		},
    { "[286 ISA] IBM PS/2 model 30-286",	"ibm_ps2_m30_286",	&m_ps2_m30_286		},

    { "[286 ISA] AMI 286 (NEAT)",		"ami_286",		&m_neat_ami		},
    { "[286 ISA] Award 286 (SCAT)",		"award_286",		&m_scat_award		},

#if defined(DEV_BRANCH) && defined(USE_MICRAL)
    { "[286 ISA] Bull Micral 45",		"bull_micral45",	&m_bull_micral45	},
#endif
    { "[286 ISA] Commodore PC-30",		"commodore_pc30",	&m_cbm_pc30		},
#if defined(DEV_BRANCH) && defined(USE_COMPAQ)
    { "[286 ISA] Compaq Portable (286)",	"compaq_portable286",	&m_cpq_p1_286		},
    { "[286 ISA] Compaq Portable II",		"compaq_portable2",	&m_cpq_p2		},
    { "[286 ISA] Compaq Portable III",		"compaq_portable3",	&m_cpq_p3		},
#endif
    { "[286 ISA] Gear GW-286CT",		"gw286ct",		&m_gw286ct		},
    { "[286 ISA] Hyundai Super-286TR",		"hyundai_super286tr",	&m_hs286tr		},
    { "[286 ISA] Samsung SPC-4200P",		"samsung_spc4200p",	&m_spc4200p		},
    { "[286 ISA] Samsung SPC-4216P",		"samsung_spc4216p",	&m_spc4216p		},
    { "[286 ISA] Toshiba T3100e",		"toshiba_t3100e",	&m_tosh_3100e		},
    { "[286 ISA] Trigem 286M",			"tg286m",		&m_tg286m		},
	
    { "[286 MCA] IBM PS/2 model 50",		"ibm_ps2_m50",		&m_ps2_m50		},

    /* 80386SX */
    { "[386SX ISA] IBM PS/1 model 2121",	"ibm_ps1_2121",		&m_ps1_2121		},
    { "[386SX ISA] IBM PS/1 m.2121+ISA",	"ibm_ps1_2121_isa",	&m_ps1_2121		},

    { "[386SX ISA] AMI 386SX (Headland)",	"ami_386sx",		&m_headland_386_ami	},
    { "[386SX ISA] AMI 386SX (OPTi495)",	"ami_386sx_opti495",	&m_opti495_386sx_ami	},
    { "[386SX ISA] Award 386SX (Opti495)",	"award_386sx_opti495",	&m_opti495_386sx_award	},
    { "[386SX ISA] MR 386SX (OPTi495)",		"mr_386sx_opti495",	&m_opti495_386sx_mr	},

    { "[386SX ISA] Amstrad MegaPC",		"amstrad_megapc",	&m_amstrad_mega_sx	},
    { "[386SX ISA] Arche AMA-932J",		"arche_ama932j",	&m_ama932j		},
    { "[386SX ISA] DTK 386SX clone",		"dtk_386",		&m_neat_dtk		},
    { "[386SX ISA] KMX-C-02",			"kmxc02",		&m_kmxc02		},

    { "[386SX MCA] IBM PS/2 model 55SX",	"ibm_ps2_m55sx",	&m_ps2_m55sx		},

    /* 80386DX */
    { "[386DX ISA] AMI 386DX (Opti495)",	"ami_386dx_opti495",	&m_opti495_386dx_ami	},
    { "[386DX ISA] Award 386DX (Opti495)",	"award_386dx_opti495",	&m_opti495_386dx_award	},
    { "[386DX ISA] MR 386DX (Opti495)",		"mr_386dx_opti495",	&m_opti495_386dx_mr	},

    { "[386DX ISA] Amstrad MegaPC 386DX",	"amstrad_megapc_dx",	&m_amstrad_mega_dx	},
#if defined(DEV_BRANCH) && defined(USE_COMPAQ)
    { "[386DX ISA] Compaq Portable III (386)",  "portable3_386",	&m_cpq_p3_386		},
#endif

    { "[386DX MCA] IBM PS/2 model 70 (type 3)",	"ibm_ps2_m70_type3",	&m_ps2_m70_3		},
    { "[386DX MCA] IBM PS/2 model 80",		"ibm_ps2_m80",		&m_ps2_m80		},

    /* 80486 */
    { "[486 ISA] IBM PS/1 model 2133",		"ibm_ps1_2133",		&m_ps1_2133		},

    { "[486 ISA] Olystar LIL1429 (ALi-1429)",	"ami_486_ali1429",	&m_ali1429_ami		},
#if defined(DEV_BRANCH) && defined(USE_SIS471)
    { "[486 ISA] AMI 486 (SiS471)",		"ami_486_sis471",	&m_sis471_ami		},
#endif
    { "[486 ISA] AMI WinBIOS486 (ALi1429)",	"ami_win486_ali1429",	&m_ali1429_win		},
    { "[486 ISA] Award 486 (Opti495)",		"award_486_opti495",	&m_opti495_486_award	},
    { "[486 ISA] MR 486 (Opti495)",		"mr_486dx_opti495",	&m_opti495_486_mr	},

    { "[486 ISA] DTK PKM-0038S E-2",		"dtk_486",		&m_dtk486		},
    { "[486 ISA] Packard Bell PB410A",		"pbell_pb410a",		&m_pb410a		},

    { "[486 MCA] IBM PS/2 model 70 (type 4)",	"ibm_ps2_m70_type4",	&m_ps2_m70_4		},

#if defined(DEV_BRANCH) && defined(USE_SIS496)
    { "[486 PCI] AMI 486 (SiS496)",		"ami_486_sis496",	&m_sis496_ami		},
#endif
    { "[486 PCI] Rise Computer R418",		"rise_r418",		&m_rise418		},

    /* Pentium, Socket4 (LX) */
    { "[Socket 4 LX] Ambra Achiever DP60PCI",	"ambra_dp60",		&m_ambradp60		},
    { "[Socket 4 LX] Intel Premiere/PCI",	"intel_revenge",	&m_batman		},

    /* Pentium, Socket5 (NX) */
    { "[Socket 5 NX] Ambra Achiever DP90   ",	"ambra_dp90",		&m_ambradp90		},
    { "[Socket 5 NX] Intel Premiere/PCI II",	"intel_plato",		&m_plato		},

    /* Pentium, Socket5 (FX) */
    { "[Socket 5 FX] ASUS P/I-P54TP4XE",	"asus_p54tp4xe",	&m_p54tp4xe		},
    { "[Socket 5 FX] Intel Advanced/ZP",	"intel_zappa",		&m_zappa		},
    { "[Socket 5 FX] PC Partner MB500N",	"pcpartner_mb500n",	&m_mb500n		},
    { "[Socket 5 FX] President Award 430FX PCI","president",		&m_president		},

    /* Pentium, Socket7 (FX) */
    { "[Socket 7 FX] Intel Advanced/ATX",	"intel_thor",		&m_thor			},
    { "[Socket 7 FX] MR Intel Advanced/ATX",	"intel_thor_mr",	&m_thor_mr		},
    { "[Socket 7 FX] Intel Advanced/EV",	"intel_endeavor",	&m_endeavor		},
    { "[Socket 7 FX] Acer V30",			"acer_v30",		&m_acer_v30		},
    { "[Socket 7 FX] Packard Bell PB640",	"pbell_pb640",		&m_pb640		},

    /* Pentium, Socket7 (HX) */
    { "[Socket 7 HX] Acer M3a",			"acer_m3a",		&m_acer_m3a		},
    { "[Socket 7 HX] Acer V35n",		"acer_v35n",		&m_acer_v35n		},
    { "[Socket 7 HX] AOpen AP53",		"aopen_ap53",		&m_ap53			},
    { "[Socket 7 HX] ASUS P/I-P55T2P4",		"asus_p55t2p4",		&m_p55t2p4		},
    { "[Socket 7 HX] SuperMicro Super P55T2S",	"supermicro_p55t2s",	&m_p55t2s		},

    /* Pentium, Socket7 (VX) */
    { "[Socket 7 VX] ASUS P/I-P55TVP4",		"asus_p55tvp4",		&m_p55tvp4		},
    { "[Socket 7 VX] Epox P55-VA",		"epox_p55va",		&m_p55va		},
    { "[Socket 7 VX] Jetway J656VXD",		"jetway_j656vxd",	&m_j656vxd		},
    { "[Socket 7 VX] Shuttle HOT-557",		"award_430vx",		&m_aw430vx		},

    /* Pentium Pro, Socket 8 (FX) */
    { "[Socket 8 FX] Tyan Titan-Pro AT (S1662, AMI)","tyan_s1662_ami",		&m_tyan_1662_ami	},
    { "[Socket 8 FX] Tyan Titan-Pro AT (S1662, Award)","tyan_s1662_award",	&m_tyan_1662_award	},
    { "[Socket 8 FX] Tyan Titan-Pro ATX (S1668, AMI)","tyan_s1668_ami",		&m_tyan_1668_ami	},
    { "[Socket 8 FX] Tyan Titan-Pro ATX (S1668), Award","tyan_s1668_award",	&m_tyan_1668_award	},

    { NULL,					NULL,			NULL			 }
};


int
machine_get_from_internal_name(const char *s)
{
    int c;

    for (c = 0; machines[c].internal_name != NULL; c++) {
	if (! strcmp(machines[c].internal_name, s))
		return(c);
    }

    /* Not found. */
    return(-1);
}


const char *
machine_get_name_ex(int m)
{
    return(machines[m].name);
}


const char *
machine_get_name(void)
{
    return(machine_get_name_ex(config.machine_type));
}


const char *
machine_get_internal_name_ex(int m)
{
    return(machines[m].internal_name);
}


const char *
machine_get_internal_name(void)
{
    return(machine_get_internal_name_ex(config.machine_type));
}


const device_t *
machine_get_device_ex(int m)
{
    return(machines[m].device);
}


const device_t *
machine_get_device(void)
{
    return(machine_get_device_ex(config.machine_type));
}


int
machine_get_config_int(const char *s)
{
    const device_t *d = machine_get_device();
    const device_config_t *c;

    if (d == NULL) return(0);

    for (c = d->config; c && c->name; c++) {
	if (! strcmp(s, c->name))
		return(config_get_int(d->name, s, c->default_int));
    }

    return(0);
}


const char *
machine_get_config_string(const char *s)
{
    const device_t *d = machine_get_device();
    const device_config_t *c;

    if (d == NULL) return(0);

    for (c = d->config; c && c->name; c++) {
	if (! strcmp(s, c->name))
		return(config_get_string(d->name, s, c->default_string));
    }

    return(NULL);
}
