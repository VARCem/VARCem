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
 * Version:	@(#)mouse_serial.c	1.0.17 2021/01/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2018,2021 Miran Grca.
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
#include "../../plat.h"
#include "mouse.h"


enum {
	PHASE_IDLE,
	PHASE_ID,
	PHASE_DATA,
	PHASE_STATUS,
	PHASE_DIAG,
	PHASE_FMT_REV
};

enum {
    REPORT_PHASE_PREPARE,
    REPORT_PHASE_TRANSMIT
};


typedef struct {
    const char	*name;				/* name of this device */
    int8_t	type,				/* type of this device */
		port;
    uint8_t	flags;				/* device flags */

    uint8_t	but,
		want_data,
		status,
		format,
		prompt,
		continuous,
		id_len,
		data_len;

    int		abs_x, abs_y;

    int		pos;
    int		oldb;
    int		phase;

    int64_t	period,
		delay;

    void	*serial;

    uint8_t	id[255],
		data[5];
} mouse_t;
#define FLAG_INPORT	0x80			/* device is MS InPort */
#define FLAG_3BTN	0x20			/* enable 3-button mode */
#define FLAG_SCALED	0x10			/* enable delta scaling */
#define FLAG_INTR	0x04			/* dev can send interrupts */
#define FLAG_FROZEN	0x02			/* do not update counters */
#define FLAG_ENABLED	0x01			/* dev is enabled for use */


static uint8_t
data_msystems(mouse_t *dev, int x, int y, int b)
{
    dev->data[0] = 0x80;
    dev->data[0] |= (b & 0x01) ? 0x00 : 0x04;	/* left button */
    dev->data[0] |= (b & 0x04) ? 0x00 : 0x01;	/* middle button */
    dev->data[0] |= (b & 0x02) ? 0x00 : 0x02;	/* right button */
    dev->data[1] = x;
    dev->data[2] = -y;
    dev->data[3] = x;				/* same as byte 1 */
    dev->data[4] = -y;				/* same as byte 2 */

    return 5;
}


static uint8_t
data_3bp(mouse_t *dev, int x, int y, int b)
{
    dev->data[0] |= (b & 0x01) ? 0x00 : 0x04;	/* left button */
    dev->data[0] |= (b & 0x04) ? 0x00 : 0x02;	/* middle button */
    dev->data[0] |= (b & 0x02) ? 0x00 : 0x01;	/* right button */
    dev->data[1] = x;
    dev->data[2] = -y;

    return 3;
}


static uint8_t
data_mmseries(mouse_t *dev, int x, int y, int b)
{
    if (x < -127)
	x = -127;
    if (y < -127)
	y = -127;

    dev->data[0] = 0x80;
    if (x >= 0)
	dev->data[0] |= 0x10;
    if (y < 0)
	dev->data[0] |= 0x08;
    dev->data[0] |= (b & 0x01) ? 0x04 : 0x00;	/* left button */
    dev->data[0] |= (b & 0x04) ? 0x02 : 0x00;	/* middle button */
    dev->data[0] |= (b & 0x02) ? 0x01 : 0x00;	/* right button */
    dev->data[1] = abs(x);
    dev->data[2] = abs(y);

    return 3;
}


static uint8_t
data_bp1(mouse_t *dev, int x, int y, int b)
{
    dev->data[0] = 0x80;
    dev->data[0] |= (b & 0x01) ? 0x10 : 0x00;	/* left button */
    dev->data[0] |= (b & 0x04) ? 0x08 : 0x00;	/* middle button */
    dev->data[0] |= (b & 0x02) ? 0x04 : 0x00;	/* right button */
    dev->data[1] = (x & 0x3f);
    dev->data[2] = (x >> 6);
    dev->data[3] = (y & 0x3f);
    dev->data[4] = (y >> 6);

    return 5;
}


static uint8_t
data_ms(mouse_t *dev, int x, int y, int z, int b)
{
    uint8_t len;

    dev->data[0] = 0x40;
    dev->data[0] |= (((y >> 6) & 0x03) << 2);
    dev->data[0] |= ((x >> 6) & 0x03);
    if (b & 0x01)
	dev->data[0] |= 0x20;
    if (b & 0x02)
	dev->data[0] |= 0x10;
    dev->data[1] = x & 0x3f;
    dev->data[2] = y & 0x3f;
    if (dev->type == MOUSE_MSWHEEL) {
		len = 4;
		dev->data[3] = z & 0x0f;
		if (b & 0x04)
			dev->data[3] |= 0x10;
	 } else if (dev->flags & FLAG_3BTN) {
		len = 3;
		if (dev->type == MOUSE_LOGITECH) {
			if (b & 0x04) {
				dev->data[3] = 0x20;
				len++;
			}
		} else {
			if ((b ^ dev->oldb) & 0x04) {
				/*
				 * Microsoft 3-button mice send a fourth byte of
				 * 0x00 when the middle button has changed.
				 */
				dev->data[3] = 0x00;
				len++;
			}
		}
    } else
	len = 3;

    return len;
}


static uint8_t
data_hex(mouse_t *dev, int x, int y, int b)
{
    char temp[6];
    uint8_t but;
    int i;

    but = 0x00;
    but |= (b & 0x01) ? 0x04 : 0x00;	/* left button */
    but |= (b & 0x04) ? 0x02 : 0x00;	/* middle button */
    but |= (b & 0x02) ? 0x01 : 0x00;	/* right button */

    /* Encode the packet as HEX values. */
    sprintf(temp, "%02X%02X%01X", (int8_t)y, (int8_t)x, but & 0x0f);

    /* And return them in reverse order. */
    for (i = 0; i < 5; i++)
	dev->data[i] = temp[4 - i];

    return i;
}


static void
ser_report(mouse_t *dev, int x, int y, int z, int b)
{
    int len = 0;

    memset(dev->data, 0x00, sizeof(dev->data));

    /* If the mouse is 2-button, ignore the middle button. */
    if (dev->but == 2)
		b &= ~0x04;

    switch(dev->format) {
		case 0:
			len = data_msystems(dev, x, y, b);
			break;

		case 1:
			len = data_3bp(dev, x, y, b);
			break;

		case 2:
			len = data_hex(dev, x, y, b);
			break;

		case 3: /* Relative */
			len = data_bp1(dev, x, y, b);
			break;
		
		case 5:
			len = data_mmseries(dev, x, y, b);
			break;

		case 6:
			len = data_bp1(dev, x, y, b);
			break;

		case 7:
			len = data_ms(dev, x, y, z, b);
			break;
		default:
		ERRLOG("MOUSE: unsupported mouse format %d?\n", dev->format);
    }

    dev->oldb = b;
    dev->data_len = len;

    dev->pos = 0;

    if (dev->phase != PHASE_DATA)
	dev->phase = PHASE_DATA;

    if (! dev->delay)
	dev->delay = dev->period;
}

static double
ser_transmit_period(mouse_t *dev, int bps, int rps)
{
    double dbps = (double) bps;
    double temp = 0.0;
    int word_len;

    switch (dev->format) {

	case 0:
	case 1:		/* Mouse Systems and Three Byte Packed formats: 8 data, no parity, 2 stop, 1 start */
		word_len = 11;
		break;
	case 2:		/* Hexadecimal format - 8 data, no parity, 1 stop, 1 start - number of stop bits is a guess because
			   it is not documented anywhere. */
		word_len = 10;
		break;
	case 3:
	case 6:		/* Bit Pad One formats: 7 data, even parity, 2 stop, 1 start */
		word_len = 11;
		break;
	case 5:		/* MM Series format: 8 data, odd parity, 1 stop, 1 start */
		word_len = 11;
		break;
	default:
	case 7:		/* Microsoft-compatible format: 7 data, no parity, 1 stop, 1 start */
		word_len = 9;
		break;
    }

    if (rps == -1)
		temp = (double) word_len;
    else {
		temp = (double) rps;
		temp = (9600.0 - (temp * 33.0));
		temp /= rps;
    }
    temp = (1000000.0 / dbps) * temp;

    return temp;
}

/* Timer expired, now send data (back) to the serial port. */
static void
ser_timer(void *priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t b = 0x00;

    switch (dev->phase) {
	case PHASE_ID:
		/* Grab next ID byte. */
		b = dev->id[dev->pos];
		if (++dev->pos == dev->id_len) {
			dev->delay = 0LL;
			dev->pos = 0;
			dev->phase = PHASE_IDLE;
		} else
			dev->delay += dev->period;
		break;

	case PHASE_DATA:
		/* Grab next data byte. */
		b = dev->data[dev->pos];
		if (++dev->pos == dev->data_len) {
			dev->delay = 0LL;
			dev->pos = 0;
			dev->phase = PHASE_IDLE;
		} else
			dev->delay += dev->period;
		break;

	case PHASE_STATUS:
		b = dev->status;
		dev->delay = 0LL;
		dev->pos = 0;
		dev->phase = PHASE_IDLE;
		break;

	case PHASE_DIAG:
		if (dev->pos)
			b = 0x00;
		else {
			/*
			 * This should return the last button status,
			 * bits 2,1,0 = L,M,R.
			 */
			b = 0x00;
		}
		if (++dev->pos == 3) {
			dev->delay = 0LL;
			dev->pos = 0;
			dev->phase = PHASE_IDLE;
		} else
			dev->delay += dev->period;
		break;

	case PHASE_FMT_REV:
		b = 0x10 | (dev->format << 1);
		dev->delay = 0LL;
		dev->pos = 0;
		dev->phase = PHASE_IDLE;
		break;

	default:
		dev->delay = 0LL;
		dev->pos = 0;
		dev->phase = PHASE_IDLE;
		return;
    }

    /* Send byte if we can. */
    if (dev->serial != NULL) {
	serial_write(dev->serial, &b, 1);
}
}


/* Callback from serial driver: RTS was toggled. */
static void
ser_callback(void *serial, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    if (serial == NULL) return;

    serial_clear(serial);

    dev->pos = 0;
    dev->phase = PHASE_ID;

    /* Wait a little while and then actually send the ID. */
    dev->delay = dev->period;
}


static int
ser_poll(int x, int y, int z, int b, void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    if (!x && !y && !z && (b == dev->oldb) && dev->continuous)
	return(0);

    DBGLOG(1, "MOUSE: poll(%i,%i,%i,%02x)\n", x, y, z, b);

    dev->oldb = b;

    dev->abs_x += x;
    if (dev->abs_x < 0)
	dev->abs_x = 0;
    if (dev->abs_x > 4095)
	dev->abs_x = 4095;

    dev->abs_y += y;
    if (dev->abs_y < 0)
	dev->abs_y = 0;
    if (dev->abs_y > 4095)
	dev->abs_y = 4095;

    if (dev->format == 3) {
	if (x > 2047) x = 2047;
	if (y > 2047) y = 2047;
	if (x <- 2048) x = -2048;
	if (y <- 2048) y = -2048;
    } else {
	if (x > 127) x = 127;
	if (y > 127) y = 127;
	if (x <- 128) x = -128;
	if (y <- 128) y = -128;
    }

    /*
     * No report if we're either in prompt mode,
     * or the mouse wants data.
     */
    if (!dev->prompt && !dev->want_data)
	ser_report(dev, x, y, z, b);

    return(1);
}


static void
ltser_write(void *serial, void *priv, uint8_t data)
{
    mouse_t *dev = (mouse_t *)priv;

    pclog(0,"MOUSE: ltwrite(%02x) @%08lx\n", data, dev->serial);

#if 0
    /* Make sure to stop any transmission when we receive a byte. */
    if (dev->phase != PHASE_IDLE) {
	dev->delay = 0LL;
	dev->phase = PHASE_IDLE;
    }
#endif

    if (dev->want_data) switch (dev->want_data) {
	case 0x2a:
		dev->data_len--;
		dev->want_data = 0;
		dev->delay = 0LL;
		dev->phase = PHASE_IDLE;

		switch (data) {
			case 0x6e:
				dev->period = ser_transmit_period(dev, 1200, -1); /*1200 bps */
				break;

			case 0x6f:
				dev->period = ser_transmit_period(dev, 2400, -1);	/* 2400 bps */
				break;

			case 0x70:
				dev->period = ser_transmit_period(dev, 4800, -1);	/* 4800 bps */
				break;

			case 0x71:
				dev->period = ser_transmit_period(dev, 9600, -1);	/* 9600 bps */
				break;

			default:
				ERRLOG("MOUSE: invalid period %02X, using 1200 bps\n", data);
		}
		break;

    } else switch (data) {
	case 0x05:
		dev->pos = 0;
		dev->phase = PHASE_DIAG;
		if (! dev->delay)
			dev->delay = dev->period;
		break;

	case 0x2a:
		dev->want_data = data;
		dev->data_len = 1;
		break;

	case 0x44:	/* Set prompt mode */
		dev->prompt = 1;
		dev->status |= 0x40;
		break;

	case 0x50:
		if (! dev->prompt) {
			dev->prompt = 1;
			dev->status |= 0x40;
		}
		/* TODO: Here we should send the current position. */
		break;

	case 0x73:	/* Status */
		dev->pos = 0;
		dev->phase = PHASE_STATUS;
		if (! dev->delay)
			dev->delay = dev->period;
		break;

	case 0x4a:	/* Report Rate Selection commands */
	case 0x4b:
	case 0x4c:
	case 0x52:
	case 0x4d:
	case 0x51:
	case 0x4e:
	case 0x4f:
		dev->prompt = 0;
		dev->status &= 0xbf;
		// dev->continuous = (data == 0x4f);
		break;

	case 0x41:
		dev->format = 6;	/* Aboslute Bit Pad One Format */
		dev->abs_x = dev->abs_y = 0;
		break;

	case 0x42:
		dev->format = 3;	/* Relative Bit Pad One Format */
		break;

	case 0x53:
		dev->format = 5;	/* MM Series Format */
		break;

	case 0x54:
		dev->format = 1;	/* Three Byte Packed Binary Format */
		break;

	case 0x55:	/* This is the Mouse Systems-compatible format */
		dev->format = 0;	/* Five Byte Packed Binary Format */
		break;

	case 0x56:
		dev->format = 7;	/* Microsoft Compatible Format */
		break;

	case 0x57:
		dev->format = 2;	/* Hexadecimal Format */
		break;

	case 0x66:
		dev->pos = 0;
		dev->phase = PHASE_FMT_REV;
		if (! dev->delay)
			dev->delay = dev->period;
		break;
    }
}


static void
ser_close(void *priv)
{
    mouse_t *dev = (mouse_t *)priv;

    /* Detach serial port from the mouse. */
    if ((dev != NULL) && (dev->serial != NULL))
	(void)serial_attach(dev->port, NULL, NULL);

    free(dev);
}


/* Initialize the device for use by the user. */
static void *
ser_init(const device_t *info, UNUSED(void *parent))
{
    static serial_ops_t ops = {
	ser_callback,	/* mcr */
	NULL,		/* read */
	ltser_write,	/* write */
    };
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

    if (dev->type == MOUSE_MSYSTEMS) {
		dev->type = info->local;
		dev->id_len = 1;
		dev->id[0] = 'H';
    } else {
		dev->format = 7;
		dev->status = 0x0f;
		dev->id_len = 1;
		dev->id[0] = 'M';

		switch(i) {
			case 3:
				dev->id_len = 2;
				dev->id[1] = '3';
				break;

			case 4:
				dev->type = MOUSE_MSWHEEL;
				dev->id_len = 6;
				dev->id[1] = 'Z';
				dev->id[2] = '@';
				break;

			default:
				break;
		}
    }

	dev->period = ser_transmit_period(dev, 1200, -1);

    /* Attach a serial port to the mouse. */
    dev->serial = serial_attach(dev->port, &ops, dev);
    if (dev->serial == NULL) {
	ERRLOG("MOUSE: %s (port COM%i not available) device disabled!\n",
						dev->name, dev->port+1);
	free(dev);
	return(NULL);
    }

    INFO("MOUSE: %s (port=COM%i, buttons=%i)\n", dev->name, dev->port+1, i);

    timer_add(ser_timer, dev, &dev->delay, &dev->delay);

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
			NULL
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
			NULL
		}
	}
    },
    {
	NULL
    }
};

static const device_config_t ltser_config[] = {
    {
	"port", "Serial Port", CONFIG_SELECTION, "", 0, {
		{
			"COM1", 0
		},
		{
			"COM2", 1
		},
		{
			NULL
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
			NULL
		}
	}
    },
    {
	NULL
    }
};

const device_t mouse_mssystems_device = {
    "Mouse Systems Serial Mouse",
    0,
    MOUSE_MSYSTEMS,
    NULL,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    NULL
};


const device_t mouse_msserial_device = {
    "Microsoft Serial Mouse",
    0,
    MOUSE_MICROSOFT,
    NULL,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};

const device_t mouse_ltserial_device = {
    "Logitech Serial Mouse",
    0,
    MOUSE_LOGITECH,
    NULL,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ltser_config
};


const device_t mouse_mswhserial_device = {
    "Microsoft Serial Wheel Mouse",
    0,
    MOUSE_MSWHEEL,
    NULL,
    ser_init, ser_close, NULL,
    ser_poll, NULL, NULL, NULL,
    ser_config
};
