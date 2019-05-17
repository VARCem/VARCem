/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Olivetti M21, M24 and M240 machines.
 *
 *		The base machine is the M24, a desktop system. The portable
 *		version of it was the M21, which is virtually the same, but
 *		has an onboard LCD instead of a CRT.  An upgraded machine
 *		was released as the M240, with upgraded BIOS, default 640KB
 *		of RAM, high-density diskette drives, etc.
 *
 * **NOTE**	This emulation is incomplete. Several of the DIP switches
 *		are not (properly) implemented.
 *
 * **TODO**	The RTC clock is very limited, and does not store any 'year'
 *		data at all, so there seems to not be a way to properly do
 *		that..  The chip's interrupt pin is not connected.
 *
 * Version:	@(#)m_olim24.c	1.0.21	2019/05/13
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include <time.h>
#define dbglog kbd_log
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../timer.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/ppi.h"
#include "../devices/system/nmi.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_speaker.h"
#include "machine.h"
#include "m_olim24.h"


/* MM58174AN registers. */
enum RTC_REGS {
    RTC_TEST = 0,	/* TEST register */
    RTC_SECOND0,	/* BCD */
    RTC_SECOND1,
    RTC_SECOND10,
    RTC_MINUTE1,	/* BCD */
    RTC_MINUTE10,
    RTC_HOUR1,		/* BCD */
    RTC_HOUR10,
    RTC_DAY1,		/* BCD, 1-base */
    RTC_DAY10,
    RTC_WEEKDAY,	/* BCD, 1-base */
    RTC_MONTH1,		/* BCD, 1-base */
    RTC_MONTH10,
    RTC_YEAR,
    RTC_RESET,		/* RESET register */
    RTC_INTR		/* Interrupts register */
};


/* Keyboard status flags. */
#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01


typedef struct {
    /* General. */
    int		type;

    /* NVR/RTC stuff. */
    nvr_t	nvr;

    /* Keyboard stuff. */
    int		wantirq;
    uint8_t	command;
    uint8_t	status;
    uint8_t	out;
    uint8_t	output_port;
    int		param,
		param_total;
    uint8_t	params[16];
    uint8_t	scan[7];

    /* Mouse stuff. */
    int		mouse_mode;
    int		x, y, b;
} olim24_t;


extern const device_t st506_xt_olim240_hdc_device;


static uint8_t	key_queue[16];
static int	key_queue_start = 0,
		key_queue_end = 0;


static void
kbd_poll(priv_t priv)
{
    olim24_t *dev = (olim24_t *)priv;

    keyboard_delay += (1000LL * TIMER_USEC);
    if (dev->wantirq) {
	dev->wantirq = 0;
	picint(1 << 1);
    }

    if (!(dev->status & STAT_OFULL) && key_queue_start != key_queue_end) {
	dev->out = key_queue[key_queue_start];
	key_queue_start = (key_queue_start + 1) & 0xf;
	dev->status |=  STAT_OFULL;
	dev->status &= ~STAT_IFULL;
	dev->wantirq = 1;
    }
}


static void
kbd_adddata(uint16_t val)
{
    key_queue[key_queue_end] = (uint8_t)(val&0xff);
    key_queue_end = (key_queue_end + 1) & 0xf;
}


static void
kbd_adddata_ex(uint16_t val)
{
    keyboard_adddata(val, kbd_adddata);
}


static void
kbd_write(uint16_t port, uint8_t val, priv_t priv)
{
    olim24_t *dev = (olim24_t *)priv;

    DBGLOG(2, "M24kbd: write(%04x,%02x)\n", port, val);

    switch (port) {
	case 0x60:
		if (dev->param != dev->param_total) {
			dev->params[dev->param++] = val;
			if (dev->param == dev->param_total) {
				switch (dev->command) {
					case 0x11:
						dev->mouse_mode = 0;
						dev->scan[0] = dev->params[0];
						dev->scan[1] = dev->params[1];
						dev->scan[2] = dev->params[2];
						dev->scan[3] = dev->params[3];
						dev->scan[4] = dev->params[4];
						dev->scan[5] = dev->params[5];
						dev->scan[6] = dev->params[6];
						break;

					case 0x12:
						dev->mouse_mode = 1;
						dev->scan[0] = dev->params[0];
						dev->scan[1] = dev->params[1];
						dev->scan[2] = dev->params[2];
						break;
					
					default:
						DEBUG("M24: bad keyboard command complete %02X\n", dev->command);
						break;
				}
			}
		} else {
			dev->command = val;
			switch (val) {
				case 0x01: /*Self-test*/
					break;

				case 0x05: /*Read ID*/
					kbd_adddata(0x00);
					break;

				case 0x11:
					dev->param = 0;
					dev->param_total = 9;
					break;

				case 0x12:
					dev->param = 0;
					dev->param_total = 4;
					break;

				default:
					ERRLOG("M24: bad keyboard command %02X\n", val);
					break;
			}
		}
		break;

	case 0x61:
		ppi.pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();
		speaker_gated = val & 1;
		speaker_enable = val & 2;
		if (speaker_enable) 
			speaker_was_enable = 1;
		pit_set_gate(&pit, 2, val & 1);
		break;
    }
}


static uint8_t
kbd_read(uint16_t port, priv_t priv)
{
    olim24_t *dev = (olim24_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x60:
		ret = dev->out;
		if (key_queue_start == key_queue_end) {
			dev->status &= ~STAT_OFULL;
			dev->wantirq = 0;
		} else {
			dev->out = key_queue[key_queue_start];
			key_queue_start = (key_queue_start + 1) & 0xf;
			dev->status |= STAT_OFULL;
			dev->status &= ~STAT_IFULL;
			dev->wantirq = 1;	
		}
		break;

	case 0x61:
		ret = ppi.pb;
		break;

	case 0x64:
		ret = dev->status;
		dev->status &= ~(STAT_RTIMEOUT | STAT_TTIMEOUT);
		break;

	default:
		ERRLOG("\nBad M24 keyboard read %04X\n", port);
		break;
    }

    return(ret);
}


static int
mse_poll(int x, int y, int z, int b, priv_t priv)
{
    olim24_t *dev = (olim24_t *)priv;

    dev->x += x;
    dev->y += y;

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 1) && !(dev->b & 1))
	kbd_adddata(dev->scan[0]);
    if (!(b & 1) && (dev->b & 1))
	kbd_adddata(dev->scan[0] | 0x80);
    dev->b = (dev->b & ~1) | (b & 1);

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 2) && !(dev->b & 2))
	kbd_adddata(dev->scan[2]);
    if (!(b & 2) && (dev->b & 2))
	kbd_adddata(dev->scan[2] | 0x80);
    dev->b = (dev->b & ~2) | (b & 2);

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 4) && !(dev->b & 4))
	kbd_adddata(dev->scan[1]);
    if (!(b & 4) && (dev->b & 4))
	kbd_adddata(dev->scan[1] | 0x80);
    dev->b = (dev->b & ~4) | (b & 4);

    if (dev->mouse_mode) {
	if (((key_queue_end - key_queue_start) & 0xf) > 12)
		return(0);

	if (!dev->x && !dev->y)
		return(0);

	dev->y = -dev->y;

	if (dev->x < -127) dev->x = -127;
	if (dev->x >  127) dev->x =  127;
	if (dev->x < -127) dev->x = 0x80 | ((-dev->x) & 0x7f);

	if (dev->y < -127) dev->y = -127;
	if (dev->y >  127) dev->y =  127;
	if (dev->y < -127) dev->y = 0x80 | ((-dev->y) & 0x7f);

	kbd_adddata(0xfe);
	kbd_adddata(dev->x);
	kbd_adddata(dev->y);

	dev->x = dev->y = 0;
    } else {
	while (dev->x < -4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
			return(0);
		dev->x += 4;
		kbd_adddata(dev->scan[3]);
	}
	while (dev->x > 4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0);
		dev->x -= 4;
		kbd_adddata(dev->scan[4]);
	}
	while (dev->y < -4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0);
		dev->y += 4;
		kbd_adddata(dev->scan[5]);
	}
	while (dev->y > 4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0);
		dev->y -= 4;
		kbd_adddata(dev->scan[6]);
	}
    }

    return(1);
}


