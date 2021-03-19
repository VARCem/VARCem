/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the following network controllers:
 *
 *		 - SMC/WD 8003E (ISA 8-bit)
 *		 - SMC/WD 8013EBT (ISA 16-bit)
 *		 - SMC/WD 8013EP/A (MCA)
 *
 *		as well as a number of compatibles.
 *
 * Version:	@(#)net_wd80x3.c	1.0.10	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include <time.h>
#define dbglog network_card_log
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../../misc/random.h"
#include "../system/mca.h"
#include "../system/pci.h"
#include "../system/pic.h"
#include "network.h"
#include "net_ne2000.h"
#include "net_dp8390.h"
#include "bswap.h"


enum {
    WD8003E = 0,			// 8-bit ISA WD8003E
    WD8013EBT,				// 16-bit ISA WD8013EBT
    WD8013EPA				// MCA WD8013EP/A
};


/* Board type codes in card ID */
#define WE_WD8003	0x01
#define WE_WD8003S	0x02
#define WE_WD8003E	0x03
#define WE_WD8013EBT	0x05
#define	WE_TOSHIBA1	0x11		// named PCETA1
#define	WE_TOSHIBA2	0x12		// named PCETA2
#define	WE_TOSHIBA3	0x13		// named PCETB
#define	WE_TOSHIBA4	0x14		// named PCETC
#define	WE_WD8003W	0x24
#define	WE_WD8003EB	0x25
#define	WE_WD8013W	0x26
#define WE_WD8013EP	0x27
#define WE_WD8013WC	0x28
#define WE_WD8013EPC	0x29
#define	WE_SMC8216T	0x2a
#define	WE_SMC8216C	0x2b
#define WE_WD8013EBP	0x2c


typedef struct {
    const char	*name;
    int		board;
    uint32_t	base_address;
    int		base_irq;

    mem_map_t	ram_mapping;

    /* POS registers, MCA boards only */
    uint8_t pos_regs[8];

    dp8390_t	dp8390;

    uint32_t	ram_addr,
		ram_size;

    uint8_t	macaddr[32];		// ASIC ROM'd MAC address, even bytes
    uint8_t	maclocal[6];		// configured MAC (local) address

    /* Memory for WD cards*/
    uint8_t	reg1;
    uint8_t	reg5;
    uint8_t	if_chip;
    uint8_t	board_chip;
} nic_t;


static void
nic_interrupt(nic_t *dev, int set)
{
    if (set)
	picint(1 << dev->base_irq);
    else
	picintc(1 << dev->base_irq);
}


/*
 * Called by the platform-specific code when an Ethernet frame
 * has been received. The destination address is tested to see
 * if it should be accepted, and if the RX ring has enough room,
 * it is copied into it and the receive process is updated.
 */
static void
nic_rx(priv_t priv, uint8_t *buf, int io_len)
{
    static uint8_t bcast_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    nic_t *dev = (nic_t *)priv;
    uint8_t pkthdr[4];
    uint8_t *startptr;
    int pages, avail;
    int idx, nextpage;
    int endbytes;

    //FIXME: move to upper layer
    ui_sb_icon_update(SB_NETWORK, 1);

    if (io_len != 60) {
	DBGLOG(1, "%s: rx_frame with length %d\n", dev->name, io_len);	
    }

    if ((dev->dp8390.CR.stop != 0) || (dev->dp8390.page_start == 0)) return;

    /*
     * Add the pkt header + CRC to the length, and work
     * out how many 256-byte pages the frame would occupy.
     */
    pages = (io_len + sizeof(pkthdr) + sizeof(uint32_t) + 255)/256;
    if (dev->dp8390.curr_page < dev->dp8390.bound_ptr) {
	avail = dev->dp8390.bound_ptr - dev->dp8390.curr_page;
    } else {
	avail = (dev->dp8390.page_stop - dev->dp8390.page_start) -
		(dev->dp8390.curr_page - dev->dp8390.bound_ptr);
    }

    /*
     * Avoid getting into a buffer overflow condition by
     * not attempting to do partial receives. The emulation
     * to handle this condition seems particularly painful.
     */
    if	((avail < pages)
#if DP8390_NEVER_FULL_RING
		 || (avail == pages)
#endif
		) {
	DEBUG("%s: no space\n", dev->name);
	ui_sb_icon_update(SB_NETWORK, 0);
	return;
    }

    if ((io_len < 40/*60*/) && !dev->dp8390.RCR.runts_ok) {
	DEBUG("%s: rejected small packet, length %d\n", dev->name, io_len);
	ui_sb_icon_update(SB_NETWORK, 0);
	return;
    }

    /* Some computers don't care... */
    if (io_len < 60)
	io_len = 60;

    DBGLOG(1, "%s: RX %x:%x:%x:%x:%x:%x > %x:%x:%x:%x:%x:%x len %d\n",
	      dev->name, buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
	      buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], io_len);

    /* Do address filtering if not in promiscuous mode. */
    if (! dev->dp8390.RCR.promisc) {
	/* If this is a broadcast frame.. */
	if (! memcmp(buf, bcast_addr, 6)) {
		/* Broadcast not enabled, we're done. */
		if (! dev->dp8390.RCR.broadcast) {
			DEBUG("%s: RX BC disabled\n", dev->name);
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}
	}

	/* If this is a multicast frame.. */
	else if (buf[0] & 0x01) {
		/* Multicast not enabled, we're done. */
		if (! dev->dp8390.RCR.multicast) {
			DEBUG("%s: RX MC disabled\n", dev->name);
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}

		/* Are we listening to this multicast address? */
		idx = mcast_index(buf);
		if (! (dev->dp8390.mchash[idx>>3] & (1<<(idx&0x7)))) {
			DEBUG("%s: RX MC not listed\n", dev->name);
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}
	}

	/* Unicast, must be for us.. */
	else if (memcmp(buf, dev->dp8390.physaddr, 6)) return;
    } else {
	DBGLOG(1, "%s: RX promiscuous receive\n", dev->name);
    }

    nextpage = dev->dp8390.curr_page + pages;
    if (nextpage >= dev->dp8390.page_stop)
	nextpage -= (dev->dp8390.page_stop - dev->dp8390.page_start);

    /* Set up packet header. */
    pkthdr[0] = 0x01;			/* RXOK - packet is OK */
    pkthdr[1] = nextpage;		/* ptr to next packet */
    pkthdr[2] = (uint8_t) ((io_len + sizeof(pkthdr)) & 0xff);	/* length-low */
    pkthdr[3] = (uint8_t) ((io_len + sizeof(pkthdr)) >> 8);	/* length-hi */
    DBGLOG(1, "%s: RX pkthdr [%02x %02x %02x %02x]\n",
	dev->name, pkthdr[0], pkthdr[1], pkthdr[2], pkthdr[3]);

    /* Copy into buffer, update curpage, and signal interrupt if config'd */
	startptr = dev->dp8390.mem + (dev->dp8390.curr_page * 256);
	memcpy(startptr, pkthdr, sizeof(pkthdr));
    if ((nextpage > dev->dp8390.curr_page) ||
	((dev->dp8390.curr_page + pages) == dev->dp8390.page_stop)) {
	memcpy(startptr+sizeof(pkthdr), buf, io_len);
    } else {
	endbytes = (dev->dp8390.page_stop - dev->dp8390.curr_page) * 256;
	memcpy(startptr+sizeof(pkthdr), buf, endbytes-sizeof(pkthdr));
	startptr = dev->dp8390.mem + (dev->dp8390.page_start * 256);	
	memcpy(startptr, buf+endbytes-sizeof(pkthdr), io_len-endbytes+8);
    }
    dev->dp8390.curr_page = nextpage;

    dev->dp8390.RSR.rx_ok = 1;
    dev->dp8390.RSR.rx_mbit = (buf[0] & 0x01) ? 1 : 0;
    dev->dp8390.ISR.pkt_rx = 1;

    if (dev->dp8390.IMR.rx_inte)
	nic_interrupt(dev, 1);

    ui_sb_icon_update(SB_NETWORK, 0);
}


