/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of the SCSI controllers.
 *
 * Version:	@(#)scsi.c	1.0.9	2018/05/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../../emu.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../disk/hdc.h"
#include "../disk/zip.h"
#include "../cdrom/cdrom.h"
#include "scsi.h"
#include "scsi_aha154x.h"
#include "scsi_buslogic.h"
#include "scsi_ncr5380.h"
#include "scsi_ncr53c810.h"
#ifdef WALTJE
# include "scsi_wd33c93.h"
#endif
#include "scsi_x54x.h"


#ifdef ENABLE_SCSI_DEV_LOG
int		scsi_dev_do_log = ENABLE_SCSI_DEV_LOG;
#endif
scsi_device_t	SCSIDevices[SCSI_ID_MAX][SCSI_LUN_MAX];
#if 0
uint8_t		SCSIPhase = 0xff;
uint8_t		SCSIStatus = SCSI_STATUS_OK;
#endif
char		scsi_fn[SCSI_NUM][512];
uint16_t	scsi_hd_location[SCSI_NUM];

uint32_t	SCSI_BufferLength;
static volatile
mutex_t		*scsiMutex;


typedef struct {
    const char		*name;
    const char		*internal_name;
    const device_t	*device;
    void		(*reset)(void *p);
} scsidev_t;


static const scsidev_t scsi_cards[] = {
    { "None",			"none",		NULL,		      NULL		  },
    { "[ISA] Adaptec AHA-1540B","aha1540b",	&aha1540b_device,     x54x_device_reset   },
    { "[ISA] Adaptec AHA-1542C","aha1542c",	&aha1542c_device,     x54x_device_reset   },
    { "[ISA] Adaptec AHA-1542CF","aha1542cf",	&aha1542cf_device,    x54x_device_reset   },
    { "[ISA] BusLogic BT-542BH","bt542bh",	&buslogic_device,     BuslogicDeviceReset },
    { "[ISA] BusLogic BT-545S",	"bt545s",	&buslogic_545s_device,BuslogicDeviceReset },
    { "[ISA] Longshine LCS-6821N","lcs6821n",	&scsi_lcs6821n_device,NULL		  },
    { "[ISA] Ranco RT1000B",	"rt1000b",	&scsi_rt1000b_device, NULL		  },
    { "[ISA] Trantor T130B",	"t130b",	&scsi_t130b_device,   NULL		  },
    { "[ISA] Sumo SCSI-AT",	"scsiat",	&scsi_scsiat_device,  NULL		  },
#ifdef WALTJE_SCSI
    { "[ISA] Generic WDC33C93",	"wd33c93",	&scsi_wd33c93_device, NULL		  },
#endif
    { "[MCA] Adaptec AHA-1640",	"aha1640",	&aha1640_device,      x54x_device_reset   },
    { "[MCA] BusLogic BT-640A",	"bt640a",	&buslogic_640a_device,BuslogicDeviceReset },
    { "[PCI] BusLogic BT-958D",	"bt958d",	&buslogic_pci_device, BuslogicDeviceReset },
    { "[PCI] NCR 53C810",	"ncr53c810",	&ncr53c810_pci_device,NULL		  },
    { "[VLB] BusLogic BT-445S",	"bt445s",	&buslogic_445s_device,BuslogicDeviceReset },
    { NULL,			NULL,		NULL,		      NULL		  },
};


void
scsi_dev_log(const char *fmt, ...)
{
#ifdef ENABLE_SCSI_DEV_LOG
    va_list ap;

    if (scsi_dev_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


int
scsi_card_available(int card)
{
    if (scsi_cards[card].device)
	return(device_available(scsi_cards[card].device));

    return(1);
}


const char *
scsi_card_getname(int card)
{
    return(scsi_cards[card].name);
}


const device_t *
scsi_card_getdevice(int card)
{
    return(scsi_cards[card].device);
}


int
scsi_card_has_config(int card)
{
    if (! scsi_cards[card].device) return(0);

    return(scsi_cards[card].device->config ? 1 : 0);
}


const char *
scsi_card_get_internal_name(int card)
{
    return(scsi_cards[card].internal_name);
}


int
scsi_card_get_from_internal_name(const char *s)
{
    int c = 0;

    while (scsi_cards[c].internal_name != NULL) {
	if (! strcmp(scsi_cards[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(0);
}


void
scsi_mutex(uint8_t start)
{
    if (start)
	scsiMutex = thread_create_mutex(L"VARCem.SCSIMutex");
      else
	thread_close_mutex((mutex_t *) scsiMutex);
}


void
scsi_card_init(void)
{
    int i, j;

    pclog("SCSI: building hard disk map...\n");
    build_scsi_hd_map();

    pclog("SCSI: building CD-ROM map...\n");
    build_scsi_cdrom_map();

    pclog("SCSI: building ZIP map...\n");
    build_scsi_zip_map();
	
    for (i=0; i<SCSI_ID_MAX; i++) {
	for (j=0; j<SCSI_LUN_MAX; j++) {
		if (scsi_disks[i][j] != 0xff) {
			SCSIDevices[i][j].LunType = SCSI_DISK;
		} else if (scsi_cdrom_drives[i][j] != 0xff) {
			SCSIDevices[i][j].LunType = SCSI_CDROM;
		} else if (scsi_zip_drives[i][j] != 0xff) {
			SCSIDevices[i][j].LunType = SCSI_ZIP;
		} else {
			SCSIDevices[i][j].LunType = SCSI_NONE;
		}
		SCSIDevices[i][j].CmdBuffer = NULL;
	}
    }

    if (scsi_cards[scsi_card].device)
	device_add(scsi_cards[scsi_card].device);
}


void
scsi_card_reset(void)
{
    void *p = NULL;

    if (scsi_cards[scsi_card].device) {
	p = device_get_priv(scsi_cards[scsi_card].device);
	if (p != NULL) {
		if (scsi_cards[scsi_card].reset)
			scsi_cards[scsi_card].reset(p);
	}
    }
}


/* Initialization function for the SCSI layer */
void
SCSIReset(uint8_t id, uint8_t lun)
{
    uint8_t cdrom_id = scsi_cdrom_drives[id][lun];
    uint8_t zip_id = scsi_zip_drives[id][lun];
    uint8_t hdd_id = scsi_disks[id][lun];

    if (hdd_id != 0xff) {
	scsi_hd_reset(hdd_id);
	SCSIDevices[id][lun].LunType = SCSI_DISK;
    } else {
	if (cdrom_id != 0xff) {
		cdrom_reset(cdrom_id);
		SCSIDevices[id][lun].LunType = SCSI_CDROM;
	} else if (zip_id != 0xff) {
		zip_reset(zip_id);
		SCSIDevices[id][lun].LunType = SCSI_ZIP;
	} else {
		SCSIDevices[id][lun].LunType = SCSI_NONE;
	}
    }

    if (SCSIDevices[id][lun].CmdBuffer != NULL) {
	free(SCSIDevices[id][lun].CmdBuffer);
	SCSIDevices[id][lun].CmdBuffer = NULL;
    }
}


void
scsi_mutex_wait(uint8_t wait)
{
    if (wait)
	thread_wait_mutex((mutex_t *) scsiMutex);
      else
	thread_release_mutex((mutex_t *) scsiMutex);
}
