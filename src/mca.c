/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the MCA bus primitives.
 *
 * Version:	@(#)mca.c	1.0.1	2018/02/14
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "io.h"
#include "mca.h"


void    (*mca_card_write[8])(int addr, uint8_t val, void *priv);
uint8_t  (*mca_card_read[8])(int addr, void *priv);
void           *mca_priv[8];

static int mca_index;
static int mca_nr_cards;


void mca_init(int nr_cards)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                mca_card_read[c] = NULL;
                mca_card_write[c] = NULL;
                mca_priv[c] = NULL;
        }
        
        mca_index = 0;
        mca_nr_cards = nr_cards;
}

void mca_set_index(int index)
{
        mca_index = index;
}

uint8_t mca_read(uint16_t port)
{
        if (mca_index >= mca_nr_cards)
                return 0xff;
        if (!mca_card_read[mca_index])
                return 0xff;
        return mca_card_read[mca_index](port, mca_priv[mca_index]);
}

void mca_write(uint16_t port, uint8_t val)
{
        if (mca_index >= mca_nr_cards)
                return;
        if (mca_card_write[mca_index])
                mca_card_write[mca_index](port, val, mca_priv[mca_index]);
}

void mca_add(uint8_t (*read)(int addr, void *priv), void (*write)(int addr, uint8_t val, void *priv), void *priv)
{
        int c;
        
        for (c = 0; c < mca_nr_cards; c++)
        {
                if (!mca_card_read[c] && !mca_card_write[c])
                {
                         mca_card_read[c] = read;
                        mca_card_write[c] = write;
                              mca_priv[c] = priv;
                        return;
                }
        }
}