static void
nic_tx(nic_t *dev, uint32_t val)
{
    dev->dp8390.CR.tx_packet = 0;
    dev->dp8390.TSR.tx_ok = 1;
    dev->dp8390.ISR.pkt_tx = 1;

    /* Generate an interrupt if not masked */
    if (dev->dp8390.IMR.tx_inte)
	nic_interrupt(dev, 1);

    dev->dp8390.tx_timer_active = 0;
}


static uint32_t
chipmem_read(nic_t *dev, uint32_t addr, unsigned int len)
{
    uint32_t retval = 0;

    if ((len == 2) && (addr & 0x1)) {
	DEBUG("%s: unaligned chipmem word read\n", dev->name);
    }

    /* ROM'd MAC address */
    if (dev->board == WD8003E) {
	if (addr <= 15) {
		retval = dev->macaddr[addr % 16];
		if (len == 2)
			retval |= (dev->macaddr[(addr + 1) % 16] << 8);
		return(retval);
	}
    }

    return(0xff);
}


/* Handle reads/writes to the 'zeroth' page of the DS8390 register file. */
static uint8_t
page0_read(nic_t *dev, uint32_t off)
{
    uint8_t retval = 0;

    switch(off) {
	case 0x01:	/* CLDA0 */
		retval = (dev->dp8390.local_dma & 0xff);
		break;

	case 0x02:	/* CLDA1 */
		retval = (dev->dp8390.local_dma >> 8);
		break;

	case 0x03:	/* BNRY */
		retval = dev->dp8390.bound_ptr;
		break;

	case 0x04:	/* TSR */
		retval = ((dev->dp8390.TSR.ow_coll    << 7) |
			  (dev->dp8390.TSR.cd_hbeat   << 6) |
			  (dev->dp8390.TSR.fifo_ur    << 5) |
			  (dev->dp8390.TSR.no_carrier << 4) |
			  (dev->dp8390.TSR.aborted    << 3) |
			  (dev->dp8390.TSR.collided   << 2) |
			  (dev->dp8390.TSR.tx_ok));
		break;

	case 0x05:	/* NCR */
		retval = dev->dp8390.num_coll;
		break;

	case 0x06:	/* FIFO */
		/* reading FIFO is only valid in loopback mode */
		DEBUG("%s: reading FIFO not supported yet\n", dev->name);
		retval = dev->dp8390.fifo;
		break;

	case 0x07:	/* ISR */
		retval = ((dev->dp8390.ISR.reset     << 7) |
			  (dev->dp8390.ISR.rdma_done << 6) |
			  (dev->dp8390.ISR.cnt_oflow << 5) |
			  (dev->dp8390.ISR.overwrite << 4) |
			  (dev->dp8390.ISR.tx_err    << 3) |
			  (dev->dp8390.ISR.rx_err    << 2) |
			  (dev->dp8390.ISR.pkt_tx    << 1) |
			  (dev->dp8390.ISR.pkt_rx));
		break;

	case 0x08:	/* CRDA0 */
		retval = (dev->dp8390.remote_dma & 0xff);
		break;

	case 0x09:	/* CRDA1 */
		retval = (dev->dp8390.remote_dma >> 8);
		break;

	case 0x0a:	/* reserved */
		DEBUG("%s: reserved Page0 read - 0x0a\n", dev->name);
		retval = 0xff;
		break;

	case 0x0b:	/* reserved */
		DEBUG("%s: reserved Page0 read - 0x0b\n", dev->name);
		retval = 0xff;
		break;

	case 0x0c:	/* RSR */
		retval = ((dev->dp8390.RSR.deferred    << 7) |
			  (dev->dp8390.RSR.rx_disabled << 6) |
			  (dev->dp8390.RSR.rx_mbit     << 5) |
			  (dev->dp8390.RSR.rx_missed   << 4) |
			  (dev->dp8390.RSR.fifo_or     << 3) |
			  (dev->dp8390.RSR.bad_falign  << 2) |
			  (dev->dp8390.RSR.bad_crc     << 1) |
			  (dev->dp8390.RSR.rx_ok));
		break;

	case 0x0d:	/* CNTR0 */
		retval = dev->dp8390.tallycnt_0;
		break;

	case 0x0e:	/* CNTR1 */
		retval = dev->dp8390.tallycnt_1;
		break;

	case 0x0f:	/* CNTR2 */
		retval = dev->dp8390.tallycnt_2;
		break;

	default:
		DEBUG("%s: Page0 register 0x%02x out of range\n",
						dev->name, off);
		break;
    }

    DBGLOG(2, "%s: Page0 read from register 0x%02x, value=0x%02x\n",
					dev->name, off, retval);
    return(retval);
}


