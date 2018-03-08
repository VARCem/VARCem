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
 * Version:	@(#)hdc.h	1.0.1	2018/02/14
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
#ifndef EMU_HDC_H
# define EMU_HDC_H


#define MFM_NUM		2	/* 2 drives per controller supported */
#define ESDI_NUM	2	/* 2 drives per controller supported */
#define XTIDE_NUM	2	/* 2 drives per controller supported */
#define IDE_NUM		8
#define SCSI_NUM	16	/* theoretically the controller can have at
				 * least 7 devices, with each device being
				 * able to support 8 units, but hey... */

extern char	*hdc_name;
extern int	hdc_current;


extern device_t	mfm_xt_xebec_device;		/* mfm_xt_xebec */
extern device_t	mfm_xt_dtc5150x_device;		/* mfm_xt_dtc */
extern device_t	mfm_at_wd1003_device;		/* mfm_at_wd1003 */

extern device_t	esdi_at_wd1007vse1_device;	/* esdi_at */
extern device_t	esdi_ps2_device;		/* esdi_mca */

extern device_t	ide_isa_device;			/* isa_ide */
extern device_t	ide_isa_2ch_device;		/* isa_ide_2ch */
extern device_t	ide_isa_2ch_opt_device;		/* isa_ide_2ch_opt */
extern device_t	ide_vlb_device;			/* vlb_ide */
extern device_t	ide_vlb_2ch_device;		/* vlb_ide_2ch */
extern device_t	ide_pci_device;			/* pci_ide */
extern device_t	ide_pci_2ch_device;		/* pci_ide_2ch */

extern device_t	xtide_device;			/* xtide_xt */
extern device_t	xtide_at_device;		/* xtide_at */
extern device_t	xtide_acculogic_device;		/* xtide_ps2 */
extern device_t	xtide_at_ps2_device;		/* xtide_at_ps2 */


extern void	hdc_init(char *name);
extern void	hdc_reset(void);

extern char	*hdc_get_name(int hdc);
extern char	*hdc_get_internal_name(int hdc);
extern device_t	*hdc_get_device(int hdc);
extern int	hdc_get_flags(int hdc);
extern int	hdc_available(int hdc);
extern int	hdc_current_is_mfm(void);


#endif	/*EMU_HDC_H*/
