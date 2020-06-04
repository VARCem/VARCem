/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of BusLogic ISA and PCI SCSI controllers. Boards
 *		supported:
 *
 *		  0 - BT-542BH ISA;
 *		  1 - BT-545S ISA;
 *		  2 - BT-958D PCI
 *
 * Version:	@(#)scsi_buslogic.c	1.0.18	2020/06/01
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#define dbglog scsi_card_log
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
#include "../system/pci.h"
#include "../system/mca.h"
#include "scsi.h"
#include "scsi_buslogic.h"
#include "scsi_device.h"
#include "scsi_x54x.h"


#define BT542_BIOS_PATH		L"scsi/buslogic/bt-542b_bios.rom"
#define BT542B_BIOS_PATH	L"scsi/buslogic/bt-542bh_bios.rom"
#define BT545_BIOS_PATH		L"scsi/buslogic/bt-545s_bios.rom"
#define BT545_AUTO_BIOS_PATH	L"scsi/buslogic/bt-545s_autoscsi.rom"
#define BT640A_BIOS_PATH	L"scsi/buslogic/bt-640a_bios.rom"
#define BT445S_BIOS_PATH	L"scsi/buslogic/bt-445s_bios.rom"
#define BT445S_AUTO_BIOS_PATH	L"scsi/buslogic/bt-445s_autoscsi.rom"
#define BT445S_SCAM_BIOS_PATH	L"scsi/buslogic/bt-445s_scam.rom"
#define BT958D_BIOS_PATH	L"scsi/buslogic/bt-958d_bios.rom"
#define BT958D_AUTO_BIOS_PATH	L"scsi/buslogic/bt-958d_autoscsi.rom"
#define BT958D_SCAM_BIOS_PATH	L"scsi/buslogic/bt-958d_scam.rom"


enum {
	CHIP_BUSLOGIC_ISA_542_1991,
    CHIP_BUSLOGIC_ISA_542,
    CHIP_BUSLOGIC_ISA,
    CHIP_BUSLOGIC_MCA,
    CHIP_BUSLOGIC_EISA,
    CHIP_BUSLOGIC_VLB,
    CHIP_BUSLOGIC_PCI
};


/*
 * Auto SCSI structure which is located in
 * host adapter RAM and contains several
 * configuration parameters.
 */
#pragma pack(push,1)
typedef struct {
    uint8_t	aInternalSignature[2];
    uint8_t	cbInformation;
    uint8_t	aHostAdaptertype[6];
    uint8_t	uReserved1;
    uint8_t	fFloppyEnabled			: 1,
		fFloppySecondary		: 1,
		fLevelSensitiveInterrupt	: 1,
		uReserved2			: 2,
		uSystemRAMAreForBIOS		: 3;
    uint8_t	uDMAChannel			: 7,
		fDMAAutoConfiguration		: 1,
		uIrqChannel			: 7,
		fIrqAutoConfiguration		: 1;
    uint8_t	uDMATransferRate;
    uint8_t	uSCSIId;
    uint8_t	uSCSIConfiguration;
    uint8_t	uBusOnDelay;
    uint8_t	uBusOffDelay;
    uint8_t	uBIOSConfiguration;
    uint16_t	u16DeviceEnabledMask;
    uint16_t	u16WidePermittedMask;
    uint16_t	u16FastPermittedMask;
    uint16_t	u16SynchronousPermittedMask;
    uint16_t	u16DisconnectPermittedMask;
    uint16_t	u16SendStartUnitCommandMask;
    uint16_t	u16IgnoreInBIOSScanMask;
    uint8_t	uPCIInterruptPin		: 2,
		uHostAdapterIoPortAddress	: 2,
		fRoundRobinScheme		: 1,
		fVesaBusSpeedGreaterThan33MHz	: 1,
		fVesaBurstWrite			: 1,
		fVesaBurstRead			: 1;
    uint16_t	u16UltraPermittedMask;
    uint32_t	uReserved5;
    uint8_t	uReserved6;
    uint8_t	uAutoSCSIMaximumLUN;
    uint8_t	fReserved7			: 1,
		fSCAMDominant			: 1,
		fSCAMenabled			: 1,
		fSCAMLevel2			: 1,
		uReserved8			: 4;
    uint8_t	fInt13Extension			: 1,
		fReserved9			: 1,
		fCDROMBoot			: 1,
		uReserved10			: 2,
		fMultiBoot			: 1,
		uReserved11			: 2;
    uint8_t	uBootTargetId			: 4,
		uBootChannel			: 4;
    uint8_t	fForceBusDeviceScanningOrder	: 1,
		uReserved12			: 7;
    uint16_t	u16NonTaggedToAlternateLunPermittedMask;
    uint16_t	u16RenegotiateSyncAfterCheckConditionMask;
    uint8_t	aReserved14[10];
    uint8_t	aManufacturingDiagnostic[2];
    uint16_t	u16Checksum;
} AutoSCSIRam;
#pragma pack(pop)

/* The local RAM. */
#pragma pack(push,1)
typedef union {
    uint8_t u8View[256];		/* byte view */
    struct {				/* structured view */
        uint8_t     u8Bios[64];		/* offset 0 - 63 is for BIOS */
        AutoSCSIRam autoSCSIData;	/* Auto SCSI structure */
    } structured;
} HALocalRAM;
#pragma pack(pop)

/** Structure for the INQUIRE_SETUP_INFORMATION reply. */
#pragma pack(push,1)
typedef struct {
    uint8_t	uSignature;
    uint8_t	uCharacterD;
    uint8_t	uHostBusType;
    uint8_t	uWideTransferPermittedId0To7;
    uint8_t	uWideTransfersActiveId0To7;
    ReplyInquireSetupInformationSynchronousValue SynchronousValuesId8To15[8];
    uint8_t	uDisconnectPermittedId8To15;
    uint8_t	uReserved2;
    uint8_t	uWideTransferPermittedId8To15;
    uint8_t	uWideTransfersActiveId8To15;
} buslogic_setup_t;
#pragma pack(pop)

/* Structure for the INQUIRE_EXTENDED_SETUP_INFORMATION. */
#pragma pack(push,1)
typedef struct {
    uint8_t	uBusType;
    uint8_t	uBiosAddress;
    uint16_t	u16ScatterGatherLimit;
    uint8_t	cMailbox;
    uint32_t	uMailboxAddressBase;
    uint8_t	uReserved1		:2,
		fFastEISA		:1,
		uReserved2		:3,
		fLevelSensitiveInterrupt:1,
		uReserved3		:1;
    uint8_t	aFirmwareRevision[3];
    uint8_t	fHostWideSCSI		:1,
		fHostDifferentialSCSI	:1,
		fHostSupportsSCAM	:1,
		fHostUltraSCSI		:1,
		fHostSmartTermination	:1,
		uReserved4		:3;
} ReplyInquireExtendedSetupInformation;
#pragma pack(pop)

/* Structure for the INQUIRE_PCI_HOST_ADAPTER_INFORMATION reply. */
#pragma pack(push,1)
typedef struct {
    uint8_t	IsaIOPort;
    uint8_t	IRQ;
    uint8_t	LowByteTerminated	:1,
		HighByteTerminated	:1,
		uReserved		:2,	/* Reserved. */
		JP1			:1,	/* Whatever that means. */
		JP2			:1,	/* Whatever that means. */
		JP3			:1,	/* Whatever that means. */
		InformationIsValid	:1;
    uint8_t	uReserved2;			/* Reserved. */
} BuslogicPCIInformation_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint32_t	DataLength;		/* data length */
    uint32_t	DataPointer;		/* data pointer */
    uint8_t	TargetId;		/* device request is sent to */
    uint8_t	LogicalUnit;		/* LUN in the device */
    uint8_t	Reserved1	: 3,	/* reserved */
		DataDirection	: 2,	/* data direction for the request */
		Reserved2	: 3;	/* reserved */
    uint8_t	CDBLength;		/* length of the SCSI CDB */
    uint8_t	CDB[12];		/* the SCSI CDB */
} ESCMD;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint8_t	Count;
    uint32_t	Address;
} MailboxInitExtended_t;
#pragma pack(pop)

