/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Driver for the PC/AT ST506 MFM/RLL Fixed Disk controller.
 *
 *		This controller was a 16bit ISA card, and it used a WD1003
 *		based design. Most cards were WD1003-WA2 or -WAH, where the
 *		-WA2 cards had a floppy controller as well (to save space.)
 *
 * Version:	@(#)hdc_st506_at.c	1.0.19	2021/03/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#define dbglog hdc_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../system/pic.h"
#include "hdc.h"
#include "hdd.h"


#define ST506_TIME		(TIMER_USEC * 10LL)

/*
 * Rough estimate - MFM drives spin at 3600 RPM, with 17 sectors
 * per track, meaning (3600/60)*17 = 1020 sectors per second, or
 * 980us per sector.
 *
 * This is required for OS/2 on slow 286 systems, as the hard
 * drive formatter will crash with 'internal processing error'
 * if write sector interrupts are too close in time.
 */
#define SECTOR_TIME		(TIMER_USEC * 980LL)


#define STAT_ERR		0x01
#define STAT_INDEX		0x02
#define STAT_ECC		0x04
#define STAT_DRQ		0x08	// data request
#define STAT_DSC		0x10
#define STAT_WRFLT		0x20
#define STAT_READY		0x40
#define STAT_BUSY		0x80

#define ERR_DAM_NOT_FOUND	0x01	// Data Address Mark not found
#define ERR_TR000		0x02	// track 0 not found
#define ERR_ABRT		0x04	// command aborted
#define ERR_ID_NOT_FOUND	0x10	// ID not found
#define ERR_DATA_CRC		0x40	// data CRC error
#define ERR_BAD_BLOCK		0x80	// bad block detected

#define CMD_RESTORE		0x10
#define CMD_READ		0x20
#define CMD_WRITE		0x30
#define CMD_VERIFY		0x40
#define CMD_FORMAT		0x50
#define CMD_SEEK		0x70
#define CMD_DIAGNOSE		0x90
#define CMD_SET_PARAMETERS	0x91


typedef struct {
    int8_t	present,		// drive is present
		hdd_num,		// drive number in system
		steprate,		// current servo step rate
		spt,			// physical #sectors per track
		hpc,			// physical #heads per cylinder
		pad;
    int16_t	tracks;			// physical #tracks per cylinder

    int8_t	cfg_spt,		// configured #sectors per track
		cfg_hpc;		// configured #heads per track

    int16_t	curcyl;			// current track number
} drive_t;

typedef struct {
    uint16_t	base_addr;		// I/O base address
    int8_t	dma,			// ... DMA channel
		irq;			// ... IRQ channel

    uint8_t	precomp,		// 1: precomp/error register
		error,
		secount,		// 2: sector count register
		sector,			// 3: sector number
		head,			// 6: head number + drive select
		command,		// 7: command/status
		status,
		fdisk;			// 8: control register
    uint16_t	cylinder;		// 4/5: cylinder LOW and HIGH

    int8_t	reset,			// controller in reset
		irqstat,		// current IRQ status
		drvsel,			// current selected drive
		pad;

    int		pos;			// offset within data buffer

    tmrval_t	callback;		// callback delay timer

    uint16_t	buffer[256];		// data buffer (16b wide)

    drive_t	drives[ST506_NUM];	// attached drives
} hdc_t;


static __inline void
irq_raise(hdc_t *dev)
{
    if (! (dev->fdisk & 0x02))
	picint(1 << dev->irq);

    dev->irqstat = 1;
}


static __inline void
irq_lower(hdc_t *dev)
{
    picintc(1 << dev->irq);
}


static __inline void
irq_update(hdc_t *dev)
{
    if (dev->irqstat && !((pic2.pend | pic2.ins) & 0x40) && !(dev->fdisk & 2))
	picint(1 << dev->irq);
}


/*
 * Return the sector offset for the current register values.
 *
 * According to the WD1002/WD1003 technical reference manual,
 * this is not done entirely correct. It specifies that the
 * parameters set with the SET_DRIVE_PARAMETERS command are
 * to be used only for multi-sector operations, and that any
 * such operation can only be executed AFTER these parameters
 * have been set. This would imply that for regular single
 * transfers, the controller uses (or, can use) the actual
 * geometry information...
 */
static int
get_sector(hdc_t *dev, off_t *addr)
{
    drive_t *drive = &dev->drives[dev->drvsel];

    /* FIXME:
     * See if this is even needed - if the code is present, IBM AT
     * diagnostics v2.07 will error with: ERROR 152 - SYSTEM BOARD.
     */
    if (drive->curcyl != dev->cylinder) {
	DEBUG("WD1003(%i): sector: wrong cylinder\n", dev->drvsel);
	return(0);
    }

    if (dev->head > drive->cfg_hpc) {
	DEBUG("WD1003(%i): sector: past end of configured heads\n",
							dev->drvsel);
	return(0);
    }

    if (dev->sector >= drive->cfg_spt+1) {
	DEBUG("WD1003(%i): sector: past end of configured sectors\n",
							dev->drvsel);
	return(0);
    }

#if 1
    /* We should check this in the SET_DRIVE_PARAMETERS command!  --FvK */
    if (dev->head > drive->hpc) {
	DEBUG("WD1003(%i): sector: past end of heads\n", dev->drvsel);
	return(0);
    }

    if (dev->sector >= drive->spt+1) {
	DEBUG("WD1003(%i): sector: past end of sectors\n", dev->drvsel);
	return(0);
    }
#endif

    *addr = ((((off_t) dev->cylinder * drive->cfg_hpc) + dev->head) *
		      		drive->cfg_spt) + (dev->sector - 1);

    return(1);
}


/* Move to the next sector using CHS addressing. */
static void
next_sector(hdc_t *dev)
{
    drive_t *drive = &dev->drives[dev->drvsel];

    if (++dev->sector == (drive->cfg_spt+1)) {
	dev->sector = 1;
	if (++dev->head == drive->cfg_hpc) {
		dev->head = 0;
		dev->cylinder++;
		if (drive->curcyl < drive->tracks)
			drive->curcyl++;
	}
    }
}


static void
do_seek(hdc_t *dev)
{
    drive_t *drive = &dev->drives[dev->drvsel];

    DEBUG("WD1003(%i): seek(%i) max=%i\n",
	  dev->drvsel, dev->cylinder, drive->tracks);

    if (dev->cylinder < drive->tracks)
	drive->curcyl = dev->cylinder;
    else
	drive->curcyl = drive->tracks - 1;
}


static void
call_back(priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    drive_t *drive = &dev->drives[dev->drvsel];
    off_t addr;

    dev->callback = 0;

    if (dev->reset) {
	DEBUG("WD1003(%i): reset\n", dev->drvsel);

	dev->status = STAT_READY|STAT_DSC;
	dev->error = 1;
	dev->secount = 1;
	dev->sector = 1;
	dev->head = 0;
	dev->cylinder = 0;

	drive->steprate = 0x0f;		// default steprate
	drive->cfg_spt = 0;		// need new parameters

	dev->reset = 0;

	hdd_active(drive->hdd_num, 0);

	return;
    }

    switch (dev->command) {
	case CMD_RESTORE:
		DEBUG("WD1003(%i): restore, step=%i\n",
			dev->drvsel, drive->steprate);
		drive->curcyl = 0;
		dev->status = STAT_READY|STAT_DSC;
		hdd_active(drive->hdd_num, 0);
		irq_raise(dev);
		break;

	case CMD_SEEK:
		DEBUG("WD1003(%i): seek, step=%i\n",
		      dev->drvsel, drive->steprate);
		do_seek(dev);
		dev->status = STAT_READY|STAT_DSC;
		hdd_active(drive->hdd_num, 0);
		irq_raise(dev);
		break;

	case CMD_READ:
		DEBUG("WD1003(%i): read(%i,%i,%i)\n",
		      dev->drvsel, dev->cylinder, dev->head, dev->sector);
		do_seek(dev);
		if (! get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}
		hdd_image_read(drive->hdd_num, addr, 1, (uint8_t *)dev->buffer);
		dev->pos = 0;
		dev->status = STAT_DRQ|STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case CMD_WRITE:
		DEBUG("WD1003(%i): write(%i,%i,%i)\n",
		      dev->drvsel, dev->cylinder, dev->head, dev->sector);
		do_seek(dev);
		if (! get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}
		hdd_image_write(drive->hdd_num, addr, 1,(uint8_t *)dev->buffer);
		dev->status = STAT_READY|STAT_DSC;
		dev->secount = (dev->secount - 1) & 0xff;
		if (dev->secount) {
			/* More sectors to do.. */
			dev->status |= STAT_DRQ;
			dev->pos = 0;
			next_sector(dev);
		} else
			hdd_active(drive->hdd_num, 0);
		irq_raise(dev);
		break;

	case CMD_VERIFY:
		DEBUG("WD1003(%i): verify(%i,%i,%i)\n",
		      dev->drvsel, dev->cylinder, dev->head, dev->sector);
		do_seek(dev);
		dev->pos = 0;
		dev->status = STAT_READY|STAT_DSC;
		hdd_active(drive->hdd_num, 0);
		irq_raise(dev);
		break;

	case CMD_FORMAT:
		DEBUG("WD1003(%i): format(%i,%i)\n",
		      dev->drvsel, dev->cylinder, dev->head);
		do_seek(dev);
		if (! get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}
		hdd_image_zero(drive->hdd_num, addr, dev->secount);
		dev->status = STAT_READY|STAT_DSC;
		hdd_active(drive->hdd_num, 0);
		irq_raise(dev);
		break;

	case CMD_DIAGNOSE:
		/*
		 * This is basically controller diagnostics - it resets
		 * drive select to 0, error and status to ready, DSC,
		 * and no error detected.
		 */
		DEBUG("WD1003(%i): diag\n", dev->drvsel);
		dev->drvsel = 0;
		drive = &dev->drives[dev->drvsel];
		drive->steprate = 0x0f;
		dev->error = 1;
		dev->status = STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	default:
		DEBUG("WD1003(%i): callback on unknown command %02x\n",
					dev->drvsel, dev->command);
		dev->status = STAT_READY|STAT_ERR|STAT_DSC;
		dev->error = ERR_ABRT;
		irq_raise(dev);
		break;
    }
}


static void
hdc_cmd(hdc_t *dev, uint8_t val)
{
    drive_t *drive = &dev->drives[dev->drvsel];

    if (! drive->present) {
	/* This happens if sofware polls all drives. */
	DEBUG("WD1003(%i): command %02x on non-present drive\n",
					dev->drvsel, val);
	dev->command = 0xff;
	dev->status = STAT_BUSY;
	timer_clock();
	dev->callback = 200LL * ST506_TIME;
	timer_update_outstanding();

	return;
    }

    irq_lower(dev);
    dev->command = val;
    dev->error = 0;

    switch (val & 0xf0) {
	case CMD_RESTORE:
		drive->steprate = (val & 0x0f);
		dev->command &= 0xf0;
		dev->status = STAT_BUSY;
		timer_clock();
		dev->callback = 100LL*ST506_TIME;
		timer_update_outstanding();
		hdd_active(drive->hdd_num, 1);
		break;

	case CMD_SEEK:
		drive->steprate = (val & 0x0f);
		dev->command &= 0xf0;
		dev->status = STAT_BUSY;
		timer_clock();
		dev->callback = 200LL*ST506_TIME;
		timer_update_outstanding();
		hdd_active(drive->hdd_num, 1);
		break;

	default:
		switch (val) {
			case CMD_READ:
			case CMD_READ+1:
			case CMD_READ+2:
			case CMD_READ+3:
				DEBUG("WD1003(%i): read, opt=%i\n",
					dev->drvsel, val&0x03);
				dev->command &= 0xfc;
				if (val & 2)
					fatal("WD1003: READ with ECC\n");
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*ST506_TIME;
				timer_update_outstanding();
				hdd_active(drive->hdd_num, 1);
				break;

			case CMD_WRITE:
			case CMD_WRITE+1:
			case CMD_WRITE+2:
			case CMD_WRITE+3:
				DEBUG("WD1003(%i): write, opt=%i\n",
					dev->drvsel, val & 0x03);
				dev->command &= 0xfc;
				if (val & 2)
					fatal("WD1003: WRITE with ECC\n");
				dev->status = STAT_DRQ|STAT_DSC;
				dev->pos = 0;
				hdd_active(drive->hdd_num, 1);
				break;

			case CMD_VERIFY:
			case CMD_VERIFY+1:
				dev->command &= 0xfe;
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*ST506_TIME;
				timer_update_outstanding();
				hdd_active(drive->hdd_num, 1);
				break;

			case CMD_FORMAT:
				dev->status = STAT_DRQ|STAT_BUSY;
				dev->pos = 0;
				hdd_active(drive->hdd_num, 1);
				break;

			case CMD_DIAGNOSE:
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*ST506_TIME;
				timer_update_outstanding();
				hdd_active(drive->hdd_num, 1);
				break;

			case CMD_SET_PARAMETERS:
				/*
				 * NOTE:
				 *
				 * We currently just set these parameters,
				 * and never bother to check if they "fit
				 * within" the actual parameters, as
				 * determined by the image loader.
				 *
				 * The difference in parameters is OK, and
				 * occurs when the BIOS or operating system
				 * decides to use a different translation
				 * scheme, but either way, it SHOULD always
				 * fit within the actual parameters!
				 *
				 * We SHOULD check that here!! --FvK
				 */
				if (drive->cfg_spt == 0) {
					/* Only accept after RESET or DIAG. */
					drive->cfg_spt = dev->secount;
					drive->cfg_hpc = dev->head+1;
				}
				DEBUG("WD1003(%i): parameters: tracks=%i, spt=%i, hpc=%i\n",
				      dev->drvsel, drive->tracks,
				      drive->cfg_spt, drive->cfg_hpc);
				dev->command = 0x00;
				dev->status = STAT_READY|STAT_DSC;
				dev->error = 1;
				irq_raise(dev);
				break;

			default:
				DEBUG("WD1003: bad command %02X\n", val);
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*ST506_TIME;
				timer_update_outstanding();
				break;
		}
    }
}


static void hdc_writew(uint16_t port, uint16_t val, priv_t priv);

static void
hdc_write(uint16_t port, uint8_t val, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    DBGLOG(2, "WD1003: write(%04x, %02x)\n", port, val);

    switch (port) {
	case 0x01f0:	/* data */
		hdc_writew(port, val | (val << 8), priv);
		return;

	case 0x01f1:	/* write precompenstation */
		dev->precomp = val;
		return;

	case 0x01f2:	/* sector count */
		dev->secount = val;
		return;

	case 0x01f3:	/* sector */
		dev->sector = val;
		return;

	case 0x01f4:	/* cylinder low */
		dev->cylinder = (dev->cylinder & 0xff00) | val;
		return;

	case 0x01f5:	/* cylinder high */
		dev->cylinder = (dev->cylinder & 0xff) | (val << 8);
		return;

	case 0x01f6:	/* drive/head */
		dev->head = val & 0x0f;
		dev->drvsel = (val & 0x10) ? 1 : 0;
		if (dev->drives[dev->drvsel].present)
			dev->status = STAT_READY|STAT_DSC;
		  else
			dev->status = 0;
		return;

	case 0x01f7:	/* command register */
		hdc_cmd(dev, val);
		break;

	case 0x03f6:	/* device control */
		val &= 0x0f;
		if ((dev->fdisk & 0x04) && !(val & 0x04)) {
			timer_clock();
			dev->callback = 500LL*ST506_TIME;
			timer_update_outstanding();
			dev->reset = 1;
			dev->status = STAT_BUSY;
		}

		if (val & 0x04) {
			/* Drive held in reset. */
			timer_clock();
			dev->callback = 0LL;
			timer_update_outstanding();
			dev->status = STAT_BUSY;
		}
		dev->fdisk = val;
		irq_update(dev);
		break;
    }
}


static void
hdc_writew(uint16_t port, uint16_t val, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    if (port > 0x01f0) {
	hdc_write(port, val & 0xff, priv);
	if (port != 0x01f7)
		hdc_write(port + 1, (val >> 8) & 0xff, priv);
    } else {
	dev->buffer[dev->pos >> 1] = val;
	dev->pos += 2;

	if (dev->pos >= 512) {
		dev->pos = 0;
		dev->status = STAT_BUSY;
		timer_clock();
		dev->callback = SECTOR_TIME;
		timer_update_outstanding();
	}
    }
}


static uint16_t hdc_readw(uint16_t port, priv_t priv);

static uint8_t
hdc_read(uint16_t port, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x01f0:	/* data */
		ret = hdc_readw(port, dev) & 0xff;
		break;

	case 0x01f1:	/* error */
		ret = dev->error;
		break;

	case 0x01f2:	/* sector count */
		ret = dev->secount;
		break;

	case 0x01f3:	/* sector */
		ret = dev->sector;
		break;

	case 0x01f4:	/* CYlinder low */
		ret = (uint8_t)(dev->cylinder & 0xff);
		break;

	case 0x01f5:	/* Cylinder high */
		ret = (uint8_t)(dev->cylinder >> 8);
		break;

	case 0x01f6:	/* drive/head */
		ret = (uint8_t)(0xa0 | dev->head | (dev->drvsel?0x10:0));
		break;

	case 0x01f7:	/* Status */
		ret = dev->status;
		irq_lower(dev);
		break;

	default:
		break;
    }

    DBGLOG(2, "WD1003: read(%04x) = %02x\n", port, ret);

    return(ret);
}