static void
page0_write(nic_t *dev, uint32_t off, uint8_t val)
{
    uint8_t val2;

    DBGLOG(2, "%s: Page0 write to register 0x%02x, value=0x%02x\n",
						dev->name, off, val);
    switch (off) {
	case 0x01:	/* PSTART */
		dev->dp8390.page_start = val;
		break;

	case 0x02:	/* PSTOP */
		dev->dp8390.page_stop = val;
		break;

	case 0x03:	/* BNRY */
		dev->dp8390.bound_ptr = val;
		break;

	case 0x04:	/* TPSR */
		dev->dp8390.tx_page_start = val;
		break;

	case 0x05:	/* TBCR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.tx_bytes &= 0xff00;
		dev->dp8390.tx_bytes |= (val & 0xff);
		break;

	case 0x06:	/* TBCR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.tx_bytes &= 0x00ff;
		dev->dp8390.tx_bytes |= ((val & 0xff) << 8);
		break;

	case 0x07:	/* ISR */
		val &= 0x7f;  /* clear RST bit - status-only bit */
		/* All other values are cleared iff the ISR bit is 1 */
		dev->dp8390.ISR.pkt_rx    &= !((int)((val & 0x01) == 0x01));
		dev->dp8390.ISR.pkt_tx    &= !((int)((val & 0x02) == 0x02));
		dev->dp8390.ISR.rx_err    &= !((int)((val & 0x04) == 0x04));
		dev->dp8390.ISR.tx_err    &= !((int)((val & 0x08) == 0x08));
		dev->dp8390.ISR.overwrite &= !((int)((val & 0x10) == 0x10));
		dev->dp8390.ISR.cnt_oflow &= !((int)((val & 0x20) == 0x20));
		dev->dp8390.ISR.rdma_done &= !((int)((val & 0x40) == 0x40));
		val = ((dev->dp8390.ISR.rdma_done << 6) |
		       (dev->dp8390.ISR.cnt_oflow << 5) |
		       (dev->dp8390.ISR.overwrite << 4) |
		       (dev->dp8390.ISR.tx_err    << 3) |
		       (dev->dp8390.ISR.rx_err    << 2) |
		       (dev->dp8390.ISR.pkt_tx    << 1) |
		       (dev->dp8390.ISR.pkt_rx));
		val &= ((dev->dp8390.IMR.rdma_inte << 6) |
		        (dev->dp8390.IMR.cofl_inte << 5) |
		        (dev->dp8390.IMR.overw_inte << 4) |
		        (dev->dp8390.IMR.txerr_inte << 3) |
		        (dev->dp8390.IMR.rxerr_inte << 2) |
		        (dev->dp8390.IMR.tx_inte << 1) |
		        (dev->dp8390.IMR.rx_inte));
		if (val == 0x00)
			nic_interrupt(dev, 0);
		break;

	case 0x08:	/* RSAR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.remote_start &= 0xff00;
		dev->dp8390.remote_start |= (val & 0xff);
		dev->dp8390.remote_dma = dev->dp8390.remote_start;
		break;

	case 0x09:	/* RSAR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.remote_start &= 0x00ff;
		dev->dp8390.remote_start |= ((val & 0xff) << 8);
		dev->dp8390.remote_dma = dev->dp8390.remote_start;
		break;

	case 0x0a:	/* RBCR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.remote_bytes &= 0xff00;
		dev->dp8390.remote_bytes |= (val & 0xff);
		break;

	case 0x0b:	/* RBCR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.remote_bytes &= 0x00ff;
		dev->dp8390.remote_bytes |= ((val & 0xff) << 8);
		break;

	case 0x0c:	/* RCR */
		/* Check if the reserved bits are set */
		if (val & 0xc0) {
			DEBUG("%s: RCR write, reserved bits set\n", dev->name);
		}

		/* Set all other bit-fields */
		dev->dp8390.RCR.errors_ok = ((val & 0x01) == 0x01);
		dev->dp8390.RCR.runts_ok  = ((val & 0x02) == 0x02);
		dev->dp8390.RCR.broadcast = ((val & 0x04) == 0x04);
		dev->dp8390.RCR.multicast = ((val & 0x08) == 0x08);
		dev->dp8390.RCR.promisc   = ((val & 0x10) == 0x10);
		dev->dp8390.RCR.monitor   = ((val & 0x20) == 0x20);

		/* Monitor bit is a little suspicious... */
		if (val & 0x20) {
			DEBUG("%s: RCR write, monitor bit set!\n", dev->name);
		}
		break;

	case 0x0d:	/* TCR */
		/* Check reserved bits */
		if (val & 0xe0) {
			DEBUG("%s: TCR write, reserved bits set\n", dev->name);
		}

		/* Test loop mode (not supported) */
		if (val & 0x06) {
			dev->dp8390.TCR.loop_cntl = (val & 0x6) >> 1;
			DEBUG("%s: TCR write, loop mode %d not supported\n",
					dev->name, dev->dp8390.TCR.loop_cntl);
		} else {
			dev->dp8390.TCR.loop_cntl = 0;
		}

		/* Inhibit-CRC not supported. */
		if (val & 0x01) {
			DEBUG("%s: TCR write, inhibit-CRC not supported\n",
								dev->name);
		}

		/* Auto-transmit disable very suspicious */
		if (val & 0x08) {
			DEBUG("%s: TCR write, auto transmit disable not supported\n",
								dev->name);
		}
		
		/* Allow collision-offset to be set, although not used */
		dev->dp8390.TCR.coll_prio = ((val & 0x08) == 0x08);
		break;

	case 0x0e:	/* DCR */
		/* the loopback mode is not suppported yet */
		if (! (val & 0x08)) {
			DEBUG("%s: DCR write, loopback mode selected\n",
							dev->name);
		}

		/* It is questionable to set longaddr and auto_rx, since
		 * they are not supported on the NE2000. Print a warning
		 * and continue. */
		if (val & 0x04) {
			DEBUG("%s: DCR write - LAS set ???\n", dev->name);
		}
		if (val & 0x10) {
			DEBUG("%s: DCR write - AR set ???\n", dev->name);
		}

		/* Set other values. */
		dev->dp8390.DCR.wdsize   = ((val & 0x01) == 0x01);
		dev->dp8390.DCR.endian   = ((val & 0x02) == 0x02);
		dev->dp8390.DCR.longaddr = ((val & 0x04) == 0x04); /* illegal ? */
		dev->dp8390.DCR.loop     = ((val & 0x08) == 0x08);
		dev->dp8390.DCR.auto_rx  = ((val & 0x10) == 0x10); /* also illegal ? */
		dev->dp8390.DCR.fifo_size = (val & 0x50) >> 5;
		break;

	case 0x0f:  /* IMR */
		/* Check for reserved bit */
		if (val & 0x80) {
			DEBUG("%s: IMR write, reserved bit set\n",dev->name);
		}

		/* Set other values */
		dev->dp8390.IMR.rx_inte    = ((val & 0x01) == 0x01);
		dev->dp8390.IMR.tx_inte    = ((val & 0x02) == 0x02);
		dev->dp8390.IMR.rxerr_inte = ((val & 0x04) == 0x04);
		dev->dp8390.IMR.txerr_inte = ((val & 0x08) == 0x08);
		dev->dp8390.IMR.overw_inte = ((val & 0x10) == 0x10);
		dev->dp8390.IMR.cofl_inte  = ((val & 0x20) == 0x20);
		dev->dp8390.IMR.rdma_inte  = ((val & 0x40) == 0x40);
		val2 = ((dev->dp8390.ISR.rdma_done << 6) |
		        (dev->dp8390.ISR.cnt_oflow << 5) |
		        (dev->dp8390.ISR.overwrite << 4) |
		        (dev->dp8390.ISR.tx_err    << 3) |
		        (dev->dp8390.ISR.rx_err    << 2) |
		        (dev->dp8390.ISR.pkt_tx    << 1) |
		        (dev->dp8390.ISR.pkt_rx));
		if (((val & val2) & 0x7f) == 0)
			nic_interrupt(dev, 0);
		  else
			nic_interrupt(dev, 1);
		break;

	default:
		DEBUG("%s: Page0 write, bad register 0x%02x\n",
						dev->name, off);
		break;
    }
}


