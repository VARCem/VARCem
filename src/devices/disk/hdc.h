/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the common disk controller handler.
 *
 * Version:	@(#)hdc.h	1.0.16	2019/04/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef EMU_HDC_H
# define EMU_HDC_H


#define ST506_NUM	2	/* 2 drives per controller supported */
#define ESDI_NUM	2	/* 2 drives per controller supported */
#define IDE_NUM		8	/* 4 controllers, 2 drives per controller */
#define XTIDE_NUM	2	/* 2 drives, part of IDE structures */
#define SCSI_NUM	16	/* theoretically the controller can have at
				 * least 7 devices, with each device being
				 * able to support 8 units, but hey... */

#define HDC_NONE	0	/* no disk controller selected */
#define HDC_INTERNAL	1	/* internal/onboard controller selected */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef EMU_DEVICE_H
extern const device_t	st506_xt_xebec_device;		/* st506_xt_xebec */
extern const device_t	st506_xt_dtc5150x_device;	/* st506_xt_dtc */
extern const device_t	st506_xt_st11_m_device;		/* st506_xt_st11_m */
extern const device_t	st506_xt_st11_r_device;		/* st506_xt_st11_r */
extern const device_t	st506_xt_wd1002a_wx1_device;	/* st506_xt_wd1002a_wx1 */
extern const device_t	st506_xt_wd1002a_27x_device;	/* st506_xt_wd1002a_27x */

extern const device_t	st506_at_wd1003_device;		/* st506_at_wd1003 */

extern const device_t	esdi_at_wd1007vse1_device;	/* esdi_at */
extern const device_t	esdi_ps2_device;		/* esdi_mca */

extern const device_t	xta_wdxt150_device;		/* xta_wdxt150 */
extern const device_t	xta_hd20_device;		/* EuroPC internal */
extern const device_t	xta_t1200_device;		/* T1200 internal */

extern const device_t	xtide_device;			/* xtide_xt */
extern const device_t	xtide_acculogic_device;		/* xtide_ps2 */

extern const device_t	ide_isa_device;			/* isa_ide */
extern const device_t	ide_isa_2ch_device;		/* isa_ide_2ch */
extern const device_t	ide_isa_2ch_opt_device;		/* isa_ide_2ch_opt */
extern const device_t	ide_vlb_device;			/* vlb_ide */
extern const device_t	ide_vlb_2ch_device;		/* vlb_ide_2ch */
extern const device_t	ide_pci_device;			/* pci_ide */
extern const device_t	ide_pci_2ch_device;		/* pci_ide_2ch */

extern const device_t	ide_ter_device;
extern const device_t	ide_qua_device;

extern const device_t	xtide_at_device;		/* xtide_at */
extern const device_t	xtide_at_ps2_device;		/* xtide_at_ps2 */
#endif


extern void	hdc_log(int level, const char *fmt, ...);
extern void	hdc_init(void);
extern void	hdc_reset(void);

extern const char *hdc_get_name(int hdc);
extern const char *hdc_get_internal_name(int hdc);
extern int	hdc_has_config(int hdc);
extern const device_t	*hdc_get_device(int hdc);
extern int	hdc_get_flags(int hdc);
extern int	hdc_available(int hdc);
extern int	hdc_get_from_internal_name(const char *s);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_HDC_H*/
