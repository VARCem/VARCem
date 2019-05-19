/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		The generic SCSI device command handler.
 *
 * Version:	@(#)scsi_device.c	1.0.15	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
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
#include <wchar.h>
#include <stdarg.h>
#define HAVE_STDARG_H
#define dbglog scsi_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../device.h"
#include "../disk/hdd.h"
#include "scsi.h"
#include "scsi_device.h"


#ifdef ENABLE_SCSI_LOG
int		scsi_do_log = ENABLE_SCSI_LOG;
#endif
scsi_device_t	scsi_devices[SCSI_ID_MAX][SCSI_LUN_MAX];


static const uint8_t	scsi_null_device_sense[18] = {
    0x70,0,SENSE_ILLEGAL_REQUEST,0,0,0,0,0,0,0,0,0,ASC_INV_LUN,0,0,0,0,0
};


static uint8_t
target_command(scsi_device_t *dev, uint8_t *cdb)
{
    if (dev->command && dev->err_stat_to_scsi) {
	dev->command(dev->p, cdb);
	return dev->err_stat_to_scsi(dev->p);
    }

    return SCSI_STATUS_CHECK_CONDITION;
}


static void
target_callback(scsi_device_t *dev)
{
    if (dev->callback)
	dev->callback(dev->p);
}


static int
target_err_stat_to_scsi(scsi_device_t *dev)
{
    if (dev->err_stat_to_scsi)
	return dev->err_stat_to_scsi(dev->p);

    return SCSI_STATUS_CHECK_CONDITION;
}


tmrval_t
scsi_device_get_callback(scsi_device_t *dev)
{
    scsi_device_data_t *sdd = (scsi_device_data_t *)dev->p;

    if (sdd)
	return sdd->callback;

    return (tmrval_t)-1;
}


uint8_t *
scsi_device_sense(scsi_device_t *dev)
{
    scsi_device_data_t *sdd = (scsi_device_data_t *)dev->p;

    if (sdd)
	return sdd->sense;

    return (uint8_t *)scsi_null_device_sense;
}


void
scsi_device_request_sense(scsi_device_t *dev, uint8_t *buffer, uint8_t alloc_length)
{
    if (dev->request_sense)
	dev->request_sense(dev->p, buffer, alloc_length);
    else
	memcpy(buffer, scsi_null_device_sense, alloc_length);
}


void
scsi_device_reset(scsi_device_t *dev)
{
    if (dev->reset)
	dev->reset(dev->p);
}


int
scsi_device_present(scsi_device_t *dev)
{
    if (dev->type == SCSI_NO_DEVICE)
	return 0;

    return 1;
}


int
scsi_device_valid(scsi_device_t *dev)
{
    if (dev->p)
	return 1;

    return 0;
}


int
scsi_device_cdb_length(scsi_device_t *dev)
{
    /* Right now, it's 12 for all devices. */
    return 12;
}


void
scsi_device_command_phase0(scsi_device_t *dev, uint8_t *cdb)
{
    if (! dev->p) {
	dev->phase = SCSI_PHASE_STATUS;
	dev->status = SCSI_STATUS_CHECK_CONDITION;
	return;
    }

    /* Execute the SCSI command immediately and get the transfer length. */
    dev->phase = SCSI_PHASE_COMMAND;
    dev->status = target_command(dev, cdb);

    if (dev->phase == SCSI_PHASE_STATUS) {
	/*
	 * Command completed (either OK or error) -
	 * Call the phase callback to complete the command.
	 */
	target_callback(dev);
    }
    /* If the phase is DATA IN or DATA OUT, finish this here. */
}


void
scsi_device_command_phase1(scsi_device_t *dev)
{
    if (! dev->p)
	return;

    /* Call the second phase. */
    target_callback(dev);
    dev->status = target_err_stat_to_scsi(dev);

    /*
     * Command second phase complete -
     * Call the callback to complete the command.
     */
    target_callback(dev);
}


int32_t *
scsi_device_get_buf_len(scsi_device_t *dev)
{
    return &dev->buffer_length;
}


#ifdef _LOGGING
void
scsi_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SCSI_LOG
    va_list ap;

    if (scsi_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif
