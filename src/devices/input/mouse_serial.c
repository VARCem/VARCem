/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Serial Mouse devices.
 *
 * TODO:	Add the Genius Serial Mouse.
 *
 * Version:	@(#)mouse_serial.c	1.0.10	2018/10/05
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#define dbglog mouse_log
#include "../../emu.h"
#include "../../device.h"
#include "../../timer.h"
#include "../ports/serial.h"
#include "mouse.h"


typedef struct {
    const char	*name;				/* name of this device */
    int8_t	type,				/* type of this device */
		port;
    uint8_t	flags;				/* device flags */

    int		pos;
    int64_t	delay;
    int		oldb;

    SERIAL	*serial;
} mouse_t;
#define FLAG_INPORT	0x80			/* device is MS InPort */
#define FLAG_3BTN	0x20			/* enable 3-button mode */
#define FLAG_SCALED	0x10			/* enable delta scaling */
#define FLAG_INTR	0x04			/* dev can send interrupts */
#define FLAG_FROZEN	0x02			/* do not update counters */
#define FLAG_ENABLED	0x01			/* dev is enabled for use */


/* Callback from serial driver: RTS was toggled. */
static void
ser_callback(struct SERIAL *serial, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    /* Start a timer to wake us up in a little while. */
    dev->pos = -1;
    dev->serial->clear_fifo(serial);
    dev->delay = 5000LL * (1LL << TIMER_SHIFT);
}


/* Callback timer expired, now send our "mouse ID" to the serial port. */
static void
ser_timer(void *priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t b[2];

    dev->delay = 0LL;

    if (dev->pos != -1) return;

    dev->pos = 0;
    switch(dev->type) {
	case MOUSE_MSYSTEMS:
		/* Identifies Mouse Systems serial mouse. */
		b[0] = 'H';
		dev->serial->write_fifo(dev->serial, b, 1);
		break;

	case MOUSE_MICROSOFT:
	case MOUSE_LOGITECH:
		/* Identifies a Microsoft/Logitech Serial mouse. */
		b[0] = 'M'; b[1] = '3';
		if (dev->flags & FLAG_3BTN)
			dev->serial->write_fifo(dev->serial, b, 2);
		  else
			dev->serial->write_fifo(dev->serial, b, 1);
		break;

	case MOUSE_MSWHEEL:
		/* Identifies multi-button Microsoft Wheel Mouse. */
		b[0] = 'M'; b[1] = 'Z';
		dev->serial->write_fifo(dev->serial, b, 2);
		break;

	default:
		ERRLOG("MOUSE: unsupported mouse type %d?\n", dev->type);
    }
}


static int
ser_poll(int x, int y, int z, int b, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t buff[16];
    int len;

    if (!x && !y && b == dev->oldb) return(1);

    DBGLOG(1, "MOUSE: poll(%d,%d,%d,%02x)\n", x, y, z, b);

    dev->oldb = b;

    if (x > 127) x = 127;
    if (y > 127) y = 127;
    if (x <- 128) x = -128;
    if (y <- 128) y = -128;

    len = 0;
    switch(dev->type) {
	case MOUSE_MSYSTEMS:
		buff[0] = 0x80;
		buff[0] |= (b & 0x01) ? 0x00 : 0x04;	/* left button */
		buff[0] |= (b & 0x02) ? 0x00 : 0x01;	/* middle button */
		buff[0] |= (b & 0x04) ? 0x00 : 0x02;	/* right button */
		buff[1] = x;
		buff[2] = -y;
		buff[3] = x;				/* same as byte 1 */
		buff[4] = -y;				/* same as byte 2 */
		len = 5;
		break;

	case MOUSE_MICROSOFT:
	case MOUSE_LOGITECH:
	case MOUSE_MSWHEEL:
		buff[0] = 0x40;
		buff[0] |= (((y >> 6) & 0x03) << 2);
		buff[0] |= ((x >> 6) & 0x03);
		if (b & 0x01) buff[0] |= 0x20;
		if (b & 0x02) buff[0] |= 0x10;
		buff[1] = x & 0x3f;
		buff[2] = y & 0x3f;
		if (dev->flags & FLAG_3BTN) {
			len = 3;
			if (b & 0x04) {
				buff[3] = 0x20;
				len++;
			}
		} else if (dev->type == MOUSE_MSWHEEL) {
			len = 4;
			buff[3] = z & 0x0f;
			if (b & 0x04)
				buff[3] |= 0x10;
		} else
			len = 3;
		break;
    }

    /* Send the packet to the bottom-half of the attached port. */
    if (dev->serial != NULL)
	dev->serial->write_fifo(dev->serial, buff, len);

    return(1);
}


static void
ser_close(void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    /* Detach serial port from the mouse. */
    if ((dev != NULL) && (dev->serial != NULL))
	(void)serial_attach(dev->port + 1, NULL, NULL);

    free(dev);
}


/* Initialize the device for use by the user. */
static void *
ser_init(const device_t *info)
{
    mouse_t *dev;
    int i;

    dev = (mouse_t *)mem_alloc(sizeof(mouse_t));
    memset(dev, 0x00, sizeof(mouse_t));
    dev->name = info->name;
    dev->type = info->local;

    dev->port = device_get_config_int("port");

    i = device_get_config_int("buttons");
    if (i > 2)
	dev->flags |= FLAG_3BTN;

    /* Attach a serial port to the mouse. */
    dev->serial = serial_attach(dev->port + 1, ser_callback, dev);
    if (dev->serial == NULL) {
	ERRLOG("MOUSE: %s (port=COM%d, butons=%d) port disabled!\n",
					dev->name, dev->port+1, i);
	free(dev);
	return(NULL);
    }

    INFO("MOUSE: %s (port=COM%d, butons=%d)\n", dev->name, dev->port+1, i);

    timer_add(ser_timer, &dev->delay, &dev->delay, dev);

    /* Tell them how many buttons we have. */
    mouse_set_buttons((dev->flags & FLAG_3BTN) ? 3 : 2);

    /* Return our private data to the I/O layer. */
    return(dev);
}


static const device_config_t ser_config[] = {
    {
	"port", "Serial Port", CONFIG_SELECTION, "", 0, {
		{
			"COM1", 0
		},
		{
			"COM2", 1
		},
		{
			""
		}
	}
    },
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


const device_t mouse_mssystems_device = {
    "Mouse Systems Serial Mouse",
    0,
    MOUSE_MSYSTEMS,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};

const device_t mouse_msserial_device = {
    "Microsoft Serial Mouse",
    0,
    MOUSE_MICROSOFT,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};

const device_t mouse_logiserial_device = {
    "Logitech Serial Mouse",
    0,
    MOUSE_LOGITECH,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};


const device_t mouse_mswhserial_device = {
    "Microsoft Serial Wheel Mouse",
    0,
    MOUSE_MSWHEEL,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};