/* Handle reads/writes to the first page of the DS8390 register file. */
static uint8_t
page1_read(nic_t *dev, uint32_t off)
{
    DBGLOG(2, "%s: Page1 read from register 0x%02x\n", dev->name, off);

    switch(off) {
	case 0x01:	/* PAR0-5 */
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		return(dev->dp8390.physaddr[off - 1]);

	case 0x07:	/* CURR */
		DBGLOG(1, "%s: returning current page: 0x%02x\n",
				dev->name, (dev->dp8390.curr_page));
		return(dev->dp8390.curr_page);

	case 0x08:	/* MAR0-7 */
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		return(dev->dp8390.mchash[off - 8]);

	default:
		DEBUG("%s: Page1 read register 0x%02x out of range\n",
							dev->name, off);
		return(0);
    }
}


static void
page1_write(nic_t *dev, uint32_t off, uint8_t val)
{
    DBGLOG(2, "%s: Page1 write to register 0x%02x, value=0x%04x\n",
					dev->name, off, val);
    switch(off) {
	case 0x01:	/* PAR0-5 */
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		dev->dp8390.physaddr[off - 1] = val;
		if (off == 6) {
			DEBUG("%s: physical address set to %02x:%02x:%02x:%02x:%02x:%02x\n",
			      dev->name,
			      dev->dp8390.physaddr[0], dev->dp8390.physaddr[1],
			      dev->dp8390.physaddr[2], dev->dp8390.physaddr[3],
			      dev->dp8390.physaddr[4], dev->dp8390.physaddr[5]);
		}
		break;

	case 0x07:	/* CURR */
		dev->dp8390.curr_page = val;
		break;

	case 0x08:	/* MAR0-7 */
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		dev->dp8390.mchash[off - 8] = val;
		break;

	default:
		DEBUG("%s: Page1 write register 0x%02x out of range\n",
							dev->name, off);
		break;
    }
}


/* Handle reads/writes to the second page of the DS8390 register file. */
static uint8_t
page2_read(nic_t *dev, uint32_t off)
{
    DBGLOG(2, "%s: Page2 read from register 0x%02x\n", dev->name, off);
  
    switch(off) {
	case 0x01:	/* PSTART */
		return(dev->dp8390.page_start);

	case 0x02:	/* PSTOP */
		return(dev->dp8390.page_stop);

	case 0x03:	/* Remote Next-packet pointer */
		return(dev->dp8390.rempkt_ptr);

	case 0x04:	/* TPSR */
		return(dev->dp8390.tx_page_start);

	case 0x05:	/* Local Next-packet pointer */
		return(dev->dp8390.localpkt_ptr);

	case 0x06:	/* Address counter (upper) */
		return(dev->dp8390.address_cnt >> 8);

	case 0x07:	/* Address counter (lower) */
		return(dev->dp8390.address_cnt & 0xff);

	case 0x08:	/* Reserved */
	case 0x09:
	case 0x0a:
	case 0x0b:
		DEBUG("%s: reserved Page2 read - register 0x%02x\n",
						dev->name, off);
		return(0xff);

	case 0x0c:	/* RCR */
		return	((dev->dp8390.RCR.monitor   << 5) |
			 (dev->dp8390.RCR.promisc   << 4) |
			 (dev->dp8390.RCR.multicast << 3) |
			 (dev->dp8390.RCR.broadcast << 2) |
			 (dev->dp8390.RCR.runts_ok  << 1) |
			 (dev->dp8390.RCR.errors_ok));

	case 0x0d:	/* TCR */
		return	((dev->dp8390.TCR.coll_prio   << 4) |
			 (dev->dp8390.TCR.ext_stoptx  << 3) |
			 ((dev->dp8390.TCR.loop_cntl & 0x3) << 1) |
			 (dev->dp8390.TCR.crc_disable));

	case 0x0e:	/* DCR */
		return	(((dev->dp8390.DCR.fifo_size & 0x3) << 5) |
			 (dev->dp8390.DCR.auto_rx  << 4) |
			 (dev->dp8390.DCR.loop     << 3) |
			 (dev->dp8390.DCR.longaddr << 2) |
			 (dev->dp8390.DCR.endian   << 1) |
			 (dev->dp8390.DCR.wdsize));

	case 0x0f:	/* IMR */
		return	((dev->dp8390.IMR.rdma_inte  << 6) |
			 (dev->dp8390.IMR.cofl_inte  << 5) |
			 (dev->dp8390.IMR.overw_inte << 4) |
			 (dev->dp8390.IMR.txerr_inte << 3) |
			 (dev->dp8390.IMR.rxerr_inte << 2) |
			 (dev->dp8390.IMR.tx_inte    << 1) |
			 (dev->dp8390.IMR.rx_inte));

	default:
		DEBUG("%s: Page2 register 0x%02x out of range\n",
						dev->name, off);
		break;
    }

    return(0);
}