typedef struct {
    rom_t	bios;
    int		ExtendedLUNCCBFormat;
    int		fAggressiveRoundRobinMode;
    HALocalRAM	LocalRAM;
    int		PCIBase;
    int		MMIOBase;
    int		chip;
    int		has_bios;
    uint32_t	bios_addr,
		bios_size,
		bios_mask;
    uint8_t     AutoSCSIROM[32768];
    uint8_t     SCAMData[65536];
} buslogic_data_t;


static wchar_t *
GetNVRFileName(buslogic_data_t *bl)
{
    switch(bl->chip) {

	case CHIP_BUSLOGIC_ISA_542_1991:
		return L"bt542b.nvr";

	case CHIP_BUSLOGIC_ISA_542:
		return L"bt542bh.nvr";

	case CHIP_BUSLOGIC_ISA:
		return L"bt545s.nvr";

	case CHIP_BUSLOGIC_MCA:
		return L"bt640a.nvr";

	case CHIP_BUSLOGIC_VLB:
		return L"bt445s.nvr";

	case CHIP_BUSLOGIC_PCI:
		return L"bt958d.nvr";

	default:
		fatal("Unrecognized BusLogic chip: %i\n", bl->chip);
		break;
    }

    return NULL;
}


static void
AutoSCSIRamSetDefaults(x54x_t *dev, uint8_t safe)
{
    buslogic_data_t *bl = (buslogic_data_t *) dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;

    memset(&(halr->structured.autoSCSIData), 0, sizeof(AutoSCSIRam));

    halr->structured.autoSCSIData.aInternalSignature[0] = 'F';
    halr->structured.autoSCSIData.aInternalSignature[1] = 'A';

    halr->structured.autoSCSIData.cbInformation = 64;

    halr->structured.autoSCSIData.uReserved1 = 6;

    halr->structured.autoSCSIData.aHostAdaptertype[0] = ' ';
    halr->structured.autoSCSIData.aHostAdaptertype[5] = ' ';
    switch (bl->chip) {
	case CHIP_BUSLOGIC_ISA_542_1991:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "542B", 4);
		break;
	
	case CHIP_BUSLOGIC_ISA_542:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "542BH", 5);
		break;

	case CHIP_BUSLOGIC_ISA:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "545S", 4);
		break;

	case CHIP_BUSLOGIC_MCA:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "640A", 4);
		break;

	case CHIP_BUSLOGIC_VLB:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "445S", 4);
		break;

	case CHIP_BUSLOGIC_PCI:
		memcpy(&(halr->structured.autoSCSIData.aHostAdaptertype[1]), "958D", 4);
		break;
    }

    halr->structured.autoSCSIData.fLevelSensitiveInterrupt = (bl->chip == CHIP_BUSLOGIC_PCI) ? 1 : 0;
    halr->structured.autoSCSIData.uSystemRAMAreForBIOS = 6;

    if (bl->chip != CHIP_BUSLOGIC_PCI) {
	switch(dev->DmaChannel) {
		case 5:
			halr->structured.autoSCSIData.uDMAChannel = 1;
			break;

		case 6:
			halr->structured.autoSCSIData.uDMAChannel = 2;
			break;

		case 7:
			halr->structured.autoSCSIData.uDMAChannel = 3;
			break;

		default:

			halr->structured.autoSCSIData.uDMAChannel = 0;
			break;
	}
    }
    halr->structured.autoSCSIData.fDMAAutoConfiguration = (bl->chip == CHIP_BUSLOGIC_PCI) ? 0 : 1;

    if (bl->chip != CHIP_BUSLOGIC_PCI) {
	switch(dev->Irq) {
		case 9:
			halr->structured.autoSCSIData.uIrqChannel = 1;
			break;

		case 10:
			halr->structured.autoSCSIData.uIrqChannel = 2;
			break;

		case 11:
			halr->structured.autoSCSIData.uIrqChannel = 3;
			break;

		case 12:
			halr->structured.autoSCSIData.uIrqChannel = 4;
			break;

		case 14:
			halr->structured.autoSCSIData.uIrqChannel = 5;
			break;

		case 15:
			halr->structured.autoSCSIData.uIrqChannel = 6;
			break;

		default:
			halr->structured.autoSCSIData.uIrqChannel = 0;
			break;
	}
    }
    halr->structured.autoSCSIData.fIrqAutoConfiguration = (bl->chip == CHIP_BUSLOGIC_PCI) ? 0 : 1;

    halr->structured.autoSCSIData.uDMATransferRate = (bl->chip == CHIP_BUSLOGIC_PCI) ? 0 : 1;

    halr->structured.autoSCSIData.uSCSIId = 7;
    halr->structured.autoSCSIData.uSCSIConfiguration = 0x3F;
    halr->structured.autoSCSIData.uBusOnDelay = (bl->chip == CHIP_BUSLOGIC_PCI) ? 0 : 7;
    halr->structured.autoSCSIData.uBusOffDelay = (bl->chip == CHIP_BUSLOGIC_PCI) ? 0 : 4;
    halr->structured.autoSCSIData.uBIOSConfiguration = (bl->has_bios) ? 0x33 : 0x32;
    if (!safe)
	halr->structured.autoSCSIData.uBIOSConfiguration |= 0x04;

    halr->structured.autoSCSIData.u16DeviceEnabledMask = 0xffff;
    halr->structured.autoSCSIData.u16WidePermittedMask = 0xffff;
    halr->structured.autoSCSIData.u16FastPermittedMask = 0xffff;
    halr->structured.autoSCSIData.u16DisconnectPermittedMask = 0xffff;

    halr->structured.autoSCSIData.uPCIInterruptPin = PCI_INTA;
    halr->structured.autoSCSIData.fVesaBusSpeedGreaterThan33MHz = 1;

    halr->structured.autoSCSIData.uAutoSCSIMaximumLUN = 7;

    halr->structured.autoSCSIData.fForceBusDeviceScanningOrder = 1;
    halr->structured.autoSCSIData.fInt13Extension = safe ? 0 : 1;
    halr->structured.autoSCSIData.fCDROMBoot = safe ? 0 : 1;
    halr->structured.autoSCSIData.fMultiBoot = safe ? 0 : 1;
    halr->structured.autoSCSIData.fRoundRobinScheme = safe ? 1 : 0;	/* 1 = aggressive, 0 = strict */

    halr->structured.autoSCSIData.uHostAdapterIoPortAddress = 2;	/* 0 = primary (330h), 1 = secondary (334h), 2 = disable, 3 = reserved */
}


static void
InitializeAutoSCSIRam(x54x_t *dev)
{
    buslogic_data_t *bl = (buslogic_data_t *) dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;

    FILE *f;

    f = plat_fopen(nvr_path(GetNVRFileName(bl)), L"rb");
    if (f != NULL) {
	(void)fread(&(bl->LocalRAM.structured.autoSCSIData), 1, 64, f);
	fclose(f);
	f = NULL;
	if (bl->chip == CHIP_BUSLOGIC_PCI) {
		x54x_io_remove(dev, dev->Base, 4);
		switch(halr->structured.autoSCSIData.uHostAdapterIoPortAddress) {
			case 0:
				dev->Base = 0x330;
				break;

			case 1:
				dev->Base = 0x334;
				break;

			default:
				dev->Base = 0;
				break;
		}
		x54x_io_set(dev, dev->Base, 4);
	}
    } else {
	AutoSCSIRamSetDefaults(dev, 0);
    }
}


