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
 * Version:	@(#)mca.c	1.0.5	2021/03/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include "../../emu.h"
#include "../../io.h"
#include "mca.h"


#define MCA_CARD_MAX	8


typedef struct mca {
    void	(*card_write)(int addr, uint8_t val, priv_t);
    uint8_t	(*card_read)(int addr, priv_t);
    uint8_t	(*card_feedb)(priv_t);
    void	(*card_reset)(priv_t);

    priv_t	priv;
} mca_t;


static mca_t	cards[MCA_CARD_MAX];
static int	mca_nr_cards;
static int	mca_index;


void
mca_init(int nr_cards)
{
    memset(cards, 0x00, sizeof(cards));

    mca_nr_cards = nr_cards;
    mca_index = 0;
}


void
mca_set_index(int idx)
{
    mca_index = idx;
}


uint8_t
mca_read(uint16_t port)
{
    if (mca_index >= mca_nr_cards)
	return 0xff;

    if (! cards[mca_index].card_read)
	return 0xff;

    return cards[mca_index].card_read(port, cards[mca_index].priv);
}


void
mca_write(uint16_t port, uint8_t val)
{
    if (mca_index >= mca_nr_cards)
	return;

    if (cards[mca_index].card_write)
	cards[mca_index].card_write(port, val, cards[mca_index].priv);
}


uint8_t 
mca_feedb(void)
{
    uint8_t ret = 0xff;

    if (mca_index >= mca_nr_cards)
	return ret;

    if (cards[mca_index].card_feedb)
	ret = cards[mca_index].card_feedb(cards[mca_index].priv);

    return ret;
}


void 
mca_reset(void)
{
    int c;

    for (c = 0; c < MCA_CARD_MAX; c++) {
	if (cards[mca_index].card_reset)
		cards[mca_index].card_reset(cards[mca_index].priv);
    }
}


void
mca_add(uint8_t (*read)(int, priv_t), void (*write)(int, uint8_t, priv_t), uint8_t (*feedb)(priv_t), void (*reset)(priv_t), priv_t priv)
{
    mca_t *ptr;
    int c;

    for (c = 0; c < mca_nr_cards; c++) {
	ptr = &cards[c];

	if (!ptr->card_read && !ptr->card_write) {
		ptr->card_read = read;
		ptr->card_write = write;
		ptr->card_feedb = feedb;
		ptr->card_reset = reset;
		ptr->priv = priv;

		return;
	}
    }
}