#if 0
static void
page2_write(nic_t *dev, uint32_t off, uint8_t val)
{
/* Maybe all writes here should be BX_PANIC()'d, since they
   affect internal operation, but let them through for now
   and print a warning. */
    DBGLOG(2, "%s: Page2 write to register 0x%02x, value=0x%04x\n",
						dev->name, off, val);
    switch(off) {
	case 0x01:	/* CLDA0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.local_dma &= 0xff00;
		dev->dp8390.local_dma |= (val & 0xff);
		break;

	case 0x02:	/* CLDA1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.local_dma &= 0x00ff;
		dev->dp8390.local_dma |= ((val & 0xff) << 8);
		break;

	case 0x03:	/* Remote Next-pkt pointer */
		dev->dp8390.rempkt_ptr = val;
		break;

	case 0x04:
		DEBUG("page 2 write to reserved register 0x04\n");
		break;

	case 0x05:	/* Local Next-packet pointer */
		dev->dp8390.localpkt_ptr = val;
		break;

	case 0x06:	/* Address counter (upper) */
		/* Clear out high byte and re-insert */
		dev->dp8390.address_cnt &= 0x00ff;
		dev->dp8390.address_cnt |= ((val & 0xff) << 8);
		break;

	case 0x07:	/* Address counter (lower) */
		/* Clear out low byte and re-insert */
		dev->dp8390.address_cnt &= 0xff00;
		dev->dp8390.address_cnt |= (val & 0xff);
		break;

	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		DEBUG(3, "%s: Page2 write to reserved register 0x%02x\n",
							dev->name, off);
		break;

	default:
		DEBUG(3, "%s: Page2 write, illegal register 0x%02x\n",
							dev->name, off);
		break;
    }
}
#endif


/* Routines for handling reads/writes to the Command Register. */
static uint8_t
read_cr(nic_t *dev)
{
    uint32_t retval;

    retval =	(((dev->dp8390.CR.pgsel    & 0x03) << 6) |
		 ((dev->dp8390.CR.rdma_cmd & 0x07) << 3) |
		  (dev->dp8390.CR.tx_packet << 2) |
		  (dev->dp8390.CR.start     << 1) |
		  (dev->dp8390.CR.stop));

    DBGLOG(1, "%s: read CR returns 0x%02x\n", dev->name, retval);

    return(retval);
}


static void
write_cr(nic_t *dev, uint8_t val)
{
    DBGLOG(1, "%s: wrote 0x%02x to CR\n", dev->name, val);

    /* Validate remote-DMA */
    if ((val & 0x38) == 0x00) {
	DEBUG("%s: CR write - invalid rDMA value 0\n", dev->name);
	val |= 0x20; /* dma_cmd == 4 is a safe default */
    }

    /* Check for s/w reset */
    if (val & 0x01) {
	dev->dp8390.ISR.reset = 1;
	dev->dp8390.CR.stop   = 1;
    } else {
	dev->dp8390.CR.stop   = 0;
    }

    dev->dp8390.CR.rdma_cmd = (val & 0x38) >> 3;

    /* If start command issued, the RST bit in the ISR */
    /* must be cleared */
    if ((val & 0x02) && !dev->dp8390.CR.start)
	dev->dp8390.ISR.reset = 0;

    dev->dp8390.CR.start = ((val & 0x02) == 0x02);
    dev->dp8390.CR.pgsel = (val & 0xc0) >> 6;
	
    /* Check for send-packet command */
    if (dev->dp8390.CR.rdma_cmd == 3) {
	/* Set up DMA read from receive ring */
	dev->dp8390.remote_start = dev->dp8390.remote_dma = dev->dp8390.bound_ptr * 256;
	dev->dp8390.remote_bytes = (uint16_t)chipmem_read(dev, dev->dp8390.bound_ptr * 256 + 2, 2);
	DBGLOG(1, "%s: sending buffer #x%x length %d\n",
		dev->name, dev->dp8390.remote_start, dev->dp8390.remote_bytes);
    }

    /* Check for start-tx */
	if ((val & 0x04) && dev->dp8390.TCR.loop_cntl) {
	if (dev->dp8390.TCR.loop_cntl) {
		nic_rx(dev, &dev->dp8390.mem[dev->dp8390.tx_page_start*256 - DP8390_WORD_MEMSTART],
			  dev->dp8390.tx_bytes);
	}
    } else if (val & 0x04) {
	if (dev->dp8390.CR.stop) {
		if (dev->dp8390.tx_bytes == 0) /* njh@bandsman.co.uk */ {
			return; /* Solaris9 probe */
		}
		DEBUG("%s: CR write - tx start, dev in reset\n", dev->name);
	}

	if (dev->dp8390.tx_bytes == 0) {
		DEBUG("%s: CR write - tx start, tx bytes == 0\n", dev->name);
	}

	/* Send the packet to the system driver */
	dev->dp8390.CR.tx_packet = 1;

	network_tx(dev->dp8390.mem, dev->dp8390.tx_bytes);

	nic_tx(dev, val);
    }

    /* Linux probes for an interrupt by setting up a remote-DMA read
     * of 0 bytes with remote-DMA completion interrupts enabled.
     * Detect this here */
    if (dev->dp8390.CR.rdma_cmd == 0x01 && dev->dp8390.CR.start && dev->dp8390.remote_bytes == 0) {
	dev->dp8390.ISR.rdma_done = 1;
	if (dev->dp8390.IMR.rdma_inte) {
		nic_interrupt(dev, 1);
		nic_interrupt(dev, 0);
	}
    }
}


