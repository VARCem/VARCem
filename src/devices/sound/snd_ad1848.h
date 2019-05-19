/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the AD1848 driver.
 *
 * Version:	@(#)snd_ad1848.h	1.0.2	2019/05/17
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
#ifndef SOUND_AD1848_H
# define SOUND_AD1848_H


typedef struct ad1848_t
{
        int index;
        uint8_t regs[16];
        uint8_t status;
        
        int trd;
        int mce;
        
        int count;
        
        int16_t out_l, out_r;
                
        tmrval_t enable;

        int irq, dma;
        
        tmrval_t freq;
        
        tmrval_t timer_count, timer_latch;

        int16_t buffer[SOUNDBUFLEN * 2];
        int pos;
} ad1848_t;

void ad1848_setirq(ad1848_t *ad1848, int irq);
void ad1848_setdma(ad1848_t *ad1848, int dma);

uint8_t ad1848_read(uint16_t addr, priv_t);
void ad1848_write(uint16_t addr, uint8_t val, priv_t);

void ad1848_update(ad1848_t *ad1848);
void ad1848_speed_changed(ad1848_t *ad1848);

void ad1848_init(ad1848_t *ad1848);


#endif	/*SOUND_AD1848_H*/
