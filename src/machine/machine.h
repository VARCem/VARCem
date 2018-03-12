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
 * Version:	@(#)machine.h	1.0.8	2018/03/11
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


/* FIXME: replace these with machine flags! */
#define PCJR	(romset==ROM_IBMPCJR)
#if defined(DEV_BRANCH) && defined(USE_GREENB)
#define AMIBIOS	(romset==ROM_AMI386SX || \
		 romset==ROM_AMI486 || \
		 romset==ROM_WIN486 || \
		 romset==ROM_4GPV31)
#else
#define AMIBIOS	(romset==ROM_AMI386SX || \
		 romset==ROM_AMI486 || \
		 romset==ROM_WIN486)
#endif


/* FIXME: try to get rid of these... */
enum {
    ROM_IBMPC = 0,	/* 301 keyboard error, 131 cassette (!!!) error */
    ROM_AMIXT,		/* XT Clone with AMI BIOS */
    ROM_DTKXT,
    ROM_IBMXT,		/* 301 keyboard error */
    ROM_GENXT,		/* 'Generic XT BIOS' */
    ROM_JUKOPC,
    ROM_PORTABLE,
    ROM_PXXT,
#if defined(DEV_BRANCH) && defined(USE_LASERXT)
    ROM_LTXT,
    ROM_LXT3,
#endif
    ROM_T1000,
    ROM_T1200,
    ROM_XI8088,
    ROM_IBMPCJR,
    ROM_TANDY,
    ROM_TANDY1000HX,
    ROM_EUROPC,
    ROM_PC1512,
    ROM_PC1640,
    ROM_PC200,
    ROM_PC2086,
    ROM_PC3086,        
    ROM_OLIM24,
    ROM_TANDY1000SL2,
    ROM_T3100E,

    ROM_AMI286,
    ROM_AWARD286,
    ROM_CMDPC30,
    ROM_PORTABLEII,
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    ROM_PORTABLEIII,
#endif
    ROM_GW286CT,
    ROM_SUPER286TR,	/* Hyundai Super-286TR/SCAT/Award */
    ROM_IBMAT,
    ROM_IBMPS1_2011,
    ROM_IBMPS2_M30_286,
    ROM_IBMXT286,
    ROM_SPC4200P,	/* Samsung SPC-4200P/SCAT/Phoenix */
    ROM_SPC4216P,	/* Samsung SPC-4216P/SCAT */

    ROM_IBMPS2_M50,

    ROM_AMI386SX,
    ROM_KMXC02,
    ROM_MEGAPC,
    ROM_AWARD386SX_OPTI495,
#if defined(DEV_BRANCH) && defined(USE_DESKPRO386)
    ROM_DESKPRO_386,
#endif
    ROM_DTK386,
    ROM_IBMPS1_2121,
    ROM_IBMPS1_2121_ISA,/* IBM PS/1 Model 2121 with ISA expansion bus */
#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
    ROM_PORTABLEIII386,
#endif

    ROM_IBMPS2_M55SX,

    ROM_AMI386DX_OPTI495,
    ROM_MEGAPCDX,	/* 386DX mdl - Note: documentation (in German) clearly says such a model exists */
    ROM_AWARD386DX_OPTI495,
    ROM_MR386DX_OPTI495,

    ROM_IBMPS2_M80,

    ROM_AMI486,
    ROM_WIN486,
#ifdef UNIMPLEMENTED_MACHINES
    ROM_VLI486SV2G,	/* ASUS VL/I-486SV2G/SiS 471/Award/SiS 85C471 */	/* 51 */
#endif
    ROM_AWARD486_OPTI495,
    ROM_DTK486,		/* DTK PKM-0038S E-2/SiS 471/Award/SiS 85C471 */
#if defined(DEV_BRANCH) && defined(USE_GREENB)
    ROM_4GPV31,		/* Green-B 4GPV3.1 ISA/VLB 486/Pentium, AMI */
#endif
    ROM_IBMPS1_2133,

    ROM_R418,		/* Rise Computer R418/SiS 496/497/Award/SMC FDC37C665 */

    ROM_REVENGE,	/* Intel Premiere/PCI I/430LX/AMI/SMC FDC37C665 */

    ROM_PLATO,		/* Intel Premiere/PCI II/430NX/AMI/SMC FDC37C665 */