static void
nic_reset(priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    DEBUG("%s: reset\n", dev->name);

    /* Initialize the MAC address area by doubling the physical address */
    dev->macaddr[0]  = dev->dp8390.physaddr[0];
    dev->macaddr[1]  = dev->dp8390.physaddr[1];
    dev->macaddr[2]  = dev->dp8390.physaddr[2];
    dev->macaddr[3]  = dev->dp8390.physaddr[3];
    dev->macaddr[4]  = dev->dp8390.physaddr[4];
    dev->macaddr[5]  = dev->dp8390.physaddr[5];
	
    /* Zero out registers and memory */
    memset(&dev->dp8390.CR,  0x00, sizeof(dev->dp8390.CR) );
    memset(&dev->dp8390.ISR, 0x00, sizeof(dev->dp8390.ISR));
    memset(&dev->dp8390.IMR, 0x00, sizeof(dev->dp8390.IMR));
    memset(&dev->dp8390.DCR, 0x00, sizeof(dev->dp8390.DCR));
    memset(&dev->dp8390.TCR, 0x00, sizeof(dev->dp8390.TCR));
    memset(&dev->dp8390.TSR, 0x00, sizeof(dev->dp8390.TSR));
    memset(&dev->dp8390.RSR, 0x00, sizeof(dev->dp8390.RSR));
    dev->dp8390.tx_timer_active = 0;
    dev->dp8390.local_dma  = 0;
    dev->dp8390.page_start = 0;
    dev->dp8390.page_stop  = 0;
    dev->dp8390.bound_ptr  = 0;
    dev->dp8390.tx_page_start = 0;
    dev->dp8390.num_coll   = 0;
    dev->dp8390.tx_bytes   = 0;
    dev->dp8390.fifo       = 0;
    dev->dp8390.remote_dma = 0;
    dev->dp8390.remote_start = 0;
    dev->dp8390.remote_bytes = 0;
    dev->dp8390.tallycnt_0 = 0;
    dev->dp8390.tallycnt_1 = 0;
    dev->dp8390.tallycnt_2 = 0;
    dev->dp8390.curr_page = 0;
    dev->dp8390.rempkt_ptr   = 0;
    dev->dp8390.localpkt_ptr = 0;
    dev->dp8390.address_cnt  = 0;

    memset(&dev->dp8390.mem, 0x00, sizeof(dev->dp8390.mem));

    /* Set power-up conditions */
    dev->dp8390.CR.stop      = 1;
    dev->dp8390.CR.rdma_cmd  = 4;
    dev->dp8390.ISR.reset    = 1;
    dev->dp8390.DCR.longaddr = 1;
	
    nic_interrupt(dev, 0);
}


static uint32_t
smc_read(nic_t *dev, uint32_t off)
{
    uint32_t sum;
    uint32_t ret = 0;

    switch(off) {
	case 0x00:
		break;

	case 0x01:
		if (dev->board == WD8003E)
			ret = dev->dp8390.physaddr[0];
		if (dev->board == WD8013EPA)
			ret = dev->reg1;
		break;

	case 0x02:
		if (dev->board == WD8003E)
			ret = dev->dp8390.physaddr[1];
		break;

	case 0x03:
		if (dev->board == WD8003E)	
			ret = dev->dp8390.physaddr[2];
		break;

	case 0x04:
		if (dev->board == WD8003E)	
			ret = dev->dp8390.physaddr[3];
		break;

	case 0x05:
		if (dev->board == WD8003E)	
			ret = dev->dp8390.physaddr[4];
		if (dev->board == WD8013EPA)
			ret = dev->reg5;
		break;

	case 0x06:
		if (dev->board == WD8003E)	
			ret = dev->dp8390.physaddr[5];
		break;

	case 0x07:
		if (dev->board == WD8013EPA) {
			if (dev->if_chip != 0x35 && dev->if_chip != 0x3A) {
				ret = 0;
				break;
			}

			ret = dev->if_chip;
		}
		break;

	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
		ret = dev->dp8390.physaddr[off - 8];
		break;

	case 0x0e:
		ret = dev->board_chip;
		break;

	case 0x0f:
		/*This has to return the byte that adds up to 0xFF*/
		sum = (dev->dp8390.physaddr[0] + \
		       dev->dp8390.physaddr[1] + \
		       dev->dp8390.physaddr[2] + \
		       dev->dp8390.physaddr[3] + \
		       dev->dp8390.physaddr[4] + \
		       dev->dp8390.physaddr[5] + \
		       dev->board_chip);

		ret = 0xff - (sum & 0xff);
		break;
    }

    DBGLOG(2, "%s: ASIC read addr=0x%02x, value=0x%04x\n",
	      dev->name, (unsigned)off, (unsigned)ret);

    return(ret);
}


static void
smc_write(nic_t *dev, uint32_t off, uint32_t val)
{
    DBGLOG(2, "%s: ASIC write addr=0x%02x, value=0x%04x\n",
		dev->name, (unsigned)off, (unsigned)val);

    switch(off) {
	case 0x00:	/* WD Control register */
		if (val & 0x80) {
			dev->dp8390.ISR.reset = 1;
			return;
		}

		mem_map_disable(&dev->ram_mapping);

		if (val & 0x40)
			mem_map_enable(&dev->ram_mapping);
		break;

	case 0x01:
		dev->reg1 = val;
		break;

	case 0x04:
		break;

	case 0x05:
		dev->reg5 = val;
		break;

	case 0x06:
		break;

	case 0x07:
		dev->if_chip = val;
		break;

	default:
		/* Invalid, but happens under Win95 device detection. */
		DEBUG("%s: ASIC write invalid address %04x, ignoring\n",
						dev->name, (unsigned)off);
		break;
    }
}


static uint8_t
nic_readb(uint16_t addr, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;
    int off = addr - dev->base_address;
    uint8_t retval = 0;

    DBGLOG(2, "%s: read addr %x\n", dev->name, addr);

    if (off == 0x10)
	retval = read_cr(dev);
    else if (off >= 0x00 && off <= 0x0f)
	retval = smc_read(dev, off);	
    else switch(dev->dp8390.CR.pgsel) {
	case 0x00:
		retval = page0_read(dev, off - 0x10);
		break;

	case 0x01:
		retval = page1_read(dev, off - 0x10);
		break;

	case 0x02:
		retval = page2_read(dev, off - 0x10);
		break;
		
	default:
		DEBUG("%s: unknown value of pgsel in read - %d\n",
				dev->name, dev->dp8390.CR.pgsel);
		break;
    }		

    return(retval);
}