static void
cmd_phase1(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    if ((dev->CmdParam == 2) && (dev->Command == 0x90)) {
	dev->CmdParamLeft = dev->CmdBuf[1];
    }

    if ((dev->CmdParam == 10) && ((dev->Command == 0x97) || (dev->Command == 0xA7))) {
	dev->CmdParamLeft = dev->CmdBuf[6];
	dev->CmdParamLeft <<= 8;
	dev->CmdParamLeft |= dev->CmdBuf[7];
	dev->CmdParamLeft <<= 8;
	dev->CmdParamLeft |= dev->CmdBuf[8];
    }

    if ((dev->CmdParam == 4) && (dev->Command == 0xA9)) {
	dev->CmdParamLeft = dev->CmdBuf[3];
	dev->CmdParamLeft <<= 8;
	dev->CmdParamLeft |= dev->CmdBuf[2];
    }
}


static uint8_t
get_host_id(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;

    if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991))
		return dev->HostID;
    else
		return halr->structured.autoSCSIData.uSCSIId;
}


static uint8_t
get_irq(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    uint8_t bl_irq[7] = { 0, 9, 10, 11, 12, 14, 15 };
    HALocalRAM *halr = &bl->LocalRAM;

    if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991) || (bl->chip == CHIP_BUSLOGIC_PCI))
	return dev->Irq;
    else
	return bl_irq[halr->structured.autoSCSIData.uIrqChannel];
}


static uint8_t
get_dma(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    uint8_t bl_dma[4] = { 0, 5, 6, 7 };
    HALocalRAM *halr = &bl->LocalRAM;

    if (bl->chip == CHIP_BUSLOGIC_PCI)
		return (dev->Base ? 7 : 0);
    else if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991))
		return dev->DmaChannel;
    else
		return bl_dma[halr->structured.autoSCSIData.uDMAChannel];
}


static uint8_t
param_len(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;

    switch (dev->Command) {
	case 0x21:
		return 5;

	case 0x25:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8F:
	case 0x92:
	case 0x96:
		return 1;

	case 0x81:
		return sizeof(MailboxInitExtended_t);

	case 0x83:
		return 12;

	case 0x90:	
	case 0x91:
		return 2;

	case 0x94:
	case 0xFB:
		return 3;

	case 0x93: /* Valid only for VLB */
		return (bl->chip == CHIP_BUSLOGIC_VLB) ? 1 : 0;

	case 0x95: /* Valid only for PCI */
		return (bl->chip == CHIP_BUSLOGIC_PCI) ? 1 : 0;

	case 0x97: /* Valid only for PCI */
	case 0xA7: /* Valid only for PCI */
		return (bl->chip == CHIP_BUSLOGIC_PCI) ? 10 : 0;

	case 0xA8: /* Valid only for PCI */
	case 0xA9: /* Valid only for PCI */
		return (bl->chip == CHIP_BUSLOGIC_PCI) ? 4 : 0;
	default:
		break;
    }

    return 0;
}


static void
SCSIBIOSDMATransfer(ESCMD *ESCSICmd, uint8_t TargetID, uint8_t LUN, int dir)
{
    uint32_t DataPointer = ESCSICmd->DataPointer;
    int DataLength = ESCSICmd->DataLength;
    uint32_t Address;
    uint32_t TransferLength;
    scsi_device_t *dev = &scsi_devices[TargetID][LUN];

    if (ESCSICmd->DataDirection == 0x03) {
	/* Non-data command. */
	DEBUG("SCSIBIOSDMATransfer(): Non-data control byte\n");
	return;
    }

    DEBUG("SCSIBIOSDMATransfer(): BIOS Data Buffer read: length %d, pointer 0x%04X\n", DataLength, DataPointer);

    /* If the control byte is 0x00, it means that the transfer direction is set up by the SCSI command without
       checking its length, so do this procedure for both read/write commands. */
    if ((DataLength > 0) && (dev->buffer_length > 0)) {
	Address = DataPointer;
	TransferLength = MIN(DataLength, dev->buffer_length);

	if (dir && ((ESCSICmd->DataDirection == CCB_DATA_XFER_OUT) || (ESCSICmd->DataDirection == 0x00))) {
		DEBUG("BusLogic BIOS DMA: Reading %i bytes from %08X\n", TransferLength, Address);
		DMAPageRead(Address, (uint8_t *)dev->cmd_buffer, TransferLength);
	} else if (!dir && ((ESCSICmd->DataDirection == CCB_DATA_XFER_IN) || (ESCSICmd->DataDirection == 0x00))) {
		DEBUG("BusLogic BIOS DMA: Writing %i bytes at %08X\n", TransferLength, Address);
		DMAPageWrite(Address, (uint8_t *)dev->cmd_buffer, TransferLength);
	}
    }
}


static void
SCSIBIOSRequestSetup(x54x_t *dev, uint8_t *CmdBuf, uint8_t *DataInBuf, uint8_t DataReply)
{	
    ESCMD *ESCSICmd = (ESCMD *)CmdBuf;
    scsi_device_t *sd = &scsi_devices[ESCSICmd->TargetId][ESCSICmd->LogicalUnit];
    uint32_t i;
    uint8_t temp_cdb[12];
    int target_cdb_len = 12;
    int phase;

    DataInBuf[0] = DataInBuf[1] = 0;

    if ((ESCSICmd->TargetId > 15) || (ESCSICmd->LogicalUnit > 7)) {
	DataInBuf[2] = CCB_INVALID_CCB;
	DataInBuf[3] = SCSI_STATUS_OK;
	return;
    }

    DEBUG("Scanning SCSI Target ID %i\n", ESCSICmd->TargetId);		

    sd->status = SCSI_STATUS_OK;

    if (! scsi_device_present(sd)) {
	DEBUG("SCSI Target ID %i has no device attached\n", ESCSICmd->TargetId);
	DataInBuf[2] = CCB_SELECTION_TIMEOUT;
	DataInBuf[3] = SCSI_STATUS_OK;
    } else {
	DEBUG("SCSI Target ID %i detected and working\n", ESCSICmd->TargetId);

	DEBUG("Transfer Control %02X\n", ESCSICmd->DataDirection);
	DEBUG("CDB Length %i\n", ESCSICmd->CDBLength);	
	if (ESCSICmd->DataDirection > 0x03) {
		DEBUG("Invalid control byte: %02X\n", ESCSICmd->DataDirection);
	}
    }

    x54x_buf_alloc(sd, ESCSICmd->DataLength);

    target_cdb_len = 12;

    if (! scsi_device_valid(sd))
	fatal("SCSI target on ID %i:%i has disappeared\n", ESCSICmd->TargetId, ESCSICmd->LogicalUnit);

    DEBUG("SCSI target command being executed on: ID %i, LUN %i\n", ESCSICmd->TargetId, ESCSICmd->LogicalUnit);

    DEBUG("SCSI Cdb[0]=0x%02X\n", ESCSICmd->CDB[0]);
    for (i = 1; i < ESCSICmd->CDBLength; i++) {
	DEBUG("SCSI Cdb[%i]=%i\n", i, ESCSICmd->CDB[i]);
    }

    memset(temp_cdb, 0, target_cdb_len);
    if (ESCSICmd->CDBLength <= target_cdb_len)
	memcpy(temp_cdb, ESCSICmd->CDB, ESCSICmd->CDBLength);
      else
	memcpy(temp_cdb, ESCSICmd->CDB, target_cdb_len);

    sd->buffer_length = ESCSICmd->DataLength;
    scsi_device_command_phase0(sd, temp_cdb);

    phase = sd->phase;
    if (phase != SCSI_PHASE_STATUS) {
	if (phase == SCSI_PHASE_DATA_IN)
		scsi_device_command_phase1(sd);
	SCSIBIOSDMATransfer(ESCSICmd, ESCSICmd->TargetId, ESCSICmd->LogicalUnit, (phase == SCSI_PHASE_DATA_OUT));
	if (phase == SCSI_PHASE_DATA_OUT)
		scsi_device_command_phase1(sd);
    }

    x54x_buf_free(sd);

    DEBUG("BIOS Request complete\n");

    if (sd->status == SCSI_STATUS_OK) {
	DataInBuf[2] = CCB_COMPLETE;
	DataInBuf[3] = SCSI_STATUS_OK;
    } else if (sd->status == SCSI_STATUS_CHECK_CONDITION) {
	DataInBuf[2] = CCB_COMPLETE;
	DataInBuf[3] = SCSI_STATUS_CHECK_CONDITION;			
    }

    dev->DataReplyLeft = DataReply;	
}


