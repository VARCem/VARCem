/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the AHA-154x series of SCSI Host Adapters
 *		made by Adaptec, Inc. These controllers were designed for
 *		the ISA bus.
 *
 * Version:	@(#)scsi_aha154x.c	1.0.17	2020/06/01
 *
 *		Based on original code from TheCollector1995 and Miran Grca.
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#define dbglog	scsi_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "../system/dma.h"
#include "../system/pic.h"
#include "../system/mca.h"
#include "scsi.h"
#include "scsi_aha154x.h"
#include "scsi_device.h"
#include "scsi_x54x.h"


#define AHA1540A_BIOS_PATH	L"scsi/adaptec/aha1540a307.bin"
#define AHA1540B_330_BIOS_PATH	L"scsi/adaptec/aha1540b320_330.bin"
#define AHA1540B_334_BIOS_PATH	L"scsi/adaptec/aha1540b320_334.bin"
#define AHA1540C_BIOS_PATH	L"scsi/adaptec/aha1542c102.bin"
#define AHA1540CF_BIOS_PATH	L"scsi/adaptec/aha1542cf211.bin"
#define AHA1540CP_BIOS_PATH	L"scsi/adaptec/aha1542cp102.bin"
#define AHA1640_BIOS_PATH	L"scsi/adaptec/aha1640.bin"


enum {
    AHA_154xA,
    AHA_154xB,
    AHA_154xC,
    AHA_154xCF,
    AHA_154xCP,
    AHA_1640
};


#define CMD_WR_EEPROM	0x22		/* UNDOC: Write EEPROM */
#define CMD_RD_EEPROM	0x23		/* UNDOC: Read EEPROM */
#define CMD_SHADOW_RAM	0x24		/* UNDOC: BIOS shadow ram */
#define CMD_BIOS_MBINIT	0x25		/* UNDOC: BIOS mailbox initialization */
#define CMD_MEM_MAP_1	0x26		/* UNDOC: Memory Mapper */
#define CMD_MEM_MAP_2	0x27		/* UNDOC: Memory Mapper */
#define CMD_EXTBIOS     0x28		/* UNDOC: return extended BIOS info */
#define CMD_MBENABLE    0x29		/* set mailbox interface enable */
#define CMD_BIOS_SCSI	0x82		/* start ROM BIOS SCSI command */


uint16_t	aha_ports[] = {
    0x0330, 0x0334, 0x0230, 0x0234,
    0x0130, 0x0134, 0x0000, 0x0000
};


#pragma pack(push,1)
typedef struct {
    uint8_t	CustomerSignature[20];
    uint8_t	uAutoRetry;
    uint8_t	uBoardSwitches;
    uint8_t	uChecksum;
    uint8_t	uUnknown;
    addr24	BIOSMailboxAddress;
} aha_setup_t;
#pragma pack(pop)


/*
 * Write data to the BIOS space.
 *
 * AHA-1542C's and up have a feature where they map a 128-byte
 * RAM space into the ROM BIOS' address space, and then use it
 * as working memory. This function implements the writing to
 * that memory.
 *
 * We enable/disable this memory through AHA command 0x24.
 */
static void
mem_write(uint32_t addr, uint8_t val, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    addr &= 0x3fff;

    if ((addr >= dev->rom_shram) && (dev->shram_mode & 1))
	dev->shadow_ram[addr & (dev->rom_shramsz - 1)] = val;
}


static uint8_t
mem_read(uint32_t addr, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    rom_t *ptr = &dev->bios;

    addr &= 0x3fff;

    if ((addr >= dev->rom_shram) && (dev->shram_mode & 2))
	return dev->shadow_ram[addr & (dev->rom_shramsz - 1)];

    return(ptr->rom[addr]);
}


static uint8_t
shram_cmd(x54x_t *dev, uint8_t cmd)
{
    /* If not supported, give up. */
    if (dev->rom_shram == 0x0000) return(0x04);

    /* Bit 0 = Shadow RAM write enable;
       Bit 1 = Shadow RAM read enable. */
    dev->shram_mode = cmd;

    /* Firmware expects 04 status. */
    return(0x04);
}


static void
eeprom_save(x54x_t *dev)
{
    FILE *fp;

    fp = plat_fopen(nvr_path(dev->nvr_path), L"wb");
    if (fp != NULL) {
	fwrite(dev->nvr, 1, NVR_SIZE, fp);
	fclose(fp);
    }
}


static uint8_t
eeprom_cmd(x54x_t *dev, uint8_t cmd,uint8_t arg,uint8_t len,uint8_t off,uint8_t *bufp)
{
    uint8_t r = 0xff;
    int c;

    DEBUG("%s: EEPROM cmd=%02x, arg=%02x len=%d, off=%02x\n",
				dev->name, cmd, arg, len, off);

    /* Only if we can handle it.. */
    if (dev->nvr == NULL) return(r);

    if (cmd == 0x22) {
	/* Write data to the EEPROM. */
	for (c = 0; c < len; c++)
		dev->nvr[(off + c) & 0xff] = bufp[c];
	r = 0;

	eeprom_save(dev);
    }

    if (cmd == 0x23) {
	/* Read data from the EEPROM. */
	for (c = 0; c < len; c++)
		bufp[c] = dev->nvr[(off + c) & 0xff];
	r = len;
    }

    return(r);
}


/* Map either the main or utility (Select) ROM into the memory space. */
static uint8_t
mmap_cmd(x54x_t *dev, uint8_t cmd)
{
    DEBUG("%s: MEMORY cmd=%02x\n", dev->name, cmd);

    switch(cmd) {
	case 0x26:
		/* Disable the mapper, so, set ROM1 active. */
		dev->bios.rom = dev->rom1;
		break;

	case 0x27:
		/* Enable the mapper, so, set ROM2 active. */
		dev->bios.rom = dev->rom2;
		break;
    }

    return(0);
}


static uint8_t
get_host_id(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return(dev->nvr[0] & 0x07);
}


static uint8_t
get_irq(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return((dev->nvr[1] & 0x07) + 9);
}


static uint8_t
get_dma(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return((dev->nvr[1] >> 4) & 0x07);
}


static uint8_t
cmd_is_fast(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    if (dev->Command == CMD_BIOS_SCSI)
	return(1);

    return(0);
}


static uint8_t
fast_cmds(void *priv, uint8_t cmd)
{
    x54x_t *dev = (x54x_t *)priv;

    if (cmd == CMD_BIOS_SCSI) {
	dev->BIOSMailboxReq++;
	return(1);
    }

    return(0);
}


static uint8_t
param_len(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    switch (dev->Command) {
	case CMD_BIOS_MBINIT:
		/* Same as 0x01 for AHA. */
		return(sizeof(MailboxInit_t));
		break;

	case CMD_SHADOW_RAM:
		return(1);
		break;	

	case CMD_WR_EEPROM:
		return(35);
		break;

	case CMD_RD_EEPROM:
		return(3);

	case CMD_MBENABLE:
		return(2);

	default:
		break;
    }

    return(0);
}


static uint8_t
aha_cmds(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    MailboxInit_t *mbi;

    if (dev->CmdParamLeft) return(0);

    DEBUG("Running Operation Code 0x%02X\n", dev->Command);
    switch (dev->Command) {
	case CMD_WR_EEPROM:	/* write EEPROM */
		/* Sent by CF BIOS. */
		dev->DataReplyLeft = eeprom_cmd(dev,
						dev->Command,
						dev->CmdBuf[0],
						dev->CmdBuf[1],
						dev->CmdBuf[2],
						&(dev->CmdBuf[3]));
		if (dev->DataReplyLeft == 0xff) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
		}
		break;

	case CMD_RD_EEPROM: /* read EEPROM */
		/* Sent by CF BIOS. */
		dev->DataReplyLeft = eeprom_cmd(dev,
						dev->Command,
						dev->CmdBuf[0],
						dev->CmdBuf[1],
						dev->CmdBuf[2],
						dev->DataBuf);
		if (dev->DataReplyLeft == 0xff) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
		}
		break;

	case CMD_SHADOW_RAM: /* Shadow RAM */
		/*
		 * For AHA1542CF, this is the command
		 * to play with the Shadow RAM.  BIOS
		 * gives us one argument (00,02,03)
		 * and expects a 0x04 back in the INTR
		 * register.  --FvK
		 */
		if (dev->rom_shramsz > 0)
			dev->Interrupt = shram_cmd(dev, dev->CmdBuf[0]);
		break;

	case CMD_BIOS_MBINIT: /* BIOS Mailbox Initialization */
		/* Sent by CF BIOS. */
		//dev->Mbx24bit = 1;
                dev->flags |= X54X_MBX_24BIT;

		mbi = (MailboxInit_t *)dev->CmdBuf;

		dev->BIOSMailboxInit = 1;
		dev->BIOSMailboxCount = mbi->Count;
		dev->BIOSMailboxOutAddr = ADDR_TO_U32(mbi->Address);

		DEBUG("Initialize BIOS Mailbox: MBO=0x%08lx, %d entries at 0x%08lx\n",
		      dev->BIOSMailboxOutAddr, mbi->Count, ADDR_TO_U32(mbi->Address));

		dev->Status &= ~STAT_INIT;
		dev->DataReplyLeft = 0;
		break;

	case CMD_MEM_MAP_1:	/* AHA memory mapper */
	case CMD_MEM_MAP_2:	/* AHA memory mapper */
		/* Sent by CF BIOS. */
		dev->DataReplyLeft = mmap_cmd(dev, dev->Command);
		break;

	case CMD_EXTBIOS: /* Return extended BIOS information */
		dev->DataBuf[0] = 0x08;
		dev->DataBuf[1] = dev->Lock;
		dev->DataReplyLeft = 2;
		break;
					
	case CMD_MBENABLE: /* Mailbox interface enable Command */
		dev->DataReplyLeft = 0;
		if (dev->CmdBuf[1] == dev->Lock) {
			if (dev->CmdBuf[0] & 1)
				dev->Lock = 1;
			  else
				dev->Lock = 0;
		}
		break;

	case 0x2c:	/* AHA-1542CP sends this */
		dev->DataBuf[0] = 0x00;
		dev->DataReplyLeft = 1;
		break;

	case 0x33:	/* AHA-1542CP sends this */
		dev->DataBuf[0] = 0x00;
		dev->DataBuf[1] = 0x00;
		dev->DataBuf[2] = 0x00;
		dev->DataBuf[3] = 0x00;
		dev->DataReplyLeft = 256;
		break;

	default:
		dev->DataReplyLeft = 0;
		dev->Status |= STAT_INVCMD;
		break;
    }

    return(0);
}


