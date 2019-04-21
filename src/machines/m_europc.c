/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Schneider EuroPC system.
 *
 * NOTES:	BIOS info (taken from MAME, thanks guys!!)
 *
 *		f000:e107	bios checksum test
 *				memory test
 *		f000:e145	irq vector init
 *		f000:e156
 *		f000:e169-d774	test of special registers 254/354
 *		f000:e16c-e817
 *		f000:e16f
 *		f000:ec08	test of special registers 800a rtc time
 *				or date error, rtc corrected
 *		f000:ef66 0xf
 *		f000:db3e 0x8..0xc
 *		f000:d7f8
 *		f000:db5f
 *		f000:e172 
 *		f000:ecc5	801a video setup error
 *		f000:d6c9	copyright output
 *		f000:e1b7
 *		f000:e1be	DI bits set mean output text!!!	(801a)
 *		f000:		0x8000 output
 *				  1 rtc error
 *				  2 rtc time or date error
 *				  4 checksum error in setup
 *				  8 rtc status corrected
 *			   	 10 video setup error
 *			  	 20 video ram bad
 *			 	 40 monitor type not recogniced
 *				 80 mouse port enabled
 *			 	100 joystick port enabled
 *		f000:e1e2-dc0c	CPU speed is 4.77 mhz
 *		f000:e1e5-f9c0	keyboard processor error
 *		f000:e1eb-c617	external LPT1 at 0x3bc
 *		f000:e1ee-e8ee	external coms at
 *
 *		Routines:
 *		  f000:c92d	output text at bp
 *		  f000:db3e	RTC read reg cl
 *	  	  f000:e8ee	piep
 *		  f000:e95e	RTC write reg cl
 *				polls until JIM 0xa is zero,
 *				output cl at jim 0xa
 *				write ah hinibble as lownibble into jim 0xa
 *				write ah lownibble into jim 0xa
 *		  f000:ef66	RTC read reg cl
 *				polls until jim 0xa is zero,
 *				output cl at jim 0xa
 *				read low 4 nibble at jim 0xa
 *				read low 4 nibble at jim 0xa
 *				return first nibble<<4|second nibble in ah
 *		  f000:f046	seldom compares ret 
 *		  f000:fe87	0 -> ds
 *
 *		Memory:
 *		  0000:0469	bit 0: b0000 memory available
 *				bit 1: b8000 memory available
 *		  0000:046a:	00 jim 250 01 jim 350
 *
 * FIXME:	Find a new way to handle the switching of color/mono on
 *		external cards. New video_get_type(int card) function?
 *
 * Version:	@(#)m_europc.c	1.0.21	2019/04/11
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Inspired by the "jim.c" file originally present, but a
 *		fully re-written module, based on the information from
 *		Schneider's schematics and technical manuals, and the
 *		input from people with real EuroPC hardware.
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/pit.h"
#include "../devices/system/nmi.h"
#include "../devices/ports/parallel.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/video/video.h"
#include "machine.h"


/* M3002 RTC chip registers. */
#define MRTC_SECONDS	0x00			/* BCD, 00-59 */
#define MRTC_MINUTES	0x01			/* BCD, 00-59 */
#define MRTC_HOURS	0x02			/* BCD, 00-23 */
#define MRTC_DAYS	0x03			/* BCD, 01-31 */
#define MRTC_MONTHS	0x04			/* BCD, 01-12 */
#define MRTC_YEARS	0x05			/* BCD, 00-99 (year only) */
#define MRTC_WEEKDAY	0x06			/* BCD, 01-07 */
#define MRTC_WEEKNO	0x07			/* BCD, 01-52 */
#define MRTC_CONF_A	0x08			/* EuroPC config, binary */
#define MRTC_CONF_B	0x09			/* EuroPC config, binary */
#define MRTC_CONF_C	0x0a			/* EuroPC config, binary */
#define MRTC_CONF_D	0x0b			/* EuroPC config, binary */
#define MRTC_CONF_E	0x0c			/* EuroPC config, binary */
#define MRTC_CHECK_LO	0x0d			/* Checksum, low byte */
#define MRTC_CHECK_HI	0x0e			/* Checksum, high byte */
#define MRTC_CTRLSTAT	0x0f			/* RTC control/status, binary */


typedef struct {
    uint16_t	jim;				/* JIM base address */

    uint8_t	regs[16];			/* JIM internal regs (8) */

    nvr_t	nvr;				/* NVR */
    uint8_t	nvr_stat;
    uint8_t	nvr_addr;
} europc_t;


/*
 * This is called every second through the NVR/RTC hook.
 *
 * We fake a 'running' RTC by updating its registers on
 * each passing second. Not exactly accurate, but good
 * enough.
 *
 * Note that this code looks nasty because of all the
 * BCD to decimal vv going on.
 *
 * FIXME: should we mark NVR as dirty?
 */
static void
rtc_ticker(nvr_t *nvr)
{
    uint8_t *regs;
    int mon, yr;

    /* Only if RTC is running.. */
    regs = nvr->regs;
    if (! (regs[MRTC_CTRLSTAT] & 0x01)) return;

    regs[MRTC_SECONDS] = RTC_BCDINC(nvr->regs[MRTC_SECONDS], 1);
    if (regs[MRTC_SECONDS] >= RTC_BCD(60)) {
	regs[MRTC_SECONDS] = RTC_BCD(0);
	regs[MRTC_MINUTES] = RTC_BCDINC(regs[MRTC_MINUTES], 1);
	if (regs[MRTC_MINUTES] >= RTC_BCD(60)) {
		regs[MRTC_MINUTES] = RTC_BCD(0);
		regs[MRTC_HOURS] = RTC_BCDINC(regs[MRTC_HOURS], 1);
		if (regs[MRTC_HOURS] >= RTC_BCD(24)) {
			regs[MRTC_HOURS] = RTC_BCD(0);
			regs[MRTC_DAYS] = RTC_BCDINC(regs[MRTC_DAYS], 1);
			mon = RTC_DCB(regs[MRTC_MONTHS]);
			yr = RTC_DCB(regs[MRTC_YEARS]) + 1900;
			if (RTC_DCB(regs[MRTC_DAYS]) > nvr_get_days(mon, yr)) {
				regs[MRTC_DAYS] = RTC_BCD(1);
				regs[MRTC_MONTHS] = RTC_BCDINC(regs[MRTC_MONTHS], 1);
				if (regs[MRTC_MONTHS] > RTC_BCD(12)) {
					regs[MRTC_MONTHS] = RTC_BCD(1);
					regs[MRTC_YEARS] = RTC_BCDINC(regs[MRTC_YEARS], 1) & 0xff;
				}
			}
		}
	}
    }
}


/* Get the current NVR time. */
static void
rtc_time_get(uint8_t *regs, struct tm *tm)
{
    /* NVR is in BCD data mode. */
    tm->tm_sec = RTC_DCB(regs[MRTC_SECONDS]);
    tm->tm_min = RTC_DCB(regs[MRTC_MINUTES]);
    tm->tm_hour = RTC_DCB(regs[MRTC_HOURS]);
    tm->tm_wday = (RTC_DCB(regs[MRTC_WEEKDAY]) - 1);
    tm->tm_mday = RTC_DCB(regs[MRTC_DAYS]);
    tm->tm_mon = (RTC_DCB(regs[MRTC_MONTHS]) - 1);
    tm->tm_year = RTC_DCB(regs[MRTC_YEARS]);
#ifdef MRTC_CENTURY
    tm->tm_year += (RTC_DCB(regs[MRTC_CENTURY]) * 100) - 1900;
#endif
}


/* Set the current NVR time. */
static void
rtc_time_set(uint8_t *regs, struct tm *tm)
{
    /* NVR is in BCD data mode. */
    regs[MRTC_SECONDS] = RTC_BCD(tm->tm_sec);
    regs[MRTC_MINUTES] = RTC_BCD(tm->tm_min);
    regs[MRTC_HOURS] = RTC_BCD(tm->tm_hour);
    regs[MRTC_WEEKDAY] = RTC_BCD(tm->tm_wday + 1);
    regs[MRTC_DAYS] = RTC_BCD(tm->tm_mday);
    regs[MRTC_MONTHS] = RTC_BCD(tm->tm_mon + 1);
    regs[MRTC_YEARS] = RTC_BCD(tm->tm_year % 100);
#ifdef MRTC_CENTURY
    regs[MRTC_CENTURY] = RTC_BCD((tm->tm_year+1900) / 100);
#endif
}


static void
rtc_start(nvr_t *nvr)
{
    struct tm tm;

    /* Initialize the internal and chip times. */
    if (time_sync != TIME_SYNC_DISABLED) {
	/* Use the internal clock's time. */
	nvr_time_get(&tm);
	rtc_time_set(nvr->regs, &tm);
    } else {
	/* Set the internal clock from the chip time. */
	rtc_time_get(nvr->regs, &tm);
	nvr_time_set(&tm);
    }
}


/* Create a valid checksum for the current NVR data. */
static uint8_t
rtc_checksum(uint8_t *ptr)
{
    uint8_t sum;
    int i;

    /* Calculate all bytes with XOR. */
    sum = 0x00;
    for (i = MRTC_CONF_A; i <= MRTC_CONF_E; i++)
	sum += ptr[i];

    return(sum);
}


/* Reset the machine's NVR to a sane state. */
static void
rtc_reset(nvr_t *nvr)
{
    /* Initialize the RTC to a known state. */
    nvr->regs[MRTC_SECONDS] = RTC_BCD(0);	/* seconds */
    nvr->regs[MRTC_MINUTES] = RTC_BCD(0);	/* minutes */
    nvr->regs[MRTC_HOURS] = RTC_BCD(0);		/* hours */
    nvr->regs[MRTC_DAYS] = RTC_BCD(1);		/* days */
    nvr->regs[MRTC_MONTHS] = RTC_BCD(1);	/* months */
    nvr->regs[MRTC_YEARS] = RTC_BCD(80);	/* years */
    nvr->regs[MRTC_WEEKDAY] = RTC_BCD(1);	/* weekday */
    nvr->regs[MRTC_WEEKNO] = RTC_BCD(1);	/* weekno */

    /*
     * EuroPC System Configuration:
     *
     * [A]	unknown
     *
     * [B]	7	1  bootdrive extern
     *			0  bootdrive intern
     *		6:5	11 invalid hard disk type
     *			10 hard disk installed, type 2
     *			01 hard disk installed, type 1
     *			00 hard disk not installed
     *		4:3	11 invalid external drive type
     *			10 external drive 720K
     *			01 external drive 360K
     *			00 external drive disabled
     *		2	unknown
     *		1:0	11 invalid internal drive type
     *			10 internal drive 720K
     *			01 internal drive 360K
     *			00 internal drive disabled
     *
     *	 [C]	7:6	unknown
     *		5	monitor detection OFF
     *		4	unknown
     *		3:2	11 illegal memory size (768K ?)
     *			10 512K
     *			01 256K
     *			00 640K
     *		1:0	11 illegal game port
     *			10 gameport as mouse port
     *			01 gameport as joysticks
     *			00 gameport disabled
     *
     * [D]	7:6	10 9MHz CPU speed
     *			01 7MHz CPU speed
     *			00 4.77 MHz CPU
     *		5	unknown
     *		4	external: color, internal: mono
     *		3	unknown
     *		2	internal video ON
     *		1:0	11 mono
     *			10 color80
     *			01 color40
     *			00 special (EGA,VGA etc)
     *
     * [E]	7:4	unknown
     *		3:0	country	(00=Deutschland, 0A=ASCII)
     */
    nvr->regs[MRTC_CONF_A] = 0x00;	/* CONFIG A */
    nvr->regs[MRTC_CONF_B] = 0x0A;	/* CONFIG B */
    nvr->regs[MRTC_CONF_C] = 0x28;	/* CONFIG C */
    nvr->regs[MRTC_CONF_D] = 0x12;	/* CONFIG D */
    nvr->regs[MRTC_CONF_E] = 0x0A;	/* CONFIG E */

    nvr->regs[MRTC_CHECK_LO] = 0x00;	/* checksum (LO) */
    nvr->regs[MRTC_CHECK_HI] = 0x00;	/* checksum (HI) */

    nvr->regs[MRTC_CTRLSTAT] = 0x01;	/* status/control */

    /* Generate a valid checksum. */
    nvr->regs[MRTC_CHECK_LO] = rtc_checksum(nvr->regs);
}


/* Execute a JIM control command. */
static void
jim_set(europc_t *dev, uint8_t reg, uint8_t val)
{
    switch(reg) {
	case 0:		/* MISC control (WO) */
		/* bit0: enable MOUSE */
		/* bit1: enable joystick */
		break;

	case 2:		/* AGA control */
		if (! (val & 0x80)) {
			/* Reset AGA. */
			break;
		}

		switch (val) {
			case 0x1f:	/* 0001 1111 */
			case 0x0b:	/* 0000 1011 */
				/*europc_jim.mode=AGA_MONO; */
				DEBUG("EuroPC: AGA Monochrome mode!\n");
				break;

			case 0x18:	/* 0001 1000 */
			case 0x1a:	/* 0001 1010 */
				/*europc_jim.mode=AGA_COLOR; */
				break;

			case 0x0e:	/* 0000 1100 */
				/*80 columns? */
				DEBUG("EuroPC: AGA 80-column mode!\n");
				break;

			case 0x0d:	/* 0000 1011 */
				/*40 columns? */
				DEBUG("EuroPC: AGA 40-column mode!\n");
				break;

			default:
				/*europc_jim.mode=AGA_OFF; */
				break;
		}
		break;

	case 4:		/* CPU Speed control */
		switch(val & 0xc0) {
			case 0x00:	/* 4.77 MHz */
/*FIXME:			cpu_set_clockscale(0, 1.0/2); */
				break;

			case 0x40:	/* 7.16 MHz */
/*FIXME:			cpu_set_clockscale(0, 3.0/4); */
				break;

			default:	/* 9.54 MHz */
/*FIXME:			cpu_set_clockscale(0, 1);break; */
				break;
		}
		break;

	default:
		break;
    }

    dev->regs[reg] = val;
}


/* Write to one of the JIM registers. */
static void
jim_write(uint16_t addr, uint8_t val, void *priv)
{
    europc_t *dev = (europc_t *)priv;
    uint8_t b;

    DBGLOG(2, "EuroPC: jim_wr(%04x, %02x)\n", addr, val);

    switch (addr & 0x000f) {
	case 0x00:		/* JIM internal registers (WRONLY) */
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:		/* JIM internal registers (R/W) */
	case 0x05:
	case 0x06:
	case 0x07:
		jim_set(dev, (addr & 0x07), val);
		break;

	case 0x0a:		/* M3002 RTC INDEX/DATA register */
		switch(dev->nvr_stat) {
			case 0:		/* save index */
				dev->nvr_addr = val & 0x0f;
				dev->nvr_stat++;
				break;

			case 1:		/* save data HI nibble */
				b = dev->nvr.regs[dev->nvr_addr] & 0x0f;
				b |= (val << 4);
				dev->nvr.regs[dev->nvr_addr] = b;
				dev->nvr_stat++;
				nvr_dosave++;
				break;

			case 2:		/* save data LO nibble */
				b = dev->nvr.regs[dev->nvr_addr] & 0xf0;
				b |= (val & 0x0f);
				dev->nvr.regs[dev->nvr_addr] = b;
				dev->nvr_stat = 0;
				nvr_dosave++;
				break;
		}
		break;

	default:
		ERRLOG("EuroPC: invalid JIM write %02x, val %02x\n", addr, val);
		break;
    }
}


/* Read from one of the JIM registers. */
static uint8_t
jim_read(uint16_t addr, void *priv)
{
    europc_t *dev = (europc_t *)priv;
    uint8_t r = 0xff;

    switch (addr & 0x000f) {
	case 0x00:		/* JIM internal registers (WRONLY) */
	case 0x01:
	case 0x02:
	case 0x03:
		r = 0x00;
		break;

	case 0x04:		/* JIM internal registers (R/W) */
	case 0x05:
	case 0x06:
	case 0x07:
		r = dev->regs[addr & 0x07];
		break;

	case 0x0a:		/* M3002 RTC INDEX/DATA register */
		switch(dev->nvr_stat) {
			case 0:
				r = 0x00;
				break;

			case 1:		/* read data HI nibble */
				r = (dev->nvr.regs[dev->nvr_addr] >> 4);
				dev->nvr_stat++;
				break;

			case 2:		/* read data LO nibble */
				r = (dev->nvr.regs[dev->nvr_addr] & 0x0f);
				dev->nvr_stat = 0;
				break;
		}
		break;

	default:
		ERRLOG("EuroPC: invalid JIM read %02x\n", addr);
		break;
    }

    DBGLOG(2, "EuroPC: jim_rd(%04x): %02x\n", addr, r);

    return(r);
}


/*
 * Initialize the mainboard 'device' of the machine.
 *
 * Its task is to allocate a clean machine data block,
 * and then simply enable the mainboard "device" which
 * allows it to reset (dev init) and configured by the
 * user.
 */
static void *
europc_init(const device_t *info, void *arg)
{
    europc_t *dev;
    uint8_t b;
    int i;

    /* Clear the machine state. */
    dev = (europc_t *)mem_alloc(sizeof(europc_t));
    memset(dev, 0x00, sizeof(europc_t));

    /* Add the machine device. */
    device_add_ex(info, dev);

    /* Get configurable things. */
    i = machine_get_config_int("js9");
    dev->jim = (i == 1) ? 0x0350 : 0x0250;

    /* Set up and initialize the NVR. */
    dev->nvr.size = machine->nvrsz;
    dev->nvr.irq = -1;
    dev->nvr.reset = rtc_reset;
    dev->nvr.start = rtc_start;
    dev->nvr.tick = rtc_ticker;
    nvr_init(&dev->nvr);

    /*
     * This is not quite correct, but it works.
     *
     * The EuroPC has an onboard CGA-class video controller
     * (AGA) which is normally used. We currently do not yet
     * support it. To keep the NVRAM valid, however, we act
     * like we have it configured.
     */
    if (video_card == VID_INTERNAL) {
	INFO("EuroPC: enabling CGA in place of AGA!\n");
	device_add(&cga_device);
    }

    DEBUG("EuroPC: NVR=[ %02x %02x %02x %02x %02x ] %sVALID\n",
	  dev->nvr.regs[MRTC_CONF_A], dev->nvr.regs[MRTC_CONF_B],
	  dev->nvr.regs[MRTC_CONF_C], dev->nvr.regs[MRTC_CONF_D],
	  dev->nvr.regs[MRTC_CONF_E],
	  (dev->nvr.regs[MRTC_CHECK_LO]!=rtc_checksum(dev->nvr.regs))?"IN":"");

    /*
     * Now that we have initialized the NVR (either from file,
     * or by setting it to defaults) we can start overriding it
     * with values set by the user.
     */
    b = (dev->nvr.regs[MRTC_CONF_D] & ~0x17);
    if (video_card != VID_INTERNAL) {
	/*
	 * OK, this is not exactly correct, either.
	 *
	 * If we use an external video card, that will not be
	 * installed until after we boot the mainboard, so we
	 * do not know about it yet. Therefore, we just peek
	 * at the configured video card type, and perform an
	 * "educated guess" as to its type..
	 */
	switch(video_card) {
		case VID_MDA:		/* MDA */
		case VID_HERCULES:	/* Hercules */
			b |= 0x03;	/* external video, mono */
			break;

		case VID_CGA:		/* Color, CGA */
			b |= 0x12;	/* external video, CGA80 */
			break;

		default:		/* all others */
			b |= 0x10;	/* external video, special */
	}
    } else {
    	i = video_type();
    	switch(i) {
		case VID_TYPE_MDA:	/* Monochrome, MDA, Hercules */
			b |= 0x03;	/* external video, mono */
			break;

		case VID_TYPE_CGA:	/* Color, CGA */
			b |= 0x12;	/* external video, CGA80 */
			break;

		case VID_TYPE_SPEC:	/* EGA, VGA etc */
			b |= 0x10;	/* external video, special */
			break;

		default:
			ERRLOG("EuroPC: unknown video type %i !\n", i);
			break;
	}
    }
    dev->nvr.regs[MRTC_CONF_D] = b;

    /* Update the memory size. */
    b = (dev->nvr.regs[MRTC_CONF_C] & 0xf3);
    switch(mem_size) {
	case 256:
		b |= 0x04;
		break;

	case 512:
		b |= 0x08;
		break;

	case 640:
		b |= 0x00;
		break;
    }
    dev->nvr.regs[MRTC_CONF_C] = b;

    /* Update CPU speed. */
    b = (dev->nvr.regs[MRTC_CONF_D] & 0x3f);
    switch(cpu_type) {
	case 0:		/* 8088, 4.77 MHz */
		b |= 0x00;
		break;

	case 1:		/* 8088, 7.15 MHz */
		b |= 0x40;
		break;

	case 2:		/* 8088, 9.56 MHz */
		b |= 0x80;
		break;
    }
    dev->nvr.regs[MRTC_CONF_D] = b;

    /* Set up game port. */
    b = (dev->nvr.regs[MRTC_CONF_C] & 0xfc);
    if (mouse_type == MOUSE_INTERNAL) {
	/* Enable the Logitech Bus Mouse device. */
	device_add(&mouse_logibus_internal_device);

	/* Configure the port for (Bus Mouse Compatible) Mouse. */
	b |= 0x01;
    } else if (game_enabled) {
	b |= 0x02;	/* enable port as joysticks */
    }
    dev->nvr.regs[MRTC_CONF_C] = b;

    /* Set up hard disks. */
    b = dev->nvr.regs[MRTC_CONF_B] & 0x84;
    if (hdc_type != HDC_NONE)
	b |= 0x20;			/* HD20 #1 */

    /* Set up floppy types. */
    if (fdd_get_type(0) != 0) {
	/* We have floppy A: */
	if (fdd_is_dd(0)) {
		if (fdd_is_525(0))
			b |= 0x01;	/* 5.25" DD */
		  else
			b |= 0x02;	/* 3.5" DD */
	} else
		ERRLOG("EuroPC: unsupported HD type for floppy drive 0\n");
    }
    if (fdd_get_type(1) != 0) {
	/* We have floppy B: */
	if (fdd_is_dd(1)) {
		b |= 0x04;		/* EXTERNAL */
		if (fdd_is_525(1))
			b |= 0x08;	/* 5.25" DD */
		  else
			b |= 0x10;	/* 3.5" DD */
	} else
		ERRLOG("EuroPC: unsupported HD type for floppy drive 1\n");
    }
    dev->nvr.regs[MRTC_CONF_B] = b;

    /* Validate the NVR checksum and save. */
    dev->nvr.regs[MRTC_CHECK_LO] = rtc_checksum(dev->nvr.regs);
    nvr_dosave++;

    machine_common_init();

    nmi_init();

    pit_set_out_func(&pit, 1, pit_refresh_timer_xt);

    /*
     * Allocate the system's I/O handlers.
     *
     * The first one is for the JIM. Note that although JIM usually
     * resides at 0x0250, a special solder jumper on the mainboard
     * (JS9) can be used to "move" it to 0x0350, to get it out of
     * the way of other cards that need this range.
     */
    io_sethandler(dev->jim, 16,
		  jim_read,NULL,NULL, jim_write,NULL,NULL, dev);

    /* Only after JIM has been initialized. */
    device_add(&keyboard_xt_device);

    /* Enable and set up the FDC. */
    device_add(&fdc_xt_device);

    /*
     * Set up and enable the HD20 disk controller.
     *
     * We only do this if we have not configured another one.
     */
    if (hdc_type == HDC_INTERNAL)
	(void)device_add(&xta_hd20_device);

    return(dev);
}


static void
europc_close(void *priv)
{
    europc_t *dev = (europc_t *)priv;
    nvr_t *nvr = &dev->nvr;

    if (nvr->fn != NULL) {
	free((wchar_t *)nvr->fn);
	nvr->fn = NULL;
    }

    free(dev);
}


static const device_config_t europc_config[] = {
    {
	"js9", "JS9 Jumper (JIM)", CONFIG_INT, "", 0,
	{
		{
			"Disabled (250h)", 0
		},
		{
			"Enabled (350h)", 1
		},
		{
			NULL
		}
	},
    },
    {
	NULL
    }
};


static const CPU cpus_europc[] = {
    { "8088/4.77", CPU_8088, 4772728, 1, 0, 0, 0, 0, CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8088/7.16", CPU_8088, 7159092, 1, 0, 0, 0, 0, CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8088/9.54", CPU_8088, 9545456, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1 },
    { NULL }
};

static const machine_t europc_info = {
    MACHINE_ISA | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE,
    0,
    512, 640, 128, 16, 0,
    {{"Siemens",cpus_europc}}
};

const device_t m_europc = {
    "Schneider EuroPC",
    DEVICE_ROOT,
    0,
    L"schneider/europc",
    europc_init, europc_close, NULL,
    NULL, NULL, NULL,
    &europc_info,
    europc_config
};