static uint8_t
buslogic_cmds(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;
    FILE *f;
    uint16_t TargetsPresentMask = 0;
    uint32_t Offset;
    int i, j;
    MailboxInitExtended_t *MailboxInitE;
    ReplyInquireExtendedSetupInformation *ReplyIESI;
    BuslogicPCIInformation_t *ReplyPI;
    int cCharsToTransfer;

    switch (dev->Command) {
	case 0x20:
		dev->DataReplyLeft = 0;
		x54x_reset_ctrl(dev, 1);
		break;

	case 0x21:
		if (dev->CmdParam == 1)
			dev->CmdParamLeft = dev->CmdBuf[0];
		dev->DataReplyLeft = 0;
		break;

	case 0x23:
		memset(dev->DataBuf, 0, 8);
		for (i = 8; i < 15; i++) {
		    dev->DataBuf[i - 8] = 0;
		    for (j = 0; j < 8; j++) {
		    	if (scsi_device_present(&scsi_devices[i][j]) &&
					(i != get_host_id(dev)))
				dev->DataBuf[i - 8] |= (1 << j);
		    }
		}
		dev->DataReplyLeft = 8;
		break;

	case 0x24:						
		for (i = 0; i < 15; i++) {
			if (scsi_device_present(&scsi_devices[i][0]) &&
						(i != get_host_id(dev)))
			    TargetsPresentMask |= (1 << i);
		}
		dev->DataBuf[0] = TargetsPresentMask & 0xff;
		dev->DataBuf[1] = TargetsPresentMask >> 8;
		dev->DataReplyLeft = 2;
		break;

	case 0x25:
		if (dev->CmdBuf[0] == 0)
			dev->IrqEnabled = 0;
		else
			dev->IrqEnabled = 1;
		return 1;

	case 0x81:
		dev->flags &= ~X54X_MBX_24BIT;

		MailboxInitE = (MailboxInitExtended_t *)dev->CmdBuf;

		dev->MailboxInit = 1;
		dev->MailboxCount = MailboxInitE->Count;
		dev->MailboxOutAddr = MailboxInitE->Address;
		dev->MailboxInAddr = MailboxInitE->Address + (dev->MailboxCount * sizeof(Mailbox32_t));

		DEBUG("Buslogic Extended Initialize Mailbox Command\n");
		DEBUG("Mailbox Out Address=0x%08X\n", dev->MailboxOutAddr);
		DEBUG("Mailbox In Address=0x%08X\n", dev->MailboxInAddr);
		DEBUG("Initialized Extended Mailbox, %d entries at 0x%08X\n",
		      MailboxInitE->Count, MailboxInitE->Address);

		dev->Status &= ~STAT_INIT;
		dev->DataReplyLeft = 0;
		break;

	case 0x83:
		if (dev->CmdParam == 12) {
			dev->CmdParamLeft = dev->CmdBuf[11];
			DEBUG("Execute SCSI BIOS Command: %u more bytes follow\n", dev->CmdParamLeft);
		} else {
			DEBUG("Execute SCSI BIOS Command: received %u bytes\n", dev->CmdBuf[0]);
			SCSIBIOSRequestSetup(dev, dev->CmdBuf, dev->DataBuf, 4);				
		}
		break;

	case 0x84:
		dev->DataBuf[0] = dev->fw_rev[4];
		dev->DataReplyLeft = 1;
		break;				

	case 0x85:
		if (strlen(dev->fw_rev) == 6)
			dev->DataBuf[0] = dev->fw_rev[5];
		else
			dev->DataBuf[0] = ' ';
		dev->DataReplyLeft = 1;
		break;

	case 0x86:
		if (bl->chip == CHIP_BUSLOGIC_PCI) {
			ReplyPI = (BuslogicPCIInformation_t *) dev->DataBuf;
			memset(ReplyPI, 0, sizeof(BuslogicPCIInformation_t));
			ReplyPI->InformationIsValid = 0;
			switch(dev->Base) {
				case 0x330:
					ReplyPI->IsaIOPort = 0;
					break;

				case 0x334:
					ReplyPI->IsaIOPort = 1;
					break;

				case 0x230:
					ReplyPI->IsaIOPort = 2;
					break;

				case 0x234:
					ReplyPI->IsaIOPort = 3;
					break;

				case 0x130:
					ReplyPI->IsaIOPort = 4;
					break;

				case 0x134:
					ReplyPI->IsaIOPort = 5;
					break;

				default:
					ReplyPI->IsaIOPort = 6;
					break;
			}
			ReplyPI->IRQ = dev->Irq;
			dev->DataReplyLeft = sizeof(BuslogicPCIInformation_t);
		} else {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;					
		}
		break;

	case 0x8B:
		/* The reply length is set by the guest and is found in the first byte of the command buffer. */
		dev->DataReplyLeft = dev->CmdBuf[0];
		memset(dev->DataBuf, 0, dev->DataReplyLeft);
		if (bl->chip == CHIP_BUSLOGIC_ISA_542)
			i = 5;
		else
			i = 4;
		cCharsToTransfer = MIN(dev->DataReplyLeft, i);

		memcpy(dev->DataBuf, &(bl->LocalRAM.structured.autoSCSIData.aHostAdaptertype[1]), cCharsToTransfer);
		break;

	case 0x8C:
		dev->DataReplyLeft = dev->CmdBuf[0];
		memset(dev->DataBuf, 0, dev->DataReplyLeft);
		break;

	case 0x8D:
		dev->DataReplyLeft = dev->CmdBuf[0];
		ReplyIESI = (ReplyInquireExtendedSetupInformation *)dev->DataBuf;
		memset(ReplyIESI, 0, sizeof(ReplyInquireExtendedSetupInformation));

		switch (bl->chip) {
			case CHIP_BUSLOGIC_ISA_542_1991:
			case CHIP_BUSLOGIC_ISA_542:
			case CHIP_BUSLOGIC_ISA:
			case CHIP_BUSLOGIC_VLB:
				ReplyIESI->uBusType = 'A';	/* ISA style */
				break;

			case CHIP_BUSLOGIC_MCA:
				ReplyIESI->uBusType = 'M';	/* MCA style */
				break;

			case CHIP_BUSLOGIC_PCI:
				ReplyIESI->uBusType = 'E';	/* PCI style */
				break;
		}
		ReplyIESI->uBiosAddress = 0xd8;
		ReplyIESI->u16ScatterGatherLimit = 8192;
		ReplyIESI->cMailbox = dev->MailboxCount;
		ReplyIESI->uMailboxAddressBase = dev->MailboxOutAddr;
		ReplyIESI->fHostWideSCSI = 1;						  /* This should be set for the BT-542B as well. */
		if ((bl->chip != CHIP_BUSLOGIC_ISA_542) && (bl->chip != CHIP_BUSLOGIC_ISA_542_1991) && (bl->chip != CHIP_BUSLOGIC_MCA))
			ReplyIESI->fLevelSensitiveInterrupt = bl->LocalRAM.structured.autoSCSIData.fLevelSensitiveInterrupt;
		if (bl->chip == CHIP_BUSLOGIC_PCI)
			ReplyIESI->fHostUltraSCSI = 1;
		memcpy(ReplyIESI->aFirmwareRevision, &(dev->fw_rev[strlen(dev->fw_rev) - 3]), sizeof(ReplyIESI->aFirmwareRevision));
		DEBUG("Return Extended Setup Information: %d\n", dev->CmdBuf[0]);
		break;

	case 0x8F:
		bl->fAggressiveRoundRobinMode = dev->CmdBuf[0] & 1;

		dev->DataReplyLeft = 0;
		break;

	case 0x90:	
		DEBUG("Store Local RAM\n");
		Offset = dev->CmdBuf[0];
		dev->DataReplyLeft = 0;
		memcpy(&(bl->LocalRAM.u8View[Offset]), &(dev->CmdBuf[2]), dev->CmdBuf[1]);
	
		dev->DataReply = 0;
		break;

	case 0x91:
		DEBUG("Fetch Local RAM\n");
		Offset = dev->CmdBuf[0];
		dev->DataReplyLeft = dev->CmdBuf[1];
		memcpy(dev->DataBuf, &(bl->LocalRAM.u8View[Offset]), dev->CmdBuf[1]);

		dev->DataReply = 0;
		break;

	case 0x93:
		if (bl->chip != CHIP_BUSLOGIC_VLB) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
			break;
		}

	case 0x92:
		if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991) || (bl->chip == CHIP_BUSLOGIC_MCA)) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
			break;
		}

		dev->DataReplyLeft = 0;

		switch (dev->CmdBuf[0]) {
			case 0:
			case 2:
				AutoSCSIRamSetDefaults(dev, 0);
				break;

			case 3:
				AutoSCSIRamSetDefaults(dev, 3);
				break;

			case 1:
				f = plat_fopen(nvr_path(GetNVRFileName(bl)), L"wb");
				if (f != NULL) {
					(void)fwrite(&(bl->LocalRAM.structured.autoSCSIData), 1, 64, f);
					fclose(f);
					f = NULL;
				}
				break;

			default:
				dev->Status |= STAT_INVCMD;
				break;
		}

		if ((bl->chip == CHIP_BUSLOGIC_PCI) && !(dev->Status & STAT_INVCMD)) {
			x54x_io_remove(dev, dev->Base, 4);
			switch(halr->structured.autoSCSIData.uHostAdapterIoPortAddress) {
				case 0:
					dev->Base = 0x330;
					break;

				case 1:
					dev->Base = 0x334;
					break;

				default:
					dev->Base = 0;
					break;
			}
			x54x_io_set(dev, dev->Base, 4);
		}
		break;

	case 0x94:
		if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991) || (bl->chip == CHIP_BUSLOGIC_MCA)) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
			break;
		}

		if (dev->CmdBuf[0]) {
			DEBUG("Invalid AutoSCSI command mode %x\n", dev->CmdBuf[0]);
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
		} else {
			dev->DataReplyLeft = dev->CmdBuf[2];
			dev->DataReplyLeft <<= 8;
			dev->DataReplyLeft |= dev->CmdBuf[1];
			memcpy(dev->DataBuf, bl->AutoSCSIROM, dev->DataReplyLeft);
			DEBUG("Returning AutoSCSI ROM (%04X %04X %04X %04X)\n", dev->DataBuf[0], dev->DataBuf[1], dev->DataBuf[2], dev->DataBuf[3]);
		}
		break;

	case 0x95:
		if (bl->chip == CHIP_BUSLOGIC_PCI) {
			if (dev->Base != 0)
				x54x_io_remove(dev, dev->Base, 4);
			if (dev->CmdBuf[0] < 6) {
				dev->Base = ((3 - (dev->CmdBuf[0] >> 1)) << 8) | ((dev->CmdBuf[0] & 1) ? 0x34 : 0x30);
				x54x_io_set(dev, dev->Base, 4);
			} else
				dev->Base = 0;
			dev->DataReplyLeft = 0;
			return 1;
		} else {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;					
		}
		break;

	case 0x96:
		if (dev->CmdBuf[0] == 0)
			bl->ExtendedLUNCCBFormat = 0;
		else if (dev->CmdBuf[0] == 1)
			bl->ExtendedLUNCCBFormat = 1;
					
		dev->DataReplyLeft = 0;
		break;

	case 0x97:
	case 0xA7:
		/* TODO: Actually correctly implement this whole SCSI BIOS Flash stuff. */
		dev->DataReplyLeft = 0;
		break;

	case 0xA8:
		if (bl->chip != CHIP_BUSLOGIC_PCI) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
			break;
		}

		Offset = dev->CmdBuf[1];
		Offset <<= 8;
		Offset |= dev->CmdBuf[0];

		dev->DataReplyLeft = dev->CmdBuf[3];
		dev->DataReplyLeft <<= 8;
		dev->DataReplyLeft |= dev->CmdBuf[2];

		memcpy(dev->DataBuf, &(bl->SCAMData[Offset]), dev->DataReplyLeft);

		dev->DataReply = 0;
		break;

	case 0xA9:
		if (bl->chip != CHIP_BUSLOGIC_PCI) {
			dev->DataReplyLeft = 0;
			dev->Status |= STAT_INVCMD;
			break;
		}

		Offset = dev->CmdBuf[1];
		Offset <<= 8;
		Offset |= dev->CmdBuf[0];

		dev->DataReplyLeft = dev->CmdBuf[3];
		dev->DataReplyLeft <<= 8;
		dev->DataReplyLeft |= dev->CmdBuf[2];

		memcpy(&(bl->SCAMData[Offset]), &(dev->CmdBuf[4]), dev->DataReplyLeft);
		dev->DataReplyLeft = 0;

		dev->DataReply = 0;
		break;

	case 0xFB:
		dev->DataReplyLeft = dev->CmdBuf[2];
		break;

	default:
		dev->DataReplyLeft = 0;
		dev->Status |= STAT_INVCMD;
		break;
    }

    return 0;
}