/* Set the current NVR time. */
#define set_nibbles(a, v) regs[(a##10)] = ((v)/10) ; regs[(a##1)] = ((v)%10)
static void
rtc_time_set(uint8_t *regs, const struct tm *tm)
{
    set_nibbles(RTC_SECOND, tm->tm_sec);
    set_nibbles(RTC_MINUTE, tm->tm_min);
    set_nibbles(RTC_HOUR, tm->tm_hour);
    regs[RTC_WEEKDAY] = tm->tm_wday;
    set_nibbles(RTC_DAY, tm->tm_mday);
    set_nibbles(RTC_MONTH, (tm->tm_mon + 1));
    regs[RTC_YEAR] = ((tm->tm_year - 84) % 4);
}


/* Get the current NVR time. */
#define get_nibbles(a) ((regs[(a##10)]*10)+regs[(a##1)])
static void
rtc_time_get(uint8_t *regs, struct tm *tm)
{
    tm->tm_sec = get_nibbles(RTC_SECOND);
    tm->tm_min = get_nibbles(RTC_MINUTE);
    tm->tm_hour = get_nibbles(RTC_HOUR);
    tm->tm_wday = regs[RTC_WEEKDAY];
    tm->tm_mday = get_nibbles(RTC_DAY);
    tm->tm_mon = (get_nibbles(RTC_MONTH) - 1);
    tm->tm_year = (regs[RTC_YEAR] + 1984);
}