static void
setup_data(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    ReplyInquireSetupInformation *ReplyISI;
    aha_setup_t *aha_setup;

    ReplyISI = (ReplyInquireSetupInformation *)dev->DataBuf;
    aha_setup = (aha_setup_t *)ReplyISI->VendorSpecificData;

    ReplyISI->fSynchronousInitiationEnabled = dev->sync & 1;
    ReplyISI->fParityCheckingEnabled = dev->parity & 1;

    U32_TO_ADDR(aha_setup->BIOSMailboxAddress, dev->BIOSMailboxOutAddr);
    aha_setup->uChecksum = 0xa3;
    aha_setup->uUnknown = 0xc2;
}


static void
do_bios_mail(x54x_t *dev)
{
    dev->MailboxIsBIOS = 1;

    if (! dev->BIOSMailboxCount) {
	DEBUG("aha_do_bios_mail(): No BIOS Mailboxes\n");
	return;
    }

    /* Search for a filled mailbox - stop if we have scanned all mailboxes. */
    for (dev->BIOSMailboxOutPosCur = 0; dev->BIOSMailboxOutPosCur < dev->BIOSMailboxCount; dev->BIOSMailboxOutPosCur++) {
	if (x54x_mbo_process(dev))
		break;
    }
}


static void
call_back(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    if (dev->BIOSMailboxInit && dev->BIOSMailboxReq)
	do_bios_mail(dev);
}