static void
setup_data(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    ReplyInquireSetupInformation *ReplyISI;
    buslogic_setup_t *bl_setup;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;

    ReplyISI = (ReplyInquireSetupInformation *)dev->DataBuf;
    bl_setup = (buslogic_setup_t *)ReplyISI->VendorSpecificData;

    ReplyISI->fSynchronousInitiationEnabled = halr->structured.autoSCSIData.u16SynchronousPermittedMask ? 1 : 0;
    ReplyISI->fParityCheckingEnabled = (halr->structured.autoSCSIData.uSCSIConfiguration & 2) ? 1 : 0;

    bl_setup->uSignature = 'B';

    /* The 'D' signature prevents Buslogic's OS/2 drivers from getting too
     * friendly with Adaptec hardware and upsetting the HBA state.
    */
    bl_setup->uCharacterD = 'D';      /* BusLogic model. */
    switch(bl->chip) {
	case CHIP_BUSLOGIC_ISA_542_1991:
	case CHIP_BUSLOGIC_ISA_542:
	case CHIP_BUSLOGIC_ISA:
		bl_setup->uHostBusType = 'A';
		break;

	case CHIP_BUSLOGIC_MCA:
		bl_setup->uHostBusType = 'B';
		break;

	case CHIP_BUSLOGIC_VLB:
		bl_setup->uHostBusType = 'E';
		break;

	case CHIP_BUSLOGIC_PCI:
		bl_setup->uHostBusType = 'F';
		break;
    }
}


static uint8_t
is_aggressive_mode(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;

    return bl->fAggressiveRoundRobinMode;
}


static uint8_t
interrupt_type(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;

    if ((bl->chip == CHIP_BUSLOGIC_ISA_542) || (bl->chip == CHIP_BUSLOGIC_ISA_542_1991) || (bl->chip == CHIP_BUSLOGIC_MCA))
		return 0;
    else
		return !!bl->LocalRAM.structured.autoSCSIData.fLevelSensitiveInterrupt;
}


static void
buslogic_reset(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;

    bl->ExtendedLUNCCBFormat = 0;
}


uint8_t	buslogic_pci_regs[256];
bar_t	buslogic_pci_bar[3];