/* Bump a dual-4bit-register. */
static int
rtc_bump(uint8_t *regs, int base, int min, int max)
{
    int k;

    k = (regs[base + 1] * 10) + regs[base];
    if (++k >= (max + min)) {
	/* Rollover, reset to 'min' and return. */
	regs[base] = min;
	regs[base + 1] = 0;
	return(1);
    }

    /* No rollover, save the bumped value. */
    regs[base] = (k % 10);
    regs[base + 1] = (k / 10);

    return(0);
}


/*
 * This is called every second through the NVR/RTC hook.
 *
 * We fake a 'running' RTC by updating its registers on
 * each passing second. Not exactly accurate, but good
 * enough.
 */
static void
rtc_ticker(nvr_t *nvr)
{
    uint8_t *regs = nvr->regs;
    int mon, yr, days;

    /* Only if RTC is running.. */
    if (! (regs[RTC_RESET] & 0x01)) return;

    if (rtc_bump(regs, RTC_SECOND1, 0, 60)) {
	if (rtc_bump(regs, RTC_MINUTE1, 0, 60)) {
		if (rtc_bump(regs, RTC_HOUR1, 0, 24)) {
			mon = get_nibbles(RTC_MONTH);
			yr = 1985 + regs[RTC_YEAR];
			days = nvr_get_days(mon, yr);
			if (rtc_bump(regs, RTC_DAY1, 1, days)) {
				if (rtc_bump(regs, RTC_MONTH1, 1, 12))
					regs[RTC_YEAR]++;
			}
		}
	}
    }
}


static void
rtc_start(nvr_t *nvr)
{
    struct tm tm;

    /* Initialize the internal and chip times. */
    if (config.time_sync != TIME_SYNC_DISABLED) {
	/* Use the internal clock's time. */
	nvr_time_get(&tm);
	rtc_time_set(nvr->regs, &tm);
    } else {
	/* Set the internal clock from the chip time. */
	rtc_time_get(nvr->regs, &tm);
	nvr_time_set(&tm);
    }

    /* Start the RTC. */
    nvr->regs[RTC_RESET] |= 0x01;
}