static uint16_t
hdc_readw(uint16_t port, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    drive_t *drive = &dev->drives[dev->drvsel];
    uint16_t ret;

    if (port > 0x01f0) {
	ret = hdc_read(port, priv);
	if (port == 0x01f7)
		ret |= 0xff00;
	else
		ret |= (hdc_read(port + 1, priv) << 8);
    } else {
	ret = dev->buffer[dev->pos >> 1];
	dev->pos += 2;
	if (dev->pos >= 512) {
		dev->pos = 0;
		dev->status = STAT_READY|STAT_DSC;
		if (dev->command == CMD_READ) {
			dev->secount = (dev->secount - 1) & 0xff;
			if (dev->secount) {
				next_sector(dev);
				dev->status = STAT_BUSY | STAT_READY | STAT_DSC;
				timer_clock();
				dev->callback = SECTOR_TIME;
				timer_update_outstanding();
			} else
				hdd_active(drive->hdd_num, 0);
		}
	}
    }

    return(ret);
}


static void
loadhd(hdc_t *dev, int c, int d, const wchar_t *fn)
{
    drive_t *drive = &dev->drives[c];

    if (! hdd_image_load(d)) {
	drive->present = 0;

	return;
    }

    drive->spt = (uint8_t)hdd[d].spt;
    drive->hpc = (uint8_t)hdd[d].hpc;
    drive->tracks = (uint16_t)hdd[d].tracks;
    drive->hdd_num = d;
    drive->present = 1;
}


