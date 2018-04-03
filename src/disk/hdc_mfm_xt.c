/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Driver for the IBM PC-XT Fixed Disk controller.
 *
 *		The original controller shipped by IBM was made by Xebec, and
 *		several variations had been made:
 *
 *		#1	Original, single drive (ST412), 10MB, 2 heads.
 *		#2	Update, single drive (ST412) but with option for a
 *			switch block that can be used to 'set' the actual
 *			drive type. Four switches are define, where switches
 *			1 and 2 define drive0, and switches 3 and 4 drive1.
 *
 *			  0  ON  ON	306  2  0
 *			  1  ON  OFF	375  8  0
 *			  2  OFF ON	306  6  256
 *			  3  OFF OFF	306  4  0
 *
 *			The latter option is the default, in use on boards
 *			without the switch block option.
 *
 *		#3	Another updated board, mostly to accomodate the new
 *			20MB disk now being shipped. The controller can have
 *			up to 2 drives, the type of which is set using the
 *			switch block:
 *
 *			     SW1 SW2	CYLS HD SPT WPC
 *			  0  ON  ON	306  4  17  0
 *			  1  ON  OFF	612  4  17  0	(type 16)
 *			  2  OFF ON    	615  4  17  300	(Seagate ST-225, 2)
 *			  3  OFF OFF	306  8  17  128 (IBM WD25, 13)
 *
 *		Examples of #3 are IBM/Xebec, WD10004A-WX1 and ST11R.
 *
 *		Since all controllers (including the ones made by DTC) use
 *		(mostly) the same API, we keep them all in this module.
 *
 * Version:	@(#)hdc_mfm_xt.c	1.0.5	2018/04/02
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../device.h"
#include "../dma.h"
#include "../io.h"
#include "../mem.h"
#include "../pic.h"
#include "../rom.h"
#include "../timer.h"
#include "../plat.h"
#include "../ui.h"
#include "hdc.h"
#include "hdd.h"


#define MFM_TIME	(2000LL*TIMER_USEC)
#define XEBEC_BIOS_FILE	L"hdd/mfm_xebec/ibm_xebec_62x0822_1985.bin"
#define DTC_BIOS_FILE	L"hdd/mfm_xebec/dtc_cxd21a.bin"


enum {
    STATE_IDLE,
    STATE_RECEIVE_COMMAND,
    STATE_START_COMMAND,
    STATE_RECEIVE_DATA,
    STATE_RECEIVED_DATA,
    STATE_SEND_DATA,
    STATE_SENT_DATA,
    STATE_COMPLETION_BYTE,
    STATE_DUNNO
};


typedef struct {
    int		spt,
		hpc;
    int		tracks;
    int		cfg_spt;
    int		cfg_hpc;
    int		cfg_cyl;
    int		current_cylinder;
    int		present;
    int		hdd_num;
} drive_t;

typedef struct {
    rom_t	bios_rom;
    int64_t	callback;
    int		state;
    uint8_t	status;
    uint8_t	command[6];
    int		command_pos;
    uint8_t	data[512];
    int		data_pos,
		data_len;
    uint8_t	sector_buf[512];
    uint8_t	irq_dma_mask;
    uint8_t	completion_byte;
    uint8_t	error;
    int		drive_sel;
    drive_t	drives[2];
    int		sector,
		head,
		cylinder;
    int		sector_count;
    uint8_t	switches;
} mfm_t;

#define STAT_IRQ 0x20
#define STAT_DRQ 0x10
#define STAT_BSY 0x08
#define STAT_CD  0x04
#define STAT_IO  0x02
#define STAT_REQ 0x01

#define IRQ_ENA 0x02
#define DMA_ENA 0x01

#define CMD_TEST_DRIVE_READY      0x00
#define CMD_RECALIBRATE           0x01
#define CMD_READ_STATUS           0x03
#define CMD_VERIFY_SECTORS        0x05
#define CMD_FORMAT_TRACK          0x06
#define CMD_READ_SECTORS          0x08
#define CMD_WRITE_SECTORS         0x0a
#define CMD_SEEK                  0x0b
#define CMD_INIT_DRIVE_PARAMS     0x0c
#define CMD_WRITE_SECTOR_BUFFER   0x0f
#define CMD_BUFFER_DIAGNOSTIC     0xe0
#define CMD_CONTROLLER_DIAGNOSTIC 0xe4
#define CMD_DTC_GET_DRIVE_PARAMS  0xfb
#define CMD_DTC_SET_STEP_RATE     0xfc
#define CMD_DTC_SET_GEOMETRY      0xfe
#define CMD_DTC_GET_GEOMETRY      0xff

#define ERR_NOT_READY              0x04
#define ERR_SEEK_ERROR             0x15
#define ERR_ILLEGAL_SECTOR_ADDRESS 0x21


static uint8_t
mfm_read(uint16_t port, void *priv)
{
    mfm_t *dev = (mfm_t *)priv;
    uint8_t temp = 0xff;

    switch (port) {
	case 0x320: /*Read data*/
		dev->status &= ~STAT_IRQ;
		switch (dev->state) {
			case STATE_COMPLETION_BYTE:
				if ((dev->status & 0xf) != (STAT_CD | STAT_IO | STAT_REQ | STAT_BSY))
					fatal("Read data STATE_COMPLETION_BYTE, status=%02x\n", dev->status);

				temp = dev->completion_byte;
				dev->status = 0;
				dev->state = STATE_IDLE;
				break;
			
			case STATE_SEND_DATA:
				if ((dev->status & 0xf) != (STAT_IO | STAT_REQ | STAT_BSY))
					fatal("Read data STATE_COMPLETION_BYTE, status=%02x\n", dev->status);
				if (dev->data_pos >= dev->data_len)
					fatal("Data write with full data!\n");
				temp = dev->data[dev->data_pos++];
				if (dev->data_pos == dev->data_len) {
					dev->status = STAT_BSY;
					dev->state = STATE_SENT_DATA;
					dev->callback = MFM_TIME;
				}
				break;

			default:
				fatal("Read data register - %i, %02x\n", dev->state, dev->status);
		}
		break;

	case 0x321: /*Read status*/
		temp = dev->status;
		break;

	case 0x322: /*Read option jumpers*/
		temp = dev->switches;
		break;
    }

    return(temp);
}


static void
mfm_write(uint16_t port, uint8_t val, void *priv)
{
    mfm_t *dev = (mfm_t *)priv;

    switch (port) {
	case 0x320: /*Write data*/
		switch (dev->state) {
			case STATE_RECEIVE_COMMAND:
				if ((dev->status & 0xf) != (STAT_BSY | STAT_CD | STAT_REQ))
					fatal("Bad write data state - STATE_START_COMMAND, status=%02x\n", dev->status);
				if (dev->command_pos >= 6)
					fatal("Command write with full command!\n");
				/*Command data*/
				dev->command[dev->command_pos++] = val;
				if (dev->command_pos == 6) {
					dev->status = STAT_BSY;
					dev->state = STATE_START_COMMAND;
					dev->callback = MFM_TIME;
				}
				break;

				case STATE_RECEIVE_DATA:
				if ((dev->status & 0xf) != (STAT_BSY | STAT_REQ))
					fatal("Bad write data state - STATE_RECEIVE_DATA, status=%02x\n", dev->status);
				if (dev->data_pos >= dev->data_len)
					fatal("Data write with full data!\n");
				/*Command data*/
				dev->data[dev->data_pos++] = val;
				if (dev->data_pos == dev->data_len) {
					dev->status = STAT_BSY;
					dev->state = STATE_RECEIVED_DATA;
					dev->callback = MFM_TIME;
				}
				break;

			default:
				hdc_log("Write data unknown state - %i %02x\n", dev->state, dev->status);
		}
		break;

	case 0x321: /*Controller reset*/
		dev->status = 0;
		break;

	case 0x322: /*Generate controller-select-pulse*/
		dev->status = STAT_BSY | STAT_CD | STAT_REQ;
		dev->command_pos = 0;
		dev->state = STATE_RECEIVE_COMMAND;
		break;

	case 0x323: /*DMA/IRQ mask register*/
		dev->irq_dma_mask = val;
		break;
    }
}


static void
mfm_complete(mfm_t *dev)
{
    dev->status = STAT_REQ | STAT_CD | STAT_IO | STAT_BSY;
    dev->state = STATE_COMPLETION_BYTE;

    if (dev->irq_dma_mask & IRQ_ENA) {
	dev->status |= STAT_IRQ;
	picint(1 << 5);
    }
}


static void
mfm_error(mfm_t *dev, uint8_t error)
{
    dev->completion_byte |= 0x02;
    dev->error = error;

#ifdef ENABLE_HDC_LOG
    hdc_log("mfm_error - %02x\n", dev->error);
#endif
}


static int
get_sector(mfm_t *dev, off64_t *addr)
{
    drive_t *drive = &dev->drives[dev->drive_sel];
    int heads = drive->cfg_hpc;

    if (drive->current_cylinder != dev->cylinder) {
#ifdef ENABLE_HDC_LOG
	hdc_log("mfm_get_sector: wrong cylinder\n");
#endif
	dev->error = ERR_ILLEGAL_SECTOR_ADDRESS;
	return(1);
    }
    if (dev->head > heads) {
#ifdef ENABLE_HDC_LOG
	hdc_log("mfm_get_sector: past end of configured heads\n");
#endif
	dev->error = ERR_ILLEGAL_SECTOR_ADDRESS;
	return(1);
    }
    if (dev->head > drive->hpc) {
#ifdef ENABLE_HDC_LOG
	hdc_log("mfm_get_sector: past end of heads\n");
#endif
	dev->error = ERR_ILLEGAL_SECTOR_ADDRESS;
	return(1);
    }
    if (dev->sector >= 17) {
#ifdef ENABLE_HDC_LOG
	hdc_log("mfm_get_sector: past end of sectors\n");
#endif
	dev->error = ERR_ILLEGAL_SECTOR_ADDRESS;
	return(1);
    }

    *addr = ((((off64_t) dev->cylinder * heads) + dev->head) *
			  17) + dev->sector;
	
    return(0);
}


static void
next_sector(mfm_t *dev)
{
    drive_t *drive = &dev->drives[dev->drive_sel];
	
    dev->sector++;
    if (dev->sector >= 17) {
	dev->sector = 0;
	dev->head++;
	if (dev->head >= drive->cfg_hpc) {
		dev->head = 0;
		dev->cylinder++;
		drive->current_cylinder++;
		if (drive->current_cylinder >= drive->cfg_cyl)
			drive->current_cylinder = drive->cfg_cyl-1;
	}
    }
}


static void
mfm_callback(void *priv)
{
    mfm_t *dev = (mfm_t *)priv;
    drive_t *drive;
    off64_t addr;

    dev->callback = 0LL;

    dev->drive_sel = (dev->command[1] & 0x20) ? 1 : 0;
    dev->completion_byte = dev->drive_sel & 0x20;
    drive = &dev->drives[dev->drive_sel];

    switch (dev->command[0]) {
	case CMD_TEST_DRIVE_READY:
		if (!drive->present)
			mfm_error(dev, ERR_NOT_READY);
		mfm_complete(dev);
		break;

	case CMD_RECALIBRATE:
		if (!drive->present)
			mfm_error(dev, ERR_NOT_READY);
		else {
			dev->cylinder = 0;
			drive->current_cylinder = 0;
		}
		mfm_complete(dev);
		break;

	case CMD_READ_STATUS:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_SEND_DATA;
				dev->data_pos = 0;
				dev->data_len = 4;
				dev->status = STAT_BSY | STAT_IO | STAT_REQ;
				dev->data[0] = dev->error;
				dev->data[1] = dev->drive_sel ? 0x20 : 0;
				dev->data[2] = dev->data[3] = 0;
				dev->error = 0;
				break;

			case STATE_SENT_DATA:
				mfm_complete(dev);
				break;
		}
		break;

	case CMD_VERIFY_SECTORS:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->cylinder = dev->command[3] | ((dev->command[2] & 0xc0) << 2);
				drive->current_cylinder = (dev->cylinder >= drive->cfg_cyl) ? drive->cfg_cyl-1 : dev->cylinder;
				dev->head = dev->command[1] & 0x1f;
				dev->sector = dev->command[2] & 0x1f;
				dev->sector_count = dev->command[4];
				do {
					if (get_sector(dev, &addr)) {
#ifdef ENABLE_HDC_LOG
						hdc_log("get_sector failed\n");
#endif
						mfm_error(dev, dev->error);
						mfm_complete(dev);
						return;
					}

					next_sector(dev);

					dev->sector_count = (dev->sector_count-1) & 0xff;
				} while (dev->sector_count);

				mfm_complete(dev);

				ui_sb_update_icon(SB_HDD | HDD_BUS_MFM, 1);
				break;

			default:
				fatal("CMD_VERIFY_SECTORS: bad state %i\n", dev->state);
		}
		break;

	case CMD_FORMAT_TRACK:
		dev->cylinder = dev->command[3] | ((dev->command[2] & 0xc0) << 2);
		drive->current_cylinder = (dev->cylinder >= drive->cfg_cyl) ? drive->cfg_cyl-1 : dev->cylinder;
		dev->head = dev->command[1] & 0x1f;

		if (get_sector(dev, &addr)) {
#ifdef ENABLE_HDC_LOG
			hdc_log("get_sector failed\n");
#endif
			mfm_error(dev, dev->error);
			mfm_complete(dev);
			return;
		}

		hdd_image_zero(drive->hdd_num, addr, 17);
				
		mfm_complete(dev);
		break;			       

	case CMD_READ_SECTORS:		
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->cylinder = dev->command[3] | ((dev->command[2] & 0xc0) << 2);
				drive->current_cylinder = (dev->cylinder >= drive->cfg_cyl) ? drive->cfg_cyl-1 : dev->cylinder;
				dev->head = dev->command[1] & 0x1f;
				dev->sector = dev->command[2] & 0x1f;
				dev->sector_count = dev->command[4];
				dev->state = STATE_SEND_DATA;
				dev->data_pos = 0;
				dev->data_len = 512;

				if (get_sector(dev, &addr)) {
					mfm_error(dev, dev->error);
					mfm_complete(dev);
					return;
				}

				hdd_image_read(drive->hdd_num, addr, 1,
					       (uint8_t *) dev->sector_buf);
				ui_sb_update_icon(SB_HDD|HDD_BUS_MFM, 1);

				if (dev->irq_dma_mask & DMA_ENA)
					dev->callback = MFM_TIME;
				else {
					dev->status = STAT_BSY | STAT_IO | STAT_REQ;
					memcpy(dev->data, dev->sector_buf, 512);
				}
				break;

			case STATE_SEND_DATA:
				dev->status = STAT_BSY;
				if (dev->irq_dma_mask & DMA_ENA) {
					for (; dev->data_pos < 512; dev->data_pos++) {
						int val = dma_channel_write(3, dev->sector_buf[dev->data_pos]);

						if (val == DMA_NODATA) {
#ifdef ENABLE_HDC_LOG
							hdc_log("CMD_READ_SECTORS out of data!\n");
#endif
							dev->status = STAT_BSY | STAT_CD | STAT_IO | STAT_REQ;
							dev->callback = MFM_TIME;
							return;
						}
					}
					dev->state = STATE_SENT_DATA;
					dev->callback = MFM_TIME;
				} else
					fatal("Read sectors no DMA! - shouldn't get here\n");
				break;

			case STATE_SENT_DATA:
				next_sector(dev);

				dev->data_pos = 0;

				dev->sector_count = (dev->sector_count-1) & 0xff;

				if (dev->sector_count) {
					if (get_sector(dev, &addr)) {
						mfm_error(dev, dev->error);
						mfm_complete(dev);
						return;
					}

					hdd_image_read(drive->hdd_num, addr, 1,
						(uint8_t *) dev->sector_buf);
					ui_sb_update_icon(SB_HDD|HDD_BUS_MFM, 1);

					dev->state = STATE_SEND_DATA;

					if (dev->irq_dma_mask & DMA_ENA)
						dev->callback = MFM_TIME;
					else {
						dev->status = STAT_BSY | STAT_IO | STAT_REQ;
						memcpy(dev->data, dev->sector_buf, 512);
					}
				} else {
					mfm_complete(dev);
					ui_sb_update_icon(SB_HDD | HDD_BUS_MFM, 0);
				}
				break;

			default:
				fatal("CMD_READ_SECTORS: bad state %i\n", dev->state);
		}
		break;

	case CMD_WRITE_SECTORS:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->cylinder = dev->command[3] | ((dev->command[2] & 0xc0) << 2);
				drive->current_cylinder = (dev->cylinder >= drive->cfg_cyl) ? drive->cfg_cyl-1 : dev->cylinder;
				dev->head = dev->command[1] & 0x1f;
				dev->sector = dev->command[2] & 0x1f;
				dev->sector_count = dev->command[4];
				dev->state = STATE_RECEIVE_DATA;
				dev->data_pos = 0;
				dev->data_len = 512;
				if (dev->irq_dma_mask & DMA_ENA)
					dev->callback = MFM_TIME;
				else
					dev->status = STAT_BSY | STAT_REQ;
				break;

			case STATE_RECEIVE_DATA:
				dev->status = STAT_BSY;
				if (dev->irq_dma_mask & DMA_ENA) {
					for (; dev->data_pos < 512; dev->data_pos++) {
						int val = dma_channel_read(3);

						if (val == DMA_NODATA) {
#ifdef ENABLE_HDC_LOG
							hdc_log("CMD_WRITE_SECTORS out of data!\n");
#endif
							dev->status = STAT_BSY | STAT_CD | STAT_IO | STAT_REQ;
							dev->callback = MFM_TIME;
							return;
						}

						dev->sector_buf[dev->data_pos] = val & 0xff;
					}

					dev->state = STATE_RECEIVED_DATA;
					dev->callback = MFM_TIME;
				} else
					fatal("Write sectors no DMA! - should never get here\n");
				break;

			case STATE_RECEIVED_DATA:
				if (! (dev->irq_dma_mask & DMA_ENA))
					memcpy(dev->sector_buf, dev->data, 512);

				if (get_sector(dev, &addr)) {
					mfm_error(dev, dev->error);
					mfm_complete(dev);
					return;
				}

				hdd_image_write(drive->hdd_num, addr, 1,
						(uint8_t *) dev->sector_buf);
				ui_sb_update_icon(SB_HDD|HDD_BUS_MFM, 1);

				next_sector(dev);
				dev->data_pos = 0;
				dev->sector_count = (dev->sector_count-1) & 0xff;

				if (dev->sector_count) {
					dev->state = STATE_RECEIVE_DATA;
					if (dev->irq_dma_mask & DMA_ENA)
						dev->callback = MFM_TIME;
					else
						dev->status = STAT_BSY | STAT_REQ;
				} else
					mfm_complete(dev);
				break;

			default:
				fatal("CMD_WRITE_SECTORS: bad state %i\n", dev->state);
		}
		break;

	case CMD_SEEK:
		if (! drive->present)
			mfm_error(dev, ERR_NOT_READY);
		else {
			int cylinder = dev->command[3] | ((dev->command[2] & 0xc0) << 2);

			drive->current_cylinder = (cylinder >= drive->cfg_cyl) ? drive->cfg_cyl-1 : cylinder;

			if (cylinder != drive->current_cylinder)
				mfm_error(dev, ERR_SEEK_ERROR);
		}
		mfm_complete(dev);
		break;

	case CMD_INIT_DRIVE_PARAMS:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_RECEIVE_DATA;
				dev->data_pos = 0;
				dev->data_len = 8;
				dev->status = STAT_BSY | STAT_REQ;
				break;

			case STATE_RECEIVED_DATA:
				drive->cfg_cyl = dev->data[1] | (dev->data[0] << 8);
				drive->cfg_hpc = dev->data[2];