/* Reset the machine's NVR to a sane state. */
static void
rtc_reset(nvr_t *nvr)
{
    /* Initialize the RTC to a known state. */
    memset(nvr->regs, 0x00, sizeof(nvr->regs));

    nvr->regs[RTC_WEEKDAY] = 0x01;
    nvr->regs[RTC_DAY1] = 0x01;
    nvr->regs[RTC_MONTH1] = 0x01;
}


/* Write to one of the RTC registers. */
static void
rtc_write(uint16_t addr, uint8_t val, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t *regs = nvr->regs;

    DBGLOG(2, "M24: rtc_wr(%04x, %02x)\n", addr, val);

    switch (addr & 0x000f) {
	case RTC_TEST:				/* not implemented */
		break;

	case RTC_MINUTE1:
	case RTC_MINUTE10:
	case RTC_HOUR1:
	case RTC_HOUR10:
	case RTC_DAY1:
	case RTC_DAY10:
	case RTC_WEEKDAY:
	case RTC_MONTH1:
	case RTC_MONTH10:
	case RTC_YEAR:
		regs[addr & 0x000f] = (val & 0x0f);
		nvr_dosave = 1;
		break;

	case RTC_RESET:
		regs[addr & 0x000f] = (val & 0x0f);
		nvr_dosave = 1;
		break;

	case RTC_INTR:
		regs[addr & 0x000f] = (val & 0x0f);
		nvr_dosave = 1;
		break;
    }
}


/* Read from one of the RTC registers. */
static uint8_t
rtc_read(uint16_t addr, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t *regs = nvr->regs;
    uint8_t r = 0xff;

    switch (addr & 0x000f) {
	case RTC_SECOND0:
		r = regs[addr & 0x000f];
		break;

	case RTC_SECOND1:
	case RTC_SECOND10:
	case RTC_MINUTE1:
	case RTC_MINUTE10:
	case RTC_HOUR1:
	case RTC_HOUR10:
	case RTC_DAY1:
	case RTC_DAY10:
	case RTC_WEEKDAY:
	case RTC_MONTH1:
	case RTC_MONTH10:
		r = regs[addr & 0x000f];
		break;

	case RTC_INTR:
		r = regs[addr & 0x000f];
		break;
    }

    DBGLOG(2, "M24: rtc_rd(%04x) = %02x\n", addr, r);

    return(r);
}


static uint8_t
m24_read(uint16_t port, priv_t priv)
{
    switch (port) {
	case 0x66:	/* System Configuration 1 (DIPSW-0) */
		/*
		 * 7-SW8: 0=2732, 1=2764 ROM
		 * 6-SW7: not used
		 * 5-SW6: 0=8250, 1=8530 SIO
		 * 4-SW5: 0=no 8087, 1=8087
		 * 3-SW4: 0=64K, 1=256K chips used
		 * 2-SW3: DRAM config
		 * 1-SW2: DRAM config
		 * 0-SW1: DRAM config
		 *
		 * SW4 SW3 SW2 SW1     BANK_0  BANK_1  EXPBRD  TOTAL
		 *  0   0   0   1	128	0	0	128
		 *  0   0   1   0	128	128	0	256
		 *  0   0   1   1	128	128	128	384
		 *  0   1   0   0	128	128	256	512
		 *  0   1   0   1	128	128	384	640
		 *  1   0   0   0	512	0	0	512
		 *  1	0   0   1	512	128	0	640
		 */
		return(0x80);

	case 0x67:	/* System Configuration 2 (DIPSW-1) */
		/*
		 * 7-SW8: #drives
		 * 6-SW7: #drives
		 * 5-SW6: monitor
		 * 4-SW5: monitor
		 * 3-SW4: HDU
		 * 2-SW3: HDU
		 * 1-SW2: *0=slow start, 1=fast start drive
		 * 0-SW1: *0=48tpi, 1=96tpi floppy
		 *
		 * SW6  SW5	Display type
		 *  1    1	Monochrome
		 *  1    0	*Color 80x25
		 *  0    1	Color 40x25
		 *  0	 0	External card
		 *
		 * SW8  SW7
		 *  1    1	4 diskette drives
		 *  1    0	3 drives
		 *  0    1	*2 drives
		 *  0    0	1 drive
		 */
		return(0x20 | 0x40 | 0x0c);
    }

    return(0xff);
}