static uint8_t
aha_mca_read(int port, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return(dev->pos_regs[port & 7]);
}


static void
aha_mca_write(int port, uint8_t val, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    /* MCA does not write registers below 0x0100. */
    if (port < 0x0102) return;

    /* Save the MCA register value. */
    dev->pos_regs[port & 7] = val;

    /* This is always necessary so that the old handler doesn't remain. */
    x54x_io_remove(dev, dev->Base, 4);

    /* Get the new assigned I/O base address. */
    dev->Base = (dev->pos_regs[3] & 7) << 8;
    dev->Base |= ((dev->pos_regs[3] & 0xc0) ? 0x34 : 0x30);

    /* Save the new IRQ and DMA channel values. */
    dev->Irq = (dev->pos_regs[4] & 0x07) + 8;
    dev->DmaChannel = dev->pos_regs[5] & 0x0f;	

    /* Extract the BIOS ROM address info. */
    if (! (dev->pos_regs[2] & 0x80)) switch(dev->pos_regs[3] & 0x38) {
	case 0x38:		/* [1]=xx11 1xxx */
		dev->rom_addr = 0xdc000;
		break;

	case 0x30:		/* [1]=xx11 0xxx */
		dev->rom_addr = 0xd8000;
		break;

	case 0x28:		/* [1]=xx10 1xxx */
		dev->rom_addr = 0xd4000;
		break;

	case 0x20:		/* [1]=xx10 0xxx */
		dev->rom_addr = 0xd0000;
		break;

	case 0x18:		/* [1]=xx01 1xxx */
		dev->rom_addr = 0xcc000;
		break;

	case 0x10:		/* [1]=xx01 0xxx */
		dev->rom_addr = 0xc8000;
		break;
    } else {
	/* Disabled. */
	dev->rom_addr = 0x000000;
    }

    /*
     * Get misc SCSI config stuff.  For now, we are only
     * interested in the configured HA target ID:
     *
     *  pos[2]=111xxxxx = 7
     *  pos[2]=000xxxxx = 0
     */
    dev->HostID = (dev->pos_regs[4] >> 5) & 0x07;

    /*
     * SYNC mode is pos[2]=xxxx1xxx.
     *
     * SCSI Parity is pos[2]=xxx1xxxx.
     */
    dev->sync = (dev->pos_regs[4] >> 3) & 1;
    dev->parity = (dev->pos_regs[4] >> 4) & 1;

    /*
     * The PS/2 Model 80 BIOS always enables a card if it finds one,
     * even if no resources were assigned yet (because we only added
     * the card, but have not run AutoConfig yet...)
     *
     * So, remove current address, if any.
     */
    mem_map_disable(&dev->bios.mapping);

    /* Initialize the device if fully configured. */
    if (dev->pos_regs[2] & 0x01) {
	/* Card enabled; register (new) I/O handler. */
	x54x_io_set(dev, dev->Base, 4);

	/* Reset the device. */
	x54x_reset_ctrl(dev, CTRL_HRST);

	/* Enable or disable the BIOS ROM. */
	if (dev->rom_addr != 0x000000) {
		mem_map_enable(&dev->bios.mapping);
		mem_map_set_addr(&dev->bios.mapping, dev->rom_addr, ROM_SIZE);
	}

	/* Say hello. */
	INFO("%s: I/O=%04x, IRQ=%d, DMA=%d, BIOS @%05X, HOST ID %i\n",
	     dev->name, dev->Base, dev->Irq, dev->DmaChannel,
	     dev->rom_addr, dev->HostID);
    }
}

