/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement a more-or-less defacto-standard RTC/NVRAM.
 *
 *		When IBM released the PC/AT machine, it came standard with a
 *		battery-backed RTC chip to keep the time of day, something
 *		that was optional on standard PC's with a myriad variants
 *		being put on the market, often on cheap multi-I/O cards.
 *
 *		The PC/AT had an on-board DS12885-series chip ("the black
 *		block") which was an RTC/clock chip with onboard oscillator
 *		and a backup battery (hence the big size.) The chip also had
 *		a small amount of RAM bytes available to the user, which was
 *		used by IBM's ROM BIOS to store machine configuration data.
 *		Later versions and clones used the 12886 and/or 1288(C)7
 *		series, or the MC146818 series, all with an external battery.
 *		Many of those batteries would create corrosion issues later
 *		on in mainboard life...
 *
 *		Since then, pretty much any PC has an implementation of that
 *		device, which became known as the "nvr" or "cmos".
 *
 * NOTES	Info extracted from the data sheets:
 *
 *		* The century register at location 32H is a BCD register
 *		  designed to automatically load the BCD value 20 as the
 *		  year register changes from 99 to 00.  The MSB of this
 *		  register is not affected when the load of 20 occurs,
 *		  and remains at the value written by the user.
 *
 *		* Rate Selector (RS3:RS0)
 *		  These four rate-selection bits select one of the 13
 *		  taps on the 15-stage divider or disable the divider
 *		  output.  The tap selected can be used to generate an
 *		  output square wave (SQW pin) and/or a periodic interrupt.
 *
 *		  The user can do one of the following:
 *		   - enable the interrupt with the PIE bit;
 *		   - enable the SQW output pin with the SQWE bit;
 *		   - enable both at the same time and the same rate; or
 *		   - enable neither.
 *
 *		  Table 3 lists the periodic interrupt rates and the square
 *		  wave frequencies that can be chosen with the RS bits.
 *		  These four read/write bits are not affected by !RESET.
 *
 *		* Oscillator (DV2:DV0)
 *		  These three bits are used to turn the oscillator on or
 *		  off and to reset the countdown chain.  A pattern of 010
 *		  is the only combination of bits that turn the oscillator
 *		  on and allow the RTC to keep time.  A pattern of 11x
 *		  enables the oscillator but holds the countdown chain in
 *		  reset.  The next update occurs at 500ms after a pattern
 *		  of 010 is written to DV0, DV1, and DV2.
 *
 *		* Update-In-Progress (UIP)
 *		  This bit is a status flag that can be monitored. When the
 *		  UIP bit is a 1, the update transfer occurs soon.  When
 *		  UIP is a 0, the update transfer does not occur for at
 *		  least 244us.  The time, calendar, and alarm information
 *		  in RAM is fully available for access when the UIP bit
 *		  is 0.  The UIP bit is read-only and is not affected by
 *		  !RESET.  Writing the SET bit in Register B to a 1
 *		  inhibits any update transfer and clears the UIP status bit.
 *
 *		* Daylight Saving Enable (DSE)
 *		  This bit is a read/write bit that enables two daylight
 *		  saving adjustments when DSE is set to 1.  On the first
 *		  Sunday in April (or the last Sunday in April in the
 *		  MC146818A), the time increments from 1:59:59 AM to
 *		  3:00:00 AM.  On the last Sunday in October when the time
 *		  first reaches 1:59:59 AM, it changes to 1:00:00 AM.
 *
 *		  When DSE is enabled, the internal logic test for the
 *		  first/last Sunday condition at midnight.  If the DSE bit
 *		  is not set when the test occurs, the daylight saving
 *		  function does not operate correctly.  These adjustments
 *		  do not occur when the DSE bit is 0. This bit is not
 *		  affected by internal functions or !RESET.
 *
 *		* 24/12
 *		  The 24/12 control bit establishes the format of the hours
 *		  byte. A 1 indicates the 24-hour mode and a 0 indicates
 *		  the 12-hour mode.  This bit is read/write and is not
 *		  affected by internal functions or !RESET.
 *
 *		* Data Mode (DM)
 *		  This bit indicates whether time and calendar information
 *		  is in binary or BCD format.  The DM bit is set by the
 *		  program to the appropriate format and can be read as
 *		  required.  This bit is not modified by internal functions
 *		  or !RESET. A 1 in DM signifies binary data, while a 0 in
 *		  DM specifies BCD data.
 *
 *		* Square-Wave Enable (SQWE)
 *		  When this bit is set to 1, a square-wave signal at the
 *		  frequency set by the rate-selection bits RS3-RS0 is driven
 *		  out on the SQW pin.  When the SQWE bit is set to 0, the
 *		  SQW pin is held low. SQWE is a read/write bit and is
 *		  cleared by !RESET.  SQWE is low if disabled, and is high
 *		  impedance when VCC is below VPF. SQWE is cleared to 0 on
 *		  !RESET.
 *
 *		* Update-Ended Interrupt Enable (UIE)
 *		  This bit is a read/write bit that enables the update-end
 *		  flag (UF) bit in Register C to assert !IRQ.  The !RESET
 *		  pin going low or the SET bit going high clears the UIE bit.
 *		  The internal functions of the device do not affect the UIE
 *		  bit, but is cleared to 0 on !RESET.
 *
 *		* Alarm Interrupt Enable (AIE)
 *		  This bit is a read/write bit that, when set to 1, permits
 *		  the alarm flag (AF) bit in Register C to assert !IRQ.  An
 *		  alarm interrupt occurs for each second that the three time
 *		  bytes equal the three alarm bytes, including a don't-care
 *		  alarm code of binary 11XXXXXX.  The AF bit does not
 *		  initiate the !IRQ signal when the AIE bit is set to 0.
 *		  The internal functions of the device do not affect the AIE
 *		  bit, but is cleared to 0 on !RESET.
 *
 *		* Periodic Interrupt Enable (PIE)
 *		  The PIE bit is a read/write bit that allows the periodic
 *		  interrupt flag (PF) bit in Register C to drive the !IRQ pin
 *		  low.  When the PIE bit is set to 1, periodic interrupts are
 *		  generated by driving the !IRQ pin low at a rate specified
 *		  by the RS3-RS0 bits of Register A.  A 0 in the PIE bit
 *		  blocks the !IRQ output from being driven by a periodic
 *		  interrupt, but the PF bit is still set at the periodic
 *		  rate.  PIE is not modified b any internal device functions,
 *		  but is cleared to 0 on !RESET.
 *
 *		* SET
 *		  When the SET bit is 0, the update transfer functions
 *		  normally by advancing the counts once per second.  When
 *		  the SET bit is written to 1, any update transfer is
 *		  inhibited, and the program can initialize the time and
 *		  calendar bytes without an update occurring in the midst of
 *		  initializing. Read cycles can be executed in a similar
 *		  manner. SET is a read/write bit and is not affected by
 *		  !RESET or internal functions of the device.
 *
 *		* Update-Ended Interrupt Flag (UF)
 *		  This bit is set after each update cycle. When the UIE
 *		  bit is set to 1, the 1 in UF causes the IRQF bit to be
 *		  a 1, which asserts the !IRQ pin.  This bit can be
 *		  cleared by reading Register C or with a !RESET. 
 *
 *		* Alarm Interrupt Flag (AF)
 *		  A 1 in the AF bit indicates that the current time has
 *		  matched the alarm time.  If the AIE bit is also 1, the
 *		  !IRQ pin goes low and a 1 appears in the IRQF bit. This
 *		  bit can be cleared by reading Register C or with a
 *		  !RESET.
 *
 *		* Periodic Interrupt Flag (PF)
 *		  This bit is read-only and is set to 1 when an edge is
 *		  detected on the selected tap of the divider chain.  The
 *		  RS3 through RS0 bits establish the periodic rate. PF is
 *		  set to 1 independent of the state of the PIE bit.  When
 *		  both PF and PIE are 1s, the !IRQ signal is active and
 *		  sets the IRQF bit. This bit can be cleared by reading
 *		  Register C or with a !RESET.
 *
 *		* Interrupt Request Flag (IRQF)
 *		  The interrupt request flag (IRQF) is set to a 1 when one
 *		  or more of the following are true:
 *		   - PF == PIE == 1
 *		   - AF == AIE == 1
 *		   - UF == UIE == 1
 *		  Any time the IRQF bit is a 1, the !IRQ pin is driven low.
 *		  All flag bits are cleared after Register C is read by the
 *		  program or when the !RESET pin is low.
 *
 *		* Valid RAM and Time (VRT)
 *		  This bit indicates the condition of the battery connected
 *		  to the VBAT pin. This bit is not writeable and should
 *		  always be 1 when read.  If a 0 is ever present, an
 *		  exhausted internal lithium energy source is indicated and
 *		  both the contents of the RTC data and RAM data are
 *		  questionable.  This bit is unaffected by !RESET.
 *
 *		This file implements a generic version of the RTC/NVRAM chip,
 *		including the later update (DS12887A) which implemented a
 *		"century" register to be compatible with Y2K.
 *
 * Version:	@(#)nvr_at.c	1.0.24	2021/02/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Mahod,
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2020 Miran Grca.
 *		Copyright 2008-2020 Sarah Walker.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <time.h>
#include "../../emu.h"
#include "../../config.h"
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../machines/machine.h"
#include "../../io.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "clk.h"
#include "nmi.h"
#include "pic.h"


/* RTC registers and bit definitions. */
#define RTC_SECONDS	0
#define RTC_ALSECONDS	1
# define AL_DONTCARE	0xc0		// Alarm time is not set
#define RTC_MINUTES	2
#define RTC_ALMINUTES	3
#define RTC_HOURS	4
# define RTC_AMPM	0x80		// PM flag if 12h format in use
#define RTC_ALHOURS	5
#define RTC_DOW		6
#define RTC_DOM		7
#define RTC_MONTH	8
#define RTC_YEAR	9
#define RTC_REGA	10
# define REGA_UIP	0x80
# define REGA_DV2	0x40
# define REGA_DV1	0x20
# define REGA_DV0	0x10
# define REGA_DV	0x70
# define REGA_RS3	0x08
# define REGA_RS2	0x04
# define REGA_RS1	0x02
# define REGA_RS0	0x01
# define REGA_RS	0x0f
#define RTC_REGB	11
# define REGB_SET	0x80
# define REGB_PIE	0x40
# define REGB_AIE	0x20
# define REGB_UIE	0x10
# define REGB_SQWE	0x08
# define REGB_DM	0x04
# define REGB_2412	0x02
# define REGB_DSE	0x01
#define RTC_REGC	12
# define REGC_IRQF	0x80
# define REGC_PF	0x40
# define REGC_AF	0x20
# define REGC_UF	0x10
#define RTC_REGD	13
# define REGD_VRT	0x80
#define RTC_CENTURY_AT	0x32		// century register for AT etc
#define RTC_CENTURY_PS	0x37		// century register for PS/1 PS/2
#define RTC_CENTURY_VIA	0x7f		// century register for VIA VT82C586B
#define RTC_ALDAY	0x7d		// VIA VT82C586B - alarm day
#define RTC_ALMONTH	0x7e		// VIA VT82C586B - alarm month
#define RTC_VALID	14		// "data valid" register (0x00 valid)
# define VALID_DNV	0xff		//  indicate "data not valid"
#define RTC_SUMH	46		// checksum, HIGH byte (some machines)
#define RTC_SUML	47		// checksum, LOW byte (some machines)


typedef struct {
    nvr_t	nvr;

    uint8_t	flags;
#define FLAG_LS_HACK	0x01
#define FLAG_PIIX4	0x02

    uint8_t	cent,
		dflt,
		read_addr;

    uint8_t     stat;

    uint8_t     addr[8], wp[2],
                bank[8], *lock;

    int16_t     count, state;

    tmrval_t	ptimer,			// periodic interval timer
		utimer,			// RTC internal update timer
		ecount;			// countdown to update
} local_t;


/* Get the current NVR time. */
static void
time_get(const local_t *dev, intclk_t *clk)
{
    const nvr_t *nvr = &dev->nvr;
    int8_t temp;

    if (nvr->regs[RTC_REGB] & REGB_DM) {
	/* NVR is in Binary data mode. */
	clk->tm_sec = nvr->regs[RTC_SECONDS];
	clk->tm_min = nvr->regs[RTC_MINUTES];
	temp = nvr->regs[RTC_HOURS];
	clk->tm_wday = (nvr->regs[RTC_DOW] - 1);
	clk->tm_mday = nvr->regs[RTC_DOM];
	clk->tm_mon = (nvr->regs[RTC_MONTH] - 1);
	clk->tm_year = nvr->regs[RTC_YEAR];
	if (dev->cent != 0xff)
		clk->tm_year += (nvr->regs[dev->cent] * 100) - 1900;
    } else {
	/* NVR is in BCD data mode. */
	clk->tm_sec = RTC_DCB(nvr->regs[RTC_SECONDS]);
	clk->tm_min = RTC_DCB(nvr->regs[RTC_MINUTES]);
	temp = RTC_DCB(nvr->regs[RTC_HOURS]);
	clk->tm_wday = (RTC_DCB(nvr->regs[RTC_DOW]) - 1);
	clk->tm_mday = RTC_DCB(nvr->regs[RTC_DOM]);
	clk->tm_mon = (RTC_DCB(nvr->regs[RTC_MONTH]) - 1);
	clk->tm_year = RTC_DCB(nvr->regs[RTC_YEAR]);
	if (dev->cent != 0xff)
		clk->tm_year += (RTC_DCB(nvr->regs[dev->cent]) * 100) - 1900;
    }

    /* Adjust for 12/24 hour mode. */
    if (nvr->regs[RTC_REGB] & REGB_2412)
	clk->tm_hour = temp;
      else
	clk->tm_hour = ((temp & ~RTC_AMPM)%12) + ((temp&RTC_AMPM) ? 12 : 0);
}


/* Set the current NVR time. */
static void
time_set(local_t *dev, const intclk_t *clk)
{
    nvr_t *nvr = &dev->nvr;
    int year = (clk->tm_year + 1900);

    if (nvr->regs[RTC_REGB] & REGB_DM) {
	/* NVR is in Binary data mode. */
	nvr->regs[RTC_SECONDS] = clk->tm_sec;
	nvr->regs[RTC_MINUTES] = clk->tm_min;
	nvr->regs[RTC_DOW] = (clk->tm_wday + 1);
	nvr->regs[RTC_DOM] = clk->tm_mday;
	nvr->regs[RTC_MONTH] = (clk->tm_mon + 1);
	nvr->regs[RTC_YEAR] = (year % 100);
	if (dev->cent != 0xff)
		nvr->regs[dev->cent] = (year / 100);

	if (nvr->regs[RTC_REGB] & REGB_2412) {
		/* NVR is in 24h mode. */
		nvr->regs[RTC_HOURS] = clk->tm_hour;
	} else {
		/* NVR is in 12h mode. */
		nvr->regs[RTC_HOURS] = (clk->tm_hour % 12) ? (clk->tm_hour%12) : 12;
		if (clk->tm_hour > 11)
			nvr->regs[RTC_HOURS] |= RTC_AMPM;
	}
    } else {
	/* NVR is in BCD data mode. */
	nvr->regs[RTC_SECONDS] = RTC_BCD(clk->tm_sec);
	nvr->regs[RTC_MINUTES] = RTC_BCD(clk->tm_min);
	nvr->regs[RTC_DOW] = RTC_BCD(clk->tm_wday + 1);
	nvr->regs[RTC_DOM] = RTC_BCD(clk->tm_mday);
	nvr->regs[RTC_MONTH] = RTC_BCD(clk->tm_mon + 1);
	nvr->regs[RTC_YEAR] = RTC_BCD(year % 100);
	if (dev->cent != 0xff)
		nvr->regs[dev->cent] = RTC_BCD(year / 100);

	if (nvr->regs[RTC_REGB] & REGB_2412) {
		/* NVR is in 24h mode. */
		nvr->regs[RTC_HOURS] = RTC_BCD(clk->tm_hour);
	} else {
		/* NVR is in 12h mode. */
		nvr->regs[RTC_HOURS] = (clk->tm_hour % 12)
					? RTC_BCD(clk->tm_hour % 12)
					: RTC_BCD(12);
		if (clk->tm_hour > 11)
			nvr->regs[RTC_HOURS] |= RTC_AMPM;
	}
    }
}


/* Check if the current time matches a set alarm time. */
static int8_t
check_alarm(const local_t *dev, int8_t addr)
{
    const nvr_t *nvr = &dev->nvr;

    return((nvr->regs[addr + 1] == nvr->regs[addr]) ||
	   ((nvr->regs[addr + 1] & AL_DONTCARE) == AL_DONTCARE));
}


/* Check for VIA stuff. */
static int8_t
check_alarm_via(const local_t *dev, int8_t addr, int8_t addr2)
{
    const nvr_t *nvr = &dev->nvr;

    if (dev->cent == RTC_CENTURY_VIA) {
	return((nvr->regs[addr2] == nvr->regs[addr]) ||
	       ((nvr->regs[addr2] & AL_DONTCARE) == AL_DONTCARE));
    }

    return(0);
}


/* Update the RTC registers from the internal clock. */
static void
update_timer(priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;
    intclk_t clk;

    /* Disable the update timer. */
    dev->utimer = dev->ecount = 0;

    /* If the SET bit is set, do not auto-update. */
    if (nvr->regs[RTC_REGB] & REGB_SET)
	return;

    /* Get the current time from the internal clock. */
    nvr_time_get(nvr, &clk);

    /* Update registers with current time. */
    time_set(dev, &clk);

    /* Clear update status. */
    dev->stat = 0x00;

    /* Check for any alarms we need to handle. */
    if (check_alarm(dev, RTC_SECONDS) &&
	check_alarm(dev, RTC_MINUTES) &&
	check_alarm(dev, RTC_HOURS) &&
	check_alarm_via(dev, RTC_DOM, RTC_ALDAY) &&
	check_alarm_via(dev, RTC_MONTH, RTC_ALMONTH)) {
	/* Set the ALARM flag. */
	nvr->regs[RTC_REGC] |= REGC_AF;

	/* Generate an interrupt. */
	if (nvr->regs[RTC_REGB] & REGB_AIE) {
		/* Indicate the alarm is active. */
		nvr->regs[RTC_REGC] |= REGC_IRQF;

		/* Generate an interrupt. */
		if (nvr->irq != -1)
			picint(1 << nvr->irq);
	}
    }

    /*
     * The flag and interrupt should be issued
     * on update ended, not started.
     */
    nvr->regs[RTC_REGC] |= REGC_UF;
    if (nvr->regs[RTC_REGB] & REGB_UIE) {
	/* Indicate the update is done. */
	nvr->regs[RTC_REGC] |= REGC_IRQF;

	/* Generate an interrupt. */
	if (nvr->irq != -1)
		picint(1 << nvr->irq);
    }
}


static void
period_load(local_t *dev)
{
    nvr_t *nvr = &dev->nvr;
    int c = nvr->regs[RTC_REGA] & REGA_RS;

    /* As per the datasheet, only a 010 pattern is valid. */
    if ((nvr->regs[RTC_REGA] & REGA_DV) != REGA_DV1) {
	dev->state = 0;
	return;
    }
    dev->state = 1;

    switch (c) {
	case 0:
		dev->state = 0;
		break;

	case 1:
	case 2:
		dev->count = 1 << (c + 6);
		break;

	default:
		dev->count = 1 << (c - 1);
		break;
    }
}


/*
 * Handle the periodic timer.
 *
 * This timer drives the SQW OUT pin (which we do not have)
 * as well as the periodic interrupt. The rate is selected
 * through the RS3:RS0 bits in REGA, with the DV bits set
 * to enable the oscillator.
 */
static void
period_timer(priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;

    if (dev->state == 1) {
	if (--dev->count == 0) {
		/* Set the Periodic Interrupt flag. */
		nvr->regs[RTC_REGC] |= REGC_PF;

		if (nvr->regs[RTC_REGB] & REGB_PIE) {
			/* Indicate the periodic interrupt. */
			nvr->regs[RTC_REGC] |= REGC_IRQF;

			/* Generate an interrupt. */
			if (nvr->irq != -1)
				picint(1 << nvr->irq);
		}

		/* Reload the period counter. */
		period_load(dev);
	}
    }

    /* Reset timer for the next period. */
    dev->ptimer = (tmrval_t)(RTCCONST * (1LL << TIMER_SHIFT));
}


/* Callback from internal clock, another second passed. */
static void
timer_tick(nvr_t *nvr)
{
    local_t *dev = (local_t *)nvr->data;

    /* Only update if there is no SET in progress. */
    if (nvr->regs[RTC_REGB] & REGB_SET) return;

    /* Set the UIP bit, announcing the update. */
    dev->stat = REGA_UIP;

    /* Schedule the actual update. */
    dev->ecount = (TIMER_USEC * 244.0);
    dev->utimer = dev->ecount;
}


/* Read from one of the NVR registers. */
static uint8_t
nvr_read(uint16_t port, priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;
    uint8_t addr_id = (port & 0x0e) >> 1;
    uint16_t sum;
    uint8_t ret = 0xff;

    cycles -= ISA_CYCLES(8);

    if ((port & 1) && (dev->bank[addr_id] == 0xff))
	return(ret);

    if (port & 1) switch(dev->addr[addr_id]) {
	case RTC_REGA:
		ret = (nvr->regs[RTC_REGA] & 0x7f) | dev->stat;
		break;

	case RTC_REGC:
		picintc(1 << nvr->irq);
		ret = nvr->regs[RTC_REGC];
		nvr->regs[RTC_REGC] = 0x00;
		break;

	case RTC_REGD:
		nvr->regs[RTC_REGD] |= REGD_VRT;
		ret = nvr->regs[RTC_REGD];
		break;

	case 0x2c:		//FIXME: ??
		ret = nvr->regs[dev->addr[addr_id]];
		if (dev->flags & FLAG_LS_HACK)
			ret &= 0x7f;
		break;

	case RTC_SUMH:
	case RTC_SUML:
		if (dev->flags & FLAG_LS_HACK) {
			sum = (nvr->regs[RTC_SUMH] << 8) | nvr->regs[RTC_SUML];
			if (nvr->regs[0x2c] & 0x80)
				sum -= 0x80;
			if (dev->addr[addr_id] == RTC_SUMH)
				ret = sum >> 8;
			else
				ret = sum & 0xff;
		} else
			ret = nvr->regs[dev->addr[addr_id]];
		break;

	default:
		ret = nvr->regs[dev->addr[addr_id]];
		break;
    } else {
	ret = dev->addr[addr_id];
	if (! dev->read_addr)
		ret &= 0x80;
    }

    return(ret);
}


/* Secondary NVR read - used by SMC. */
static uint8_t
nvr_sec_read(uint16_t port, priv_t priv)
{
    return(nvr_read(0x0072 + (port & 1), priv));
}


/* This must be exposed because ACPI uses it. */
void
nvr_reg_write(uint16_t reg, uint8_t val, priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;
    struct tm tm;
    uint16_t sum;
    uint8_t old, i;

    old = nvr->regs[reg];

    switch(reg) {
	case RTC_REGA:
		nvr->regs[RTC_REGA] = val;
		period_load(dev);
		break;

	case RTC_REGB:
		nvr->regs[RTC_REGB] = val;
		if (((old ^ val) & REGB_SET) && (val & REGB_SET)) {
			/* According to the datasheet... */
			nvr->regs[RTC_REGA] &= ~REGA_UIP;
			nvr->regs[RTC_REGB] &= ~REGB_UIE;
		}
		break;

	case RTC_REGC:          /* R/O */
	case RTC_REGD:          /* R/O */
		break;

        case RTC_SUML:
        case RTC_SUMH:
		if (dev->flags & FLAG_LS_HACK) {
			/*
			 * Registers 2EH and 2FH are a simple
			 * sum of the values of 0EH to 2DH.
			 */
			sum = 0x0000;
			for (i = RTC_VALID; i < RTC_SUMH; i++)
				sum += (uint16_t)nvr->regs[i];
			nvr->regs[RTC_SUMH] = sum >> 8;
			nvr->regs[RTC_SUML] = sum & 0xff;
			break;
		}
		/*FALLTHROUGH*/

	default:		/* non-RTC registers are just NVRAM */
		if (dev->wp[0] && (reg >= 0x38) && (reg <= 0x3f))
			break;
		if (dev->wp[1] && (reg >= 0xb8) && (reg <= 0xbf))
			break;
		if (dev->lock[reg])
			break;
		if (nvr->regs[reg] != val) {
			nvr->regs[reg] = val;
			nvr_dosave = 1;
		}
		break;
    }

    if ((old != val) && (config.time_sync != TIME_SYNC_ENABLED)) {
	if ((reg < RTC_REGA) ||
	    ((dev->cent != 0xff) && (reg == dev->cent))) {
		/* Update internal clock. */
		time_get(dev, &tm);
		nvr_time_set(&tm, nvr);
		nvr_dosave = 1;
	}
    }
}


/* Write to one of the NVR registers. */
static void
nvr_write(uint16_t port, uint8_t val, priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;
    uint8_t addr_id = (port & 0x0e) >> 1;
    int f;

    cycles -= ISA_CYCLES(8);

    if (port & 1) {
	if (dev->bank[addr_id] == 0xff)
		return;

	nvr_reg_write(dev->addr[addr_id], val, priv);

	return;
    }

    /*
     * Some chipsets use a 256byte NVRAM emulation, but
     * but ports 70H and 71H always access 128 bytes.
     */
    dev->addr[addr_id] = (val & (nvr->size - 1));

    if (addr_id == 0x00)
	dev->addr[addr_id] &= 0x7f;
    else if ((addr_id == 0x01) && (dev->flags & FLAG_PIIX4))
	dev->addr[addr_id] = (dev->addr[addr_id] & 0x7f) | 0x80;
    else if (dev->bank[addr_id] > 0)
	dev->addr[addr_id] = (dev->addr[addr_id] & 0x7f) |
			     (0x80 * dev->bank[addr_id]);

    f = machine_get_flags();
    if (!(f & MACHINE_MCA) && !(f & MACHINE_NONMI))
	nmi_mask = (~val & 0x80);
}


/* Secondary NVR write - used by SMC. */
static void
nvr_sec_write(uint16_t port, uint8_t val, priv_t priv)
{
    nvr_write(0x0072 + (port & 1), val, priv);
}


/* Reset the RTC state to 1980/01/01 00:00. */
static void
nvr_reset(nvr_t *nvr)
{
    local_t *dev = (local_t *)nvr->data;

    memset(nvr->regs, dev->dflt, nvr->size);
    nvr->regs[RTC_DOM] = 1;
    nvr->regs[RTC_MONTH] = 1;
    nvr->regs[RTC_YEAR] = RTC_BCD(80);
    if (dev->cent != 0xff)
	nvr->regs[dev->cent] = RTC_BCD(19);
}


/* Process after loading from file. */
static void
nvr_start(nvr_t *nvr)
{
    local_t *dev = (local_t *)nvr->data;
    int i, found = 0;
    intclk_t clk;

    /* Check if we are dealing with a blank NVR. */
    for (i = 0; i < nvr->size; i++) {
	if (nvr->regs[i] == dev->dflt)
		found++;
    }
    if (found == i)
        nvr->regs[RTC_VALID] = VALID_DNV;

    /* Initialize the internal and chip times. */
    if (config.time_sync == TIME_SYNC_DISABLED) {
	/* TimeSync disabled, set internal clock from the chip time. */
	time_get(dev, &clk);
	nvr_time_set(&clk, nvr);
    } else {
	/* TimeSync enabled, set chip time to internal clock. */
	nvr_time_get(nvr, &clk);
	time_set(dev, &clk);
    }

    /* Start the RTC. */
    nvr->regs[RTC_REGA] = (REGA_RS2|REGA_RS1);
    nvr->regs[RTC_REGB] = REGB_2412;
}


static void
nvr_at_recalc(priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;

    /* Update the periodic timer. */
    dev->ptimer = (tmrval_t)(RTCCONST * (1LL << TIMER_SHIFT));

    /* Update the update timer, if it is currently running. */
    dev->utimer = 0;
    if (dev->ecount > 0)
	dev->utimer = dev->ecount;

//FIXME: should do in nvr.c!
    nvr->onesec_time = 0;
    nvr->onesec_time = (10000LL * TIMER_USEC);
}


static void
nvr_at_close(priv_t priv)
{
    local_t *dev = (local_t *)priv;
    nvr_t *nvr = &dev->nvr;

    dev->ptimer = 0;
    dev->utimer = 0;

    free(dev->lock);

    nvr_close(nvr);

}


static priv_t
nvr_at_init(const device_t *info, UNUSED(void *parent))
{
    local_t *dev;
    nvr_t *nvr;
    int size;

    /* This is machine specific. */
    size = machine_get_nvrsize();

    /* Allocate an NVR for this machine. */
    dev = (local_t *)mem_alloc(sizeof(local_t));
    memset(dev, 0x00, sizeof(local_t));
    dev->lock = (uint8_t *)mem_alloc(size);
    memset(dev->lock, 0x00, size);
    dev->dflt = 0x00;
    dev->flags = 0x00;

    nvr = &dev->nvr;
    nvr->size = size;
    nvr->data = dev;

    switch(info->local & 7) {
	case 0:		/* standard IBM PC/AT (no century register) */
		dev->cent = 0xff;
		nvr->irq = 8;
		break;

        case 5:		/* Lucky Star LS-486E */
		dev->flags |= FLAG_LS_HACK;
		/*FALLTHROUGH*/

	case 1:		/* standard AT or compatible */
		dev->cent = RTC_CENTURY_AT;
		if (info->local & 0x08)
			dev->flags |= FLAG_PIIX4;
		nvr->irq = 8;
		break;

	case 2:		/* standard IBM PC/AT (FF cleared) */
		dev->cent = RTC_CENTURY_AT;
		dev->dflt = 0xff;
		nvr->irq = 8;
		break;

	case 3:		/* Amstrad PC's */
		dev->cent = RTC_CENTURY_AT;
		dev->dflt = 0xff;
		nvr->irq = 1;
		break;

	case 4:		/* PS/1 or PS/2 */
		dev->cent = RTC_CENTURY_PS;
		dev->dflt = 0xff;
		nvr->irq = 8;
		break;

	case 6:         /* VIA VT82C586B */
		dev->cent = RTC_CENTURY_VIA;
		nvr->irq = 8;
		break;
    }

    dev->read_addr = 1;

    /* Set up any local handlers here. */
    nvr->reset = nvr_reset;
    nvr->start = nvr_start;
    nvr->tick = timer_tick;
    io_sethandler(0x0070, 2,
		  nvr_read,NULL,NULL, nvr_write,NULL,NULL, dev);
    if (info->local & 0x08)
	io_sethandler(0x0072, 2,
		      nvr_read,NULL,NULL, nvr_write,NULL,NULL, dev);

    /* Initialize the generic NVR. */
    nvr_init(nvr);

    /* Start the timers. */
    timer_add(update_timer, (priv_t)dev, &dev->utimer, &dev->utimer);
    period_load(dev);
    timer_add(period_timer, (priv_t)dev, &dev->ptimer, &dev->ptimer);

    return(dev);
}


const device_t at_nvr_old_device = {
    "Old PC/AT NVRAM (no century)",
    DEVICE_ISA | DEVICE_AT,
    0x00,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t at_nvr_device = {
    "PC/AT NVRAM",
    DEVICE_ISA | DEVICE_AT,
    0x01,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t ibmat_nvr_device = {
    "IBM PC/AT NVRAM (ff cleared)",
    DEVICE_ISA | DEVICE_AT,
    0x02,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t amstrad_nvr_device = {
    "Amstrad NVRAM",
    DEVICE_ISA | DEVICE_AT,
    0x03,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t ps_nvr_device = {
    "PS/1 or PS/2 NVRAM",
    DEVICE_PS2,
    0x04,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t piix4_nvr_device = {
    "Intel PIIX4 PC/AT NVRAM",
    DEVICE_ISA | DEVICE_AT,
    0x08 | 0x01,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t ls486e_nvr_device = {
    "Lucky Star LS-486E PC/AT NVRAM",
    DEVICE_ISA | DEVICE_AT,
    0x08 | 0x05,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};

const device_t via_nvr_device = {
    "VIA PC/AT NVRAM",
    DEVICE_ISA | DEVICE_AT,
    0x08 | 0x06,
    NULL,
    nvr_at_init, nvr_at_close, NULL,
    NULL,
    nvr_at_recalc,
    NULL, NULL,
    NULL
};


void
nvr_read_addr_set(priv_t priv, int set)
{
    local_t *dev = (local_t *)priv;

    dev->read_addr = set;
}


void
nvr_wp_set(priv_t priv, int set, int h)
{
    local_t *dev = (local_t *)priv;

    dev->wp[h] = set;
}


void
nvr_bank_set(priv_t priv, int base, uint8_t bank)
{
    local_t *dev = (local_t *)priv;

    dev->bank[base] = bank;
}


void
nvr_lock_set(priv_t priv, int base, int size, int lock)
{
    local_t *dev = (local_t *)priv;
    int i;

    for (i = 0; i < size; i++)
	dev->lock[base + i] = lock;
}


void
nvr_at_handler(priv_t priv, int set, uint16_t base)
{
    local_t *dev = (local_t *)priv;

    if (set)
	io_sethandler(base, 2,
		      nvr_read,NULL,NULL, nvr_write,NULL,NULL, dev);
    else
	io_removehandler(base, 2,
			 nvr_read,NULL,NULL, nvr_write,NULL,NULL, dev);
}


void
nvr_at_sec_handler(priv_t priv, int set, uint16_t base)
{
    local_t *dev = (local_t *)priv;

    if (set)
	io_sethandler(base, 2,
		      nvr_sec_read,NULL,NULL, nvr_sec_write,NULL,NULL, dev);
    else
	io_removehandler(base, 2,
			 nvr_sec_read,NULL,NULL, nvr_sec_write,NULL,NULL, dev);
}