static void
BIOSUpdate(buslogic_data_t *bl)
{
    int bios_enabled = buslogic_pci_bar[2].addr_regs[0] & 0x01;

    if (!bl->has_bios) {
	return;
    }

    /* PCI BIOS stuff, just enable_disable. */
    if ((bl->bios_addr > 0) && bios_enabled) {
	mem_map_enable(&bl->bios.mapping);
	mem_map_set_addr(&bl->bios.mapping, bl->bios_addr, bl->bios_size);
	DEBUG("BT-958D: BIOS now at: %06X\n", bl->bios_addr);
    } else {
	DEBUG("BT-958D: BIOS disabled\n");
	mem_map_disable(&bl->bios.mapping);
    }
}


static uint8_t
PCIRead(int func, int addr, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
#ifdef _LOGGING
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
#endif
    uint8_t ret = 0xff;

    switch (addr) {
	case 0x00:
		ret = 0x4b;
		break;

	case 0x01:
		ret = 0x10;
		break;

	case 0x02:
		ret = 0x40;
		break;

	case 0x03:
		ret = 0x10;
		break;

	case 0x04:
		ret = buslogic_pci_regs[0x04] & 0x03;	/*Respond to IO and memory accesses*/
		break;

	case 0x05:
		ret = 0;
		break;

	case 0x07:
		ret = 2;
		break;

	case 0x08:
		ret = 1;			/*Revision ID*/
		break;

	case 0x09:
		ret = 0;			/*Programming interface*/
		break;

	case 0x0A:
		ret = 0;			/*Subclass*/
		break;

	case 0x0B:
		ret = 1;			/*Class code*/
		break;

	case 0x0E:
		ret = 0;			/*Header type */
		break;

	case 0x10:
		ret = (buslogic_pci_bar[0].addr_regs[0] & 0xe0) | 1;	/*I/O space*/
		break;

	case 0x11:
		ret = buslogic_pci_bar[0].addr_regs[1];
		break;

	case 0x12:
		ret = buslogic_pci_bar[0].addr_regs[2];
		break;

	case 0x13:
		ret = buslogic_pci_bar[0].addr_regs[3];
		break;

	case 0x14:
		ret = (buslogic_pci_bar[1].addr_regs[0] & 0xe0);	/*Memory space*/
		break;

	case 0x15:
		ret = (buslogic_pci_bar[1].addr_regs[1] & 0xc0);
		break;

	case 0x16:
		ret = buslogic_pci_bar[1].addr_regs[2];
		break;

	case 0x17:
		ret = buslogic_pci_bar[1].addr_regs[3];
		break;

	case 0x2C:
		ret = 0x4b;
		break;

	case 0x2D:
		ret = 0x10;
		break;

	case 0x2E:
		ret = 0x40;
		break;

	case 0x2F:
		ret = 0x10;
		break;

	case 0x30:			/* PCI_ROMBAR */
		ret = buslogic_pci_bar[2].addr_regs[0] & 0x01;
		DEBUG("BT-958D: BIOS BAR 00 = %02X\n", ret);
		break;


	case 0x31:			/* PCI_ROMBAR 15:11 */
		ret = buslogic_pci_bar[2].addr_regs[1];
		DEBUG("BT-958D: BIOS BAR 01 = %02X\n", (ret & bl->bios_mask));
		break;

	case 0x32:			/* PCI_ROMBAR 23:16 */
		ret = buslogic_pci_bar[2].addr_regs[2];
		DEBUG("BT-958D: BIOS BAR 02 = %02X\n", ret);
		break;

	case 0x33:			/* PCI_ROMBAR 31:24 */
		ret = buslogic_pci_bar[2].addr_regs[3];
		DEBUG("BT-958D: BIOS BAR 03 = %02X\n", ret);
		break;

	case 0x3C:
		ret = dev->Irq;
		break;

	case 0x3D:
		ret = PCI_INTA;
		break;
    }

    DBGLOG(2, "BT-958D: reading register %02X: %02X\n", addr & 0xff, ret);

    return(ret);
}


static void
PCIWrite(int func, int addr, uint8_t val, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    uint8_t valxor;

    DBGLOG(2, "BT-958D: Write value %02X to register %02X\n", val, addr & 0xff);

    switch (addr) {
	case 0x04:
		valxor = (val & 0x27) ^ buslogic_pci_regs[addr];
		if (valxor & PCI_COMMAND_IO) {
			x54x_io_remove(dev, bl->PCIBase, 32);
			if ((bl->PCIBase != 0) && (val & PCI_COMMAND_IO))
				x54x_io_set(dev, bl->PCIBase, 32);
		}
		if (valxor & PCI_COMMAND_MEM) {
			x54x_mem_disable(dev);
			if ((bl->MMIOBase != 0) && (val & PCI_COMMAND_MEM))
				x54x_mem_set_addr(dev, bl->MMIOBase);
		}
		buslogic_pci_regs[addr] = val & 0x27;
		break;

	case 0x10:
		val &= 0xe0;
		val |= 1;

	case 0x11: case 0x12: case 0x13:
		/* I/O Base set. */
		/* First, remove the old I/O. */
		x54x_io_remove(dev, bl->PCIBase, 32);
		/* Then let's set the PCI regs. */
		buslogic_pci_bar[0].addr_regs[addr & 3] = val;
		/* Then let's calculate the new I/O base. */
		bl->PCIBase = buslogic_pci_bar[0].addr & 0xffe0;
		/* Log the new base. */
		DEBUG("BusLogic PCI: New I/O base is %04X\n" , bl->PCIBase);
		/* We're done, so get out of the here. */
		if (buslogic_pci_regs[4] & PCI_COMMAND_IO) {
			if (bl->PCIBase != 0)
				x54x_io_set(dev, bl->PCIBase, 32);
		}
		return;

	case 0x14:
		val &= 0xe0;

	case 0x15: case 0x16: case 0x17:
		/* MMIO Base set. */
		/* First, remove the old I/O. */
		x54x_mem_disable(dev);
		/* Then let's set the PCI regs. */
		buslogic_pci_bar[1].addr_regs[addr & 3] = val;
		/* Then let's calculate the new I/O base. */
		bl->MMIOBase = buslogic_pci_bar[1].addr & 0xffffffe0;
		/* Log the new base. */
		DEBUG("BusLogic PCI: New MMIO base is %04X\n" , bl->MMIOBase);
		/* We're done, so get out of the here. */
		if (buslogic_pci_regs[4] & PCI_COMMAND_MEM) {
			if (bl->MMIOBase != 0)
				x54x_mem_set_addr(dev, bl->MMIOBase);
		}
		return;	

	case 0x30:			/* PCI_ROMBAR */
	case 0x31:			/* PCI_ROMBAR */
	case 0x32:			/* PCI_ROMBAR */
	case 0x33:			/* PCI_ROMBAR */
		buslogic_pci_bar[2].addr_regs[addr & 3] = val;
		buslogic_pci_bar[2].addr &= 0xffffc001;
		bl->bios_addr = buslogic_pci_bar[2].addr & 0xffffc000;
		DEBUG("BT-958D: BIOS BAR %02X = NOW %02X (%02X)\n", addr & 3, buslogic_pci_bar[2].addr_regs[addr & 3], val);
		BIOSUpdate(bl);
		return;

	case 0x3C:
		buslogic_pci_regs[addr] = val;
		if (val != 0xFF) {
			DEBUG("BusLogic IRQ now: %i\n", val);
			dev->Irq = val;
		} else
			dev->Irq = 0;
		return;
    }
}


static void
InitializeLocalRAM(buslogic_data_t *bl)
{
    memset(bl->LocalRAM.u8View, 0, sizeof(HALocalRAM));
    if (bl->chip == CHIP_BUSLOGIC_PCI) {
	bl->LocalRAM.structured.autoSCSIData.fLevelSensitiveInterrupt = 1;
    } else {
	bl->LocalRAM.structured.autoSCSIData.fLevelSensitiveInterrupt = 0;
    }

    bl->LocalRAM.structured.autoSCSIData.u16DeviceEnabledMask = ~0;
    bl->LocalRAM.structured.autoSCSIData.u16WidePermittedMask = ~0;
    bl->LocalRAM.structured.autoSCSIData.u16FastPermittedMask = ~0;
    bl->LocalRAM.structured.autoSCSIData.u16SynchronousPermittedMask = ~0;
    bl->LocalRAM.structured.autoSCSIData.u16DisconnectPermittedMask = ~0;
    bl->LocalRAM.structured.autoSCSIData.fRoundRobinScheme = 0;
    bl->LocalRAM.structured.autoSCSIData.u16UltraPermittedMask = ~0;
}