#if 0
static uint8_t
aha_mca_feedb(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return (dev->pos_regs[2] & 0x01);
}
#endif

/* Initialize the board's ROM BIOS. */
static void
set_bios(x54x_t *dev)
{
    uint32_t size;
    uint32_t mask;
    uint32_t temp;
    FILE *fp;
    int i;

    /* Only if this device has a BIOS ROM. */
    if (dev->bios_path == NULL) return;

    /* Open the BIOS image file and make sure it exists. */
    DEBUG("%s: loading BIOS from '%ls'\n", dev->name, dev->bios_path);
    if ((fp = rom_fopen(dev->bios_path, L"rb")) == NULL) {
	ERRLOG("%s: BIOS ROM not found!\n", dev->name);
	return;
    }

    /*
     * Manually load and process the ROM image.
     *
     * We *could* use the system "rom_init" function here, but for
     * this special case, we can't: we may need WRITE access to the
     * memory later on.
     */
    (void)fseek(fp, 0L, SEEK_END);
    temp = ftell(fp);
    (void)fseek(fp, 0L, SEEK_SET);

    /* Load first chunk of BIOS (which is the main BIOS, aka ROM1.) */
    dev->rom1 = (uint8_t *)mem_alloc(ROM_SIZE);
    (void)fread(dev->rom1, ROM_SIZE, 1, fp);
    temp -= ROM_SIZE;
    if (temp > 0) {
	dev->rom2 = (uint8_t *)mem_alloc(ROM_SIZE);
	(void)fread(dev->rom2, ROM_SIZE, 1, fp);
	temp -= ROM_SIZE;
    } else {
	dev->rom2 = NULL;
    }
    if (temp != 0) {
	ERRLOG("%s: BIOS ROM size invalid!\n", dev->name);
	free(dev->rom1);
	if (dev->rom2 != NULL)
		free(dev->rom2);
	(void)fclose(fp);
	return;
    }
    temp = ftell(fp);
    if (temp > ROM_SIZE)
	temp = ROM_SIZE;
    (void)fclose(fp);

    /* Adjust BIOS size in chunks of 2K, as per BIOS spec. */
    size = 0x10000;
    if (temp <= 0x8000)
	size = 0x8000;
    if (temp <= 0x4000)
	size = 0x4000;
    if (temp <= 0x2000)
	size = 0x2000;
    mask = (size - 1);
    INFO("%s: BIOS at 0x%06lX, size %lu, mask %08lx\n",
         dev->name, dev->rom_addr, size, mask);

    /* Initialize the ROM entry for this BIOS. */
    memset(&dev->bios, 0x00, sizeof(rom_t));

    /* Enable ROM1 into the memory map. */
    dev->bios.rom = dev->rom1;

    /* Set up an address mask for this memory. */
    dev->bios.mask = mask;

    /* Map this system into the memory map. */
    mem_map_add(&dev->bios.mapping, dev->rom_addr, size,
		mem_read, NULL, NULL, /* aha_mem_readw, aha_mem_readl, */
		mem_write, NULL, NULL,
		dev->bios.rom, MEM_MAPPING_EXTERNAL, dev);
    mem_map_disable(&dev->bios.mapping);

    /*
     * Patch the ROM BIOS image for stuff Adaptec deliberately
     * made hard to understand. Well, maybe not, maybe it was
     * their way of handling issues like these at the time..
     *
     * Patch 1: emulate the I/O ADDR SW setting by patching a
     *	    byte in the BIOS that indicates the I/O ADDR
     *	    switch setting on the board.
     */
    if (dev->rom_ioaddr != 0x0000) {
	/* Look up the I/O address in the table. */
	for (i = 0; i < 8; i++)
		if (aha_ports[i] == dev->Base) break;
	if (i == 8) {
		ERRLOG("%s: invalid I/O address %04x selected!\n",
					dev->name, dev->Base);
		return;
	}
	dev->bios.rom[dev->rom_ioaddr] = (uint8_t)i;

	/* Negation of the DIP switches to satify the checksum. */
	dev->bios.rom[dev->rom_ioaddr + 1] = (uint8_t)((i ^ 0xff) + 1);
    }
}


