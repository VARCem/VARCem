/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Driver for the ESDI controller (WD1007-vse1) for PC/AT.
 *
 * Version:	@(#)hdc_esdi_at.c	1.0.17	2019/05/13
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
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#define dbglog hdc_log
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../timer.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../system/pic.h"
#include "hdc.h"
#include "hdd.h"


#define HDC_TIME		(TIMER_USEC*10LL)

#define ESDI_BIOS_FILE		L"disk/esdi/at/62-000279-061.bin"


#define STAT_ERR		0x01
#define STAT_INDEX		0x02
#define STAT_CORRECTED_DATA	0x04
#define STAT_DRQ		0x08	/* Data request */
#define STAT_DSC                0x10
#define STAT_SEEK_COMPLETE      0x20
#define STAT_READY		0x40
#define STAT_BUSY		0x80

#define ERR_DAM_NOT_FOUND       0x01	/* Data Address Mark not found */
#define ERR_TR000               0x02	/* track 0 not found */
#define ERR_ABRT		0x04	/* command aborted */
#define ERR_ID_NOT_FOUND	0x10	/* ID not found */
#define ERR_DATA_CRC	        0x40	/* data CRC error */
#define ERR_BAD_BLOCK	        0x80	/* bad block detected */

#define CMD_NOP                 0x00
#define CMD_RESTORE		0x10
#define CMD_READ		0x20
#define CMD_WRITE		0x30
#define CMD_VERIFY		0x40
#define CMD_FORMAT		0x50
#define CMD_SEEK   		0x70
#define CMD_DIAGNOSE		0x90
#define CMD_SET_PARAMETERS	0x91
#define CMD_READ_PARAMETERS	0xec


typedef struct {
    int		cfg_spt;
    int		cfg_hpc;
    int		current_cylinder;
    int		real_spt;
    int		real_hpc;
    int		real_tracks;
    int		present;
    int		hdd_num;
} drive_t;

typedef struct {
    uint8_t	status;
    uint8_t	error;
    int		secount,sector,cylinder,head,cylprecomp;
    uint8_t	command;
    uint8_t	fdisk;
    int		pos;

    int		drive_sel;
    int		reset;
    uint16_t	buffer[256];
    int		irqstat;

    int64_t	callback;

    drive_t	drives[2];

    rom_t	bios_rom;
} hdc_t;


static __inline void
irq_raise(hdc_t *dev)
{
    /* If enabled in the control register.. */
    if (! (dev->fdisk & 0x02)) {
	/* .. raise IRQ14. */
	picint(1 << 14);
    }

    dev->irqstat = 1;
}


static __inline void
irq_lower(hdc_t *dev)
{
    if (dev->irqstat) {
	if (! (dev->fdisk & 0x02))
		picintc(1 << 14);

	dev->irqstat = 0;
    }
}


/* Return the sector offset for the current register values. */
static int
get_sector(hdc_t *dev, off64_t *addr)
{
    drive_t *drive = &dev->drives[dev->drive_sel];
    int heads = drive->cfg_hpc;
    int sectors = drive->cfg_spt;
    int c, h, s;

    if (dev->head > heads) {
	DEBUG("dev_get_sector: past end of configured heads\n");
	return(1);
    }

    if (dev->sector >= sectors+1) {
	DEBUG("dev_get_sector: past end of configured sectors\n");
	return(1);
    }

    if (drive->cfg_spt==drive->real_spt && drive->cfg_hpc==drive->real_hpc) {
	*addr = ((((off64_t) dev->cylinder * heads) + dev->head) *
					sectors) + (dev->sector - 1);
    } else {
	/*
	 * When performing translation, the firmware seems to leave 1
	 * sector per track inaccessible (spare sector)
	 */

	*addr = ((((off64_t) dev->cylinder * heads) + dev->head) *
					sectors) + (dev->sector - 1);

	s = *addr % (drive->real_spt - 1);
	h = (*addr / (drive->real_spt - 1)) % drive->real_hpc;
	c = (*addr / (drive->real_spt - 1)) / drive->real_hpc;

	*addr = ((((off64_t)c * drive->real_hpc) + h) * drive->real_spt) + s;
    }
        
    return(0);
}


/* Move to the next sector using CHS addressing. */
static void
next_sector(hdc_t *dev)
{
    drive_t *drive = &dev->drives[dev->drive_sel];

    dev->sector++;
    if (dev->sector == (drive->cfg_spt + 1)) {
	dev->sector = 1;
	if (++dev->head == drive->cfg_hpc) {
		dev->head = 0;
		dev->cylinder++;
		if (drive->current_cylinder < drive->real_tracks)
			drive->current_cylinder++;
	}
    }
}


static void
hdc_writew(uint16_t port, uint16_t val, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    dev->buffer[dev->pos >> 1] = val;
    dev->pos += 2;

    if (dev->pos >= 512) {
	dev->pos = 0;
	dev->status = STAT_BUSY;
	timer_clock();

	/* 390.625 us per sector at 10 Mbit/s = 1280 kB/s. */
	dev->callback = (3125LL * TIMER_USEC) / 8LL;

	timer_update_outstanding();
    }
}


static void
hdc_write(uint16_t port, uint8_t val, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    DBGLOG(2, "WD1007 write(%04x, %02x)\n", port, val);

    switch (port) {
	case 0x1f0:	/* data */
		hdc_writew(port, val | (val << 8), priv);
		return;

	case 0x1f1:	/* write precompensation */
		dev->cylprecomp = val;
		return;

	case 0x1f2:	/* sector count */
		dev->secount = val;
		return;

	case 0x1f3:	/* sector */
		dev->sector = val;
		return;

	case 0x1f4:	/* cylinder low */
		dev->cylinder = (dev->cylinder & 0xFF00) | val;
		return;

	case 0x1f5:	/* cylinder high */
		dev->cylinder = (dev->cylinder & 0xFF) | (val << 8);
		return;

	case 0x1f6: /* drive/Head */
		dev->head = val & 0xF;
		dev->drive_sel = (val & 0x10) ? 1 : 0;
		if (dev->drives[dev->drive_sel].present) {
			dev->status = STAT_READY|STAT_DSC;
		} else {
			dev->status = 0;
		}
		return;

	case 0x1f7:	/* command register */
		irq_lower(dev);
		dev->command = val;
		dev->error = 0;

		DEBUG("WD1007: command %02x\n", val & 0xf0);
		switch (val & 0xf0) {
			case CMD_RESTORE:
				dev->command &= ~0x0f; /*mask off step rate*/
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*HDC_TIME;
				timer_update_outstanding();
				break;

			case CMD_SEEK:
				dev->command &= ~0x0f; /*mask off step rate*/
				dev->status = STAT_BUSY;
				timer_clock();
				dev->callback = 200LL*HDC_TIME;
				timer_update_outstanding();
				break;

			default:
				switch (val) {
					case CMD_NOP:
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;

					case CMD_READ:
					case CMD_READ+1:
					case CMD_READ+2:
					case CMD_READ+3:
						dev->command &= ~0x03;
						if (val & 0x02)
							fatal("Read with ECC\n");

					case 0xa0:
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;

					case CMD_WRITE:
					case CMD_WRITE+1:
					case CMD_WRITE+2:
					case CMD_WRITE+3:
						dev->command &= ~0x03;
						if (val & 0x02)
							fatal("Write with ECC\n");
						dev->status = STAT_DRQ | STAT_DSC;
						dev->pos = 0;
						break;

					case CMD_VERIFY:
					case CMD_VERIFY+1:
						dev->command &= ~0x01;
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;

					case CMD_FORMAT:
						dev->status = STAT_DRQ;
						dev->pos = 0;
						break;

					case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 30LL*HDC_TIME;
						timer_update_outstanding();
						break;

					case CMD_DIAGNOSE: /* Execute Drive Diagnostics */
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;

					case 0xe0: /*???*/
					case CMD_READ_PARAMETERS:
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;

					default:
						DEBUG("WD1007: bad command %02X\n", val);
					case 0xe8: /*???*/
						dev->status = STAT_BUSY;
						timer_clock();
						dev->callback = 200LL*HDC_TIME;
						timer_update_outstanding();
						break;
				}
		}
		break;

	case 0x3f6: /* Device control */
		if ((dev->fdisk & 0x04) && !(val & 0x04)) {
			timer_clock();
			dev->callback = 500LL*HDC_TIME;
			timer_update_outstanding();
			dev->reset = 1;
			dev->status = STAT_BUSY;
		}

		if (val & 0x04) {
			/*Drive held in reset*/
			timer_clock();
			dev->callback = 0LL;
			timer_update_outstanding();
			dev->status = STAT_BUSY;
		}
		dev->fdisk = val;

		/* Lower IRQ on IRQ disable. */
		if ((val & 0x02) && !(dev->fdisk & 0x02))
			picintc(1 << 14);
		break;
	}
}


static uint16_t
hdc_readw(uint16_t port, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    uint16_t temp;

    temp = dev->buffer[dev->pos >> 1];
    dev->pos += 2;

    if (dev->pos >= 512) {
	dev->pos=0;
	dev->status = STAT_READY | STAT_DSC;
	if (dev->command == CMD_READ || dev->command == 0xa0) {
		dev->secount = (dev->secount - 1) & 0xff;
		if (dev->secount) {
			next_sector(dev);
			dev->status = STAT_BUSY;
			timer_clock();

			/* 390.625 us per sector at 10 Mbit/s = 1280 kB/s. */
			dev->callback = (3125LL * TIMER_USEC) / 8LL;

			timer_update_outstanding();
		}
	}
    }

    return(temp);
}


static uint8_t
hdc_read(uint16_t port, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    uint8_t temp = 0xff;

    switch (port) {
	case 0x1f0:	/* data */
		temp = hdc_readw(port, dev) & 0xff;
		break;

	case 0x1f1:	/* error */
		temp = dev->error;
		break;

	case 0x1f2:	/* sector count */
		temp = dev->secount;
		break;

	case 0x1f3:	/* sector */
		temp = dev->sector;
		break;

	case 0x1f4:	/* cylinder low */
		temp = (uint8_t)(dev->cylinder&0xff);
		break;

	case 0x1f5:	/* cylinder high */
		temp = (uint8_t)(dev->cylinder>>8);
		break;

	case 0x1f6:	/* drive/Head */
		temp = (uint8_t)(0xa0|dev->head|(dev->drive_sel?0x10:0));
		break;

	case 0x1f7:	/* status */
		irq_lower(dev);
		temp = dev->status;
		break;
    }

    DBGLOG(2, "WD1007 read(%04x) = %02x\n", port, temp);

    return(temp);
}


static void
hdc_callback(priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    drive_t *drive = &dev->drives[dev->drive_sel];
    off64_t addr;

    dev->callback = 0LL;
    if (dev->reset) {
	dev->status = STAT_READY|STAT_DSC;
	dev->error = 1;
	dev->secount = 1;
	dev->sector = 1;
	dev->head = 0;
	dev->cylinder = 0;
	dev->reset = 0;

	hdd_active(drive->hdd_num, 0);

	return;
    }

    DEBUG("WD1007: command %02x\n", dev->command);

    switch (dev->command) {
	case CMD_RESTORE:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
		} else {
			drive->current_cylinder = 0;
			dev->status = STAT_READY|STAT_DSC;
		}
		irq_raise(dev);
		break;

	case CMD_SEEK:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
		} else {
			dev->status = STAT_READY|STAT_DSC;
		}
		irq_raise(dev);
		break;

	case CMD_READ:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		if (get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}

		hdd_active(drive->hdd_num, 1);

		hdd_image_read(drive->hdd_num, addr, 1,
			      (uint8_t *)dev->buffer);

		dev->pos = 0;
		dev->status = STAT_DRQ|STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case CMD_WRITE:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		if (get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}

		hdd_active(drive->hdd_num, 1);

		hdd_image_write(drive->hdd_num, addr, 1,
				(uint8_t *)dev->buffer);

		irq_raise(dev);
		dev->secount = (dev->secount - 1) & 0xff;
		if (dev->secount) {
			dev->status = STAT_DRQ|STAT_READY|STAT_DSC;
			dev->pos = 0;
			next_sector(dev);
		} else {
			dev->status = STAT_READY|STAT_DSC;
		}
		break;

	case CMD_VERIFY:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		if (get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}

		hdd_active(drive->hdd_num, 1);

		hdd_image_read(drive->hdd_num, addr, 1,
			      (uint8_t *)dev->buffer);

		next_sector(dev);
		dev->secount = (dev->secount - 1) & 0xff;
		if (dev->secount)
			dev->callback = 6LL*HDC_TIME;
		else {
			dev->pos = 0;
			dev->status = STAT_READY|STAT_DSC;
			irq_raise(dev);
		}
		break;

	case CMD_FORMAT:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		if (get_sector(dev, &addr)) {
			dev->error = ERR_ID_NOT_FOUND;
			dev->status = STAT_READY|STAT_DSC|STAT_ERR;
			irq_raise(dev);
			break;
		}

		hdd_active(drive->hdd_num, 1);

		hdd_image_zero(drive->hdd_num, addr, dev->secount);

		dev->status = STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case CMD_DIAGNOSE:
		dev->error = 1;	 /*no error detected*/
		dev->status = STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
		if (drive->present == 0) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		drive->cfg_spt = dev->secount;
		drive->cfg_hpc = dev->head+1;
		DEBUG("WD1007: parameters: spt=%i hpc=%i\n", drive->cfg_spt,drive->cfg_hpc);
		if (! dev->secount)
			fatal("WD1007: secount=0\n");
		dev->status = STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case CMD_NOP:
		dev->status = STAT_READY|STAT_ERR|STAT_DSC;
		dev->error = ERR_ABRT;
		irq_raise(dev);
		break;

	case 0xe0:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		switch (dev->cylinder >> 8) {
			case 0x31:
				dev->cylinder = drive->real_tracks;
				break;

			case 0x33:
				dev->cylinder = drive->real_hpc;
				break;

			case 0x35:
				dev->cylinder = 0x200;
				break;

			case 0x36:
				dev->cylinder = drive->real_spt;
				break;

			default:
				DEBUG("WD1007: bad read config %02x\n",
						dev->cylinder >> 8);
				break;
		}
		dev->status = STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	case 0xa0:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
		} else {
			memset(dev->buffer, 0x00, 512);
			memset(&dev->buffer[3], 0xff, 512-6);
			dev->pos = 0;
			dev->status = STAT_DRQ|STAT_READY|STAT_DSC;
		}
		irq_raise(dev);
		break;

	case CMD_READ_PARAMETERS:
		if (! drive->present) {
			dev->status = STAT_READY|STAT_ERR|STAT_DSC;
			dev->error = ERR_ABRT;
			irq_raise(dev);
			break;
		}

		memset(dev->buffer, 0x00, 512);
		dev->buffer[0] = 0x44;	/* general configuration */
		dev->buffer[1] = drive->real_tracks; /* number of non-removable cylinders */
		dev->buffer[2] = 0;	/* number of removable cylinders */
		dev->buffer[3] = drive->real_hpc;    /* number of heads */
		dev->buffer[4] = 600;	/* number of unformatted bytes/track */
		dev->buffer[5] = dev->buffer[4] * drive->real_spt; /* number of unformatted bytes/sector */
		dev->buffer[6] = drive->real_spt; /* number of sectors */
		dev->buffer[7] = 0;	/*minimum bytes in inter-sector gap*/
		dev->buffer[8] = 0;	/* minimum bytes in postamble */
		dev->buffer[9] = 0;	/* number of words of vendor status */
		/* controller info */
		dev->buffer[20] = 2; 	/* controller type */
		dev->buffer[21] = 1;	/* sector buffer size, in sectors */
		dev->buffer[22] = 0;	/* ecc bytes appended */
		dev->buffer[27] = 'W' | ('D' << 8);
		dev->buffer[28] = '1' | ('0' << 8);
		dev->buffer[29] = '0' | ('7' << 8);
		dev->buffer[30] = 'V' | ('-' << 8);
		dev->buffer[31] = 'S' | ('E' << 8);
		dev->buffer[32] = '1';
		dev->buffer[47] = 0;	/* sectors per interrupt */
		dev->buffer[48] = 0;	/* can use double word read/write? */
		dev->pos = 0;
		dev->status = STAT_DRQ|STAT_READY|STAT_DSC;
		irq_raise(dev);
		break;

	default:
		DEBUG("WD1007: callback on unknown command %02x\n", dev->command);
		/*FALLTHROUGH*/

	case 0xe8:
		dev->status = STAT_READY|STAT_ERR|STAT_DSC;
		dev->error = ERR_ABRT;
		irq_raise(dev);
		break;
    }

    hdd_active(drive->hdd_num, 0);
}