static void
olim24_close(priv_t priv)
{
    olim24_t *dev = (olim24_t *)priv;
    nvr_t *nvr = &dev->nvr;

    /* Make sure NVR gets saved. */
    nvr_dosave = 1;
    nvr_save();
    if (nvr->fn != NULL)
	free((wchar_t *)nvr->fn);

    free(dev);
}


static priv_t
olim24_init(const device_t *info, void *arg)
{
    olim24_t *dev;

    dev = (olim24_t *)mem_alloc(sizeof(olim24_t));
    memset(dev, 0x00, sizeof(olim24_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    machine_common_init();

    nmi_init();

    /* Set up and initialize the NVR. */
    dev->nvr.size = 16;
    dev->nvr.irq = -1;
    dev->nvr.reset = rtc_reset;
    dev->nvr.start = rtc_start;
    dev->nvr.tick = rtc_ticker;
    nvr_init(&dev->nvr);
    io_sethandler(0x0070, 16,
		  rtc_read,NULL,NULL, rtc_write,NULL,NULL, &dev->nvr);

    io_sethandler(0x0066, 2,
		  m24_read,NULL,NULL, NULL,NULL,NULL, dev);

    /* Initialize the video adapter. */
    m_olim24_vid_init(0);

    /* Initialize the keyboard. */
    dev->status = STAT_LOCK | STAT_CD;
    dev->scan[0] = 0x1c;
    dev->scan[1] = 0x53;
    dev->scan[2] = 0x01;
    dev->scan[3] = 0x4b;
    dev->scan[4] = 0x4d;
    dev->scan[5] = 0x48;
    dev->scan[6] = 0x50;
    io_sethandler(0x0060, 2,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    io_sethandler(0x0064, 1,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    keyboard_set_table(scancode_xt);
    keyboard_send = kbd_adddata_ex;
    keyboard_scan = 1;
    timer_add(kbd_poll, dev, &keyboard_delay, TIMER_ALWAYS_ENABLED);

    /* Tell mouse driver about our internal mouse. */
    mouse_reset();
    mouse_set_poll(mse_poll, dev);

    device_add(&fdc_xt_device);

    if (dev->type == 2)
	device_add(&st506_xt_olim240_hdc_device);

    return((priv_t)dev);
}


static const machine_t m24_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_8086},{"NEC",cpus_nec}}
};

const device_t m_oli_m24 = {
    "Olivetti M24",
    DEVICE_ROOT,
    0,
    L"olivetti/m24",
    olim24_init, olim24_close, NULL,
    NULL, NULL, NULL,
    &m24_info,
    NULL
};


static const machine_t pc6300_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_8086},{"NEC",cpus_nec}}
};

const device_t m_att_6300 = {
    "AT&T PC6300",
    DEVICE_ROOT,
    1,
    L"att/pc6300",
    olim24_init, olim24_close, NULL,
    NULL, NULL, NULL,
    &pc6300_info,
    NULL
};


static const machine_t m240_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_8086},{"NEC",cpus_nec}}
};

const device_t m_oli_m240 = {
    "Olivetti M240",
    DEVICE_ROOT,
    2,
    L"olivetti/m240",
    olim24_init, olim24_close, NULL,
    NULL, NULL, NULL,
    &m240_info,
    NULL
};
