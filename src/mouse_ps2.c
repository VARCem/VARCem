/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of PS/2 series Mouse devices.
 *
 * Version:	@(#)mouse_ps2.c	1.0.5	2018/04/20
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
#include <stdlib.h>
#include <wchar.h>
#include "emu.h"
#include "config.h"
#include "device.h"
#include "keyboard.h"
#include "mouse.h"


enum {
    MODE_STREAM,
    MODE_REMOTE,
    MODE_ECHO
};


typedef struct {
    const char	*name;				/* name of this device */
    int8_t	type;				/* type of this device */

    int		mode;

    uint8_t	flags;
    uint8_t	resolution;
    uint8_t	sample_rate;

    uint8_t	command;

    int		x, y, z, b;

    uint8_t	last_data[6];
} mouse_t;
#define FLAG_INTELLI	0x80			/* device is IntelliMouse */
#define FLAG_INTMODE	0x40			/* using Intellimouse mode */
#define FLAG_SCALED	0x20			/* enable delta scaling */
#define FLAG_ENABLED	0x10			/* dev is enabled for use */
#define FLAG_CTRLDAT	0x08			/* ctrl or data mode */


int mouse_scan = 0;


static void
ps2_write(uint8_t val, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t temp;

    if (dev->flags & FLAG_CTRLDAT) {
	dev->flags &= ~FLAG_CTRLDAT;

	switch (dev->command) {
		case 0xe8:	/* set mouse resolution */
			dev->resolution = val;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xf3:	/* set sample rate */
			dev->sample_rate = val;
			keyboard_at_adddata_mouse(0xfa);
			break;

		default:
			keyboard_at_adddata_mouse(0xfc);
	}
    } else {
	dev->command = val;

	switch (dev->command) {
		case 0xe6:	/* set scaling to 1:1 */
			dev->flags &= ~FLAG_SCALED;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xe7:	/* set scaling to 2:1 */
			dev->flags |= FLAG_SCALED;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xe8:	/* set mouse resolution */
			dev->flags |= FLAG_CTRLDAT;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xe9:	/* status request */
			keyboard_at_adddata_mouse(0xfa);
			temp = (dev->flags & 0x3f);
			if (mouse_buttons & 0x01)
				temp |= 0x01;
			if (mouse_buttons & 0x02)
				temp |= 0x02;
			if (mouse_buttons & 0x04)
				temp |= 0x03;
			keyboard_at_adddata_mouse(temp);
			keyboard_at_adddata_mouse(dev->resolution);
			keyboard_at_adddata_mouse(dev->sample_rate);
			break;

		case 0xf2:	/* read ID */
			keyboard_at_adddata_mouse(0xfa);
			if (dev->flags & FLAG_INTMODE)
				keyboard_at_adddata_mouse(0x03);
			  else
				keyboard_at_adddata_mouse(0x00);
			break;

		case 0xf3:	/* set command mode */
			dev->flags |= FLAG_CTRLDAT;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xf4:	/* enable */
			dev->flags |= FLAG_ENABLED;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xf5:	/* disable */
			dev->flags &= ~FLAG_ENABLED;
			keyboard_at_adddata_mouse(0xfa);
			break;

		case 0xff:	/* reset */
			dev->mode  = MODE_STREAM;
			dev->flags &= 0x80;
			keyboard_at_adddata_mouse(0xfa);
			keyboard_at_adddata_mouse(0xaa);
			keyboard_at_adddata_mouse(0x00);
			break;

		default:
			keyboard_at_adddata_mouse(0xfe);
	}
    }

    if (dev->flags & FLAG_INTELLI) {
	for (temp=0; temp<5; temp++)	
		dev->last_data[temp] = dev->last_data[temp+1];
	dev->last_data[5] = val;

	if (dev->last_data[0] == 0xf3 && dev->last_data[1] == 0xc8 &&
	    dev->last_data[2] == 0xf3 && dev->last_data[3] == 0x64 &&
	    dev->last_data[4] == 0xf3 && dev->last_data[5] == 0x50)
		dev->flags |= FLAG_INTMODE;
    }
}


static int
ps2_poll(int x, int y, int z, int b, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t buff[3];

    if (!x && !y && !z && b == dev->b) return(1);

    if (! (dev->flags & FLAG_ENABLED)) return(1);

    if (! mouse_scan) return(1);

    dev->x += x;
    dev->y -= y;
    dev->z -= z;
    if ((dev->mode == MODE_STREAM) &&
	((mouse_queue_end-mouse_queue_start) & 0x0f) < 13) {
	dev->b = b;

	if (dev->x > 255) dev->x = 255;
	if (dev->x < -256) dev->x = -256;
	if (dev->y > 255) dev->y = 255;
	if (dev->y < -256) dev->y = -256;
	if (dev->z < -8) dev->z = -8;
	if (dev->z > 7) dev->z = 7;

	memset(buff, 0x00, sizeof(buff));
	buff[0] = 0x08;
	if (dev->x < 0)
		buff[0] |= 0x10;
	if (dev->y < 0)
		buff[0] |= 0x20;
	if (mouse_buttons & 0x01)
		buff[0] |= 0x01;
	if (mouse_buttons & 0x02)
		buff[0] |= 0x02;
	if (dev->flags & FLAG_INTELLI) {
		if (mouse_buttons & 0x04)
			buff[0] |= 0x04;
	}
	buff[1] = (dev->x & 0xff);
	buff[2] = (dev->y & 0xff);

	keyboard_at_adddata_mouse(buff[0]);
	keyboard_at_adddata_mouse(buff[1]);
	keyboard_at_adddata_mouse(buff[2]);
	if (dev->flags & FLAG_INTELLI)
		keyboard_at_adddata_mouse(dev->z);

	dev->x = dev->y = dev->z = 0;
    }

    return(1);
}


/*
 * Initialize the device for use by the user.
 *
 * We also get called from the various machines.
 */
void *
mouse_ps2_init(const device_t *info)
{
    mouse_t *dev;
    int i;

    dev = (mouse_t *)malloc(sizeof(mouse_t));
    memset(dev, 0x00, sizeof(mouse_t));
    dev->name = info->name;
    dev->type = info->local;

    dev->mode = MODE_STREAM;
    i = device_get_config_int("buttons");
    if (i > 2)
        dev->flags |= FLAG_INTELLI;

    /* Hook into the general AT Keyboard driver. */
    keyboard_at_set_mouse(ps2_write, dev);

    pclog("MOUSE: %s (buttons=%d)\n",
	dev->name, (dev->flags & FLAG_INTELLI)? 3 : 2);

    /* Tell them how many buttons we have. */
    mouse_set_buttons((dev->flags & FLAG_INTELLI) ? 3 : 2);

    /* Return our private data to the I/O layer. */
    return(dev);
}


static void
ps2_close(void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    /* Unhook from the general AT Keyboard driver. */
    keyboard_at_set_mouse(NULL, NULL);

    free(dev);
}


static const device_config_t ps2_config[] = {
    {
	"buttons", "Buttons", CONFIG_SELECTION, "", 2, {
		{
			"Two", 2
		},
		{
			"Three", 3
		},
		{
			"Wheel", 4
		},
		{
			""
		}
	}
    },
    {
	"", "", -1
    }
};


const device_t mouse_ps2_device = {
    "Standard PS/2 Mouse",
    0,
    MOUSE_PS2,
    mouse_ps2_init, ps2_close, NULL,
    ps2_poll, NULL, NULL, NULL,
    ps2_config
};