static void
init_nvr(x54x_t *dev)
{
    /* Initialize the on-board EEPROM. */
    dev->nvr[0] = dev->HostID;			/* SCSI ID 7 */
    dev->nvr[0] |= (0x10 | 0x20 | 0x40);
    dev->nvr[1] = dev->Irq-9;			/* IRQ15 */
    dev->nvr[1] |= (dev->DmaChannel<<4);	/* DMA6 */
    dev->nvr[2] = (EE2_HABIOS	| 		/* BIOS enabled		*/
		   EE2_DYNSCAN	|		/* scan bus		*/
		   EE2_EXT1G | EE2_RMVOK);	/* Imm return on seek	*/
    dev->nvr[3] = SPEED_50;			/* speed 5.0 MB/s	*/
    dev->nvr[6] = (EE6_TERM	|		/* host term enable	*/
		   EE6_RSTBUS);			/* reset SCSI bus on boot*/
}


/* Initialize the board's EEPROM (NVR.) */
static void
set_nvr(x54x_t *dev)
{
    FILE *fp;

    /* Only if this device has an EEPROM. */
    if (dev->nvr_path == NULL) return;

    /* Allocate and initialize the EEPROM. */
    dev->nvr = (uint8_t *)mem_alloc(NVR_SIZE);
    memset(dev->nvr, 0x00, NVR_SIZE);

    fp = plat_fopen(nvr_path(dev->nvr_path), L"rb");
    if (fp != NULL) {
	(void)fread(dev->nvr, 1, NVR_SIZE, fp);
	fclose(fp);
    } else {
	init_nvr(dev);
    }
}