static uint8_t
buslogic_mca_read(int port, void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return(dev->pos_regs[port & 7]);
}


static void
buslogic_mca_write(int port, uint8_t val, void *priv)
{
    x54x_t *dev = (x54x_t *) priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;
    HALocalRAM *halr = &bl->LocalRAM;

    /* MCA does not write registers below 0x0100. */
    if (port < 0x0102) return;

    /* Save the MCA register value. */
    dev->pos_regs[port & 7] = val;

    /* This is always necessary so that the old handler doesn't remain. */
    x54x_io_remove(dev, dev->Base, 4);

    /* Get the new assigned I/O base address. */
    if (dev->pos_regs[3]) {
	dev->Base = dev->pos_regs[3] << 8;
	dev->Base |= ((dev->pos_regs[2] & 0x10) ? 0x34 : 0x30);
    } else {
	dev->Base = 0x0000;
    }

    /* Save the new IRQ and DMA channel values. */
    dev->Irq = ((dev->pos_regs[2] >> 1) & 0x07) + 8;
    dev->DmaChannel = dev->pos_regs[5] & 0x0f;	

    /* Extract the BIOS ROM address info. */
    if (dev->pos_regs[2] & 0xe0)  switch(dev->pos_regs[2] & 0xe0) {
	case 0xe0: /* [0]=111x xxxx */
		bl->bios_addr = 0xDC000;
		break;

	case 0x00: /* [0]=000x xxxx */
		bl->bios_addr = 0;
		break;
		
	case 0xc0: /* [0]=110x xxxx */
		bl->bios_addr = 0xD8000;
		break;

	case 0xa0: /* [0]=101x xxxx */
		bl->bios_addr = 0xD4000;
		break;

	case 0x80: /* [0]=100x xxxx */
		bl->bios_addr = 0xD0000;
		break;

	case 0x60: /* [0]=011x xxxx */
		bl->bios_addr = 0xCC000;
		break;

	case 0x40: /* [0]=010x xxxx */
		bl->bios_addr = 0xC8000;
		break;

	case 0x20: /* [0]=001x xxxx */
		bl->bios_addr = 0xC4000;
		break;
    } else {
	/* Disabled. */
	bl->bios_addr = 0x000000;
    }

    /*
     * Get misc SCSI config stuff.  For now, we are only
     * interested in the configured HA target ID:
     *
     *  pos[2]=111xxxxx = 7
     *  pos[2]=000xxxxx = 0
     */
    dev->HostID = (dev->pos_regs[4] >> 5) & 0x07;
    halr->structured.autoSCSIData.uSCSIId = dev->HostID;

    /*
     * SYNC mode is pos[2]=xxxxxx1x.
     *
     * SCSI Parity is pos[2]=xxx1xxxx.
     *
     * DOS Disk Space > 1GBytes is pos[2] = xxxx1xxx.
     */
    /* Parity. */
    halr->structured.autoSCSIData.uSCSIConfiguration &= ~2;
    halr->structured.autoSCSIData.uSCSIConfiguration |= (dev->pos_regs[4] & 2);

    /* Sync. */
    halr->structured.autoSCSIData.u16SynchronousPermittedMask = (dev->pos_regs[4] & 0x10) ? 0xffff : 0x0000;

    /* DOS Disk Space > 1GBytes */
    halr->structured.autoSCSIData.uBIOSConfiguration &= ~4;
    halr->structured.autoSCSIData.uBIOSConfiguration |= (dev->pos_regs[4] & 8) ? 4 : 0;

    switch(dev->DmaChannel) {
	case 5:
		halr->structured.autoSCSIData.uDMAChannel = 1;
		break;
	case 6:
		halr->structured.autoSCSIData.uDMAChannel = 2;
		break;
	case 7:
		halr->structured.autoSCSIData.uDMAChannel = 3;
		break;
	default:
		halr->structured.autoSCSIData.uDMAChannel = 0;
		break;
    }

    switch(dev->Irq) {
	case 9:
		halr->structured.autoSCSIData.uIrqChannel = 1;
		break;
	case 10:
		halr->structured.autoSCSIData.uIrqChannel = 2;
		break;
	case 11:
		halr->structured.autoSCSIData.uIrqChannel = 3;
		break;
	case 12:
		halr->structured.autoSCSIData.uIrqChannel = 4;
		break;
	case 14:
		halr->structured.autoSCSIData.uIrqChannel = 5;
		break;
	case 15:
		halr->structured.autoSCSIData.uIrqChannel = 6;
		break;
	default:
		halr->structured.autoSCSIData.uIrqChannel = 0;
		break;
    }

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
	if (bl->has_bios && (bl->bios_addr != 0x000000)) {
		mem_map_enable(&bl->bios.mapping);
		mem_map_set_addr(&bl->bios.mapping, bl->bios_addr, ROM_SIZE);
	}

	/* Say hello. */
	INFO("BT-640A: I/O=%04x, IRQ=%d, DMA=%d, BIOS @%05X, HOST ID %i\n",
	     dev->Base, dev->Irq, dev->DmaChannel, bl->bios_addr, dev->HostID);
    }
}

#if 0
static uint8_t
buslogic_mca_feedb(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;

    return (dev->pos_regs[2] & 0x01);
}
#endif

void
BuslogicDeviceReset(void *priv)
{
    x54x_t *dev = (x54x_t *)priv;
    buslogic_data_t *bl = (buslogic_data_t *)dev->ven_data;

    x54x_device_reset(dev);

    InitializeLocalRAM(bl);
    InitializeAutoSCSIRam(dev);
}