static void
nic_writeb(uint16_t addr, uint8_t val, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;
    int off = addr - dev->base_address;

    DBGLOG(2, "%s: write addr %x, value %x\n", dev->name, addr, val);

    if (off == 0x10)
	write_cr(dev, val);
    else if (off >= 0x00 && off <= 0x0f)
	smc_write(dev, off, val);	
    else switch(dev->dp8390.CR.pgsel) {
	case 0x00:
		page0_write(dev, off - 0x10, val);
		break;

	case 0x01:
		page1_write(dev, off - 0x10, val);
		break;
		
	default:
		DEBUG("%s: unknown value of pgsel in write - %d\n",
				dev->name, dev->dp8390.CR.pgsel);
		break;
    }			
}


static void
nic_ioset(nic_t *dev, uint16_t addr)
{	
    io_sethandler(addr, 0x20,
		  nic_readb,NULL,NULL, nic_writeb,NULL,NULL, dev);
}


static void
nic_ioremove(nic_t *dev, uint16_t addr)
{	
    io_removehandler(addr, 0x20,
		     nic_readb,NULL,NULL, nic_writeb,NULL,NULL, dev);
}


static uint32_t
ram_read(priv_t priv, uint32_t addr, int len)
{
    nic_t *dev = (nic_t *)priv;
    uint32_t ret;

    if ((addr & 0x3fff) >= 0x2000) {
	if (len == 2)
		return 0xffff;
	else if (len == 1)
		return 0xff;
	else
		return 0xffffffff;
    }

    ret = dev->dp8390.mem[addr & 0x1fff];

    if (len == 2 || len == 4)
	ret |= dev->dp8390.mem[(addr+1) & 0x1fff] << 8;

    if (len == 4) {
	ret |= dev->dp8390.mem[(addr+2) & 0x1fff] << 16;
	ret |= dev->dp8390.mem[(addr+3) & 0x1fff] << 24;
    }

    return ret;	
}


static uint8_t
ram_readb(uint32_t addr, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    return ram_read(dev, addr, 1);
}


static uint16_t
ram_readw(uint32_t addr, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    return ram_read(dev, addr, 2);
}


static uint32_t
ram_readl(uint32_t addr, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    return ram_read(dev, addr, 4);
}


static void
ram_write(priv_t priv, uint32_t addr, uint32_t val, int len)
{
    nic_t *dev = (nic_t *)priv;

    if ((addr & 0x3fff) >= 0x2000)
	return;

    dev->dp8390.mem[addr & 0x1fff] = val & 0xff;
    if (len == 2 || len == 4)
	dev->dp8390.mem[(addr+1) & 0x1fff] = val >> 8;
    if (len == 4) {
	dev->dp8390.mem[(addr+2) & 0x1fff] = val >> 16;
	dev->dp8390.mem[(addr+3) & 0x1fff] = val >> 24;
    }
}


static void
ram_writeb(uint32_t addr, uint8_t val, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    ram_write(dev, addr, val, 1);
}


static void
ram_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    ram_write(dev, addr, val, 2);
}


static void
ram_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    ram_write(dev, addr, val, 4);
}


static uint8_t
nic_mca_read(int port, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    return(dev->pos_regs[port & 7]);
}


static void
nic_mca_write(int port, uint8_t val, priv_t priv)
{
    nic_t *dev = (nic_t *)priv;
    int8_t irqs[4] = { 3, 4, 10, 15 };
    uint32_t ram_size = 0;

    /* MCA does not write registers below 0x0100. */
    if (port < 0x0102) return;

    /* Save the MCA register value. */
    dev->pos_regs[port & 7] = val;

    nic_ioremove(dev, dev->base_address);

    dev->base_address = 0x800 + (((dev->pos_regs[2] & 0xf0) >> 4) * 0x1000);
    dev->base_irq = irqs[(dev->pos_regs[5] & 0x0c) >> 2];

    dev->ram_addr = 0xC0000 + ((dev->pos_regs[3] & 0x0f) * 0x2000) + ((dev->pos_regs[3] & 0x80) ? 0xF00000 : 0);
    ram_size = (dev->pos_regs[3] & 0x10) ? 0x4000 : 0x2000;
	
    /*
     * The PS/2 Model 80 BIOS always enables a card if it finds one,
     * even if no resources were assigned yet (because we only added
     * the card, but have not run AutoConfig yet...)
     *
     * So, remove current address, if any.
     */

    /* Initialize the device if fully configured. */
    if (dev->pos_regs[2] & 0x01) {
	/* Card enabled; register (new) I/O handler. */
	nic_ioset(dev, dev->base_address);

	nic_reset(dev);

	mem_map_add(&dev->ram_mapping, dev->ram_addr, ram_size,
		    ram_readb, ram_readw, ram_readl,
		    ram_writeb, ram_writew, ram_writel,
		    NULL, MEM_MAPPING_EXTERNAL, dev);	

	mem_map_disable(&dev->ram_mapping);

	INFO("%s: attached IO=0x%X IRQ=%i, RAM=0x%06X\n",
	     dev->name, dev->base_address, dev->base_irq, dev->ram_addr);
    }
}


static uint8_t
nic_mca_feedb(priv_t priv)
{
    return 1;
}


static void
nic_close(priv_t priv)
{
    nic_t *dev = (nic_t *)priv;

    /* Make sure the platform layer is shut down. */
    network_close();

    nic_ioremove(dev, dev->base_address);

    DEBUG("%s: closed\n", dev->name);

    free(dev);
}