#ifdef ENABLE_HDC_LOG
				hdc_log("Drive %i: cylinders=%i, heads=%i\n", dev->drive_sel, drive->cfg_cyl, drive->cfg_hpc);
#endif
				mfm_complete(dev);
				break;

			default:
				fatal("CMD_INIT_DRIVE_PARAMS bad state %i\n", dev->state);
		}
		break;

	case CMD_WRITE_SECTOR_BUFFER:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_RECEIVE_DATA;
				dev->data_pos = 0;
				dev->data_len = 512;
				if (dev->irq_dma_mask & DMA_ENA)
					dev->callback = MFM_TIME;
				else
					dev->status = STAT_BSY | STAT_REQ;
				break;

			case STATE_RECEIVE_DATA:
				if (dev->irq_dma_mask & DMA_ENA) {
					dev->status = STAT_BSY;

					for (; dev->data_pos < 512; dev->data_pos++) {
						int val = dma_channel_read(3);

						if (val == DMA_NODATA) {
							hdc_log("CMD_WRITE_SECTOR_BUFFER out of data!\n");
							dev->status = STAT_BSY | STAT_CD | STAT_IO | STAT_REQ;
							dev->callback = MFM_TIME;
							return;
						}
					
						dev->data[dev->data_pos] = val & 0xff;
					}

					dev->state = STATE_RECEIVED_DATA;
					dev->callback = MFM_TIME;
				} else
					fatal("CMD_WRITE_SECTOR_BUFFER - should never get here!\n");
				break;

			case STATE_RECEIVED_DATA:
				memcpy(dev->sector_buf, dev->data, 512);
				mfm_complete(dev);
				break;

			default:
				fatal("CMD_WRITE_SECTOR_BUFFER bad state %i\n", dev->state);
		}
		break;

	case CMD_BUFFER_DIAGNOSTIC:
	case CMD_CONTROLLER_DIAGNOSTIC:
		mfm_complete(dev);
		break;

	case 0xfa:
		mfm_complete(dev);
		break;

	case CMD_DTC_SET_STEP_RATE:
		mfm_complete(dev);
		break;

	case CMD_DTC_GET_DRIVE_PARAMS:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_SEND_DATA;
				dev->data_pos = 0;
				dev->data_len = 4;
				dev->status = STAT_BSY | STAT_IO | STAT_REQ;
				memset(dev->data, 0, 4);
				dev->data[0] = drive->tracks & 0xff;
				dev->data[1] = 17 | ((drive->tracks >> 2) & 0xc0);
				dev->data[2] = drive->hpc-1;