static void *
buslogic_init(const device_t *info, UNUSED(void *parent))
{
    x54x_t *dev;
    const wchar_t *bios_rom_name;
    uint16_t bios_rom_size;
    uint16_t bios_rom_mask;
    uint8_t has_autoscsi_rom;
    wchar_t *autoscsi_rom_name = 0;
    uint16_t autoscsi_rom_size;
    uint8_t has_scam_rom;
    wchar_t *scam_rom_name = 0;
    uint16_t scam_rom_size;
    FILE *f;
    buslogic_data_t *bl;
    uint32_t bios_rom_addr;

    /* Call common initializer. */
    dev = (x54x_t *)x54x_init(info);
    bios_rom_name = info->path;

    dev->ven_data = mem_alloc(sizeof(buslogic_data_t));
    memset(dev->ven_data, 0x00, sizeof(buslogic_data_t));
    bl = (buslogic_data_t *)dev->ven_data;

    dev->bus = info->flags;
    if (!(info->flags & DEVICE_MCA) && !(info->flags & DEVICE_PCI)) {
	    dev->Base = device_get_config_hex16("base");
	    dev->Irq = device_get_config_int("irq");
	    dev->DmaChannel = device_get_config_int("dma");
    }
    else if (info->flags & DEVICE_PCI) {
	    dev->Base = 0;
    }
    dev->HostID = 7;		/* default HA ID */
    dev->setup_info_len = sizeof(buslogic_setup_t);
    dev->max_id = 7;
	dev->flags = X54X_INT_GEOM_WRITABLE;

    bl->chip = info->local;
    bl->PCIBase = 0;
    bl->MMIOBase = 0;
    if (info->flags & DEVICE_PCI) {
	bios_rom_addr = 0xd8000;
	bl->has_bios = device_get_config_int("bios");
    } else if (info->flags & DEVICE_MCA) {
	bios_rom_addr = 0xd8000;
	bl->has_bios = 1;
    } else {
    	bios_rom_addr = device_get_config_hex20("bios_addr");
	bl->has_bios = !!bios_rom_addr;
    }

    dev->ven_cmd_phase1 = cmd_phase1;
    dev->ven_get_host_id = get_host_id;
    dev->ven_get_irq = get_irq;
    dev->ven_get_dma = get_dma;
    dev->get_ven_param_len = param_len;
    dev->ven_cmds = buslogic_cmds;
    dev->interrupt_type = interrupt_type;
    dev->is_aggressive_mode = is_aggressive_mode;
    dev->get_ven_data = setup_data;
    dev->ven_reset = buslogic_reset;

    strcpy(dev->vendor, "BusLogic");

    bl->fAggressiveRoundRobinMode = 1;

    switch(bl->chip) {
	case CHIP_BUSLOGIC_ISA_542_1991:
		strcpy(dev->name, "BT-542B");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 0;
		has_scam_rom = 0;
		dev->fw_rev = "AA221";
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		dev->max_id = 7;		/* narrow SCSI */
		break;
	case CHIP_BUSLOGIC_ISA_542:
		strcpy(dev->name, "BT-542BH");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 0;
		has_scam_rom = 0;
		dev->fw_rev = "AA335";
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		dev->max_id = 7;		/* narrow SCSI */
		break;

	case CHIP_BUSLOGIC_ISA:
	default:
		strcpy(dev->name, "BT-545S");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 1;
		autoscsi_rom_name = BT545_AUTO_BIOS_PATH;
		autoscsi_rom_size = 0x4000;
		has_scam_rom = 0;
		dev->fw_rev = "AA421E";
		dev->ha_bps = 10000000.0;	/* fast SCSI */
		dev->max_id = 7;		/* narrow SCSI */
		break;

	case CHIP_BUSLOGIC_MCA:
		strcpy(dev->name, "BT-640A");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 0;
		has_scam_rom = 0;
		dev->fw_rev = "BA150";
		dev->bit32 = 1;
		dev->pos_regs[0] = 0x08;	/* MCA board ID */
		dev->pos_regs[1] = 0x07;	
		mca_add(buslogic_mca_read, buslogic_mca_write, dev);
		dev->ha_bps = 5000000.0;	/* normal SCSI */
		dev->max_id = 7;		/* narrow SCSI */
		break;

	case CHIP_BUSLOGIC_VLB:
		strcpy(dev->name, "BT-445S");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 1;
		autoscsi_rom_name = BT445S_AUTO_BIOS_PATH;
		autoscsi_rom_size = 0x4000;
		has_scam_rom = 1;
		scam_rom_name = BT445S_SCAM_BIOS_PATH;
		scam_rom_size = 0x0200;
		dev->fw_rev = "AA507B";
		dev->bit32 = 1;
		dev->ha_bps = 10000000.0;	/* fast SCSI */
		dev->max_id = 7;		/* narrow SCSI */
		break;

	case CHIP_BUSLOGIC_PCI:
		strcpy(dev->name, "BT-958D");
		bios_rom_size = 0x4000;
		bios_rom_mask = 0x3fff;
		has_autoscsi_rom = 1;
		autoscsi_rom_name = BT958D_AUTO_BIOS_PATH;
		autoscsi_rom_size = 0x8000;
		has_scam_rom = 1;
		scam_rom_name = BT958D_SCAM_BIOS_PATH;
		scam_rom_size = 0x0200;
		dev->fw_rev = "AA507B";
		dev->flags |= (X54X_CDROM_BOOT | X54X_32BIT);
		dev->ha_bps = 20000000.0;	/* ultra SCSI */
		dev->max_id = 15;		/* wide SCSI */
		break;
    }

    if ((dev->Base != 0) && !(dev->bus & DEVICE_MCA) && !(dev->bus & DEVICE_PCI)) {
	x54x_io_set(dev, dev->Base, 4);
    }

    memset(bl->AutoSCSIROM, 0xff, 32768);

    memset(bl->SCAMData, 0x00, 65536);

    if (bl->has_bios) {
	bl->bios_size = bios_rom_size;

	bl->bios_mask = 0xffffc000;

	rom_init(&bl->bios, bios_rom_name, bios_rom_addr, bios_rom_size, bios_rom_mask, 0, MEM_MAPPING_EXTERNAL);

	if (has_autoscsi_rom) {
		f = rom_fopen(autoscsi_rom_name, L"rb");
		if (f != NULL) {
			(void)fread(bl->AutoSCSIROM, 1, autoscsi_rom_size, f);
			fclose(f);
		}
	}

	if (has_scam_rom) {
		f = rom_fopen(scam_rom_name, L"rb");
		if (f != NULL) {
			(void)fread(bl->SCAMData, 1, scam_rom_size, f);
			fclose(f);
		}
	}
    } else {
	bl->bios_size = 0;
	bl->bios_mask = 0;
    }

    if (bl->chip == CHIP_BUSLOGIC_PCI) {
	dev->pci_slot = pci_add_card(PCI_ADD_NORMAL, PCIRead, PCIWrite, dev);

	buslogic_pci_bar[0].addr_regs[0] = 1;
	buslogic_pci_bar[1].addr_regs[0] = 0;
       	buslogic_pci_regs[0x04] = 3;

	/* Enable our BIOS space in PCI, if needed. */
	if (bl->has_bios)
		buslogic_pci_bar[2].addr = 0xFFFFC000;
	  else
		buslogic_pci_bar[2].addr = 0;

	x54x_mem_init(dev, 0xfffd0000);
	x54x_mem_disable(dev);
    }

    if ((bl->chip == CHIP_BUSLOGIC_MCA) || (bl->chip == CHIP_BUSLOGIC_PCI))
	mem_map_disable(&bl->bios.mapping);

    DEBUG("Buslogic on port 0x%04X\n", dev->Base);

    x54x_device_reset(dev);

    if ((bl->chip != CHIP_BUSLOGIC_ISA_542) && (bl->chip != CHIP_BUSLOGIC_ISA_542_1991) && (bl->chip != CHIP_BUSLOGIC_MCA)) {
		InitializeLocalRAM(bl);
		InitializeAutoSCSIRam(dev);
    }

    return(dev);
}


static const device_config_t BT_ISA_Config[] = {
    {
        "base", "Address", CONFIG_HEX16, "", 0x334,
        {
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


static const device_config_t BT958D_Config[] = {
    {
        "bios", "Enable BIOS", CONFIG_BINARY, "", 0
    },
    {
	NULL
    }
};


const device_t buslogic_542b_device = {
    "Buslogic BT-542B ISA",
    DEVICE_ISA | DEVICE_AT,
    CHIP_BUSLOGIC_ISA_542_1991,
    BT542B_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    BT_ISA_Config
};

const device_t buslogic_device = {
    "Buslogic BT-542BH ISA",
    DEVICE_ISA | DEVICE_AT,
    CHIP_BUSLOGIC_ISA_542,
    BT542_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    BT_ISA_Config
};

const device_t buslogic_545s_device = {
    "Buslogic BT-545S ISA",
    DEVICE_ISA | DEVICE_AT,
    CHIP_BUSLOGIC_ISA,
    BT545_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    BT_ISA_Config
};

const device_t buslogic_640a_device = {
    "Buslogic BT-640A MCA",
    DEVICE_MCA,
    CHIP_BUSLOGIC_MCA,
    BT640A_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t buslogic_445s_device = {
    "Buslogic BT-445S ISA",
    DEVICE_VLB,
    CHIP_BUSLOGIC_VLB,
    BT445S_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    BT_ISA_Config
};

const device_t buslogic_pci_device = {
    "Buslogic BT-958D PCI",
    DEVICE_PCI,
    CHIP_BUSLOGIC_PCI,
    BT958D_BIOS_PATH,
    buslogic_init, x54x_close, NULL,
    NULL, NULL, NULL, NULL,
    BT958D_Config
};