/* General initialization routine for all boards. */
static void *
aha_init(const device_t *info, UNUSED(void *parent))
{
    x54x_t *dev;

    /* Call common initializer. */
    dev = (x54x_t *)x54x_init(info);
    dev->bios_path = info->path;

    /*
     * Set up the (initial) I/O address, IRQ and DMA info.
     *
     * Note that on MCA, configuration is handled by the BIOS,
     * and so any info we get here will be overwritten by the
     * MCA-assigned values later on!
     */
    dev->Base = device_get_config_hex16("base");
    dev->Irq = device_get_config_int("irq");
    dev->DmaChannel = device_get_config_int("dma");
    dev->rom_addr = device_get_config_hex20("bios_addr");
    dev->HostID = 7;		/* default HA ID */
    dev->setup_info_len = sizeof(aha_setup_t);
    dev->max_id = 7;
    dev->flags = 0;

    dev->ven_callback = call_back;
    dev->ven_cmd_is_fast = cmd_is_fast;
    dev->ven_fast_cmds = fast_cmds;
    dev->get_ven_param_len = param_len;
    dev->ven_cmds = aha_cmds;
    dev->get_ven_data = setup_data;

    strcpy(dev->vendor, "Adaptec");

    /* Perform per-board initialization. */
    switch(dev->type) {
        case AHA_154xA:
		strcpy(dev->name, "AHA-154xA");
		dev->fw_rev = "A003";	/* The 3.07 microcode says A006. */
		//dev->bios_path = L"roms/scsi/adaptec/aha1540a307.bin"; /*Only for port 0x330*/
		/* This is configurable from the configuration for the 154xB, the rest of the controllers read it from the EEPROM. */
		dev->HostID = device_get_config_int("hostid");
		dev->rom_shram = 0x3F80;	/* shadow RAM address base */
		dev->rom_shramsz = 128;		/* size of shadow RAM */
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		break;
	case AHA_154xB:
		strcpy(dev->name, "AHA-154xB");
		switch(dev->Base) {
			case 0x0330:
				dev->bios_path = AHA1540B_330_BIOS_PATH;
				break;

			case 0x0334:
				dev->bios_path = AHA1540B_334_BIOS_PATH;
				break;
		}
		dev->fw_rev = "A005";	/* The 3.2 microcode says A012. */
		dev->HostID = device_get_config_int("hostid");
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		break;

	case AHA_154xC:
		strcpy(dev->name, "AHA-154xC");
		dev->nvr_path = L"aha1542c.nvr";
		dev->fw_rev = "D001";
		dev->rom_shram = 0x3f80;	/* shadow RAM address base */
		dev->rom_shramsz = 128;		/* size of shadow RAM */
		dev->rom_ioaddr = 0x3f7e;	/* [2:0] idx into addr table */
		dev->rom_fwhigh = 0x0022;	/* firmware version (hi/lo) */
                dev->flags |= X54X_CDROM_BOOT;
		dev->ven_get_host_id = get_host_id;	/* function to return host ID from EEPROM */
		dev->ven_get_irq = get_irq;	/* function to return IRQ from EEPROM */
		dev->ven_get_dma = get_dma;	/* function to return DMA channel from EEPROM */
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		break;

	case AHA_154xCF:
		strcpy(dev->name, "AHA-154xCF");
		dev->nvr_path = L"aha1542cf.nvr";
		dev->fw_rev = "E001";
		dev->rom_shram = 0x3f80;	/* shadow RAM address base */
		dev->rom_shramsz = 128;		/* size of shadow RAM */
		dev->rom_ioaddr = 0x3f7e;	/* [2:0] idx into addr table */
		dev->rom_fwhigh = 0x0022;	/* firmware version (hi/lo) */
    		dev->cdrom_boot = 1;
		dev->ven_get_host_id = get_host_id;	/* function to return host ID from EEPROM */
		dev->ven_get_irq = get_irq;	/* function to return IRQ from EEPROM */
		dev->ven_get_dma = get_dma;	/* function to return DMA channel from EEPROM */
		dev->ha_bps = 10000000.0;	/* fast SCSI */
		break;

	case AHA_154xCP:
		strcpy(dev->name, "AHA-154xCP");
		dev->nvr_path = L"aha1540cp.nvr";
		dev->fw_rev = "F001";
		dev->rom_shram = 0x3f80;	/* shadow RAM address base */
		dev->rom_shramsz = 128;		/* size of shadow RAM */
		dev->rom_ioaddr = 0x3f7e;	/* [2:0] idx into addr table */
		dev->rom_fwhigh = 0x0055;	/* firmware version (hi/lo) */
		dev->ven_get_host_id = get_host_id;	/* function to return host ID from EEPROM */
		dev->ven_get_irq = get_irq;	/* function to return IRQ from EEPROM */
		dev->ven_get_dma = get_dma;	/* function to return DMA channel from EEPROM */
		dev->ha_bps = 10000000.0;	/* fast SCSI */
		break;

	case AHA_1640:
		strcpy(dev->name, "AHA-1640");
		dev->fw_rev = "BB01";

		dev->flags |= X54X_LBA_BIOS;

		/* Enable MCA. */
		dev->pos_regs[0] = 0x1f;	/* MCA board ID */
		dev->pos_regs[1] = 0x0f;	
		//mca_add(aha_mca_read, aha_mca_write, aha_mca_feedb, dev);
		mca_add(aha_mca_read, aha_mca_write, dev);
                dev->ha_bps = 5000000.0;	/* normal SCSI */
		break;
    }	

    /* Initialize ROM BIOS if needed. */
    set_bios(dev);

    /* Initialize EEPROM (NVR) if needed. */
    set_nvr(dev);

    if (dev->Base != 0) {
	/* Initialize the device. */
	x54x_device_reset(dev);

        if (!(dev->bus & DEVICE_MCA)) {
		/* Register our address space. */
	        x54x_io_set(dev, dev->Base, 4);

		/* Enable the memory. */
		if (dev->rom_addr != 0x000000) {
			mem_map_enable(&dev->bios.mapping);
			mem_map_set_addr(&dev->bios.mapping, dev->rom_addr, ROM_SIZE);
		}
	}
    }

    return(dev);
}