#ifdef ENABLE_HDC_LOG
				hdc_log("Get drive params %02x %02x %02x %i\n", dev->data[0], dev->data[1], dev->data[2], drive->tracks);
#endif
				break;

			case STATE_SENT_DATA:
				mfm_complete(dev);
				break;

			default:
				fatal("CMD_INIT_DRIVE_PARAMS bad state %i\n", dev->state);
		}
		break;

	case CMD_DTC_GET_GEOMETRY:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_SEND_DATA;
				dev->data_pos = 0;
				dev->data_len = 16;
				dev->status = STAT_BSY | STAT_IO | STAT_REQ;
				memset(dev->data, 0, 16);
				dev->data[0x4] = drive->tracks & 0xff;
				dev->data[0x5] = (drive->tracks >> 8) & 0xff;
				dev->data[0xa] = drive->hpc;
				break;

			case STATE_SENT_DATA:
				mfm_complete(dev);
				break;
		}
		break;

	case CMD_DTC_SET_GEOMETRY:
		switch (dev->state) {
			case STATE_START_COMMAND:
				dev->state = STATE_RECEIVE_DATA;
				dev->data_pos = 0;
				dev->data_len = 16;
				dev->status = STAT_BSY | STAT_REQ;
				break;

			case STATE_RECEIVED_DATA:
				/*Bit of a cheat here - we always report the actual geometry of the drive in use*/
				mfm_complete(dev);
			break;
		}
		break;

	default:
		fatal("Unknown Xebec command - %02x %02x %02x %02x %02x %02x\n",
			dev->command[0], dev->command[1],
			dev->command[2], dev->command[3],
			dev->command[4], dev->command[5]);
    }
}


