/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Zenith SupersPORT, ZWL-184-02.
 *
 *		This machine has a 80C88 running at 4.77 or 8 MHz, 1 MB
 *		of RAM (640K of which usable), an embedded EMS controller,
 *		an embedded CGA-like controller that for the internal LCD,
 *		or for driving an external monitor, it has a Ricoh RP5C15
 *		RTC chip, one or two internal 3.5" (720K) drives, one or
 *		two external floppy drives (3.5" or 5.25"), and it can
 *		have one or two internal hard disks, 10 or 20 MB.
 *
 * **NOTES**	We need to know how to read the 'configuration switches',
 *		so we can make this more complete. The TRM does mention it,
 *		but sadly, no details, it just refers to the interrupt they
 *		made for it.
 *
 *		Although the video code sort-of works, much more work has
 *		to be done on implementing other parts of the Yamaha V6355
 *		chip that implements the video controller.
 *
 * Version:	@(#)m_zenith.c	1.0.9	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original patch for PCem by 'Tux'
 *
 *		Copyright 2019 Fred N. van Kempen.
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
#include <wchar.h>
#include <time.h>
#include "../emu.h"
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pit.h"
#include "../devices/ports/game.h"
#include "../devices/ports/parallel.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"
#include "m_zenith.h"


/*
 * Ricoh RP5C15 registers.
 *
 * In modes 0 and 1, registers are accessible as noted.
 * In modes 2 and 3, addresses 00 through 0c are treated
 * as non-volatile RAM, 4 bits wide.
 */
enum RTC_REGS {
    RTC_SECOND1 = 0,			/* BCD */
    RTC_CLOCK_OUT = RTC_SECOND1,	/* Bank 1 */
    RTC_SECOND10,
    RTC_ADJUST = RTC_SECOND10,		/* Bank 1 */
    RTC_MINUTE1,			/* BCD */
    RTC_MINUTE10,
    RTC_HOUR1,				/* BCD */
    RTC_HOUR10,
    RTC_WEEKDAY,			/* BCD, 1-base */
    RTC_DAY1,				/* BCD, 1-base */
    RTC_DAY10,
    RTC_MONTH1,				/* BCD, 1-base */
    RTC_MONTH10,
    RTC_SELECT24 = RTC_MONTH10,		/* Bank 1 */
    RTC_YEAR1,				/* BCD */
    RTC_LEAP = RTC_YEAR1,		/* Bank 1 */
    RTC_YEAR10,
    RTC_MODE,				/* MODE */
    RTC_TEST,				/* TEST */
    RTC_RESET				/* RESET */
};


typedef struct {
    int		lcd;				/* use internal LCD */

    /* NVR/RTC stuff. */
    nvr_t	nvr;

    mem_map_t	sp_mapping;

    uint8_t	*sp_ram;
} zenith_t;


/* Set the current NVR time. */
#define set_nibbles(a, v) regs[(a##10)] = ((v)/10) ; regs[(a##1)] = ((v)%10)
static void
rtc_time_set(uint8_t *regs, const struct tm *tm)
{
    set_nibbles(RTC_SECOND, tm->tm_sec);
    set_nibbles(RTC_MINUTE, tm->tm_min);
    set_nibbles(RTC_HOUR, tm->tm_hour);
    regs[RTC_WEEKDAY] = (tm->tm_wday - 1);
    set_nibbles(RTC_DAY, (tm->tm_mday - 1));
    set_nibbles(RTC_MONTH, (tm->tm_mon - 1));
    set_nibbles(RTC_YEAR, (tm->tm_year - 80));
}


/* Get the current NVR time. */
#define get_nibbles(a) ((regs[(a##10)]*10)+regs[(a##1)])
static void
rtc_time_get(uint8_t *regs, struct tm *tm)
{
    tm->tm_sec = get_nibbles(RTC_SECOND);
    tm->tm_min = get_nibbles(RTC_MINUTE);
    tm->tm_hour = get_nibbles(RTC_HOUR);
    tm->tm_wday = regs[RTC_WEEKDAY] + 1;
    tm->tm_mday = get_nibbles(RTC_DAY) + 1;
    tm->tm_mon = get_nibbles(RTC_MONTH) + 1;
    tm->tm_year = get_nibbles(RTC_YEAR) + 80;
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
    if (! (regs[RTC_MODE] & 0x08)) return;

    if (rtc_bump(regs, RTC_SECOND1, 0, 60)) {
	if (rtc_bump(regs, RTC_MINUTE1, 0, 60)) {
		if (rtc_bump(regs, RTC_HOUR1, 0, 24)) {
			mon = get_nibbles(RTC_MONTH);
			yr = 1980 + get_nibbles(RTC_YEAR);
			days = nvr_get_days(mon, yr);
			if (rtc_bump(regs, RTC_DAY1, 0, days)) {
				if (rtc_bump(regs, RTC_MONTH1, 0, 12))
					rtc_bump(regs, RTC_YEAR1, 0, 99);
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

#if 0
    /* Start the RTC (done by BIOS.) */
    nvr->regs[RTC_MODE] |= 0x08;
#endif
}


/* Reset the machine's NVR to a sane state. */
static void
rtc_reset(nvr_t *nvr)
{
    /* Initialize the RTC to a known state. */
    memset(nvr->regs, 0x00, sizeof(nvr->regs));

#if 0
    /* Not needed, chip is 0-based. */
    nvr->regs[RTC_WEEKDAY] = 0x00;
    nvr->regs[RTC_DAY1] = 0x00;
    nvr->regs[RTC_MONTH1] = 0x00;
#endif
}


/* Write to one of the RTC registers. */
static void
rtc_write(uint16_t addr, uint8_t val, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t *ptr;

INFO("Zenith: rtc_wr(%04x, %02x)\n", addr, val);

    /* Point to the correct bank. */
    ptr = &nvr->regs[16 * (nvr->regs[RTC_MODE] & 0x03)];

    switch (addr & 0x000f) {
	case RTC_MODE:
	case RTC_TEST:
	case RTC_RESET:
		nvr->regs[addr & 0x000f] = (val & 0x0f);
		nvr_dosave = 1;
		break;

	default:
		ptr[addr & 0x000f] = (val & 0x0f);
		nvr_dosave = 1;
		break;

    }
}


/* Read from one of the RTC registers. */
static uint8_t
rtc_read(uint16_t addr, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t r = 0xff;
    uint8_t *ptr;

    /* Point to the correct bank. */
    ptr = &nvr->regs[16 * (nvr->regs[RTC_MODE] & 0x03)];

    switch (addr & 0x000f) {
	case RTC_MODE:		/* write-only registers */
	case RTC_TEST:
	case RTC_RESET:
		r = 0x00;
		break;

	default:
		r = ptr[addr & 0x000f];
		break;
    }
    r &= 0x0f;

INFO("Zenith: rtc_rd(%04x) = %02x\n", addr, r);
    return(r);
}


/* Read from the Scratchpad RAM, which is apparently 2K in size. */
static uint8_t
sp_read(uint32_t addr, priv_t priv)
{
    zenith_t *dev = (zenith_t *)priv;

    return dev->sp_ram[addr & 0x3fff];
}


/* Write to the Scratchpad RAM, which is apparently 2K in size. */
static void
sp_write(uint32_t addr, uint8_t val, priv_t priv)
{
    zenith_t *dev = (zenith_t *)priv;

    dev->sp_ram[addr & 0x3fff] = val;
}


static void 
zenith_close(priv_t priv)
{
    zenith_t *dev = (zenith_t *)priv;

    /* Make sure NVR gets saved. */
    nvr_dosave = 1;
    nvr_save();

    if (dev->sp_ram != NULL)
	free(dev->sp_ram);

    free(dev);
}


static priv_t
zenith_init(const device_t *info, void *arg)
{		
    zenith_t *dev;
    priv_t vid;

    dev = (zenith_t *)mem_alloc(sizeof(zenith_t));
    memset(dev, 0x00, sizeof(zenith_t));	

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    dev->lcd = machine_get_config_int("lcd");

    machine_common_init();

    nmi_init();

    /* Set up our DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_xt_refresh_timer);

    /* Set up and initialize the Ricoh RP5C15 RTC. */
    dev->nvr.size = 64;
    dev->nvr.irq = -1;
    dev->nvr.reset = rtc_reset;
    dev->nvr.start = rtc_start;
    dev->nvr.tick = rtc_ticker;
    nvr_init(&dev->nvr);
    io_sethandler(0x0050, 16,
		  rtc_read,NULL,NULL, rtc_write,NULL,NULL, &dev->nvr);

    /* Disable ROM area at F0000, need it for Scratchpad RAM. */
    mem_map_disable(&bios_mapping[4]);
    mem_map_disable(&bios_mapping[5]);

    /* Allocate and initialize the Scratchpad RAM area. */
    dev->sp_ram = (uint8_t *)mem_alloc(16384);
    memset(dev->sp_ram, 0x00, 16384);	
   
    /* Create and enable a mapping for this memory. */
    mem_map_add(&dev->sp_mapping, 0xf0000, 16384,
		sp_read,NULL,NULL, sp_write,NULL,NULL,
		dev->sp_ram, MEM_MAPPING_EXTERNAL, dev);

    /* Only use the LCD if configured. */
    if (config.video_card == VID_INTERNAL) {
	vid = device_add_parent(&zenith_ss_video_device, dev);
	zenith_vid_set_internal(vid, dev->lcd);
    }

    device_add(&keyboard_generic_device);

    device_add(&fdc_xt_device);

    return((priv_t)dev);
}


static const device_config_t ss_config[] = {
    {
	"lcd", "Internal LCD", CONFIG_SELECTION, "", 1,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled", 1
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

static const machine_t ss_info = {
    MACHINE_ISA | MACHINE_VIDEO,
    0,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_8088}}
};

const device_t m_zenith_ss = {
    "Zenith SupersPort",
    DEVICE_ROOT,
    0,
    L"zenith/supersport",
    zenith_init, zenith_close, NULL,
    NULL, NULL, NULL,
    &ss_info,
    ss_config
};