static void
loadhd(hdc_t *dev, int hdd_num, int d, const wchar_t *fn)
{
    drive_t *drive = &dev->drives[hdd_num];

    if (! hdd_image_load(d)) {
	DEBUG("WD1007: drive %d not present!\n", d);
	drive->present = 0;
	return;
    }

    drive->cfg_spt = drive->real_spt = (uint8_t)hdd[d].spt;
    drive->cfg_hpc = drive->real_hpc = (uint8_t)hdd[d].hpc;
    drive->real_tracks = (uint8_t)hdd[d].tracks;
    drive->hdd_num = d;
    drive->present = 1;
}


static void
wd1007vse1_close(priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    drive_t *drive;
    int d;

    for (d=0; d<2; d++) {
	drive = &dev->drives[d];

	hdd_image_close(drive->hdd_num);
    }

    free(dev);

    ui_sb_icon_update(SB_DISK | HDD_BUS_ESDI, 0);
}


static priv_t
wd1007vse1_init(const device_t *info, UNUSED(void *parent))
{
    hdc_t *dev;
    int c, d;

    dev = (hdc_t *)mem_alloc(sizeof(hdc_t));
    memset(dev, 0x00, sizeof(hdc_t));

    c = 0;
    for (d = 0; d < HDD_NUM; d++) {
	if ((hdd[d].bus==HDD_BUS_ESDI) && (hdd[d].bus_id.esdi_channel<ESDI_NUM)) {
		loadhd(dev, hdd[d].bus_id.esdi_channel, d, hdd[d].fn);

		hdd_active(d, 0);

		if (++c >= ESDI_NUM) break;
	}
    }

    dev->status = STAT_READY|STAT_DSC;
    dev->error = 1;

    rom_init(&dev->bios_rom, info->path,
	     0xc8000, 0x4000, 0x3fff, 0, MEM_MAPPING_EXTERNAL);

    io_sethandler(0x01f0, 1,
		  hdc_read, hdc_readw, NULL,
		  hdc_write, hdc_writew, NULL, dev);
    io_sethandler(0x01f1, 7,
		  hdc_read, NULL, NULL,
		  hdc_write, NULL, NULL, dev);
    io_sethandler(0x03f6, 1, NULL, NULL, NULL,
		  hdc_write, NULL, NULL, dev);

    timer_add(hdc_callback, dev, &dev->callback, &dev->callback);

    return((priv_t)dev);
}


const device_t esdi_at_wd1007vse1_device = {
    "PC/AT ESDI Fixed Disk Adapter",
    DEVICE_ISA | DEVICE_AT,
    (HDD_BUS_ESDI << 8) | 0,
    ESDI_BIOS_FILE,
    wd1007vse1_init, wd1007vse1_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
