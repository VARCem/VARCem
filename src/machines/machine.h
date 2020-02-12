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
 * Version:	@(#)machine.h	1.0.35	2020/02/12
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
#ifndef EMU_MACHINE_H
# define EMU_MACHINE_H


/* Machine feature flags. */
#define MACHINE_PC		0x000000	/* PC architecture */
#define MACHINE_AT		0x000001	/* PC/AT architecture */
#define MACHINE_PS2		0x000002	/* PS/2 architecture */
#define MACHINE_NONMI		0x000004	/* sys does not have NMI's */
#define MACHINE_ISA		0x000010	/* sys has ISA bus */
#define MACHINE_CBUS		0x000020	/* sys has C-BUS bus */
#define MACHINE_EISA		0x000040	/* sys has EISA bus */
#define MACHINE_VLB		0x000080	/* sys has VL bus */
#define MACHINE_MCA		0x000100	/* sys has MCA bus */
#define MACHINE_PCI		0x000200	/* sys has PCI bus */
#define MACHINE_AGP		0x000400	/* sys has AGP bus */
#define MACHINE_FDC		0x010000	/* sys has int FDC */
#define MACHINE_HDC		0x020000	/* sys has int HDC */
#define MACHINE_HDC_PS2		0x040000	/* sys has int PS/2 HDC */
#define MACHINE_MOUSE		0x080000	/* sys has int mouse */
#define MACHINE_SOUND		0x100000	/* sys has int sound */
#define MACHINE_VIDEO		0x200000	/* sys has int video */
#define MACHINE_SCSI		0x400000	/* sys has int SCSI */
#define MACHINE_NETWORK		0x800000	/* sys has int network */

#define IS_ARCH(a)		(machine->flags & (a)) ? 1 : 0;


typedef struct {
    uint32_t		flags,
			flags_fixed;
    uint32_t		min_ram,
			max_ram;
    int			ram_granularity;
    int			nvrsz;
    int			slow_mhz;

    struct {
	const char	*name;
#ifdef EMU_CPU_H
	const CPU	*cpus;
#else
	void		*cpus;
#endif
    }			cpu[5];
} machine_t;


/* Global variables. */
#ifdef EMU_DEVICE_H
extern const device_t	m_pc81,
			m_pc82;

extern const device_t	m_xt82,
			m_xt86;

extern const device_t	m_xt_ami,
			m_xt_award,
			m_xt_phoenix;
extern const device_t	m_xt_openxt;

extern const device_t	m_xt_dtk;
extern const device_t	m_xt_juko;

extern const device_t	m_pcjr;

extern const device_t	m_amstrad_1512;
extern const device_t	m_amstrad_1640;
extern const device_t	m_amstrad_200;
extern const device_t	m_amstrad_ppc;
extern const device_t	m_amstrad_2086;
extern const device_t	m_amstrad_3086;

extern const device_t	m_amstrad_mega_sx;
extern const device_t	m_amstrad_mega_dx;

extern const device_t	m_europc;

extern const device_t	m_laser_xt;
extern const device_t	m_laser_xt3;

extern const device_t	m_oli_m24;
extern const device_t	m_oli_m240;
extern const device_t	m_att_6300;

extern const device_t	m_tandy_1k;
extern const device_t	m_tandy_1k_hx;
extern const device_t	m_tandy_1k_sl2;

extern const device_t	m_tosh_1000;
extern const device_t	m_tosh_1200;

extern const device_t	m_xi8088;

extern const device_t	m_zenith_ss;

extern const device_t	m_at;
extern const device_t	m_xt286;

extern const device_t	m_neat_ami;
extern const device_t	m_neat_dtk;

#if defined(DEV_BRANCH) && defined(USE_MICRAL)
extern const device_t	m_bull_micral45;
#endif

extern const device_t	m_tg286m;
extern const device_t	m_headland_386_ami;
extern const device_t	m_ama932j;

extern const device_t	m_scat_award;
extern const device_t	m_hs286tr;
extern const device_t	m_gw286ct;
extern const device_t	m_spc4200p;
extern const device_t	m_spc4216p;
extern const device_t	m_kmxc02;

extern const device_t	m_tosh_3100e;

extern const device_t	m_cbm_pc30;

#if defined(DEV_BRANCH) && defined(USE_COMPAQ)
extern const device_t	m_cpq_p1;
extern const device_t	m_cpq_p1_286;
extern const device_t	m_cpq_p2;
extern const device_t	m_cpq_p3;
extern const device_t	m_cpq_p3_386;
extern const device_t	m_cpq_dp_386;
#endif

extern const device_t	m_ps1_2011;
extern const device_t	m_ps1_2121;
extern const device_t	m_ps1_2133;

extern const device_t	m_ps2_m30_286;

extern const device_t	m_ps2_m50;
extern const device_t	m_ps2_m55sx;
extern const device_t	m_ps2_m70_3;
extern const device_t	m_ps2_m70_4;
extern const device_t	m_ps2_m80;

extern const device_t	m_ali1429_ami;
extern const device_t	m_ali1429_win;

extern const device_t	m_opti495_386sx_ami;
extern const device_t	m_opti495_386dx_ami;
extern const device_t	m_opti495_486_ami;
extern const device_t	m_opti495_386sx_award;
extern const device_t	m_opti495_386dx_award;
extern const device_t	m_opti495_486_award;
extern const device_t	m_opti495_386sx_mr;
extern const device_t	m_opti495_386dx_mr;
extern const device_t	m_opti495_486_mr;

#if defined(DEV_BRANCH) && defined(USE_SIS471)
extern const device_t	m_sis471_ami;
#endif
extern const device_t	m_dtk486;

#if defined(DEV_BRANCH) && defined(USE_SIS496)
extern const device_t	m_sis496_ami;
#endif
extern const device_t	m_rise418;

/* Acer boards */
extern const device_t	m_acer_m3a;
extern const device_t	m_acer_v30;
extern const device_t	m_acer_v35n;

/* Ambra machines */
extern const device_t	m_ambradp60;
extern const device_t	m_ambradp90;

/* Intel boards */
extern const device_t	m_batman;
extern const device_t	m_plato;
extern const device_t	m_endeavor;
extern const device_t	m_zappa;
extern const device_t	m_thor;
extern const device_t	m_thor_mr;

/* Asus Boards */
extern const device_t	m_p54tp4xe;
extern const device_t	m_p55t2p4;
extern const device_t	m_p55tvp4;

extern const device_t	m_ap53;
extern const device_t	m_mb500n;
extern const device_t	m_president;
extern const device_t	m_aw430vx;
extern const device_t	m_p55va;
extern const device_t	m_j656vxd;
extern const device_t	m_p55t2s;

/* Packard Bell machines */
extern const device_t	m_pb410a;
extern const device_t	m_pb640;

/* Tyan boards */
extern const device_t	m_tyan_1662_ami;
extern const device_t	m_tyan_1662_award;
extern const device_t	m_tyan_1668_ami;
extern const device_t	m_tyan_1668_award;

#endif


/* Core functions. */
extern int		machine_get_from_internal_name(const char *s);
extern const char	*machine_get_name(void);
extern const char	*machine_get_internal_name(void);
#ifdef EMU_DEVICE_H
extern const device_t	*machine_get_device(void);
#endif
extern int		machine_get_flags(void);
extern int		machine_get_flags_fixed(void);
extern int		machine_get_maxmem(void);
extern int		machine_get_memsize(int memsz);
extern int		machine_get_nvrsize(void);
extern uint32_t		machine_get_speed(int turbo);

extern const char	*machine_get_name_ex(int m);
extern const char	*machine_get_internal_name_ex(int m);
#ifdef EMU_DEVICE_H
extern const device_t	*machine_get_device_ex(int m);
extern const device_t	*machine_load(void);
#endif
extern int		machine_available(int m);
extern void		machine_reset(void);
extern int		machine_get_config_int(const char *s);
extern const char	*machine_get_config_string(const char *s);


/* Initialization functions for boards and systems. */
extern void		machine_common_init(void);

/* Functions shared by other machines. */
extern void		m_xt_refresh_timer(int new_out, int old_out);

extern void		m_at_refresh_timer(int new_out, int old_out);
extern void		m_at_common_init(void);
extern void		m_at_init(void);
extern void		m_at_common_ide_init(void);
extern void		m_at_ide_init(void);

extern void		m_at_ps2_init(void);
extern void		m_at_ps2_ide_init(void);


#endif	/*EMU_MACHINE_H*/
