/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ISA Bus (de)Bugger expansion card
 *		sold as a DIY kit in the late 1980's in The Netherlands.
 *		This card was a assemble-yourself 8bit ISA addon card for
 *		PC and AT systems that had several tools to aid in low-
 *		level debugging (mostly for faulty BIOSes, bootloaders
 *		and system kernels...)
 *
 *		The standard version had a total of 16 LEDs (8 RED, plus
 *		8 GREEN), two 7-segment displays and one 8-position DIP
 *		switch block on board for use as debugging tools.
 *
 *		The "Plus" version, added an extra 2 7-segment displays,
 *		as well as a very simple RS-232 serial interface that
 *		could be used as a mini-console terminal.
 *
 *		Two I/O ports were used; one for control, at offset 0 in
 *		I/O space, and one for data, at offset 1 in I/O space.
 *		Both registers could be read from and written to. Although
 *		the author has a vague memory of a DIP switch to set the
 *		board's I/O address, comments in old software seems to
 *		indicate that it was actually fixed to 0x7A (and 0x7B.)
 *
 *		A READ on the data register always returned the actual
 *		state of the DIP switch. Writing data to the LEDs was done
 *		in two steps.. first, the block number (RED or GREEN) was
 *		written to the CTRL register, and then the actual LED data
 *		was written to the DATA register. Likewise, data for the
 *		7-segment displays was written.
 *
 *		The serial port was a bit different, and its operation is
 *		not verified, but two extra bits in the control register
 *		were used to set up parameters, and also the actual data
 *		input and output.
 *
 * TODO:	Still have to implement the RS232 Serial Port Parameters
 *		configuration register (CTRL_SPCFG bit set) but have to
 *		remember that stuff first...
 *
 * Version:	@(#)bugger.c	1.0.11	2019/05/13
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
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
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "bugger.h"


/* BugBugger registers. */
#define BUG_CTRL	0
# define CTRL_RLED	0x00		/* write to the RED LED block */
# define CTRL_GLED	0x01		/* write to the GREEN LED block */
# define CTRL_SEG1	0x02		/* write to the RIGHT 7SEG displays */
# define CTRL_SEG2	0x04		/* write to the LEFT 7SEG displays */
# define CTRL_SPORT	0x20		/* enable the serial port */
# define CTRL_SPCFG	0x40		/* set up the serial port */
# define CTRL_INIT	0x80		/* enable and reset the card */
# define CTRL_RESET	0xff		/* this resets the board */
#define BUG_DATA	1

#define FIFO_LEN	256
#define UISTR_LEN	24


typedef struct {
    const char	*name;
    uint16_t	base;

    uint8_t	ctrl,			/* control register */
		data,			/* data register */
		ledr,			/* RED and GREEN LEDs */
		ledg,
		seg1,			/* LEFT and RIGHT 7SEG displays */
		seg2,
		spcfg;			/* serial port configuration */
     uint8_t	*bptr,			/* serial port data buffer */
		buff[FIFO_LEN];
} bugger_t;


/* Update the system's UI with the actual Bugger status. */
static void
bug_update(bugger_t *dev)
{
    char temp[UISTR_LEN];

    /* Format all current info in a string. */
    sprintf(temp, "%02X:%02X %c%c%c%c%c%c%c%c-%c%c%c%c%c%c%c%c",
		dev->seg2, dev->seg1,
		(dev->ledg&0x80)?'G':'g', (dev->ledg&0x40)?'G':'g',
		(dev->ledg&0x20)?'G':'g', (dev->ledg&0x10)?'G':'g',
		(dev->ledg&0x08)?'G':'g', (dev->ledg&0x04)?'G':'g',
		(dev->ledg&0x02)?'G':'g', (dev->ledg&0x01)?'G':'g',
		(dev->ledr&0x80)?'R':'r', (dev->ledr&0x40)?'R':'r',
		(dev->ledr&0x20)?'R':'r', (dev->ledr&0x10)?'R':'r',
		(dev->ledr&0x08)?'R':'r', (dev->ledr&0x04)?'R':'r',
		(dev->ledr&0x02)?'R':'r', (dev->ledr&0x01)?'R':'r');

    /* Send formatted string to the UI. */
    ui_sb_text_set(SB_TEXT|1, temp);
}


/* Flush the serial port. */
static void
bug_spflsh(bugger_t *dev)
{
    *dev->bptr = '\0';
    INFO("BUGGER: serial port [%s]\n", dev->buff);
    dev->bptr = dev->buff;
}


/* Handle a write to the Serial Port Data register. */
static void
bug_wsport(bugger_t *dev, uint8_t val)
{
    uint8_t old = dev->ctrl;

    DEBUG("BUGGER: sport %02x\n", val);

    /* Clear the SPORT bit to indicate we are busy. */
    dev->ctrl &= ~CTRL_SPORT;

    /* Delay while processing byte.. */
    if (dev->bptr == &dev->buff[FIFO_LEN-1]) {
	/* Buffer full, gotta flush. */
	bug_spflsh(dev);
    }

    /* Write (store) the byte. */
    *dev->bptr++ = val;

    /* Restore the SPORT bit. */
    dev->ctrl |= (old & CTRL_SPORT);
}


/* Handle a write to the Serial Port Configuration register. */
static void
bug_wspcfg(bugger_t *dev, uint8_t val)
{
    dev->spcfg = val;

    DEBUG("BUGGER: spcfg %02x\n", dev->spcfg);
}


/* Handle a write to the control register. */
static void
bug_wctrl(bugger_t *dev, uint8_t val)
{
    if (val == CTRL_RESET) {
	/* User wants us to reset. */
	dev->ctrl = CTRL_INIT;
	dev->spcfg = 0x00;
	dev->bptr = NULL;
    } else {
	/* If turning off the serial port, flush it. */
	if ((dev->ctrl & CTRL_SPORT) && !(val & CTRL_SPORT))
		bug_spflsh(dev);

	/* FIXME: did they do this using an XOR of operation bits?  --FvK */

	if (val & CTRL_SPCFG) {
		/* User wants to configure the serial port. */
		dev->ctrl &= ~(CTRL_SPORT|CTRL_SEG2|CTRL_SEG1|CTRL_GLED);
		dev->ctrl |= CTRL_SPCFG;
	} else if (val & CTRL_SPORT) {
		/* User wants to talk to the serial port. */
		dev->ctrl &= ~(CTRL_SPCFG|CTRL_SEG2|CTRL_SEG1|CTRL_GLED);
		dev->ctrl |= CTRL_SPORT;
		if (dev->bptr == NULL)
			dev->bptr = dev->buff;
	} else if (val & CTRL_SEG2) {
		/* User selected SEG2 (LEFT, Plus only) for output. */
		dev->ctrl &= ~(CTRL_SPCFG|CTRL_SPORT|CTRL_SEG1|CTRL_GLED);
		dev->ctrl |= CTRL_SEG2;
	} else if (val & CTRL_SEG1) {
		/* User selected SEG1 (RIGHT) for output. */
		dev->ctrl &= ~(CTRL_SPCFG|CTRL_SPORT|CTRL_SEG2|CTRL_GLED);
		dev->ctrl |= CTRL_SEG1;
	} else if (val & CTRL_GLED) {
		/* User selected the GREEN LEDs for output. */
		dev->ctrl &= ~(CTRL_SPCFG|CTRL_SPORT|CTRL_SEG2|CTRL_SEG1);
		dev->ctrl |= CTRL_GLED;
	} else {
		/* User selected the RED LEDs for output. */
		dev->ctrl &=
		    ~(CTRL_SPCFG|CTRL_SPORT|CTRL_SEG2|CTRL_SEG1|CTRL_GLED);
	}
    }

    /* Update the UI with active settings. */
    DBGLOG(1, "BUGGER: ctrl %02x\n", dev->ctrl);

    bug_update(dev);
}


/* Handle a write to the data register. */
static void
bug_wdata(bugger_t *dev, uint8_t val)
{
    dev->data = val;

    if (dev->ctrl & CTRL_SPCFG)
	bug_wspcfg(dev, val);
      else if (dev->ctrl & CTRL_SPORT)
	bug_wsport(dev, val);
      else {
	if (dev->ctrl & CTRL_SEG2)
		dev->seg2 = val;
	  else if (dev->ctrl & CTRL_SEG1)
		dev->seg1 = val;
	  else if (dev->ctrl & CTRL_GLED)
		dev->ledg = val;
	  else
		dev->ledr = val;

	DBGLOG(1, "BUGGER: data %02x\n", dev->data);
    }

    /* Update the UI with active settings. */
    bug_update(dev);
}


/* Reset the ISA BusBugger controller. */
static void
bug_reset(bugger_t *dev)
{
    /* Clear the data register. */
    dev->data = 0x00;

    /* Clear the RED and GREEN LEDs. */
    dev->ledr = 0x00; dev->ledg = 0x00;

    /* Clear both 7SEG displays. */
    dev->seg1 = 0x00; dev->seg2 = 0x00;
 
    /* Reset the control register (updates UI.) */
    bug_wctrl(dev, CTRL_RESET);
}


/* Handle a WRITE operation to one of our registers. */
static void
bug_write(uint16_t port, uint8_t val, priv_t priv)
{
    bugger_t *dev = (bugger_t *)priv;

    switch (port - dev->base) {
	case BUG_CTRL:		/* control register */
		if (val == CTRL_RESET) {
			/* Perform a full reset. */
			bug_reset(dev);
		} else if (dev->ctrl & CTRL_INIT) {
			/* Only allow writes if initialized. */
			bug_wctrl(dev, val);
		}
		break;

	case BUG_DATA:		/* data register */
		if (dev->ctrl & CTRL_INIT) {
			bug_wdata(dev, val);
		}
		break;

    }
}


/* Handle a READ operation from one of our registers. */
static uint8_t
bug_read(uint16_t port, priv_t priv)
{
    bugger_t *dev = (bugger_t *)priv;
    uint8_t ret = 0xff;

    if (dev->ctrl & CTRL_INIT) switch (port - dev->base) {
	case BUG_CTRL:		/* control register */
		ret = dev->ctrl;
		break;

	case BUG_DATA:		/* data register */
		if (dev->ctrl & CTRL_SPCFG) {
			ret = dev->spcfg;
		} else if (dev->ctrl & CTRL_SPORT) {
			ret = 0x00;		/* input not supported */
		} else {
			/* Just read the DIP switch. */
			ret = dev->data;
		}
		break;

	default:
		break;
    }

    return(ret);
}


/* Remove the ISA BusBugger emulator from the system. */
static void
bug_close(priv_t priv)
{
    bugger_t *dev = (bugger_t *)priv;

    io_removehandler(dev->base, 2,
		     bug_read,NULL,NULL, bug_write,NULL,NULL, dev);

    free(dev);
}


/* Initialize the ISA BusBugger emulator. */
static priv_t
bug_init(const device_t *info, UNUSED(void *parent))
{
    bugger_t *dev;

    dev = (bugger_t *)mem_alloc(sizeof(bugger_t));
    memset(dev, 0x00, sizeof(bugger_t));
    dev->name = info->name;
    dev->base = BUGGER_ADDR;

    INFO("BUGGER: %s (I/O=%04x)\n", dev->name, dev->base);

    /* Initialize local registers. */
    bug_reset(dev);

    io_sethandler(dev->base, 2,
		  bug_read,NULL,NULL, bug_write,NULL,NULL, dev);

    return(dev);
}


const device_t bugger_device = {
    "ISA/PCI Bus Bugger",
    DEVICE_ISA,
    0,
    NULL,
    bug_init, bug_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
