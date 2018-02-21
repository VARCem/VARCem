/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for a defacto-standard RTC/NVRAM device.
 *
 * Version:	@(#)nvr.h	1.0.1	2018/02/14
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
#ifndef EMU_NVR_H
# define EMU_NVR_H


/* Conversion from BCD to Binary and vice versa. */
#define RTC_BCD(x)	(((x) % 10) | (((x) / 10) << 4))
#define RTC_DCB(x)	((((x) & 0xf0) >> 4) * 10 + ((x) & 0x0f))

/* RTC registers and bit definitions. */
#define RTC_SECONDS	0
#define RTC_ALSECONDS	1
#define RTC_MINUTES	2
#define RTC_ALMINUTES	3
#define RTC_HOURS	4
# define RTC_AMPM	0x80		/* PM flag if 12h format in use */
#define RTC_ALHOURS	5
#define RTC_DOW		6
#define RTC_DOM		7
#define RTC_MONTH	8
#define RTC_YEAR	9
#define RTC_REGA	10
# define REGA_UIP	0x80
# define REGA_DV2	0x40
# define REGA_DV1	0x20
# define REGA_DV0	0x10
# define REGA_DV	0x70
# define REGA_RS3	0x08
# define REGA_RS2	0x04
# define REGA_RS1	0x02
# define REGA_RS0	0x01
# define REGA_RS	0x0f
#define RTC_REGB	11
# define REGB_SET	0x80
# define REGB_PIE	0x40
# define REGB_AIE	0x20
# define REGB_UIE	0x10
# define REGB_SQWE	0x08
# define REGB_DM	0x04
# define REGB_2412	0x02
# define REGB_DSE	0x01
#define RTC_REGC	12
# define REGC_IRQF	0x80
# define REGC_PF	0x40
# define REGC_AF	0x20
# define REGC_UF	0x10
#define RTC_REGD	13
# define REGD_VRT	0x80
#define RTC_CENTURY	0x32		/* century register */
#define RTC_REGS	14		/* number of registers */



/* Define a (defacto-standard) RTC/NVRAM chip. */
typedef struct _nvr_ {
    uint8_t	regs[RTC_REGS+114];	/* these are the registers */

    int64_t	mask,
		irq,
		addr;

    wchar_t	*fname;

    int64_t	upd_stat,
		upd_ecount,
		onesec_time,
		onesec_cnt,
    		rtctime,
		oldmachine;

    /* Hooks to internal RTC I/O functions. */
    void	(*set)(struct _nvr_ *, uint16_t, uint8_t);
    uint8_t	(*get)(struct _nvr_ *, uint16_t);

    /* Hooks to alternative load/save functions. */
    int8_t	(*load)(wchar_t *fname);
    int8_t	(*save)(wchar_t *fname);

    /* Hook to RTC ticker handler. */
    void	(*hook)(struct _nvr_ *);
} nvr_t;


extern int	enable_sync;
extern int	nvr_dosave;


extern void	nvr_init(nvr_t *);
extern int	nvr_load(void);
extern int	nvr_save(void);
extern void	nvr_recalc(void);

extern wchar_t	*nvr_path(wchar_t *str);
extern FILE	*nvr_fopen(wchar_t *str, wchar_t *mode);

extern void	nvr_at_init(int64_t irq);
extern void	nvr_at_close(void);


#endif	/*EMU_NVR_H*/