    ROM_P54TP4XE,	/* ASUS P/I-P55TP4XE/430FX/Award/SMC FDC37C665 */
    ROM_ENDEAVOR,	/* Intel Advanced_EV/430FX/AMI/NS PC87306 */
    ROM_ZAPPA,		/* Intel Advanced_ZP/430FX/AMI/NS PC87306 */
#ifdef UNIMPLEMENTED_MACHINES
    ROM_POWERMATE_V,	/* NEC PowerMate V/430FX/Phoenix/SMC FDC37C66 5*/
#endif
    ROM_MB500N,		/* PC Partner MB500N/430FX/Award/SMC FDC37C665 */
    ROM_PRESIDENT,	/* President Award 430FX PCI/430FX/Award/Unknown SIO */

    ROM_THOR,		/* Intel Advanced_ATX/430FX/AMI/NS PC87306 */
#if defined(DEV_BRANCH) && defined(USE_MRTHOR)
    ROM_MRTHOR,		/* Intel Advanced_ATX/430FX/MR.BIOS/NS PC87306 */
#endif

    ROM_ACERM3A,	/* Acer M3A/430HX/Acer/SMC FDC37C932FR */
    ROM_ACERV35N,	/* Acer V35N/430HX/Acer/SMC FDC37C932FR */
    ROM_AP53,		/* AOpen AP53/430HX/AMI/SMC FDC37C665/669 */
    ROM_P55T2P4,	/* ASUS P/I-P55T2P4/430HX/Award/Winbond W8387F*/
    ROM_P55T2S,		/* ASUS P/I-P55T2S/430HX/AMI/NS PC87306 */

    ROM_P55TVP4,	/* ASUS P/I-P55TVP4/430HX/Award/Winbond W8387F*/
    ROM_430VX,		/* Award 430VX PCI/430VX/Award/UMC UM8669F*/
    ROM_P55VA,		/* Epox P55-VA/430VX/Award/SMC FDC37C932FR*/

#if defined(DEV_BRANCH) && defined(USE_I686)
    ROM_440FX,		/* Tyan Titan-Pro AT/440FX/Award BIOS/SMC FDC37C665 */
    ROM_S1668,		/* Tyan Titan-Pro ATX/440FX/AMI/SMC FDC37C669 */
#endif

    ROM_MAX
};


/* Machine feature flags. */
#define MACHINE_PC		0x000000	/* PC architecture */
#define MACHINE_AT		0x000001	/* PC/AT architecture */
#define MACHINE_PS2		0x000002	/* PS/2 architecture */
#define MACHINE_ISA		0x000010	/* sys has ISA bus */
#define MACHINE_CBUS		0x000020	/* sys has C-BUS bus */
#define MACHINE_EISA		0x000040	/* sys has EISA bus */
#define MACHINE_VLB		0x000080	/* sys has VL bus */
#define MACHINE_MCA		0x000100	/* sys has MCA bus */
#define MACHINE_PCI		0x000200	/* sys has PCI bus */
#define MACHINE_AGP		0x000400	/* sys has AGP bus */
#define MACHINE_HDC		0x001000	/* sys has int HDC */
#define MACHINE_HDC_PS2		0x002000	/* sys has int PS/2 HDC */
#define MACHINE_MOUSE		0x004000	/* sys has int mouse */
#define MACHINE_VIDEO		0x008000	/* sys has int video */

#define IS_ARCH(m, a)		(machines[(m)].flags & (a)) ? 1 : 0;


typedef struct _machine_ {
    const char		*name;
    int			id;
    const char		*internal_name;
    const wchar_t	*bios_path;
    struct {
	const char	*name;
#ifdef EMU_CPU_H
	CPU		*cpus;
#else
	void		*cpus;
#endif
    }		cpu[5];
    int		fixed_gfxcard;
    int		flags;
    int		min_ram, max_ram;
    int		ram_granularity;
    int		nvrsz;
    void	(*init)(struct _machine_ *);
#ifdef EMU_DEVICE_H
    device_t	*(*get_device)(void);
#else
    void	*get_device;
#endif
    void	(*nvr_close)(void);
} machine_t;


/* Global variables. */
extern machine_t	machines[];
extern int		machine;
extern int		romset;
extern int		AT, PCI;
extern int		romspresent[ROM_MAX];


/* Core functions. */
extern int	machine_count(void);
extern int	machine_getromset(void);
extern int	machine_getmachine(int romset);
extern char	*machine_getname(void);
extern char	*machine_get_internal_name(void);
extern int	machine_get_machine_from_internal_name(char *s);
extern int	machine_available(int id);
extern int	machine_detect(void);
extern void	machine_init(void);
#ifdef EMU_DEVICE_H
extern device_t	*machine_getdevice(int machine);
#endif
extern int	machine_getromset_ex(int m);
extern char	*machine_get_internal_name_ex(int m);
extern int	machine_get_nvrmask(int m);
extern void	machine_close(void);


