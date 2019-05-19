/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the system timer module.
 *
 * Version:	@(#)timer.h	1.0.5	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef EMU_TIMER_H
# define EMU_TIMER_H


#define TIMER_SHIFT		6

#define TIMER_ALWAYS_ENABLED	&timer_one


typedef int64_t	tmrval_t;


extern tmrval_t	TIMER_USEC;
extern tmrval_t	timer_one;
extern tmrval_t	timer_start;
extern tmrval_t	timer_count;


#define timer_start_period(cycles)				\
        timer_start = cycles;

#define timer_end_period(cycles)				\
	do {							\
                tmrval_t __diff = timer_start - (cycles);	\
		timer_count -= __diff;				\
                timer_start = cycles;				\
		if (timer_count <= 0) {				\
			timer_process();			\
			timer_update_outstanding();		\
		}						\
	} while (0)

#define timer_clock()						\
	do {							\
                tmrval_t __diff;					\
                if (AT) {					\
                        __diff = timer_start - (cycles << TIMER_SHIFT);	\
                        timer_start = cycles << TIMER_SHIFT;	\
                } else {					\
                        __diff = timer_start - (cycles * xt_cpu_multi); \
                        timer_start = cycles * xt_cpu_multi;	\
                }						\
		timer_count -= __diff;				\
		timer_process();				\
        	timer_update_outstanding();			\
	} while (0)


extern void	timer_process(void);
extern void	timer_update_outstanding(void);
extern void	timer_reset(void);
extern int	timer_add(void (*callback)(priv_t), priv_t priv,
			  tmrval_t *count, tmrval_t *enable);


#endif	/*EMU_TIMER_H*/