static void
st506_close(priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    int d;

    for (d = 0; d < ST506_NUM; d++) {
	drive_t *drive = &dev->drives[d];

	hdd_image_close(drive->hdd_num);		

	hdd_active(drive->hdd_num, 0);
    }

    free(dev);
}


static priv_t
st506_init(const device_t *info, UNUSED(void *parent))
{
    hdc_t *dev;
    int c, d;

    dev = (hdc_t *)mem_alloc(sizeof(hdc_t));
    memset(dev, 0x00, sizeof(hdc_t));

    INFO("HDC: WD1003: ISA ST506/RLL Fixed Disk Adapter initializing ...\n");

    c = 0;
    for (d = 0; d < HDD_NUM; d++) {
	if ((hdd[d].bus == HDD_BUS_ST506) && (hdd[d].bus_id.st506_channel < ST506_NUM)) {
		loadhd(dev, hdd[d].bus_id.st506_channel, d, hdd[d].fn);

		hdd_active(d, 0);

		INFO("WD1003(%i): (%ls) geometry %i/%i/%i\n", c, hdd[d].fn,
		     (int)hdd[d].tracks, (int)hdd[d].hpc, (int)hdd[d].spt);

		if (++c >= ST506_NUM) break;
	}
    }

    dev->status = STAT_READY|STAT_DSC;		/* drive is ready */
    dev->error = 1;				/* no errors */

    dev->base_addr = 0x01f0;
    dev->dma = -1;
    dev->irq = 14;

    io_sethandler(dev->base_addr, 1,
		  hdc_read, hdc_readw, NULL, hdc_write, hdc_writew, NULL, dev);
#if 0
    io_sethandler(dev->base_addr + 1, 7,
		  hdc_read, NULL,      NULL, hdc_write, NULL,       NULL, dev);
#else
    //FIXME: I thought only register 0 was 16bit ??
    io_sethandler(dev->base_addr + 1, 7,
		  hdc_read, hdc_readw, NULL, hdc_write, hdc_writew, NULL, dev);
#endif
    io_sethandler(0x03f6, 1,
		  NULL,     NULL,      NULL, hdc_write, NULL,       NULL, dev);

    timer_add(call_back, dev, &dev->callback, &dev->callback);	

    return(dev);
}


const device_t st506_at_wd1003_device = {
    "IBM PC/AT Fixed Disk Adapter",
    DEVICE_ISA | DEVICE_AT,
    (HDD_BUS_ST506 << 8) | 0,
    NULL,
    st506_init, st506_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
