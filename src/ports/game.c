/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of a generic Game Port.
 *
 * Version:	@(#)game.c	1.0.8	2018/04/26
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../machine/machine.h"
#include "../cpu/cpu.h"
#include "../device.h"
#include "../io.h"
#include "../timer.h"
#include "game.h"
#include "game_dev.h"
#include "../game/joystick.h"


typedef struct {
    int64_t	count;
    int		axis_nr;
    struct _game_ *game;
} g_axis_t;

typedef struct _game_ {
    uint8_t	state;

    g_axis_t	axis[4];

    const gamedev_t *joystick;
    void	*joystick_priv;
} game_t;


static game_t	*game_global = NULL;


static int
game_time(int axis)
{
    if (axis == AXIS_NOT_PRESENT) return(0);

    axis += 32768;
    axis = (axis * 100) / 65; /*Axis now in ohms*/
    axis = (axis * 11) / 1000;

    return((int)(TIMER_USEC * (axis + 24))); /*max = 11.115 ms*/
}


static void
game_write(uint16_t addr, uint8_t val, void *priv)
{
    game_t *dev = (game_t *)priv;
    int i;

    timer_clock();

    dev->state |= 0x0f;

    for (i = 0; i < 4; i++) {
	dev->axis[i].count =
		game_time(dev->joystick->read_axis(dev->joystick_priv, i));
    }
//FIXME: removed too much stuff here!  --FvK
    dev->joystick->write(dev->joystick_priv);

    cycles -= ISA_CYCLES(8);
}


static uint8_t
game_read(uint16_t addr, void *priv)
{
    game_t *dev = (game_t *)priv;
    uint8_t ret;

    timer_clock();

    ret = dev->state | dev->joystick->read(dev->joystick_priv);

    cycles -= ISA_CYCLES(8);

    return(ret);
}


/* Timer expired... game over. Just couldn't resist... --FvK */
static void
game_over(void *priv)
{
    g_axis_t *axis = (g_axis_t *)priv;
    game_t *dev = axis->game;

    dev->state &= ~(1 << axis->axis_nr);
    axis->count = 0;

    if (axis == &dev->axis[0])
	dev->joystick->a0_over(dev->joystick_priv);
}


static void *
game_init(const device_t *info)
{
    game_t *dev;
    int i;

    pclog("GAME: initializing, type=%d\n", joystick_type);

    if (joystick_type == JOYSTICK_NONE) return(NULL);

    dev = (game_t *)malloc(sizeof(game_t));
    memset(dev, 0x00, sizeof(game_t));
    game_global = dev;

    for (i = 0; i < 4; i++) {
	dev->axis[i].game = dev;
	dev->axis[i].axis_nr = i;
    }

    dev->joystick = gamedev_get_device(joystick_type);
    dev->joystick_priv = dev->joystick->init();

    switch(info->local) {
	case 0:		/* regular game port */
		io_sethandler(0x0200, 8,
			      game_read,NULL,NULL, game_write,NULL,NULL, dev);
		break;

	case 1:		/* special case, 1 I/O port only */
		io_sethandler(0x0201, 1,
			      game_read,NULL,NULL, game_write,NULL,NULL, dev);
		break;

    }

    timer_add(game_over,
	      &dev->axis[0].count, &dev->axis[0].count, &dev->axis[0]);
    timer_add(game_over,
	      &dev->axis[1].count, &dev->axis[1].count, &dev->axis[1]);
    timer_add(game_over,
	      &dev->axis[2].count, &dev->axis[2].count, &dev->axis[2]);
    timer_add(game_over,
	      &dev->axis[3].count, &dev->axis[3].count, &dev->axis[3]);

    return(dev);
}


static void
game_close(void *priv)
{
    game_t *dev = (game_t *)priv;

    if (dev == NULL) return;

    dev->joystick->close(dev->joystick_priv);

    game_global = NULL;

    free(dev);
}


const device_t game_device = {
    "Standard Game Port",
    0,
    0,
    game_init, game_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t game_201_device = {
    "Custom Game Port (201H)",
    0,
    1,
    game_init, game_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


void
game_update_joystick_type(void)
{
    game_t *dev = game_global;

    if (dev != NULL) {
	dev->joystick->close(dev->joystick_priv);
	dev->joystick = gamedev_get_device(joystick_type);
	dev->joystick_priv = dev->joystick->init();
    }
}
