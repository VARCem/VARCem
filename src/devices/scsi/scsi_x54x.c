/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the code common to the AHA-154x series of
 *		SCSI Host Adapters made by Adaptec, Inc. and the BusLogic
 *		series of SCSI Host Adapters made by Buslogic (now Mylex.)
 *
 *		These controllers were designed for various buses.
 *
 * Version:	@(#)scsi_x54x.c	1.0.22	2021/11/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#define dbglog scsi_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../system/dma.h"
#include "../system/pic.h"
#include "../system/pci.h"
#include "../system/mca.h"
#include "scsi.h"
#include "scsi_device.h"
#include "scsi_aha154x.h"
#include "scsi_x54x.h"


#define X54X_RESET_DURATION_US	UINT64_C(50000)


static void
x54x_irq(x54x_t *dev, int set)
{
    int int_type = 0;
    int irq;

    if (dev->ven_get_irq)
	irq = dev->ven_get_irq(dev);
    else
	irq = dev->Irq;

    if (dev->bus & DEVICE_PCI) {
	DEBUG("PCI IRQ: %02X, PCI_INTA\n", dev->pci_slot);
        if (set)
       	        pci_set_irq(dev->pci_slot, PCI_INTA);
	  else
       	        pci_clear_irq(dev->pci_slot, PCI_INTA);
    } else {
	if (set) {
		if (dev->interrupt_type)
			int_type = dev->interrupt_type(dev);

		if (int_type)
			picintlevel(1 << irq);
		  else
			picint(1 << irq);
	} else
		picintc(1 << irq);
    }
}


static void
raise_irq(x54x_t *dev, int suppress, uint8_t Interrupt)
{
    if (Interrupt & (INTR_MBIF | INTR_MBOA)) {
	if (! (dev->Interrupt & INTR_HACC))
		dev->Interrupt |= Interrupt;		/* Report now. */
	  else
		dev->PendingInterrupt |= Interrupt;	/* Report later. */
    } else if (Interrupt & INTR_HACC) {
	if (dev->Interrupt == 0 || dev->Interrupt == (INTR_ANY | INTR_HACC)) {
		DEBUG("%s: RaiseInterrupt(): Interrupt=%02X\n",
			dev->name, dev->Interrupt);
	}
	dev->Interrupt |= Interrupt;
    } else
	DEBUG("%s: RaiseInterrupt(): Invalid interrupt state!\n", dev->name);

    dev->Interrupt |= INTR_ANY;

    if (dev->IrqEnabled && !suppress)
	x54x_irq(dev, 1);
}


static void
clear_irq(x54x_t *dev)
{
    DEBUG("%s: lowering IRQ %i (stat 0x%02x)\n",
		dev->name, dev->Irq, dev->Interrupt);

    dev->Interrupt = 0;

    x54x_irq(dev, 0);
    if (dev->PendingInterrupt) {
	DEBUG("%s: Raising Interrupt 0x%02X (Pending)\n",
		dev->name, dev->Interrupt);
	if (dev->MailboxOutInterrupts || !(dev->Interrupt & INTR_MBOA))
		raise_irq(dev, 0, dev->PendingInterrupt);
	dev->PendingInterrupt = 0;
    }
}


static void
target_check(uint8_t id, uint8_t lun)
{
    if (! scsi_device_valid(&scsi_devices[id][lun]))
	fatal("BIOS INT13 device on ID %i:%i has disappeared\n", id, lun);
}


static uint8_t
completion_code(uint8_t *sense)
{
    uint8_t ret = 0xff;

    switch (sense[12]) {
	case ASC_NONE:
		ret = 0x00;
		break;

	case ASC_ILLEGAL_OPCODE:
	case ASC_INV_FIELD_IN_CMD_PACKET:
	case ASC_INV_FIELD_IN_PARAMETER_LIST:
	case ASC_DATA_PHASE_ERROR:
		ret = 0x01;
		break;

	case 0x12:
	case ASC_LBA_OUT_OF_RANGE:
		ret = 0x02;
		break;

	case ASC_WRITE_PROTECTED:
		ret = 0x03;
		break;

	case 0x14:
	case 0x16:
		ret = 0x04;
		break;

	case ASC_INCOMPATIBLE_FORMAT:
	case ASC_ILLEGAL_MODE_FOR_THIS_TRACK:
		ret = 0x0c;
		break;

	case 0x10:
	case 0x11:
		ret = 0x10;
		break;

	case 0x17:
	case 0x18:
		ret = 0x11;
		break;

	case 0x01:
	case 0x03:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
		ret = 0x20;
		break;

	case 0x15:
	case 0x02:
		ret = 0x40;
		break;

	case 0x25:
		ret = 0x80;
		break;

	case ASC_NOT_READY:
	case ASC_MEDIUM_MAY_HAVE_CHANGED:
	case 0x29:
	case ASC_CAPACITY_DATA_CHANGED:
	case ASC_MEDIUM_NOT_PRESENT:
		ret = 0xaa;
		break;
    };

    return(ret);
}


static uint8_t
bios_scsi_command(scsi_device_t *dev, uint8_t *cdb, uint8_t *buf, int len, uint32_t addr)
{
    dev->buffer_length = -1;

    scsi_device_command_phase0(dev, cdb);

    if (dev->phase == SCSI_PHASE_STATUS)
	return(completion_code(scsi_device_sense(dev)));

    dev->cmd_buffer = (uint8_t *)malloc(dev->buffer_length);
    if (dev->phase == SCSI_PHASE_DATA_IN) {
	scsi_device_command_phase1(dev);
	if (len > 0) {
		if (buf)
			memcpy(buf, dev->cmd_buffer, dev->buffer_length);
		else
			DMAPageWrite(addr, dev->cmd_buffer, dev->buffer_length);
	}
    } else if (dev->phase == SCSI_PHASE_DATA_OUT) {
	if (len > 0) {
		if (buf)
			memcpy(dev->cmd_buffer, buf, dev->buffer_length);
		else
			DMAPageRead(addr, dev->cmd_buffer, dev->buffer_length);
	}
	scsi_device_command_phase1(dev);
    }

    if (dev->cmd_buffer != NULL) {
	free(dev->cmd_buffer);
	dev->cmd_buffer = NULL;
    }

    return(completion_code(scsi_device_sense(dev)));
}


static uint8_t
bios_read_capacity(scsi_device_t *sd, uint8_t *buf)
{
    uint8_t *cdb;
    uint8_t ret;

    cdb = (uint8_t *)mem_alloc(12);
    memset(cdb, 0, 12);
    cdb[0] = GPCMD_READ_CDROM_CAPACITY;

    memset(buf, 0, 8);

    ret = bios_scsi_command(sd, cdb, buf, 8, 0);

    free(cdb);

    return(ret);
}


static uint8_t
bios_inquiry(scsi_device_t *sd, uint8_t *buf)
{
    uint8_t *cdb;
    uint8_t ret;

    cdb = (uint8_t *) malloc(12);
    memset(cdb, 0, 12);
    cdb[0] = GPCMD_INQUIRY;
    cdb[4] = 36;

    memset(buf, 0, 36);

    ret = bios_scsi_command(sd, cdb, buf, 36, 0);

    free(cdb);

    return(ret);
}


static uint8_t
bios_command_08(scsi_device_t *sd, uint8_t *buffer)
{
    uint8_t *rcbuf;
    uint8_t ret;
    int i;

    memset(buffer, 0x00, 6);

    rcbuf = (uint8_t *)mem_alloc(8);

    ret = bios_read_capacity(sd, rcbuf);
    if (ret) {
	free(rcbuf);
	return(ret);
    }

    memset(buffer, 0x00, 6);

    for (i = 0; i < 4; i++)
	buffer[i] = rcbuf[i];
    for (i = 4; i < 6; i++)
	buffer[i] = rcbuf[(i + 2) ^ 1];
    DEBUG("BIOS Command 0x08: %02X %02X %02X %02X %02X %02X\n",
	  buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);

    free(rcbuf);

    return(0);
}


static int
bios_command_15(scsi_device_t *sd, uint8_t *buffer)
{
    uint8_t *inqbuf, *rcbuf;
    uint8_t ret;
    int i;

    memset(buffer, 0x00, 6);

    inqbuf = (uint8_t *)mem_alloc(36);
    ret = bios_inquiry(sd, inqbuf);
    if (ret) {
	free(inqbuf);
	return(ret);
    }

    buffer[4] = inqbuf[0];
    buffer[5] = inqbuf[1];

    rcbuf = (uint8_t *)mem_alloc(8);
    ret = bios_read_capacity(sd, rcbuf);
    if (ret) {
	free(rcbuf);
	free(inqbuf);
	return(ret);
    }

    for (i = 0; i < 4; i++)
	buffer[i] = rcbuf[i];

    DEBUG("BIOS Command 0x15: %02X %02X %02X %02X %02X %02X\n",
	  buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);

    free(rcbuf);
    free(inqbuf);

    return(0);
}


/* This returns the completion code. */
static uint8_t
bios_command(x54x_t *x54x, uint8_t max_id, BIOSCMD *cmd, int8_t islba)
{
    const int bios_cmd_to_scsi[18] = {
	0, 0, GPCMD_READ_10, GPCMD_WRITE_10, GPCMD_VERIFY_10, 0, 0,
	GPCMD_FORMAT_UNIT, 0, 0, 0, 0, GPCMD_SEEK_10, 0, 0, 0,
	GPCMD_TEST_UNIT_READY, GPCMD_REZERO_UNIT
    };
    uint8_t cdb[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t *buf;
    scsi_device_t *dev = NULL;
    uint32_t dma_address = 0;
    uint32_t lba;
    int sector_len = cmd->secount;
    uint8_t ret = 0x00;

    if (islba)
	lba = lba32_blk(cmd);
      else
	lba = (cmd->u.chs.cyl << 9) + (cmd->u.chs.head << 5) + cmd->u.chs.sec;

    DEBUG("BIOS Command = 0x%02X\n", cmd->command);

    if ((cmd->id > max_id) || (cmd->lun > 7)) {
	DEBUG("BIOS target %i or LUN %i are above maximum\n", cmd->id, cmd->lun);
	ret = 0x80;
    }

    if (cmd->lun) {
	DEBUG("BIOS target LUN is not 0\n");
	ret = 0x80;
    }

    if (! ret) {
	/* Get pointer to selected device. */
	dev = &scsi_devices[cmd->id][cmd->lun];
	dev->buffer_length = 0;

	if (dev->cmd_buffer != NULL) {
		free(dev->cmd_buffer);
		dev->cmd_buffer = NULL;
	}

	if (! scsi_device_present(dev)) {
		DEBUG("BIOS Target ID %i has no device attached\n", cmd->id);
		ret = 0x80;
	} else {
		if ((dev->type == SCSI_REMOVABLE_CDROM) && !(x54x->flags & X54X_CDROM_BOOT)) {
			DEBUG("BIOS Target ID %i is CD-ROM on unsupported BIOS\n", cmd->id);
			return(0x80);
		} else {
			dma_address = ADDR_TO_U32(cmd->dma_address);

			DEBUG("BIOS Data Buffer write: length %d, pointer 0x%04X\n",
				 		sector_len, dma_address);
		}
	}
    }

    if (! ret) switch(cmd->command) {
	case 0x00:	/* Reset Disk System, in practice it's a nop */
		ret = 0x00;
		break;

	case 0x01:	/* Read Status of Last Operation */
		target_check(cmd->id, cmd->lun);

		/*
		 * Assuming 14 bytes because that is the default
		 * length for SCSI sense, and no command-specific
		 * indication is given.
		 */
		dev->buffer_length = 14;
		dev->cmd_buffer = (uint8_t *)mem_alloc(dev->buffer_length);
		memset(dev->cmd_buffer, 0x00, dev->buffer_length);

		if (sector_len > 0) {
			DEBUG("BIOS DMA: Reading 14 bytes at %08X\n",
							dma_address);
			DMAPageWrite(dma_address, scsi_device_sense(dev), 14);
		}

		if (dev && (dev->cmd_buffer != NULL)) {
			free(dev->cmd_buffer);
			dev->cmd_buffer = NULL;
		}

		break;

	case 0x02:	/* Read Desired Sectors to Memory */
	case 0x03:	/* Write Desired Sectors from Memory */
	case 0x04:	/* Verify Desired Sectors */
	case 0x0c:	/* Seek */
		target_check(cmd->id, cmd->lun);

		cdb[0] = bios_cmd_to_scsi[cmd->command];
		cdb[1] = (cmd->lun & 7) << 5;
		cdb[2] = (lba >> 24) & 0xff;
		cdb[3] = (lba >> 16) & 0xff;
		cdb[4] = (lba >> 8) & 0xff;
		cdb[5] = lba & 0xff;
		if (cmd->command != 0x0c)
			cdb[8] = sector_len;
		DBGLOG(1, "BIOS CMD(READ/WRITE/VERIFY, %08lx, %d)\n",
						lba, cmd->secount);

		ret = bios_scsi_command(dev, cdb, NULL, sector_len, dma_address);
		if (cmd->command == 0x0c)
			ret = !!ret;
		break;

	case 0x05:	/* Format Track, invalid since SCSI has no tracks */
	case 0x0a:	/* ???? */
	case 0x0b:	/* ???? */
	case 0x12:	/* ???? */
	case 0x13:	/* ???? */
//FIXME: add a longer delay here --FvK
		ret = 0x01;
		break;

	case 0x06:	/* Identify SCSI Devices, in practice it's a nop */
	case 0x09:	/* Initialize Drive Pair Characteristics, in practice it's a nop */
	case 0x0d:	/* Alternate Disk Reset, in practice it's a nop */
	case 0x0e:	/* Read Sector Buffer */
	case 0x0f:	/* Write Sector Buffer */
	case 0x14:	/* Controller Diagnostic */
//FIXME: add a longer delay here --FvK
		ret = 0x00;
		break;

	case 0x07:	/* Format Unit */
	case 0x10:	/* Test Drive Ready */
	case 0x11:	/* Recalibrate */
		target_check(cmd->id, cmd->lun);

		cdb[0] = bios_cmd_to_scsi[cmd->command];
		cdb[1] = (cmd->lun & 7) << 5;

		ret = bios_scsi_command(dev, cdb, NULL, sector_len, dma_address);
		break;

	case 0x08:	/* Read Drive Parameters */
	case 0x15:	/* Read DASD Type */
		target_check(cmd->id, cmd->lun);

		dev->buffer_length = 6;
		dev->cmd_buffer = (uint8_t *)mem_alloc(dev->buffer_length);
		memset(dev->cmd_buffer, 0x00, dev->buffer_length);

		buf = (uint8_t *) malloc(6);
		if (cmd->command == 0x08)
			ret = bios_command_08(dev, buf);
		else
			ret = bios_command_15(dev, buf);

		DEBUG("BIOS DMA: Reading 4 bytes at %08X\n", dma_address);
		DMAPageWrite(dma_address, buf, 4);
		free(buf);

		if (dev->cmd_buffer != NULL) {
			free(dev->cmd_buffer);
			dev->cmd_buffer = NULL;
		}

		break;

	default:
		DEBUG("BIOS: Unimplemented command: %02X\n", cmd->command);
		return(0x01);
    }
	
    DEBUG("BIOS Request %02X complete: %02X\n", cmd->command, ret);

    return(ret);
}


static void
cmd_done(x54x_t *dev, int suppress)
{
    int fast = 0;

    dev->DataReply = 0;
    dev->Status |= STAT_IDLE;

    if (dev->ven_cmd_is_fast)
	fast = dev->ven_cmd_is_fast(dev);

    if ((dev->Command != CMD_START_SCSI) || fast) {
	dev->Status &= ~STAT_DFULL;
	DEBUG("%s: Raising IRQ %i\n", dev->name, dev->Irq);
	raise_irq(dev, suppress, INTR_HACC);
    }

    dev->Command = 0xff;
    dev->CmdParam = 0;
}


static void
add_to_period(x54x_t *dev, int TransferLength)
{
    dev->temp_period += (tmrval_t)TransferLength;
}


static void
mbi_setup(x54x_t *dev, uint32_t CCBPointer, CCBU *CmdBlock,
	  uint8_t HostStatus, uint8_t TargetStatus, uint8_t mbcc)
{
    Req_t *req = &dev->Req;

    req->CCBPointer = CCBPointer;
    memcpy(&(req->CmdBlock), CmdBlock, sizeof(CCB32));
    req->Is24bit = !!(dev->flags & X54X_MBX_24BIT);
    req->HostStatus = HostStatus;
    req->TargetStatus = TargetStatus;
    req->MailboxCompletionCode = mbcc;

    DEBUG("Mailbox in setup\n");
}


static void
x54x_ccb(x54x_t *dev)
{
    Req_t *req = &dev->Req;

    /* Rewrite the CCB up to the CDB. */
    DEBUG("CCB completion code and statuses rewritten (pointer %08X)\n",
	  req->CCBPointer);

    DMAPageWrite(req->CCBPointer + 0x000D, &(req->MailboxCompletionCode), 1);
    DMAPageWrite(req->CCBPointer + 0x000E, &(req->HostStatus), 1);
    DMAPageWrite(req->CCBPointer + 0x000F, &(req->TargetStatus), 1);
    add_to_period(dev, 3);

    if (dev->MailboxOutInterrupts)
	dev->ToRaise = INTR_MBOA | INTR_ANY;
      else
	dev->ToRaise = 0;
}


static void
x54x_mbi(x54x_t *dev)
{
    Req_t *req = &dev->Req;
    addr24 CCBPointer;
    CCBU *CmdBlock = &(req->CmdBlock);
    uint8_t HostStatus = req->HostStatus;
    uint8_t TargetStatus = req->TargetStatus;
    uint32_t MailboxCompletionCode = req->MailboxCompletionCode;
    uint32_t Incoming;

    Incoming = dev->MailboxInAddr + (dev->MailboxInPosCur * ((dev->flags & X54X_MBX_24BIT) ? sizeof(Mailbox_t) : sizeof(Mailbox32_t)));

    if (MailboxCompletionCode != MBI_NOT_FOUND) {
	CmdBlock->common.HostStatus = HostStatus;
	CmdBlock->common.TargetStatus = TargetStatus;

	/* Rewrite the CCB up to the CDB. */
	DEBUG("CCB statuses rewritten (pointer %08X)\n", req->CCBPointer);
    	DMAPageWrite(req->CCBPointer + 0x000E, &(req->HostStatus), 1);
	DMAPageWrite(req->CCBPointer + 0x000F, &(req->TargetStatus), 1);
	add_to_period(dev, 2);
    } else {
	DEBUG("Mailbox not found!\n");
    }

    DEBUG("Host Status 0x%02X, Target Status 0x%02X\n",HostStatus,TargetStatus);

    if (dev->flags & X54X_MBX_24BIT) {
	U32_TO_ADDR(CCBPointer, req->CCBPointer);
	DEBUG("Mailbox 24-bit: Status=0x%02X, CCB at 0x%04X\n", req->MailboxCompletionCode, CCBPointer);
	DMAPageWrite(Incoming, &(req->MailboxCompletionCode), 1);
	DMAPageWrite(Incoming + 1, (uint8_t *)&CCBPointer, 3);
	add_to_period(dev, 4);
	DEBUG("%i bytes of 24-bit mailbox written to: %08X\n", sizeof(Mailbox_t), Incoming);
    } else {
	U32_TO_ADDR(CCBPointer, req->CCBPointer);
	DEBUG("Mailbox 32-bit: Status=0x%02X, CCB at 0x%04X\n", req->MailboxCompletionCode, CCBPointer);
	DMAPageWrite(Incoming, (uint8_t *)&(req->CCBPointer), 4);
	DMAPageWrite(Incoming + 4, &(req->HostStatus), 1);
	DMAPageWrite(Incoming + 5, &(req->TargetStatus), 1);
	DMAPageWrite(Incoming + 7, &(req->MailboxCompletionCode), 1);
	add_to_period(dev, 7);
	DEBUG("%i bytes of 32-bit mailbox written to: %08X\n", sizeof(Mailbox32_t), Incoming);
    }

    dev->MailboxInPosCur++;
    if (dev->MailboxInPosCur >= dev->MailboxCount)
	dev->MailboxInPosCur = 0;

    dev->ToRaise = INTR_MBIF | INTR_ANY;
    if (dev->MailboxOutInterrupts)
	dev->ToRaise |= INTR_MBOA;
}


static void
read_sge(x54x_t *dev, int Is24bit, uint32_t Address, SGE32 *SG)
{
    SGE SGE24;

    if (Is24bit) {
	DMAPageRead(Address, (uint8_t *)&SGE24, sizeof(SGE));
	add_to_period(dev, sizeof(SGE));

	/* Convert the 24-bit entries into 32-bit entries. */
	DEBUG("Read S/G block: %06X, %06X\n", SGE24.Segment, SGE24.SegmentPointer);
	SG->Segment = ADDR_TO_U32(SGE24.Segment);
	SG->SegmentPointer = ADDR_TO_U32(SGE24.SegmentPointer);
    } else {
	DMAPageRead(Address, (uint8_t *)SG, sizeof(SGE32));
	add_to_period(dev, sizeof(SGE32));
    }
}


static int
get_length(x54x_t *dev, Req_t *req, int Is24bit)
{
    uint32_t DataPointer, DataLength;
    uint32_t SGEntryLength = (Is24bit ? sizeof(SGE) : sizeof(SGE32));
    SGE32 SGBuffer;
    uint32_t DataToTransfer = 0, i = 0;

    if (Is24bit) {
	DataPointer = ADDR_TO_U32(req->CmdBlock.old_fmt.DataPointer);
	DataLength = ADDR_TO_U32(req->CmdBlock.old_fmt.DataLength);
	DEBUG("Data length: %08X\n", req->CmdBlock.old_fmt.DataLength);
    } else {
	DataPointer = req->CmdBlock.new_fmt.DataPointer;
	DataLength = req->CmdBlock.new_fmt.DataLength;
    }
    DEBUG("Data Buffer write: length %d, pointer 0x%04X\n",
	  DataLength, DataPointer);

    if (!DataLength)
	return(0);

    if (req->CmdBlock.common.ControlByte != 0x03) {
	if (req->CmdBlock.common.Opcode == SCATTER_GATHER_COMMAND ||
	    req->CmdBlock.common.Opcode == SCATTER_GATHER_COMMAND_RES) {
		for (i = 0; i < DataLength; i += SGEntryLength) {
			read_sge(dev, Is24bit, DataPointer + i, &SGBuffer);

			DataToTransfer += SGBuffer.Segment;
		}
		return(DataToTransfer);
	} else if (req->CmdBlock.common.Opcode == SCSI_INITIATOR_COMMAND ||
		   req->CmdBlock.common.Opcode == SCSI_INITIATOR_COMMAND_RES) {
		return(DataLength);
	}
	return(0);
    }

    return(0);
}


static void
set_residue(x54x_t *dev, Req_t *req, int32_t TransferLength)
{
    uint32_t Residue = 0;
    addr24 Residue24;
    int32_t BufLen = scsi_devices[req->TargetID][req->LUN].buffer_length;

    if ((req->CmdBlock.common.Opcode == SCSI_INITIATOR_COMMAND_RES) ||
	(req->CmdBlock.common.Opcode == SCATTER_GATHER_COMMAND_RES)) {

	if ((TransferLength > 0) && (req->CmdBlock.common.ControlByte < 0x03)) {
		TransferLength -= BufLen;
		if (TransferLength > 0)
			Residue = TransferLength;
	}

	if (req->Is24bit) {
		U32_TO_ADDR(Residue24, Residue);
    		DMAPageWrite(req->CCBPointer + 0x0004, (uint8_t *)&Residue24, 3);
		add_to_period(dev, 3);
		DEBUG("24-bit Residual data length for reading: %d\n", Residue);
	} else {
    		DMAPageWrite(req->CCBPointer + 0x0004, (uint8_t *)&Residue, 4);
		add_to_period(dev, 4);
		DEBUG("32-bit Residual data length for reading: %d\n", Residue);
	}
    }
}


static void
buf_dma_transfer(x54x_t *dev, Req_t *req, int Is24bit, int TransferLength, int dir)
{
    uint32_t DataPointer, DataLength;
    uint32_t SGEntryLength = (Is24bit ? sizeof(SGE) : sizeof(SGE32));
    uint32_t Address, i = 0;
    uint32_t BufLen;
    uint8_t read_from_host = (dir && ((req->CmdBlock.common.ControlByte == CCB_DATA_XFER_OUT) || (req->CmdBlock.common.ControlByte == 0x00)));
    uint8_t write_to_host = (!dir && ((req->CmdBlock.common.ControlByte == CCB_DATA_XFER_IN) || (req->CmdBlock.common.ControlByte == 0x00)));
    int sg_pos = 0;
    SGE32 SGBuffer;
    uint32_t DataToTransfer = 0;
    scsi_device_t *sd;

    sd = &scsi_devices[req->TargetID][req->LUN];
    BufLen = sd->buffer_length;

    if (Is24bit) {
	DataPointer = ADDR_TO_U32(req->CmdBlock.old_fmt.DataPointer);
	DataLength = ADDR_TO_U32(req->CmdBlock.old_fmt.DataLength);
    } else {
	DataPointer = req->CmdBlock.new_fmt.DataPointer;
	DataLength = req->CmdBlock.new_fmt.DataLength;
    }
    DEBUG("Data Buffer %s: length %d (%u), pointer 0x%04X\n",
	     dir ? "write" : "read", BufLen, DataLength, DataPointer);

    if ((req->CmdBlock.common.ControlByte != 0x03) && TransferLength && BufLen) {
	if ((req->CmdBlock.common.Opcode == SCATTER_GATHER_COMMAND) ||
	    (req->CmdBlock.common.Opcode == SCATTER_GATHER_COMMAND_RES)) {

		/* If the control byte is 0x00, it means that the transfer direction is set up by the SCSI command without
		   checking its length, so do this procedure for both no read/write commands. */
		if ((DataLength > 0) && (req->CmdBlock.common.ControlByte < 0x03)) {
			for (i = 0; i < DataLength; i += SGEntryLength) {
				read_sge(dev, Is24bit, DataPointer + i, &SGBuffer);

				Address = SGBuffer.SegmentPointer;
				DataToTransfer = MIN((int)SGBuffer.Segment, BufLen);

				if (read_from_host && DataToTransfer) {
					DEBUG("Reading S/G segment %i: length %i, pointer %08X\n", i, DataToTransfer, Address);
					DMAPageRead(Address, &sd->cmd_buffer[sg_pos], DataToTransfer);
				}
				else if (write_to_host && DataToTransfer) {
					DEBUG("Writing S/G segment %i: length %i, pointer %08X\n", i, DataToTransfer, Address);
					DMAPageWrite(Address, &sd->cmd_buffer[sg_pos], DataToTransfer);
				}
				else
					DEBUG("No action on S/G segment %i: length %i, pointer %08X\n", i, DataToTransfer, Address);

				sg_pos += SGBuffer.Segment;

				BufLen -= SGBuffer.Segment;
#if 0 	/*BUFLEN_IS_UNSIGNED*/
				if (BufLen < 0)
					BufLen = 0;
#endif

				DEBUG("After S/G segment done: %i, %i\n", sg_pos, BufLen);
			}
		}
	} else if ((req->CmdBlock.common.Opcode == SCSI_INITIATOR_COMMAND) ||
		   (req->CmdBlock.common.Opcode == SCSI_INITIATOR_COMMAND_RES)) {
		Address = DataPointer;

		if ((DataLength > 0) && (BufLen > 0) && (req->CmdBlock.common.ControlByte < 0x03)) {
			if (read_from_host)
				DMAPageRead(Address, sd->cmd_buffer, MIN(BufLen, (int)DataLength));
			  else if (write_to_host)
				DMAPageWrite(Address, sd->cmd_buffer, MIN(BufLen, (int)DataLength));
		}
	}
    }
}


void
x54x_buf_alloc(scsi_device_t *sd, int length)
{
    if (sd->cmd_buffer != NULL) {
	free(sd->cmd_buffer);
	sd->cmd_buffer = NULL;
    }

    DEBUG("Allocating data buffer (%i bytes)\n", length);
    sd->cmd_buffer = (uint8_t *)mem_alloc(length);
    memset(sd->cmd_buffer, 0, length);
}


void
x54x_buf_free(scsi_device_t *sd)
{
    if (sd->cmd_buffer != NULL) {
	free(sd->cmd_buffer);
	sd->cmd_buffer = NULL;
    }
}


static uint8_t
ConvertSenseLength(uint8_t RequestSenseLength)
{
    DEBUG("Unconverted Request Sense length %i\n", RequestSenseLength);

    if (RequestSenseLength == 0)
	RequestSenseLength = 14;
    else if (RequestSenseLength == 1)
	RequestSenseLength = 0;

    DEBUG("Request Sense length %i\n", RequestSenseLength);

    return(RequestSenseLength);
}


uint32_t
SenseBufferPointer(Req_t *req)
{
    uint32_t SenseBufferAddress;

    if (req->Is24bit) {
	SenseBufferAddress = req->CCBPointer;
	SenseBufferAddress += req->CmdBlock.common.CdbLength + 18;
    } else {
	SenseBufferAddress = req->CmdBlock.new_fmt.SensePointer;
    }

    return(SenseBufferAddress);
}


static void
SenseBufferFree(x54x_t *dev, Req_t *req, int Copy)
{
    uint8_t SenseLength = ConvertSenseLength(req->CmdBlock.common.RequestSenseLength);
    uint32_t SenseBufferAddress;
    uint8_t temp_sense[256];

    if (SenseLength && Copy) {
        scsi_device_request_sense(&scsi_devices[req->TargetID][req->LUN], temp_sense, SenseLength);

	/*
	 * The sense address, in 32-bit mode, is located in the
	 * Sense Pointer of the CCB, but in 24-bit mode, it is
	 * located at the end of the Command Descriptor Block.
	 */
	SenseBufferAddress = SenseBufferPointer(req);

	DEBUG("Request Sense address: %02X\n", SenseBufferAddress);

	DEBUG("SenseBufferFree(): Writing %i bytes at %08X\n",
					SenseLength, SenseBufferAddress);
	DMAPageWrite(SenseBufferAddress, temp_sense, SenseLength);
	add_to_period(dev, SenseLength);
	DEBUG("Sense data written to buffer: %02X %02X %02X\n",
		temp_sense[2], temp_sense[12], temp_sense[13]);
    }
}


static void
scsi_cmd(x54x_t *dev)
{
    Req_t *req = &dev->Req;
    uint8_t phase, bit24 = !!req->Is24bit;
    uint32_t i, SenseBufferAddress;
    int target_data_len, target_cdb_len = 12;
    uint8_t temp_cdb[12];
    int32_t *BufLen;
    tmrval_t p;
    scsi_device_t *sd;

    sd = &scsi_devices[req->TargetID][req->LUN];

    target_cdb_len = 12;
    target_data_len = get_length(dev, req, bit24);

    if (! scsi_device_valid(sd))
	fatal("SCSI target on %i:%i has disappeared\n", req->TargetID, req->LUN);

    DEBUG("target_data_len = %i\n", target_data_len);
    DEBUG("SCSI command being executed on ID %i:%i\n", req->TargetID, req->LUN);
    DEBUG("SCSI CDB[0]=0x%02X\n", req->CmdBlock.common.Cdb[0]);
    for (i = 1; i < req->CmdBlock.common.CdbLength; i++)
	DEBUG("SCSI CDB[%i]=%i\n", i, req->CmdBlock.common.Cdb[i]);

    memset(temp_cdb, 0x00, target_cdb_len);
    if (req->CmdBlock.common.CdbLength <= target_cdb_len) {
	memcpy(temp_cdb, req->CmdBlock.common.Cdb,
	       req->CmdBlock.common.CdbLength);
	add_to_period(dev, req->CmdBlock.common.CdbLength);
    } else {
	memcpy(temp_cdb, req->CmdBlock.common.Cdb, target_cdb_len);
	add_to_period(dev, target_cdb_len);
    }

    dev->Residue = 0;

    BufLen = scsi_device_get_buf_len(sd);
    *BufLen = target_data_len;

    DEBUG("Command buffer: %08X\n", sd->cmd_buffer);

    scsi_device_command_phase0(sd, temp_cdb);

    phase = sd->phase;

    DEBUG("Control byte: %02X\n", (req->CmdBlock.common.ControlByte == 0x03));

    if (phase != SCSI_PHASE_STATUS) {
	if ((temp_cdb[0] == 0x03) && (req->CmdBlock.common.ControlByte == 0x03)) {
		/* Request sense in non-data mode - sense goes to sense buffer. */
		*BufLen = ConvertSenseLength(req->CmdBlock.common.RequestSenseLength);
	    	x54x_buf_alloc(sd, *BufLen);
		scsi_device_command_phase1(sd);
		if ((sd->status != SCSI_STATUS_OK) && (*BufLen > 0)) {
			SenseBufferAddress = SenseBufferPointer(req);
			DMAPageWrite(SenseBufferAddress, sd->cmd_buffer, *BufLen);
			add_to_period(dev, *BufLen);
		}
	} else {
		p = scsi_device_get_callback(sd);
		if (p <= 0LL)
			add_to_period(dev, *BufLen);
		else
			dev->media_period += p;
	    	x54x_buf_alloc(sd, MIN(target_data_len, *BufLen));
		if (phase == SCSI_PHASE_DATA_OUT)
			buf_dma_transfer(dev, req, bit24, target_data_len, 1);
		scsi_device_command_phase1(sd);
		if (phase == SCSI_PHASE_DATA_IN)
			buf_dma_transfer(dev, req, bit24, target_data_len, 0);

		SenseBufferFree(dev, req, (sd->status != SCSI_STATUS_OK));
	}
    } else
	SenseBufferFree(dev, req, (sd->status != SCSI_STATUS_OK));

    set_residue(dev, req, target_data_len);

    x54x_buf_free(sd);

    DEBUG("Request complete\n");

    if (sd->status == SCSI_STATUS_OK) {
	mbi_setup(dev, req->CCBPointer, &req->CmdBlock,
		  CCB_COMPLETE, SCSI_STATUS_OK, MBI_SUCCESS);
    } else if (sd->status == SCSI_STATUS_CHECK_CONDITION) {
	mbi_setup(dev, req->CCBPointer, &req->CmdBlock,
		  CCB_COMPLETE, SCSI_STATUS_CHECK_CONDITION, MBI_ERROR);
    }

    DEBUG("scsi_devices[%i][%i].status = %02X\n", req->TargetID, req->LUN, sd->status);
}


static void
x54x_notify(x54x_t *dev)
{
    if (dev->MailboxIsBIOS)
	x54x_ccb(dev);
    else
	x54x_mbi(dev);
}


static void
req_setup(x54x_t *dev, uint32_t CCBPointer, Mailbox32_t *Mailbox32)
{	
    Req_t *req = &dev->Req;
    scsi_device_t *sd;

    /* Fetch data from the Command Control Block. */
    DMAPageRead(CCBPointer, (uint8_t *)&req->CmdBlock, sizeof(CCB32));
    add_to_period(dev, sizeof(CCB32));

    req->Is24bit = !!(dev->flags & X54X_MBX_24BIT);
    req->CCBPointer = CCBPointer;
    req->TargetID = req->Is24bit ? req->CmdBlock.old_fmt.Id : req->CmdBlock.new_fmt.Id;
    req->LUN = req->Is24bit ? req->CmdBlock.old_fmt.Lun : req->CmdBlock.new_fmt.Lun;

    if ((req->TargetID > dev->max_id) || (req->LUN > 7)) {
	DEBUG("SCSI Target ID %i or LUN %i is not valid\n", req->TargetID, req->LUN);
	mbi_setup(dev, CCBPointer, &req->CmdBlock,
		  CCB_SELECTION_TIMEOUT, SCSI_STATUS_OK, MBI_ERROR);
	DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
	x54x_notify(dev);
	return;
    }
    sd = &scsi_devices[req->TargetID][req->LUN];

    DEBUG("Scanning SCSI Target ID %i\n", req->TargetID);

    sd->status = SCSI_STATUS_OK;

    /* If there is no device at ID:0, timeout the selection - the LUN is then checked later. */
    if (! scsi_device_present(sd)) {
	DEBUG("SCSI Target ID %i and LUN %i have no device attached\n", req->TargetID, req->LUN);
	mbi_setup(dev, CCBPointer, &req->CmdBlock,
		  CCB_SELECTION_TIMEOUT, SCSI_STATUS_OK, MBI_ERROR);
	DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
	x54x_notify(dev);
    } else {
	DEBUG("SCSI Target ID %i detected and working\n", req->TargetID);
	DEBUG("Transfer Control %02X\n", req->CmdBlock.common.ControlByte);
	DEBUG("CDB Length %i\n", req->CmdBlock.common.CdbLength);	
	DEBUG("CCB Opcode %x\n", req->CmdBlock.common.Opcode);		

	if ((req->CmdBlock.common.Opcode > 0x04) && (req->CmdBlock.common.Opcode != 0x81)) {
		DEBUG("Invalid opcode: %02X\n",
		      req->CmdBlock.common.ControlByte);

		mbi_setup(dev, CCBPointer, &req->CmdBlock, CCB_INVALID_OP_CODE, SCSI_STATUS_OK, MBI_ERROR);
		DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
		x54x_notify(dev);
		return;
	}
	if (req->CmdBlock.common.Opcode == 0x81) {
		DEBUG("Bus reset opcode\n");

		scsi_device_reset(sd);
		mbi_setup(dev, req->CCBPointer, &req->CmdBlock,
			  CCB_COMPLETE, SCSI_STATUS_OK, MBI_SUCCESS);

		DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
		x54x_notify(dev);
		return;
	}

	if (req->CmdBlock.common.ControlByte > 0x03) {
		DEBUG("Invalid control byte: %02X\n",
			req->CmdBlock.common.ControlByte);
		mbi_setup(dev, CCBPointer, &req->CmdBlock, CCB_INVALID_DIRECTION, SCSI_STATUS_OK, MBI_ERROR);
		DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
		x54x_notify(dev);
		return;
	}

	DEBUG("%s: Callback: Process SCSI request\n", dev->name);
	scsi_cmd(dev);

	DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
	x54x_notify(dev);
    }
}


static void
req_abort(x54x_t *dev, uint32_t CCBPointer)
{
    CCBU CmdBlock;

    /* Fetch data from the Command Control Block. */
    DMAPageRead(CCBPointer, (uint8_t *)&CmdBlock, sizeof(CCB32));
    add_to_period(dev, sizeof(CCB32));

    mbi_setup(dev, CCBPointer, &CmdBlock, 0x26, SCSI_STATUS_OK, MBI_NOT_FOUND);
    DEBUG("%s: Callback: Send incoming mailbox\n", dev->name);
    x54x_notify(dev);
}


static uint32_t
x54x_mbo(x54x_t *dev, Mailbox32_t *Mailbox32)
{
    Mailbox_t MailboxOut;
    uint32_t Outgoing;
    uint32_t ccbp;
    uint32_t Addr;
    uint32_t Cur;

    if (dev->MailboxIsBIOS) {
	Addr = dev->BIOSMailboxOutAddr;
	Cur = dev->BIOSMailboxOutPosCur;
    } else {
	Addr = dev->MailboxOutAddr;
	Cur = dev->MailboxOutPosCur;
    }

    if (dev->flags & X54X_MBX_24BIT) {
	Outgoing = Addr + (Cur * sizeof(Mailbox_t));
	DMAPageRead(Outgoing, (uint8_t *)&MailboxOut, sizeof(Mailbox_t));
	add_to_period(dev, sizeof(Mailbox_t));

	ccbp = *(uint32_t *) &MailboxOut;
	Mailbox32->CCBPointer = (ccbp >> 24) | ((ccbp >> 8) & 0xff00) | ((ccbp << 8) & 0xff0000);
	Mailbox32->u.out.ActionCode = MailboxOut.CmdStatus;
    } else {
	Outgoing = Addr + (Cur * sizeof(Mailbox32_t));

	DMAPageRead(Outgoing, (uint8_t *)Mailbox32, sizeof(Mailbox32_t));
	add_to_period(dev, sizeof(Mailbox32_t));
    }

    return(Outgoing);
}


uint8_t
x54x_mbo_process(x54x_t *dev)
{
    Mailbox32_t mb32;
    uint32_t Outgoing;
    uint8_t CmdStatus = MBO_FREE;
    uint32_t CodeOffset = 0;

    CodeOffset = (dev->flags & X54X_MBX_24BIT) ? 0 : 7;

    Outgoing = x54x_mbo(dev, &mb32);

    if (mb32.u.out.ActionCode == MBO_START) {
	DEBUG("Start Mailbox Command\n");
	req_setup(dev, mb32.CCBPointer, &mb32);
    } else if (!dev->MailboxIsBIOS && (mb32.u.out.ActionCode == MBO_ABORT)) {
	DEBUG("Abort Mailbox Command\n");
	req_abort(dev, mb32.CCBPointer);
    } /* else {
	DEBUG("Invalid action code: %02X\n", mb32.u.out.ActionCode);
    } */

    if ((mb32.u.out.ActionCode == MBO_START) || (!dev->MailboxIsBIOS && (mb32.u.out.ActionCode == MBO_ABORT))) {
	/* We got the mailbox, mark it as free in the guest. */
	DEBUG("x54x_do_mail(): Writing %i bytes at %08X\n", sizeof(CmdStatus), Outgoing + CodeOffset);
	DMAPageWrite(Outgoing + CodeOffset, &CmdStatus, 1);
	add_to_period(dev, 1);

	if (dev->ToRaise)
		raise_irq(dev, 0, dev->ToRaise);

	if (dev->MailboxIsBIOS)
		dev->BIOSMailboxReq--;
	  else
		dev->MailboxReq--;

	return(1);
    }

    return(0);
}


static void
do_mail(x54x_t *dev)
{
    int aggressive = 1;

    dev->MailboxIsBIOS = 0;

    if (dev->is_aggressive_mode) {
	aggressive = dev->is_aggressive_mode(dev);
	DEBUG("Processing mailboxes in %s mode...\n", aggressive ? "aggressive" : "strict");
    }/* else {
	DEBUG("Defaulting to process mailboxes in %s mode...\n", aggressive ? "aggressive" : "strict");
    }*/

    if (!dev->MailboxCount) {
	DEBUG("x54x_do_mail(): No Mailboxes\n");
	return;
    }

    if (aggressive) {
	/* Search for a filled mailbox - stop if we have scanned all mailboxes. */
	for (dev->MailboxOutPosCur = 0; dev->MailboxOutPosCur < dev->MailboxCount; dev->MailboxOutPosCur++) {
		if (x54x_mbo_process(dev))
			break;
	}
    } else {
	/* Strict round robin mode - only process the current mailbox and advance the pointer if successful. */
	if (x54x_mbo_process(dev)) {
		dev->MailboxOutPosCur++;
		dev->MailboxOutPosCur %= dev->MailboxCount;
	}
    }
}


static void
cmd_callback(priv_t priv)
{
    x54x_t *dev = (x54x_t *)priv;
    double period;
    int mailboxes_present, bios_mailboxes_present;

    mailboxes_present = (!(dev->Status & STAT_INIT) && dev->MailboxInit && dev->MailboxReq);
    bios_mailboxes_present = (dev->ven_callback && dev->BIOSMailboxInit && dev->BIOSMailboxReq);
    if (!mailboxes_present && !bios_mailboxes_present) {
	/* If we did not get anything, do nothing and wait 10 us. */
	dev->timer_period = 10LL * TIMER_USEC;
	return;
    }

    dev->temp_period = dev->media_period = 0LL;

    if (!mailboxes_present) {
	/* Do only BIOS mailboxes. */
	dev->ven_callback(dev);
    } else if (!bios_mailboxes_present) {
	/* Do only normal mailboxes. */
	do_mail(dev);
    } else {
	/* Do both kinds of mailboxes. */
	if (dev->callback_phase)
		dev->ven_callback(dev);
	else
		do_mail(dev);

	dev->callback_phase = (dev->callback_phase + 1) & 0x01;
    }

    period = (1000000.0 / dev->ha_bps) * ((double) TIMER_USEC) * ((double) dev->temp_period);
    dev->timer_period = dev->media_period + ((tmrval_t) period) + (40LL * TIMER_USEC);

    DEBUG("Temporary period: %" PRId64 " us (%" PRIi64 " periods)\n",
	  dev->timer_period, dev->temp_period);
}


static uint8_t
x54x_in(uint16_t port, priv_t priv)
{
    x54x_t *dev = (x54x_t *)priv;
    uint8_t ret;

    switch (port & 3) {
	case 0:
	default:
		ret = dev->Status;
		break;

	case 1:
		ret = dev->DataBuf[dev->DataReply];
		if (dev->DataReplyLeft) {
			dev->DataReply++;
			dev->DataReplyLeft--;
			if (! dev->DataReplyLeft)
				cmd_done(dev, 0);
		}
		break;

	case 2:
		if (dev->flags & X54X_INT_GEOM_WRITABLE)
			ret = dev->Interrupt;
		else
			ret = dev->Interrupt & ~0x70;
		break;

	case 3:
		/* Bits according to ASPI4DOS.SYS v3.36:
		 *   0		Not checked
		 *   1		Must be 0
		 *   2		Must be 0-0-0-1
		 *   3		Must be 0
		 *   4		Must be 0-1-0-0
		 *   5		Must be 0
		 *   6		Not checked
		 *   7		Not checked
		 */
		if (dev->flags & X54X_INT_GEOM_WRITABLE)
			ret = dev->Geometry;
		else {
			switch(dev->Geometry) {
				case 0: default: ret = 'A'; break;
				case 1: ret = 'D'; break;
				case 2: ret = 'A'; break;
				case 3: ret = 'P'; break;
			}
			ret ^= 1;
			dev->Geometry++;
			dev->Geometry &= 0x03;
			break;
		}
		break;
    }

    DBGLOG(2, "%s: Read Port 0x%02X, Value %02X\n", dev->name, port, ret);

    return(ret);
}


static uint16_t
x54x_inw(uint16_t port, priv_t priv)
{
    return((uint16_t) x54x_in(port, priv));
}


static uint32_t
x54x_inl(uint16_t port, priv_t priv)
{
    return((uint32_t) x54x_in(port, priv));
}


static uint8_t
x54x_read(uint32_t port, priv_t priv)
{
    return(x54x_in(port & 3, priv));
}


static uint16_t
x54x_readw(uint32_t port, priv_t priv)
{
    return(x54x_inw(port & 3, priv));
}


static uint32_t
x54x_readl(uint32_t port, priv_t priv)
{
    return(x54x_inl(port & 3, priv));
}


static void
x54x_reset_poll(priv_t priv)
{
    x54x_t *dev = (x54x_t *)priv;

    dev->Status = STAT_INIT | STAT_IDLE;

    dev->ResetCB = 0LL;
}


static void
x54x_reset(x54x_t *dev)
{
    int i, j;

    clear_irq(dev);
    if (dev->flags & X54X_INT_GEOM_WRITABLE)
	dev->Geometry = 0x80;
      else
	dev->Geometry = 0x00;
    dev->callback_phase = 0;
    dev->Command = 0xFF;
    dev->CmdParam = 0;
    dev->CmdParamLeft = 0;
    dev->flags |= X54X_MBX_24BIT;
    dev->MailboxInPosCur = 0;
    dev->MailboxOutInterrupts = 0;
    dev->PendingInterrupt = 0;
    dev->IrqEnabled = 1;
    dev->MailboxCount = 0;
    dev->MailboxOutPosCur = 0;

    /* Reset all devices on controller reset. */
    for (i = 0; i < SCSI_ID_MAX; i++) {
	for (j = 0; j < SCSI_LUN_MAX; j++)
		scsi_device_reset(&scsi_devices[i][j]);
    }

    if (dev->ven_reset)
	dev->ven_reset(dev);
}


void
x54x_reset_ctrl(x54x_t *dev, uint8_t Reset)
{
    /* Say hello! */
    INFO("%s %s (IO=0x%04X, IRQ=%d, DMA=%d, BIOS @%05lX) ID=%d\n",
	 dev->vendor, dev->name, dev->Base, dev->Irq, dev->DmaChannel,
	 dev->rom_addr, dev->HostID);

    x54x_reset(dev);

    if (Reset) {
	dev->Status = STAT_STST;
	dev->ResetCB = X54X_RESET_DURATION_US * TIMER_USEC;
    } else
	dev->Status = STAT_INIT | STAT_IDLE;
}


static void
x54x_out(uint16_t port, uint8_t val, priv_t priv)
{
    ReplyInquireSetupInformation *ReplyISI;
    x54x_t *dev = (x54x_t *)priv;
    MailboxInit_t *mbi;
    int i = 0;
    uint8_t j = 0;
    BIOSCMD *cmd;
    uint16_t cyl = 0;
    int suppress = 0;
    uint32_t FIFOBuf;
    uint8_t reset;
    addr24 Address;
    uint8_t host_id = dev->HostID;
    uint8_t irq = 0;

    DEBUG("%s: Write Port 0x%02X, Value %02X\n", dev->name, port, val);

    switch (port & 3) {
	case 0:
		if ((val & CTRL_HRST) || (val & CTRL_SRST)) {
			reset = (val & CTRL_HRST);
			DEBUG("Reset completed = %x\n", reset);
			x54x_reset_ctrl(dev, reset);
			DEBUG("Controller reset: ");
			break;
		}

		if (val & CTRL_SCRST) {
			/* Reset all devices on SCSI bus reset. */
			for (i = 0; i < SCSI_ID_MAX; i++) {
				for (j = 0; j < SCSI_LUN_MAX; j++)
					scsi_device_reset(&scsi_devices[i][j]);
			}
		}

		if (val & CTRL_IRST) {
			clear_irq(dev);
			DEBUG("Interrupt reset: ");
		}
		break;

	case 1:
		/* Fast path for the mailbox execution command. */
		if ((val == CMD_START_SCSI) && (dev->Command == 0xff)) {
			dev->MailboxReq++;
			DEBUG("Start SCSI command: ");
			return;
		}
		if (dev->ven_fast_cmds) {
			if (dev->Command == 0xff) {
				if (dev->ven_fast_cmds(dev, val))
					return;
			}
		}

		if (dev->Command == 0xff) {
			dev->Command = val;
			dev->CmdParam = 0;
			dev->CmdParamLeft = 0;

			dev->Status &= ~(STAT_INVCMD | STAT_IDLE);
			DEBUG("%s: Operation Code 0x%02X\n", dev->name, val);
			switch (dev->Command) {
				case CMD_MBINIT:
					dev->CmdParamLeft = sizeof(MailboxInit_t);
					break;

				case CMD_BIOSCMD:
					dev->CmdParamLeft = 10;
					break;

				case CMD_EMBOI:
				case CMD_BUSON_TIME:
				case CMD_BUSOFF_TIME:
				case CMD_DMASPEED:
				case CMD_RETSETUP:
				case CMD_ECHO:
				case CMD_OPTIONS:
					dev->CmdParamLeft = 1;
					break;	

				case CMD_SELTIMEOUT:
					dev->CmdParamLeft = 4;
					break;

				case CMD_WRITE_CH2:
				case CMD_READ_CH2:
					dev->CmdParamLeft = 3;
					break;

				default:
					if (dev->get_ven_param_len)
						dev->CmdParamLeft = dev->get_ven_param_len(dev);
					break;
			}
		} else {
			dev->CmdBuf[dev->CmdParam] = val;
			dev->CmdParam++;
			dev->CmdParamLeft--;

			if (dev->ven_cmd_phase1)
				dev->ven_cmd_phase1(dev);
		}
		
		if (! dev->CmdParamLeft) {
			DEBUG("Running Operation Code 0x%02X\n", dev->Command);
			switch (dev->Command) {
				case CMD_NOP: /* No Operation */
					dev->DataReplyLeft = 0;
					break;

				case CMD_MBINIT: /* mailbox initialization */
                    dev->flags |= X54X_MBX_24BIT;

                    mbi = (MailboxInit_t *)dev->CmdBuf;

					dev->MailboxInit = 1;
					dev->MailboxCount = mbi->Count;
					dev->MailboxOutAddr = ADDR_TO_U32(mbi->Address);
					dev->MailboxInAddr = dev->MailboxOutAddr + (dev->MailboxCount * sizeof(Mailbox_t));

					DEBUG("Initialize Mailbox: MBO=0x%08lx, MBI=0x%08lx, %d entries at 0x%08lx\n",
						dev->MailboxOutAddr,
						dev->MailboxInAddr,
						mbi->Count,
						ADDR_TO_U32(mbi->Address));

					dev->Status &= ~STAT_INIT;
					dev->DataReplyLeft = 0;
					DEBUG("Mailbox init: ");
					break;

				case CMD_BIOSCMD: /* execute BIOS */
					cmd = (BIOSCMD *)dev->CmdBuf;
					if (!(dev->flags & X54X_LBA_BIOS)) {
						/* 1640 uses LBA. */
						cyl = ((cmd->u.chs.cyl & 0xff) << 8) | ((cmd->u.chs.cyl >> 8) & 0xff);
						cmd->u.chs.cyl = cyl;
					}
					if (dev->flags & X54X_LBA_BIOS) {
						/* 1640 uses LBA. */
						DEBUG("BIOS LBA=%06lx (%lu)\n",
							lba32_blk(cmd),
							lba32_blk(cmd));
					} else {
						cmd->u.chs.head &= 0xf;
						cmd->u.chs.sec &= 0x1f;
						DEBUG("BIOS CHS=%04X/%02X%02X\n",
							cmd->u.chs.cyl,
							cmd->u.chs.head,
							cmd->u.chs.sec);
					}
					dev->DataBuf[0] = bios_command(dev, dev->max_id, cmd, !!(dev->flags & X54X_LBA_BIOS));
					DEBUG("BIOS Completion/Status Code %x\n", dev->DataBuf[0]);
					dev->DataReplyLeft = 1;
					break;

				case CMD_INQUIRY: /* Inquiry */
					memcpy(dev->DataBuf, dev->fw_rev, 4);
					DEBUG("Adapter inquiry: %c %c %c %c\n", dev->fw_rev[0], dev->fw_rev[1], dev->fw_rev[2], dev->fw_rev[3]);
					dev->DataReplyLeft = 4;
					break;

				case CMD_EMBOI: /* enable MBO Interrupt */
					if (dev->CmdBuf[0] <= 1) {
						dev->MailboxOutInterrupts = dev->CmdBuf[0];
						DEBUG("Mailbox out interrupts: %s\n", dev->MailboxOutInterrupts ? "ON" : "OFF");
						suppress = 1;
					} else {
						dev->Status |= STAT_INVCMD;
					}
					dev->DataReplyLeft = 0;
					break;

				case CMD_SELTIMEOUT: /* Selection Time-out */
					dev->DataReplyLeft = 0;
					break;

				case CMD_BUSON_TIME: /* bus-on time */
					dev->BusOnTime = dev->CmdBuf[0];
					dev->DataReplyLeft = 0;
					DEBUG("Bus-on time: %d\n", dev->CmdBuf[0]);
					break;

				case CMD_BUSOFF_TIME: /* bus-off time */
					dev->BusOffTime = dev->CmdBuf[0];
					dev->DataReplyLeft = 0;
					DEBUG("Bus-off time: %d\n", dev->CmdBuf[0]);
					break;

				case CMD_DMASPEED: /* DMA Transfer Rate */
					dev->ATBusSpeed = dev->CmdBuf[0];
					dev->DataReplyLeft = 0;
					DEBUG("DMA transfer rate: %02X\n", dev->CmdBuf[0]);
					break;

				case CMD_RETDEVS: /* return Installed Devices */
					memset(dev->DataBuf, 0x00, 8);

				        if (dev->ven_get_host_id)
						host_id = dev->ven_get_host_id(dev);

					for (i = 0; i < 8; i++) {
					    dev->DataBuf[i] = 0x00;

					    /* Skip the HA .. */
					    if (i == host_id) continue;

					    for (j = 0; j < SCSI_LUN_MAX; j++) {
						if (scsi_device_present(&scsi_devices[i][j]))
						    dev->DataBuf[i] |= (1<<j);
					    }
					}
					dev->DataReplyLeft = i;
					break;

				case CMD_RETCONF: /* return Configuration */
					if (dev->ven_get_dma)
						dev->DataBuf[0] = (1<<dev->ven_get_dma(dev));
					else
						dev->DataBuf[0] = (1<<dev->DmaChannel);

					if (dev->ven_get_irq)
						irq = dev->ven_get_irq(dev);
					else
						irq = dev->Irq;

					if (irq >= 9)
					    dev->DataBuf[1]=(1<<(irq-9));
					else
					    dev->DataBuf[1]=0;
					if (dev->ven_get_host_id)
						dev->DataBuf[2] = dev->ven_get_host_id(dev);
					else
						dev->DataBuf[2] = dev->HostID;
					DEBUG("Configuration data: %02X %02X %02X\n", dev->DataBuf[0], dev->DataBuf[1], dev->DataBuf[2]);
					dev->DataReplyLeft = 3;
					break;

				case CMD_RETSETUP: /* return Setup */
				{
					ReplyISI = (ReplyInquireSetupInformation *)dev->DataBuf;
					memset(ReplyISI, 0x00, sizeof(ReplyInquireSetupInformation));

					ReplyISI->uBusTransferRate = dev->ATBusSpeed;
					ReplyISI->uPreemptTimeOnBus = dev->BusOnTime;
					ReplyISI->uTimeOffBus = dev->BusOffTime;
					ReplyISI->cMailbox = dev->MailboxCount;
					U32_TO_ADDR(ReplyISI->MailboxAddress, dev->MailboxOutAddr);

					if (dev->get_ven_data) {
						dev->get_ven_data(dev);
					}

					dev->DataReplyLeft = dev->CmdBuf[0];
					DEBUG("Return Setup Information: %d (length: %i)\n", dev->CmdBuf[0], sizeof(ReplyInquireSetupInformation));
				}
				break;

				case CMD_ECHO: /* ECHO data */
					dev->DataBuf[0] = dev->CmdBuf[0];
					dev->DataReplyLeft = 1;
					break;

				case CMD_WRITE_CH2:	/* write channel 2 buffer */
					dev->DataReplyLeft = 0;
					Address.hi = dev->CmdBuf[0];
					Address.mid = dev->CmdBuf[1];
					Address.lo = dev->CmdBuf[2];
					FIFOBuf = ADDR_TO_U32(Address);
					DEBUG("Adaptec LocalRAM: Reading 64 bytes at %08X\n", FIFOBuf);
					DMAPageRead(FIFOBuf, dev->dma_buffer, 64);
					break;

				case CMD_READ_CH2:	/* write channel 2 buffer */
					dev->DataReplyLeft = 0;
					Address.hi = dev->CmdBuf[0];
					Address.mid = dev->CmdBuf[1];
					Address.lo = dev->CmdBuf[2];
					FIFOBuf = ADDR_TO_U32(Address);
					DEBUG("Adaptec LocalRAM: Writing 64 bytes at %08X\n", FIFOBuf);
					DMAPageWrite(FIFOBuf, dev->dma_buffer, 64);
					break;

				case CMD_OPTIONS: /* Set adapter options */
					if (dev->CmdParam == 1)
						dev->CmdParamLeft = dev->CmdBuf[0];
					dev->DataReplyLeft = 0;
					break;

				default:
					if (dev->ven_cmds)
						suppress = dev->ven_cmds(dev);
					else {
						dev->DataReplyLeft = 0;
						dev->Status |= STAT_INVCMD;
					}
					break;
			}
		}

		if (dev->DataReplyLeft)
			dev->Status |= STAT_DFULL;
		else if (!dev->CmdParamLeft)
			cmd_done(dev, suppress);
		break;

	case 2:
		if (dev->flags & X54X_INT_GEOM_WRITABLE)
			dev->Interrupt = val;
		break;

	case 3:
		if (dev->flags & X54X_INT_GEOM_WRITABLE)
			dev->Geometry = val;
		break;
    }
}


static void
x54x_outw(uint16_t port, uint16_t val, priv_t priv)
{
    x54x_out(port, val & 0xFF, priv);
}


static void
x54x_outl(uint16_t port, uint32_t val, priv_t priv)
{
    x54x_out(port, val & 0xFF, priv);
}


static void
x54x_write(uint32_t port, uint8_t val, priv_t priv)
{
    x54x_out(port & 3, val, priv);
}


static void
x54x_writew(uint32_t port, uint16_t val, priv_t priv)
{
    x54x_outw(port & 3, val, priv);
}


static void
x54x_writel(uint32_t port, uint32_t val, priv_t priv)
{
    x54x_outl(port & 3, val, priv);
}


void
x54x_io_set(x54x_t *dev, uint32_t base, uint8_t len)
{
    int bit32 = 0;

    if (dev->bus & DEVICE_PCI)
	bit32 = 1;
    else if ((dev->bus & DEVICE_MCA) && (dev->flags & X54X_32BIT))
	bit32 = 1;

    if (bit32) {
	DEBUG("x54x: [PCI] Setting I/O handler at %04X\n", base);
	io_sethandler(base, len,
		      x54x_in, x54x_inw, x54x_inl,
                      x54x_out, x54x_outw, x54x_outl, dev);
    } else {
	DEBUG("x54x: [ISA] Setting I/O handler at %04X\n", base);
	io_sethandler(base, len,
		      x54x_in, x54x_inw, NULL,
                      x54x_out, x54x_outw, NULL, dev);
    }
}


void
x54x_io_remove(x54x_t *dev, uint32_t base, uint8_t len)
{
    int bit32 = 0;

    if (dev->bus & DEVICE_PCI)
	bit32 = 1;
    else if ((dev->bus & DEVICE_MCA) && (dev->flags & X54X_32BIT))
	bit32 = 1;

    DEBUG("x54x: Removing I/O handler at %04X\n", base);

    if (bit32) {
	io_removehandler(base, len,
		      x54x_in, x54x_inw, x54x_inl,
                      x54x_out, x54x_outw, x54x_outl, dev);
    } else {
	io_removehandler(base, len,
		      x54x_in, x54x_inw, NULL,
                      x54x_out, x54x_outw, NULL, dev);
    }
}


void
x54x_mem_init(x54x_t *dev, uint32_t addr)
{
    int bit32 = 0;

    if (dev->bus & DEVICE_PCI)
	bit32 = 1;
    else if ((dev->bus & DEVICE_MCA) && (dev->flags & X54X_32BIT))
	bit32 = 1;

    if (bit32) {
	mem_map_add(&dev->mmio_mapping, addr, 0x20,
		    x54x_read, x54x_readw, x54x_readl,
		    x54x_write, x54x_writew, x54x_writel,
		    NULL, MEM_MAPPING_EXTERNAL, dev);
    } else {
	mem_map_add(&dev->mmio_mapping, addr, 0x20,
		    x54x_read, x54x_readw, NULL,
		    x54x_write, x54x_writew, NULL,
		    NULL, MEM_MAPPING_EXTERNAL, dev);
    }
}


void
x54x_mem_enable(x54x_t *dev)
{
    mem_map_enable(&dev->mmio_mapping);
}


void
x54x_mem_set_addr(x54x_t *dev, uint32_t base)
{
    mem_map_set_addr(&dev->mmio_mapping, base, 0x20);
}


void
x54x_mem_disable(x54x_t *dev)
{
    mem_map_disable(&dev->mmio_mapping);
}


void
x54x_close(priv_t priv)
{
    x54x_t *dev = (x54x_t *)priv;

    if (dev) {
	/* Tell the timer to terminate. */
	dev->timer_period = 0LL;

	dev->MailboxInit = dev->BIOSMailboxInit = 0;
	dev->MailboxCount = dev->BIOSMailboxCount = 0;
	dev->MailboxReq = dev->BIOSMailboxReq = 0;

	if (dev->ven_data)
		free(dev->ven_data);

	if (dev->nvr != NULL)
		free(dev->nvr);

	free(dev);
	dev = NULL;
    }
}


/* General initialization routine for all boards. */
priv_t
x54x_init(const device_t *info)
{
    x54x_t *dev;

    /* Allocate control block and set up basic stuff. */
    dev = (x54x_t *)mem_alloc(sizeof(x54x_t));
    if (dev == NULL) return(dev);
    memset(dev, 0x00, sizeof(x54x_t));
    dev->type = info->local;

    dev->bus = info->flags;
    dev->callback_phase = 0;

    timer_add(x54x_reset_poll, dev, &dev->ResetCB, &dev->ResetCB);
    dev->timer_period = 10LL * TIMER_USEC;
    timer_add(cmd_callback, dev, &dev->timer_period, TIMER_ALWAYS_ENABLED);

    return(dev);
}


void
x54x_device_reset(x54x_t *dev)
{
    x54x_reset_ctrl(dev, 1);

    dev->ResetCB = 0LL;
    dev->Status = STAT_IDLE | STAT_INIT;
}
