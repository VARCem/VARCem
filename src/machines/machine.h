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
 * Version:	@(#)machine.h	1.0.24	2018/09/21
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
#define MACHINE_HDC		0x010000	/* sys has int HDC */
#define MACHINE_HDC_PS2		0x020000	/* sys has int PS/2 HDC */
#define MACHINE_MOUSE		0x040000	/* sys has int mouse */
#define MACHINE_SOUND		0x080000	/* sys has int sound */
#define MACHINE_VIDEO		0x100000	/* sys has int video */

#define IS_ARCH(m, a)		(machines[(m)].flags & (a)) ? 1 : 0;


typedef struct _machine_ {
    const char		*name;
    const char		*internal_name;
    const wchar_t	*bios_path;
    struct {
	const char	*name;
#ifdef EMU_CPU_H
	CPU		*cpus;
#else
	void		*cpus;
#endif
    }			cpu[5];
    int			fixed_vidcard;	/* FIXME: change to "fixed_flags" */
    int			flags;
    uint32_t		min_ram,
			max_ram;
    int			ram_granularity;
    int			nvrsz;
    void		(*init)(const struct _machine_ *, void *);
#ifdef EMU_DEVICE_H
    const device_t	*device;
#else
    const void		*device;
#endif
    void		(*close)(void);
} machine_t;


/* Global variables. */
extern const machine_t	machines[];
extern int		AT, PCI;


/* Core functions. */
extern const char	*machine_getname(void);
extern const char	*machine_getname_ex(int id);
extern const char	*machine_get_internal_name(void);
extern const char	*machine_get_internal_name_ex(int m);
extern int		machine_get_from_internal_name(const char *s);
extern int		machine_available(int id);
extern void		machine_reset(void);
extern void		machine_close(void);
extern uint32_t		machine_speed(void);
#ifdef EMU_DEVICE_H
extern const device_t	*machine_getdevice(int machine);
#endif
extern int		machine_get_config_int(const char *s);
extern const char	*machine_get_config_string(const char *s);


/* Initialization functions for boards and systems. */
extern void		machine_common_init(const machine_t *, void *);

extern void		machine_at_common_init(const machine_t *, void *);
extern void		machine_at_init(const machine_t *, void *);
extern void		machine_at_ibm_init(const machine_t *, void *);
extern void		machine_at_ps2_init(const machine_t *, void *);
extern void		machine_at_common_ide_init(const machine_t *, void *);
extern void		machine_at_ide_init(const machine_t *, void *);
extern void		machine_at_ps2_ide_init(const machine_t *, void *);

extern void		machine_at_t3100e_init(const machine_t *, void *);

extern void		machine_at_p54tp4xe_init(const machine_t *, void *);
extern void		machine_at_endeavor_init(const machine_t *, void *);
extern void		machine_at_zappa_init(const machine_t *, void *);
extern void		machine_at_mb500n_init(const machine_t *, void *);
extern void		machine_at_president_init(const machine_t *, void *);
extern void		machine_at_thor_init(const machine_t *, void *);
extern void		machine_at_pb640_init(const machine_t *, void *);

extern void		machine_at_acerm3a_init(const machine_t *, void *);
extern void		machine_at_acerv35n_init(const machine_t *, void *);
extern void		machine_at_ap53_init(const machine_t *, void *);
extern void		machine_at_p55t2p4_init(const machine_t *, void *);
extern void		machine_at_p55t2s_init(const machine_t *, void *);

extern void		machine_at_batman_init(const machine_t *, void *);
extern void		machine_at_plato_init(const machine_t *, void *);

extern void		machine_at_p55tvp4_init(const machine_t *, void *);
extern void		machine_at_i430vx_init(const machine_t *, void *);
extern void		machine_at_p55va_init(const machine_t *, void *);
extern void		machine_at_j656vxd_init(const machine_t *, void *);

#if defined(DEV_BRANCH) && defined(USE_I686)
extern void		machine_at_i440fx_init(const machine_t *, void *);
extern void		machine_at_s1668_init(const machine_t *, void *);
#endif
extern void		machine_at_ali1429_init(const machine_t *, void *);
extern void		machine_at_cmdpc_init(const machine_t *, void *);

extern void		machine_at_tg286m_init(const machine_t *, void *);
extern void		machine_at_ama932j_init(const machine_t *, void *);
extern void		machine_at_neat_init(const machine_t *, void *);
extern void		machine_at_neat_ami_init(const machine_t *, void *);

extern void		machine_at_opti495_init(const machine_t *, void *);
extern void		machine_at_opti495_ami_init(const machine_t *, void *);

extern void		machine_at_scat_init(const machine_t*, void*);
extern void		machine_at_scat_gw286ct_init(const machine_t*, void*);
extern void		machine_at_scat_spc4216p_init(const machine_t*, void*);

extern void		machine_at_scatsx_init(const machine_t*, void*);

extern void		machine_at_compaq_p1_init(const machine_t*, void*);
extern void		machine_at_compaq_p2_init(const machine_t*, void*);
extern void		machine_at_compaq_p3_init(const machine_t*, void*);
extern void		machine_at_compaq_p3_386_init(const machine_t*, void*);

extern void		machine_at_dtk486_init(const machine_t *, void *);

extern void		machine_at_r418_init(const machine_t *, void *);

extern void		machine_at_wd76c10_init(const machine_t *, void *);

extern void		machine_pcjr_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_pcjr_device;
#endif

extern void		machine_ps1_m2011_init(const machine_t *, void *);
extern void		machine_ps1_m2121_init(const machine_t *, void *);
extern void		machine_ps1_m2133_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern void		ps1_hdc_inform(void *, void *);
extern void		ps1_set_feedback(void *);
extern const device_t	m_ps1_device;
extern const device_t	ps1_hdc_device;
#endif

extern void		machine_ps2_m30_286_init(const machine_t *, void *);
extern void		machine_ps2_model_50_init(const machine_t *, void *);
extern void		machine_ps2_model_55sx_init(const machine_t *, void *);
extern void		machine_ps2_model_70_type3_init(const machine_t *, void *);
extern void		machine_ps2_model_70_type4_init(const machine_t *, void *);
extern void		machine_ps2_model_80_init(const machine_t *, void *);

extern void		machine_amstrad_1512_init(const machine_t *, void *);
extern void		machine_amstrad_1640_init(const machine_t *, void *);
extern void		machine_amstrad_200_init(const machine_t *, void *);
extern void		machine_amstrad_2086_init(const machine_t *, void *);
extern void		machine_amstrad_3086_init(const machine_t *, void *);
extern void		machine_amstrad_mega_init(const machine_t *, void *);

extern void		machine_europc_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_europc_device;
#endif

extern void		machine_olim24_init(const machine_t *, void *);
extern void		machine_olim24_video_init(void);

extern void		machine_tandy1k_init(const machine_t *, void *);
extern void		machine_tandy1k_hx_init(const machine_t *, void *);
extern void		machine_tandy1k_sl2_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_tandy1k_device;
extern const device_t	m_tandy1k_hx_device;
extern const device_t	m_tandy1k_sl2_device;
#endif
extern int		tandy1k_eeprom_read(void);

extern void		machine_pc_init(const machine_t *, void *);
extern void		machine_xt_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_pc_device;
extern const device_t	m_xt_device;
#endif

extern void		machine_xt_compaq_p1_init(const machine_t *, void *);

#if defined(DEV_BRANCH) && defined(USE_LASERXT)
extern void		machine_xt_laserxt_init(const machine_t *, void *);
extern void		machine_xt_lxt3_init(const machine_t *, void *);
#endif

extern void		machine_xt_t1000_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_xt_t1000_device;
extern const device_t	t1000_video_device;
extern const device_t	t1200_video_device;
#endif
extern void		machine_xt_t1200_init(const machine_t *, void *);
extern void		machine_xt_t1x00_close(void);

extern void		machine_xt_xi8088_init(const machine_t *, void *);
#ifdef EMU_DEVICE_H
extern const device_t	m_xi8088_device;
#endif


#endif	/*EMU_MACHINE_H*/