static priv_t
nic_init(const device_t *info, UNUSED(void *parent))
{
    uint32_t mac;
    nic_t *dev;

    dev = (nic_t *)mem_alloc(sizeof(nic_t));
    memset(dev, 0x00, sizeof(nic_t));
    dev->name = info->name;
    dev->board = info->local;

    switch (dev->board) {
	case WD8003E:
		dev->board_chip = WE_WD8003E;
		dev->maclocal[0] = 0x00;  /* 00:00:C0 (WD/SMC OID) */
		dev->maclocal[1] = 0x00;
		dev->maclocal[2] = 0xC0;
		break;

	case WD8013EBT:
		dev->board_chip = WE_WD8013EBT;
		dev->maclocal[0] = 0x00;  /* 00:00:C0 (WD/SMC OID) */
		dev->maclocal[1] = 0x00;
		dev->maclocal[2] = 0xC0;
		break;

	case WD8013EPA:
		dev->board_chip = WE_WD8013EP | 0x80;
		dev->maclocal[0] = 0x00;  /* 00:00:C0 (WD/SMC OID) */
		dev->maclocal[1] = 0x00;
		dev->maclocal[2] = 0xC0;
		dev->pos_regs[0] = 0xC8;
		dev->pos_regs[1] = 0x61;
		break;
    }

    if (dev->board != WD8013EPA) {
	dev->base_address = device_get_config_hex16("base");
	dev->base_irq = device_get_config_int("irq");
	dev->ram_addr = device_get_config_hex20("ram_addr");
    } else {
	mca_add(nic_mca_read, nic_mca_write, nic_mca_feedb, NULL, dev);	
    }

    /* See if we have a local MAC address configured. */
    mac = device_get_config_mac("mac", -1);

    /*
     * Make this device known to the I/O system.
     * PnP and PCI devices start with address spaces inactive.
     */
    if (dev->board != WD8013EPA)
	nic_ioset(dev, dev->base_address);

    /* Set up our BIA. */
    if (mac & 0xff000000) {
	/* Generate new local MAC. */
	dev->maclocal[3] = random_generate();
	dev->maclocal[4] = random_generate();
	dev->maclocal[5] = random_generate();
	mac = (((int) dev->maclocal[3]) << 16);
	mac |= (((int) dev->maclocal[4]) << 8);
	mac |= ((int) dev->maclocal[5]);
	device_set_config_mac("mac", mac);
    } else {
	dev->maclocal[3] = (mac>>16) & 0xff;
	dev->maclocal[4] = (mac>>8) & 0xff;
	dev->maclocal[5] = (mac & 0xff);
    }
    memcpy(dev->dp8390.physaddr, dev->maclocal, sizeof(dev->maclocal));

    INFO("%s: I/O=%04x, IRQ=%i, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
	 dev->name, dev->base_address, dev->base_irq,
	 dev->dp8390.physaddr[0], dev->dp8390.physaddr[1],
	 dev->dp8390.physaddr[2], dev->dp8390.physaddr[3],
	 dev->dp8390.physaddr[4], dev->dp8390.physaddr[5]);

    /* Reset the board. */
    if (dev->board != WD8013EPA)
	nic_reset(dev);

    /* Attach ourselves to the network module. */
    if (! network_attach(dev, dev->maclocal, nic_rx)) {
	nic_close(dev);

	return(NULL);
    }

    /* Map this system into the memory map. */
    if (dev->board != WD8013EPA) {
	mem_map_add(&dev->ram_mapping, dev->ram_addr, 0x4000,
		    ram_readb,NULL,NULL, ram_writeb,NULL,NULL,
		    NULL, MEM_MAPPING_EXTERNAL, dev);
	mem_map_disable(&dev->ram_mapping);

	INFO("%s: attached IO=0x%X IRQ=%i, RAM addr=0x%06x\n",
	     dev->name, dev->base_address, dev->base_irq, dev->ram_addr);
    }

    return((priv_t)dev);
}


static const device_config_t wd8003_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x300,
	{
		{
			"240H", 0x240
		},
		{
			"280H", 0x280
		},
		{
			"300H", 0x300
		},
		{
			"380H", 0x380
		},
		{
			NULL
		}
	}
    },
    {
	"irq", "IRQ", CONFIG_SELECTION, "", 3,
	{
		{
			"IRQ 2", 2
		},
		{
			"IRQ 3", 3
		},
		{
			"IRQ 5", 5
		},
		{
			"IRQ 7", 7
		},
		{
			NULL
		}
	}
    },
    {
	"mac", "MAC Address", CONFIG_MAC, "", -1
    },
    {
	"ram_addr", "RAM address", CONFIG_HEX20, "", 0xD0000,
	{
		{
			"C800H", 0xc8000
		},
		{
			"CC00H", 0xcc000
		},
		{
			"D000H", 0xd0000
		},
		{
			"D400H", 0xd4000
		},
		{
			"D800H", 0xd8000
		},
		{
			"DC00H", 0xdc000
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

static const device_config_t wd8013_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x300,
	{
		{
			"240H", 0x240
		},
		{
			"280H", 0x280
		},
		{
			"300H", 0x300
		},
		{
			"380H", 0x380
		},
		{
			NULL
		}
	}
    },
    {
	"irq", "IRQ", CONFIG_SELECTION, "", 3,
	{
		{
			"IRQ 2", 2
		},
		{
			"IRQ 3", 3
		},
		{
			"IRQ 5", 5
		},
		{
			"IRQ 7", 7
		},
		{
			"IRQ 10", 10
		},
		{
			"IRQ 11", 11
		},
		{
			"IRQ 15", 15
		},
		{
			NULL
		}
	}
    },
    {
	"mac", "MAC Address", CONFIG_MAC, "", -1
    },
    {
	"ram_addr", "RAM address", CONFIG_HEX20, "", 0xD0000,
	{
		{
			"C800H", 0xc8000
		},
		{
			"CC00H", 0xcc000
		},
		{
			"D000H", 0xd0000
		},
		{
			"D400H", 0xd4000
		},
		{
			"D800H", 0xd8000
		},
		{
			"DC00H", 0xdc000
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

static const device_config_t mca_config[] = {
    {
	"mac", "MAC Address", CONFIG_MAC, "", -1
    },
    {
	NULL
    }
};


const device_t wd8003e_device = {
    "Western Digital WD8003E",
    DEVICE_ISA,
    WD8003E,
    NULL,
    nic_init, nic_close, NULL,
    NULL, NULL, NULL, NULL,
    wd8003_config
};

const device_t wd8013ebt_device = {
    "Western Digital WD8013EBT",
    DEVICE_ISA,
    WD8013EBT,
    NULL,
    nic_init, nic_close, NULL,
    NULL, NULL, NULL, NULL,
    wd8013_config
};

const device_t wd8013epa_device = {
    "Western Digital WD8013EP/A",
    DEVICE_MCA,
    WD8013EPA,
    NULL,
    nic_init, nic_close, NULL,
    NULL, NULL, NULL, NULL,
    mca_config
};
