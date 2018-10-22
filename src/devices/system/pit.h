/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for Intel 8253 timer module.
 *
 * Version:	@(#)pit.h	1.0.4	2018/09/05
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
#ifndef EMU_PIT_H
# define EMU_PIT_H


typedef struct {
    int		nr;
    struct PIT	*pit;
} PIT_nr;

typedef struct PIT {
    uint32_t	l[3];
    int64_t	c[3];
    uint8_t	m[3];
    uint8_t	ctrl,
		ctrls[3];
    int		wp,
		rm[3],
		wm[3];
    uint16_t	rl[3];
    int		thit[3];
    int		delay[3];
    int		rereadlatch[3];
    int		gate[3];
    int		out[3];
    int64_t	running[3];
    int		enabled[3];
    int		newcount[3];
    int		count[3];
    int		using_timer[3];
    int		initial[3];
    int		latched[3];
    int		disabled[3];

    uint8_t	read_status[3];
    int		do_read_status[3];

    PIT_nr	pit_nr[3];

    void	(*set_out_funcs[3])(int new_out, int old_out);
} PIT;


extern PIT	pit,
		pit2;
extern double	PITCONST;
extern float	CGACONST,
		MDACONST,
		VGACONST1,
		VGACONST2,
		RTCCONST;


extern void	pit_init(void);
extern void	pit_ps2_init(void);
extern void	pit_reset(PIT *pit);
extern void	pit_set_gate(PIT *pit, int channel, int gate);
extern void	pit_set_using_timer(PIT *pit, int t, int using_timer);
extern void	pit_set_out_func(PIT *pit, int t, void (*func)(int new_out, int old_out));

extern float    pit_timer0_freq(void);
extern void	pit_irq0_timer_pcjr(int new_out, int old_out);
extern void	pit_refresh_timer_xt(int new_out, int old_out);
extern void	pit_refresh_timer_at(int new_out, int old_out);

extern void     setrtcconst(float clock);
extern void     setpitclock(float clock);


#endif	/*EMU_PIT_H*/