/* Initialization functions for boards and systems. */
extern void	machine_common_init(machine_t *);

extern void	machine_at_common_init(machine_t *);
extern void	machine_at_init(machine_t *);
extern void	machine_at_ps2_init(machine_t *);
extern void	machine_at_common_ide_init(machine_t *);
extern void	machine_at_ide_init(machine_t *);
extern void	machine_at_ps2_ide_init(machine_t *);
extern void	machine_at_top_remap_init(machine_t *);
extern void	machine_at_ide_top_remap_init(machine_t *);

extern void	machine_at_ibm_init(machine_t *);

extern void	machine_at_t3100e_init(machine_t *);

extern void	machine_at_p54tp4xe_init(machine_t *);
extern void	machine_at_endeavor_init(machine_t *);
extern void	machine_at_zappa_init(machine_t *);
extern void	machine_at_mb500n_init(machine_t *);
extern void	machine_at_president_init(machine_t *);
extern void	machine_at_thor_init(machine_t *);

extern void	machine_at_acerm3a_init(machine_t *);
extern void	machine_at_acerv35n_init(machine_t *);
extern void	machine_at_ap53_init(machine_t *);
extern void	machine_at_p55t2p4_init(machine_t *);
extern void	machine_at_p55t2s_init(machine_t *);

extern void	machine_at_batman_init(machine_t *);
extern void	machine_at_plato_init(machine_t *);

extern void	machine_at_p55tvp4_init(machine_t *);
extern void	machine_at_i430vx_init(machine_t *);
extern void	machine_at_p55va_init(machine_t *);

#if defined(DEV_BRANCH) && defined(USE_I686)
extern void	machine_at_i440fx_init(machine_t *);
extern void	machine_at_s1668_init(machine_t *);
#endif
extern void	machine_at_ali1429_init(machine_t *);
extern void	machine_at_cmdpc_init(machine_t *);

extern void	machine_at_headland_init(machine_t *);
extern void	machine_at_neat_init(machine_t *);
extern void	machine_at_neat_ami_init(machine_t *);
extern void	machine_at_opti495_init(machine_t *);
extern void	machine_at_opti495_ami_init(machine_t *);
extern void	machine_at_scat_init(machine_t *);
extern void	machine_at_scatsx_init(machine_t *);
extern void	machine_at_compaq_init(machine_t *);

extern void	machine_at_dtk486_init(machine_t *);
extern void	machine_at_r418_init(machine_t *);

extern void	machine_at_wd76c10_init(machine_t *);

#if defined(DEV_BRANCH) && defined(USE_GREENB)
extern void	machine_at_4gpv31_init(machine_t *);
#endif

extern void	machine_pcjr_init(machine_t *);

extern void	machine_ps1_m2011_init(machine_t *);
extern void	machine_ps1_m2121_init(machine_t *);
extern void	machine_ps1_m2133_init(machine_t *);

extern void	machine_ps2_m30_286_init(machine_t *);
extern void	machine_ps2_model_50_init(machine_t *);
extern void	machine_ps2_model_55sx_init(machine_t *);
extern void	machine_ps2_model_80_init(machine_t *);

extern void	machine_amstrad_init(machine_t *);

extern void	machine_europc_init(machine_t *);
#ifdef EMU_DEVICE_H
extern device_t europc_device,
                europc_hdc_device;
#endif

extern void	machine_olim24_init(machine_t *);
extern void	machine_olim24_video_init(void);

extern void	machine_tandy1k_init(machine_t *);
extern int	tandy1k_eeprom_read(void);

extern void	machine_xt_init(machine_t *);
extern void	machine_xt_compaq_init(machine_t *);
#if defined(DEV_BRANCH) && defined(USE_LASERXT)
extern void	machine_xt_laserxt_init(machine_t *);
#endif

extern void	machine_xt_t1000_init(machine_t *);
extern void	machine_xt_t1200_init(machine_t *);

extern void	machine_xt_xi8088_init(machine_t *);

#ifdef EMU_DEVICE_H
extern device_t	*xi8088_get_device(void);

extern device_t	*pcjr_get_device(void);

extern device_t	*tandy1k_get_device(void);
extern device_t	*tandy1k_hx_get_device(void);

extern device_t	*t1000_get_device(void);
extern device_t	*t1200_get_device(void);

extern device_t	*at_endeavor_get_device(void);
#endif


#endif	/*EMU_MACHINE_H*/