static void
loadhd(mfm_t *dev, int c, int d, const wchar_t *fn)
{
    drive_t *drive = &dev->drives[d];

    if (! hdd_image_load(d)) {
	drive->present = 0;
	return;
    }
	
    drive->spt = (uint8_t)hdd[c].spt;
    drive->hpc = (uint8_t)hdd[c].hpc;
    drive->tracks = (uint16_t)hdd[c].tracks;
    drive->hdd_num = c;
    drive->present = 1;
}


static const struct {
    uint16_t	tracks;
    uint8_t	hpc;
} hd_types[4] = {
    { 306, 4 },	/* Type 0 */
    { 612, 4 }, /* Type 16 */
    { 615, 4 }, /* Type 2 */
    { 306, 8 }  /* Type 13 */
};


static void
mfm_set_switches(mfm_t *dev)
{
    int c, d;
	
    dev->switches = 0;
	
    for (d=0; d<2; d++) {
	drive_t *drive = &dev->drives[d];

	if (! drive->present) continue;

	for (c=0; c<4; c++) {
		if (drive->spt == 17 &&
		    drive->hpc == hd_types[c].hpc &&
		    drive->tracks == hd_types[c].tracks) {
			dev->switches |= (c << (d ? 0 : 2));
			break;
		}
	}

#ifdef ENABLE_HDC_LOG
	if (c == 4)
		hdc_log("WARNING: drive %d has unsupported format %d/%d/%d !\n",
			d, drive->tracks, drive->hpc, drive->spt);
#endif
    }
}