static const device_config_t aha_154xb_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x334,
        {
                {
                        "None",      0
                },
                {
                        "330H", 0x330
                },
                {
                        "334H", 0x334
                },
                {
                        "230H", 0x230
                },
                {
                        "234H", 0x234
                },
                {
                        "130H", 0x130
                },
                {
                        "134H", 0x134
                },
                {
                        NULL
                }
        }
    },
    {
        "irq", "IRQ", CONFIG_SELECTION, "", 9,
        {
                {
                        "IRQ 9", 9
                },
                {
                        "IRQ 10", 10
                },
                {
                        "IRQ 11", 11
                },
                {
                        "IRQ 12", 12
                },
                {
                        "IRQ 14", 14
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
        "dma", "DMA channel", CONFIG_SELECTION, "", 6,
        {
                {
                        "DMA 5", 5
                },
                {
                        "DMA 6", 6
                },
                {
                        "DMA 7", 7
                },
                {
                        NULL
                }
        }
    },
    {
        "hostid", "Host ID", CONFIG_SELECTION, "", 7,
        {
                {
                        "0", 0
                },
                {
                        "1", 1
                },
                {
                        "2", 2
                },
                {
                        "3", 3
                },
                {
                        "4", 4
                },
                {
                        "5", 5
                },
                {
                        "6", 6
                },
                {
                        "7", 7
                },
                {
                        NULL
                }
        }
    },
    {
        "bios_addr", "BIOS Address", CONFIG_HEX20, "", 0,
        {
                {
                        "Disabled", 0
                },
                {
                        "C800H", 0xc8000
                },
                {
                        "D000H", 0xd0000
                },
                {
                        "D800H", 0xd8000
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


static const device_config_t aha_154x_config[] = {
    {
        "base", "Address", CONFIG_HEX16, "", 0x334,
        {
                {
                        "None",      0
                },
                {
                        "330H", 0x330
                },
                {
                        "334H", 0x334
                },
                {
                        "230H", 0x230
                },
                {
                        "234H", 0x234
                },
                {
                        "130H", 0x130
                },
                {
                        "134H", 0x134
                },
                {
                        NULL
                }
        }
    },
    {
        "irq", "IRQ", CONFIG_SELECTION, "", 9,
        {
                {
                        "IRQ 9", 9
                },
                {
                        "IRQ 10", 10
                },
                {
                        "IRQ 11", 11
                },
                {
                        "IRQ 12", 12
                },
                {
                        "IRQ 14", 14
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
        "dma", "DMA channel", CONFIG_SELECTION, "", 6,
        {
                {
                        "DMA 5", 5
                },
                {
                        "DMA 6", 6
                },
                {
                        "DMA 7", 7
                },
                {
                        NULL
                }
        }
    },
    {
        "bios_addr", "BIOS Address", CONFIG_HEX20, "", 0,
        {
                {
                        "Disabled", 0
                },
                {
                        "C800H", 0xc8000
                },
                {
                        "D000H", 0xd0000
                },
                {
                        "D800H", 0xd8000
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

const device_t aha1540a_device = {
    "Adaptec AHA-1540A",
    DEVICE_ISA | DEVICE_AT,
    AHA_154xA,
    AHA1540A_BIOS_PATH,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    aha_154xb_config
};

const device_t aha1540b_device = {
    "Adaptec AHA-1540B",
    DEVICE_ISA | DEVICE_AT,
    AHA_154xB,
    NULL,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    aha_154xb_config
};

const device_t aha1542c_device = {
    "Adaptec AHA-1542C",
    DEVICE_ISA | DEVICE_AT,
    AHA_154xC,
    AHA1540C_BIOS_PATH,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    aha_154x_config
};

const device_t aha1542cf_device = {
    "Adaptec AHA-1542CF",
    DEVICE_ISA | DEVICE_AT,
    AHA_154xCF,
    AHA1540CF_BIOS_PATH,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    aha_154x_config
};

const device_t aha1542cp_device = {
    "Adaptec AHA-1542CP",
    DEVICE_ISA | DEVICE_AT,
    AHA_154xCP,
    AHA1540CP_BIOS_PATH,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    aha_154x_config
};

const device_t aha1640_device = {
    "Adaptec AHA-1640",
    DEVICE_MCA,
    AHA_1640,
    AHA1640_BIOS_PATH,
    aha_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