static void *
mfm_init(const device_t *info)
{
    wchar_t *fn = NULL;
    mfm_t *dev;
    int i, c;

    dev = malloc(sizeof(mfm_t));
    memset(dev, 0x00, sizeof(mfm_t));

#ifdef ENABLE_HDC_LOG
    hdc_log("MFM: looking for disks..\n");
#endif
    c = 0;
    for (i=0; i<HDD_NUM; i++) {
	if ((hdd[i].bus == HDD_BUS_MFM) && (hdd[i].mfm_channel < MFM_NUM)) {
#ifdef ENABLE_HDC_LOG
		hdc_log("Found MFM hard disk on channel %i\n", hdd[i].mfm_channel);
#endif
		loadhd(dev, i, hdd[i].mfm_channel, hdd[i].fn);

		if (++c > MFM_NUM) break;
	}
    }
#ifdef ENABLE_HDC_LOG
    hdc_log("MFM: %d disks loaded.\n", c);
#endif

    switch(info->local) {
	case 0:		/* Xebec */
		fn = XEBEC_BIOS_FILE;
		mfm_set_switches(dev);
		break;

	case 1:		/* DTC5150 */
		fn = DTC_BIOS_FILE;
		dev->switches = 0xff;
		dev->drives[0].cfg_cyl = dev->drives[0].tracks;
		dev->drives[0].cfg_hpc = dev->drives[0].hpc;
		dev->drives[1].cfg_cyl = dev->drives[1].tracks;
		dev->drives[1].cfg_hpc = dev->drives[1].hpc;
		break;
    }

    rom_init(&dev->bios_rom, fn,
	     0xc8000, 0x4000, 0x3fff, 0, MEM_MAPPING_EXTERNAL);
		
    io_sethandler(0x0320, 4,
		  mfm_read,NULL,NULL, mfm_write,NULL,NULL, dev);

    timer_add(mfm_callback, &dev->callback, &dev->callback, dev);

    return(dev);
}


static void
mfm_close(void *priv)
{
    mfm_t *dev = (mfm_t *)priv;
    int d;

    for (d=0; d<2; d++) {
	drive_t *drive = &dev->drives[d];

	hdd_image_close(drive->hdd_num);
    }

    free(dev);
}


static int
xebec_available(void)
{
    return(rom_present(XEBEC_BIOS_FILE));
}


static int
dtc5150x_available(void)
{
    return(rom_present(DTC_BIOS_FILE));
}


const device_t mfm_xt_xebec_device = {
    "IBM PC Fixed Disk Adapter",
    DEVICE_ISA,
    0,
    mfm_init, mfm_close, NULL,
    xebec_available, NULL, NULL, NULL,
    NULL
};

const device_t mfm_xt_dtc5150x_device = {
    "DTC 5150X",
    DEVICE_ISA,
    1,
    mfm_init, mfm_close, NULL,
    dtc5150x_available, NULL, NULL, NULL,
    NULL
};
