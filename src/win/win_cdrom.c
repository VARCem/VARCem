/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the platform-side of CDROM drives.
 *		Implementation of the CD-ROM host drive IOCTL interface for
 *		Windows using SCSI Passthrough Direct.
 *
 * FIXME:	Not yet fully working!  Getting there, though ;-)
 *
 * Version:	@(#)win_cdrom.c	1.0.13 	2018/10/19
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
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
#define UNICODE
#define WINVER 0x0600
#include <windows.h>
#include <io.h>
#include <ntddcdrm.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog	cdrom_host_log
#include "../emu.h"
#include "../config.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/cdrom/cdrom.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/scsi/scsi_cdrom.h"
#include "win.h"


enum {
    CD_STOPPED = 0,
    CD_PLAYING,
    CD_PAUSED
};


typedef struct {
    HANDLE	hIOCTL;

    int8_t	is_playing;
    int8_t	is_ready;
    int8_t	disc_changed;
    int8_t	capacity_read;

    int		requested_blocks,
		actual_requested_blocks;
    int		last_subchannel_pos;
    int		last_track_nr;
    uint32_t	last_track_pos;
    uint32_t	sector_pos;

    CDROM_TOC	toc;

    uint8_t	sub_q_data_format[16];
    uint8_t	sub_q_channel_data[256];
    uint8_t	sense[256];
    uint8_t	rcbuf[16];
    wchar_t	path[128];
} cdrom_host_t;

#define cdrom_sense_error  hdev->sense[0]
#define cdrom_sense_key    hdev->sense[2]
#define cdrom_asc          hdev->sense[12]
#define cdrom_ascq         hdev->sense[13]


#ifdef ENABLE_CDROM_HOST_LOG
int	cdrom_host_do_log = ENABLE_CDROM_HOST_LOG;
#endif


#ifdef USE_HOST_CDROM


#if defined(_LOGGING) && defined(ENABLE_CDROM_HOST_LOG)
static void
cdrom_host_log(int level, const char *fmt, ...)
{
    va_list ap;

    if (cdrom_host_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
}
#endif


uint8_t	cdrom_host_drive_available_num = 0;
uint8_t	cdrom_host_drive_available[26];


#ifdef _DEBUG
static void
toc_dump(CDROM_TOC *toc, int start)
{
    uint32_t addr, lba;
    int c;

    DEBUG("IOCTL TOC:\n");

    for (c = start; c <= toc->LastTrack; c++) {
	addr = MSFtoLBA(toc->TrackData[c].Address[1],
			toc->TrackData[c].Address[2],
			toc->TrackData[c].Address[3]);

	lba = MSFtoLBA(toc->TrackData[c].Address[1],
		       toc->TrackData[c].Address[2],
		       toc->TrackData[c].Address[3]) - 150;

	DEBUG("      %02X  %02x  msf=%08lx lba=%08i\n",
	  toc->TrackData[c].TrackNumber, toc->TrackData[c].Control, addr, lba);
    }
    DEBUG("\n");
}
#endif


/* Read the TOC from the current media in the drive. */
static int
get_toc(cdrom_t *dev, CDROM_TOC *toc)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;
    int ret;

    DEBUG("IOCTL get_toc(%c): ", dev->host_drive);

    ret = DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_READ_TOC,
			  NULL, 0, toc, sizeof(CDROM_TOC), &size, NULL);
    if (! ret) {
	DEBUG(" ERR %lu\n", GetLastError());
	return(0);
    }
    DEBUG("OK\n");

#ifdef _DEBUG
    toc_dump(toc, 0);
#endif

    return(1);
}


/*
 * Check if there is media in the drive.
 *
 * We use the VERIFY2 variant of the ioctl code, as
 * that is faster. Since we get called often, speed
 * is important here!
 */
static int
check_media(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;

    if (! DeviceIoControl(hdev->hIOCTL, IOCTL_STORAGE_CHECK_VERIFY2,
			  NULL, 0, NULL, 0, &size, NULL)) {
	if (GetLastError() == ERROR_NOT_READY) return 0;

	return -1;
    }

    return 1;
}


static int
get_last_block(cdrom_t *dev, int msf, int maxlen, int single)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t address, lb;
    int c;

    DEBUG("IOCTL get_last_block(%c)\n", dev->host_drive);

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    dev->cd_state = CD_STOPPED;

    /* Grab the current TOC. */
    if (hdev->disc_changed) {
	if (! get_toc(dev, &hdev->toc)) return 0;

	hdev->disc_changed = 0;
    }

    lb = 0;
    for (c = 0; c <= hdev->toc.LastTrack; c++) {
	address = MSFtoLBA(hdev->toc.TrackData[c].Address[1],
			   hdev->toc.TrackData[c].Address[2],
			   hdev->toc.TrackData[c].Address[3]);
	if (address > lb)
		lb = address;
    }

    return lb;
}


static void read_capacity(cdrom_t *dev, uint8_t *b);


/* API: check if device is ready for use. */
static int
ioctl_ready(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    DEBUG("IOCTL ready(%c)\n", dev->host_drive);

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    /*
     * On OPEN, we issue the GetStatus call, and
     * then keep it 'up to date' using system event
     * notifications, so there is no real need to
     * call the function all the time.
     */
    if (! hdev->is_ready) return 0;

    if (hdev->disc_changed) {
	/* The disc has changed, so re-read the TOC. */
	if (! get_toc(dev, &hdev->toc)) return 0;

	/* Mark as stopped and ready. */
	dev->cd_state = CD_STOPPED;

	/* We now have a valid TOC loaded. */
	hdev->disc_changed = 0;

	/*
	 * With this, we read the READ CAPACITY command output
	 * from the host drive into our cache buffer.
	 */
	hdev->capacity_read = 0;
	read_capacity(dev, NULL);

	dev->cdrom_capacity = get_last_block(dev, 0, 4096, 0);
	DEBUG("IOCTL ready: capacity = %lu\n", dev->cdrom_capacity);

	if (dev->host_drive != dev->prev_host_drive)
		dev->prev_host_drive = dev->host_drive;

	/* Notify the drive. */
	if (dev->insert)
		dev->insert(dev->p);
    }

    /* OK, we handled it. */
    return 1;
}


/* API: we got a Medium Changed notification. */
static void
notify_change(cdrom_t *dev, int media_present)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    INFO("IOCTL media update: %i\n", media_present);

    hdev->is_ready = media_present;
    hdev->disc_changed = 1;
}


/*
 * API: enable or disable the removal of media in the drive.
 *
 * Not all drives support this, so it is optional.
 */
static void
media_lock(cdrom_t *dev, int lock)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    PREVENT_MEDIA_REMOVAL pmr;
    DWORD size;

    /* See if we can prevent media removal on this device. */
    pmr.PreventMediaRemoval = (lock) ? TRUE : FALSE;
    if (! DeviceIoControl(hdev->hIOCTL, IOCTL_STORAGE_MEDIA_REMOVAL,
			  &pmr, sizeof(pmr), NULL, 0, &size, NULL)) {
	ERRLOG("HostCDROM: unable to %sable Media Removal on volume %c:\n",
				dev->host_drive, (lock) ? "dis" : "en");
	dev->can_lock = 0;
	dev->is_locked = 0;
    } else {
	ERRLOG("HostCDROM: Media Removal on volume %c: %sabled\n",
				dev->host_drive, (lock) ? "dis" : "en");
	dev->can_lock = 1;
	dev->is_locked = lock;
    }
}


static uint32_t
ioctl_size(cdrom_t *dev)
{
    uint8_t buffer[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32_t capacity = 0;

    read_capacity(dev, buffer);

    capacity = ((uint32_t)buffer[0]) << 24;
    capacity |= ((uint32_t)buffer[1]) << 16;
    capacity |= ((uint32_t)buffer[2]) << 8;
    capacity |= (uint32_t)buffer[3];

    return capacity + 1;
}


static int
ioctl_status(cdrom_t *dev)
{
    int ret = CD_STATUS_EMPTY;

    /* This should never happen. */
    if (! dev->host_drive) return ret;

    if (! ioctl_ready(dev)) return ret;

    switch(dev->cd_state) {
	case CD_PLAYING:
		ret = CD_STATUS_PLAYING;
		break;

	case CD_PAUSED:
		ret = CD_STATUS_PAUSED;
		break;

	case CD_STOPPED:
		ret = CD_STATUS_STOPPED;
		break;

	default:
		break;
    }

    return ret;
}


/* API: stop the CD-ROM device. */
static void
ioctl_stop(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    /* This should never happen. */
    if (! dev->host_drive) return;

    if (hdev->is_playing)
	hdev->is_playing = 0;

    dev->cd_state = CD_STOPPED;
}


static void
ioctl_close(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;

    DEBUG("IOCTL close(%i) handle=%08lx\n", hdev->is_playing, hdev->hIOCTL);

    hdev->is_playing = 0;

    ioctl_stop(dev);

    hdev->disc_changed = 1;

    /* Unlock the media in the drive if we locked it. */
    if (dev->can_lock && dev->is_locked)
	media_lock(dev, 0);

    /* See if we can now unlock this volume. */
    if (! DeviceIoControl(hdev->hIOCTL, FSCTL_UNLOCK_VOLUME,
			  NULL, 0, NULL, 0, &size, NULL)) {
	ERRLOG("HostCDROM: unable to unlock volume %c: (ignored)\n",
						dev->host_drive);
    }

    /* All done, close the device. */
    CloseHandle(hdev->hIOCTL);

    free(hdev);

    dev->local = NULL;
    dev->reset = NULL;
}


static void
ioctl_eject(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;

    /* This should never happen. */
    if (! dev->host_drive) return;

    if (hdev->is_playing) {
	hdev->is_playing = 0;
	ioctl_stop(dev);
    }

    dev->cd_state = CD_STOPPED;

    (void)DeviceIoControl(hdev->hIOCTL, IOCTL_STORAGE_EJECT_MEDIA,
			  NULL, 0, NULL, 0, &size, NULL);
}


static void
ioctl_load(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;

    /* This should never happen. */
    if (! dev->host_drive) return;

    if (hdev->is_playing) {
	hdev->is_playing = 0;

	ioctl_stop(dev);
    }

    dev->cd_state = CD_STOPPED;

    (void)DeviceIoControl(hdev->hIOCTL, IOCTL_STORAGE_LOAD_MEDIA,
			  NULL, 0, NULL, 0, &size, NULL);

    /*
     * With this, we read the READ CAPACITY command output
     * from the host drive into our cache buffer.
     */
    hdev->capacity_read = 0;
    read_capacity(dev, NULL);

    dev->cdrom_capacity = get_last_block(dev, 0, 4096, 0);
}


static int
read_toc(cdrom_t *dev, uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t last_block;
    uint32_t address;
    uint32_t temp;
    DWORD size;
    int c, d, len = 4;

    DEBUG("IOCTL read_toc(%i) msf=%i maxlen=%i single=%i\n",
	  starttrack, msf, maxlen, single);

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    dev->cd_state = CD_STOPPED;

    (void)DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_READ_TOC,
			  NULL, 0, &hdev->toc, sizeof(hdev->toc), &size, NULL);

    hdev->disc_changed = 0;

    b[2] = hdev->toc.FirstTrack;
    b[3] = hdev->toc.LastTrack;

    d = 0;
    for (c = 0; c <= hdev->toc.LastTrack; c++) {
	if (hdev->toc.TrackData[c].TrackNumber >= starttrack) {
		d = c;
		break;
	}
    }

    last_block = 0;

    for (c = d; c<= hdev->toc.LastTrack; c++) {
	if ((len + 8) > maxlen) break;

	b[len++] = 0; /*Reserved*/
	b[len++] = (hdev->toc.TrackData[c].Adr<<4)|hdev->toc.TrackData[c].Control;
	b[len++] = hdev->toc.TrackData[c].TrackNumber;
	b[len++] = 0; /*Reserved*/

	address = MSFtoLBA(hdev->toc.TrackData[c].Address[1],
			   hdev->toc.TrackData[c].Address[2],
				   hdev->toc.TrackData[c].Address[3]);

	if (address > last_block)
		last_block = address;

	if (msf) {
		b[len++] = hdev->toc.TrackData[c].Address[0];
		b[len++] = hdev->toc.TrackData[c].Address[1];
		b[len++] = hdev->toc.TrackData[c].Address[2];
		b[len++] = hdev->toc.TrackData[c].Address[3];
	} else {
		temp = MSFtoLBA(hdev->toc.TrackData[c].Address[1],
				hdev->toc.TrackData[c].Address[2],
				hdev->toc.TrackData[c].Address[3]) - 150;
		b[len++] = temp >> 24;
		b[len++] = temp >> 16;
		b[len++] = temp >> 8;
		b[len++] = temp;
	}

	if (single) break;
    }

    b[0] = (uint8_t) (((len-2) >> 8) & 0xff);
    b[1] = (uint8_t) ((len-2) & 0xff);

    return len;
}


static int
read_toc_session(cdrom_t *dev, uint8_t *b, int msf, int maxlen)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    CDROM_TOC_SESSION_DATA toc;
    CDROM_READ_TOC_EX toc_ex;
    uint32_t temp;
    DWORD size;
    int len = 4;

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    dev->cd_state = CD_STOPPED;
    memset(&toc, 0, sizeof(toc));

    memset(&toc_ex, 0, sizeof(toc_ex));
    toc_ex.Format = CDROM_READ_TOC_EX_FORMAT_SESSION;
    toc_ex.Msf = msf;
    toc_ex.SessionTrack = 0;

    (void)DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_READ_TOC_EX,
			  &toc_ex, sizeof(toc_ex),
			  &toc, sizeof(toc), &size, NULL);

    b[2] = toc.FirstCompleteSession;
    b[3] = toc.LastCompleteSession;
    b[len++] = 0; /*Reserved*/
    b[len++] = (toc.TrackData[0].Adr << 4) | toc.TrackData[0].Control;
    b[len++] = toc.TrackData[0].TrackNumber;
    b[len++] = 0; /*Reserved*/
    if (msf) {
	b[len++] = toc.TrackData[0].Address[0];
	b[len++] = toc.TrackData[0].Address[1];
	b[len++] = toc.TrackData[0].Address[2];
	b[len++] = toc.TrackData[0].Address[3];
    } else {
	temp = MSFtoLBA(toc.TrackData[0].Address[1],
			toc.TrackData[0].Address[2],
			toc.TrackData[0].Address[3]) - 150;
	b[len++] = temp >> 24;
	b[len++] = temp >> 16;
	b[len++] = temp >> 8;
	b[len++] = temp;
    }

    return len;
}


static int
read_toc_raw(cdrom_t *dev, uint8_t *b, int maxlen)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    CDROM_TOC_FULL_TOC_DATA toc;
    CDROM_READ_TOC_EX toc_ex;
    DWORD size;
    int i, len = 4;

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    dev->cd_state = CD_STOPPED;
    memset(&toc, 0, sizeof(toc));

    memset(&toc_ex, 0, sizeof(toc_ex));
    toc_ex.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
    toc_ex.Msf = 1;
    toc_ex.SessionTrack = 0;

    (void)DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_READ_TOC_EX,
			  &toc_ex, sizeof(toc_ex),
			  &toc, sizeof(toc), &size, NULL);

    if (maxlen >= 3)
	b[2] = toc.FirstCompleteSession;
    if (maxlen >= 4)
	b[3] = toc.LastCompleteSession;

    if (len >= maxlen)
	return len;

    size -= sizeof(CDROM_TOC_FULL_TOC_DATA);
    size /= sizeof(toc.Descriptors[0]);

    for (i = 0; i <= (int)size; i++) {
	b[len++] = toc.Descriptors[i].SessionNumber;
	if (len == maxlen)
		return len;
	b[len++] = (toc.Descriptors[i].Adr<<4)|toc.Descriptors[i].Control;
	if (len == maxlen)
		return len;
	b[len++] = 0;
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].Reserved1; /*Reserved*/
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].MsfExtra[0];
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].MsfExtra[1];
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].MsfExtra[2];
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].Zero;
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].Msf[0];
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].Msf[1];
	if (len == maxlen)
		return len;
	b[len++] = toc.Descriptors[i].Msf[2];
	if (len == maxlen)
		return len;
    }

    return len;
}


/* Get track number for specified position. */
static int
get_track_nr(cdrom_t *dev, uint32_t pos)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t track_address;
    int c, track = 0;

    DEBUG("IOCTL: get_track_nr(%i): ", pos);

    if (hdev->disc_changed) {
	DEBUG("disc changed\n");
	return 0;
    }

    if (hdev->last_track_pos == pos) {
	DEBUG("hdev->last_track_pos == pos\n");
	return hdev->last_track_nr;
    }

    for (c = 0; c < hdev->toc.LastTrack; c++) {
	track_address = MSFtoLBA(hdev->toc.TrackData[c].Address[1],
				 hdev->toc.TrackData[c].Address[2],
				 hdev->toc.TrackData[c].Address[3]) - 150;

	if (track_address <= pos) {
		DEBUG("track = %i, ", c);
		track = c;
	}
    }

    hdev->last_track_pos = pos;
    hdev->last_track_nr = track;

    DEBUG("return %i\n", track);

    return track;
}


/* Get MSF for start of specified track. */
static uint32_t
get_track_msf(cdrom_t *dev, int track)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t ret = 0xffffffff;
    int c;

    DEBUG("IOCTL: get_track_msf(%i): ", track);

    if (hdev->disc_changed) {
	DEBUG("disc changed\n");
	return 0;
    }

    for (c = hdev->toc.FirstTrack; c < hdev->toc.LastTrack; c++) {
	if (c == track) {
		ret = hdev->toc.TrackData[c].Address[3] +
		      (hdev->toc.TrackData[c].Address[2] << 8) +
		      (hdev->toc.TrackData[c].Address[1] << 16);
	}
    }

    DEBUG("return %08lx\n", ret);

    return ret;
}


/* API: start playing audio at specified position. */
static uint8_t
audio_play(cdrom_t *dev, uint32_t pos, uint32_t len, int is_msf)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t start_msf = 0, end_msf = 0;
    int m = 0, s = 0, f = 0;

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    DEBUG("IOCTL audio_play(%08lx) len=%08lx msf=%i\n", pos, len, is_msf);

    if (is_msf == 2) {
	start_msf = get_track_msf(dev, pos);
	end_msf = get_track_msf(dev, len);

	if ((start_msf == 0xffffffff) || (end_msf == 0xffffffff))
		return 0;

	m = (start_msf >> 16) & 0xff;
	s = (start_msf >> 8) & 0xff;
	f = start_msf & 0xff;
	pos = MSFtoLBA(m, s, f) - 150;
	m = (end_msf >> 16) & 0xff;
	s = (end_msf >> 8) & 0xff;
	f = end_msf & 0xff;
	len = MSFtoLBA(m, s, f) - 150;
    } else if (is_msf == 1) {
	m = (pos >> 16) & 0xff;
	s = (pos >> 8) & 0xff;
	f = pos & 0xff;

	if (pos == 0xffffff) {
		DEBUG("Playing from current position (MSF)\n");
		pos = dev->seek_pos;
	} else {
		pos = MSFtoLBA(m, s, f) - 150;
	}

	m = (len >> 16) & 0xff;
	s = (len >> 8) & 0xff;
	f = len & 0xff;
	len = MSFtoLBA(m, s, f) - 150;
    } else if (is_msf == 0) {
	if (pos == 0xffffffff) {
		DEBUG("Playing from current position\n");
		pos = dev->seek_pos;
	}
	len += pos;
    }

    dev->seek_pos = pos;
    dev->cd_end = len;

    if (! hdev->is_playing)
	hdev->is_playing = 1;

    dev->cd_state = CD_PLAYING;

    return 1;
}


/* API: stop playing audio. */
static void
audio_stop(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    DEBUG("IOCTL: audio_stop(%i)\n", hdev->is_playing);

    hdev->is_playing = 0;

    dev->cd_state = CD_STOPPED;
}


/* API: pause playing audio. */
static void
audio_pause(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    /* This should never happen. */
    if (! dev->host_drive) return;

    DEBUG("IOCTL: audio_pause(%i) state=%d\n",
		hdev->is_playing, dev->cd_state);

    if (dev->cd_state == CD_PLAYING)
	dev->cd_state = CD_PAUSED;
}


/* API: resume playing audio. */
static void
audio_resume(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    /* This should never happen. */
    if (! dev->host_drive) return;

    DEBUG("IOCTL: audio_resume(%i) state=%d\n",
		hdev->is_playing, dev->cd_state);

    if (dev->cd_state == CD_PAUSED)
	dev->cd_state = CD_PLAYING;
}


/* API: callback from sound module. */
static int
audio_callback(cdrom_t *dev, int16_t *bufp, int len)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    RAW_READ_INFO in;
    DWORD size;

    DBGLOG(1, "IOCTL: audio_callback(%i) state=%i: ",
			dev->sound_on, dev->cd_state);

    if (!dev->sound_on || (dev->cd_state != CD_PLAYING)) {
	if (dev->cd_state == CD_PLAYING) {
		dev->seek_pos += (len >> 11);
		DBGLOG(1, "playing but mute\n");
	} else {
		DBGLOG(1, "not playing\n");
	}
	hdev->is_playing = 0;

	/* Create some silent data. */
	memset(bufp, 0, len * 2);

	return 0;
    }

    DBGLOG(1, "dev->cd_buflen = %i, len = %i, ", dev->cd_buflen, len);

    while (dev->cd_buflen < len) {
	if (dev->seek_pos < dev->cd_end) {
		in.DiskOffset.LowPart = dev->seek_pos * 2048;
		in.DiskOffset.HighPart = 0;
		in.SectorCount = 1;
		in.TrackMode = CDDA;		

		if (! DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_RAW_READ,
				      &in, sizeof(in),
				      &dev->cd_buffer[dev->cd_buflen], 2352,
				      &size, NULL)) {

			memset(&dev->cd_buffer[dev->cd_buflen], 0,
			       (BUF_SIZE - dev->cd_buflen) * 2);
			hdev->is_playing = 0;

			dev->cd_state = CD_STOPPED;
			dev->cd_buflen = len;
			DBGLOG(1, "read sector error, stopped\n");
			return 0;
		}

		dev->seek_pos++;
		dev->cd_buflen += (2352 / 2);
		DBGLOG(1, "dev->seek_pos = %i\n", dev->seek_pos);
	} else {
		memset(&dev->cd_buffer[dev->cd_buflen], 0,
			(BUF_SIZE - dev->cd_buflen) * 2);

		dev->cd_state = CD_STOPPED;
		dev->cd_buflen = len;

		hdev->is_playing = 0;

		DBGLOG(1, "reached the end\n");
		return 0;
	}
    }

    /* Copy more audio data into the sound buffer. */
    memcpy(bufp, dev->cd_buffer, len * 2);

    /* Move the audio buffer around. */
    //FIXME: should be re-done to avoid moving!!  --FvK
    memcpy(&dev->cd_buffer[0], &dev->cd_buffer[len], (BUF_SIZE - len) * 2);
    dev->cd_buflen -= len;

    return 1;
}


static uint8_t
get_current_subchannel(cdrom_t *dev, uint8_t *b, int msf)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    CDROM_SUB_Q_DATA_FORMAT insub;
    SUB_Q_CHANNEL_DATA sub;
    uint32_t cdpos, track_address, dat;
    uint32_t temp;
    DWORD size;
    int c, pos = 0, track;

    /* This should never happen. */
    if (! dev->host_drive) return 0;

    cdpos = dev->seek_pos;

    if (hdev->last_subchannel_pos == cdpos) {
	memcpy(&insub, hdev->sub_q_data_format, sizeof(insub));
	memcpy(&sub, hdev->sub_q_channel_data, sizeof(sub));
    } else {
	insub.Format = IOCTL_CDROM_CURRENT_POSITION;

	if (! DeviceIoControl(hdev->hIOCTL, IOCTL_CDROM_READ_Q_CHANNEL,
			      &insub, sizeof(insub), &sub, sizeof(sub),
						&size, NULL)) return 0;

	memset(hdev->sub_q_data_format, 0, 16);
	memcpy(hdev->sub_q_data_format, &insub, sizeof(insub));
	memset(hdev->sub_q_channel_data, 0, 256);
	memcpy(hdev->sub_q_channel_data, &sub, sizeof(sub));
	hdev->last_subchannel_pos = cdpos;
    }        

    if (dev->cd_state == CD_PLAYING || dev->cd_state == CD_PAUSED) {
	track = get_track_nr(dev, cdpos);

	track_address = MSFtoLBA(hdev->toc.TrackData[track].Address[1],
				 hdev->toc.TrackData[track].Address[2],
				 hdev->toc.TrackData[track].Address[3]) - 150;

	DEBUG("get_current_subchannel(): cdpos = %i, track = %i, track_address = %i\n", cdpos, track, track_address);

	b[pos++] = sub.CurrentPosition.Control;
	b[pos++] = track + 1;
	b[pos++] = sub.CurrentPosition.IndexNumber;

	if (msf) {
		dat = cdpos + 150;
		b[pos + 3] = (uint8_t)(dat % 75); dat /= 75;
		b[pos + 2] = (uint8_t)(dat % 60); dat /= 60;
		b[pos + 1] = (uint8_t)dat;
		b[pos]     = 0;
		pos += 4;
		dat = cdpos - track_address;
		b[pos + 3] = (uint8_t)(dat % 75); dat /= 75;
		b[pos + 2] = (uint8_t)(dat % 60); dat /= 60;
		b[pos + 1] = (uint8_t)dat;
		b[pos]     = 0;
		pos += 4;
	} else {
		b[pos++] = (cdpos >> 24) & 0xff;
		b[pos++] = (cdpos >> 16) & 0xff;
		b[pos++] = (cdpos >> 8) & 0xff;
		b[pos++] = cdpos & 0xff;
		cdpos -= track_address;
		b[pos++] = (cdpos >> 24) & 0xff;
		b[pos++] = (cdpos >> 16) & 0xff;
		b[pos++] = (cdpos >> 8) & 0xff;
		b[pos++] = cdpos & 0xff;
	}

	if (dev->cd_state == CD_PLAYING) return 0x11;

	return 0x12;
    }

    b[pos++] = sub.CurrentPosition.Control;
    b[pos++] = sub.CurrentPosition.TrackNumber;
    b[pos++] = sub.CurrentPosition.IndexNumber;

    DEBUG("cdpos = %i, track_address = %i\n",
	MSFtoLBA(sub.CurrentPosition.AbsoluteAddress[1],
		 sub.CurrentPosition.AbsoluteAddress[2],
		 sub.CurrentPosition.AbsoluteAddress[3]),
	MSFtoLBA(sub.CurrentPosition.TrackRelativeAddress[1],
		 sub.CurrentPosition.TrackRelativeAddress[2],
		 sub.CurrentPosition.TrackRelativeAddress[3]));
        
    if (msf) {
	for (c = 0; c < 4; c++)
		b[pos++] = sub.CurrentPosition.AbsoluteAddress[c];

	for (c = 0; c < 4; c++)
		b[pos++] = sub.CurrentPosition.TrackRelativeAddress[c];
    } else {
	temp = MSFtoLBA(sub.CurrentPosition.AbsoluteAddress[1],
			sub.CurrentPosition.AbsoluteAddress[2],
			sub.CurrentPosition.AbsoluteAddress[3]) - 150;
	b[pos++] = temp >> 24;
	b[pos++] = temp >> 16;
	b[pos++] = temp >> 8;
	b[pos++] = temp;

	temp = MSFtoLBA(sub.CurrentPosition.TrackRelativeAddress[1],
			sub.CurrentPosition.TrackRelativeAddress[2],
			sub.CurrentPosition.TrackRelativeAddress[3]) - 150;
	b[pos++] = temp >> 24;
	b[pos++] = temp >> 16;
	b[pos++] = temp >> 8;
	b[pos++] = temp;
    }

    return 0x13;
}


static int
is_track_audio(cdrom_t *dev, uint32_t pos, int ismsf)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    uint32_t track_address = 0;
    int c, control = 0;

    if (hdev->disc_changed)
	return 0;

    for (c = 0; hdev->toc.TrackData[c].TrackNumber != 0xaa; c++) {
	if (ismsf) {
		track_address = hdev->toc.TrackData[c].Address[3];
		track_address |= (hdev->toc.TrackData[c].Address[2] << 8);
		track_address |= (hdev->toc.TrackData[c].Address[1] << 16);
	} else {
		track_address = MSFtoLBA(hdev->toc.TrackData[c].Address[1],
					 hdev->toc.TrackData[c].Address[2],
					 hdev->toc.TrackData[c].Address[3]);
		track_address -= 150;
	}

	if (hdev->toc.TrackData[c].TrackNumber >= hdev->toc.FirstTrack &&
	    hdev->toc.TrackData[c].TrackNumber <= hdev->toc.LastTrack &&
	    track_address <= pos)
		control = hdev->toc.TrackData[c].Control;
    }

    if ((control & 0x0d) <= 1)
	return 1;

    return 0;
}


struct sptd_with_sense {
    SCSI_PASS_THROUGH s;
    ULONG Filler;
    UCHAR sense[32];
    UCHAR data[65536];
};


/*
 *   00,   08,   10,   18,   20,   28,   30,   38
 */
static const int flags_to_size[5][32] = {
  {    0,    0, 2352, 2352, 2352, 2352, 2352, 2352,  /* 00-38 (CD-DA) */
    2352, 2352, 2352, 2352, 2352, 2352, 2352, 2352,  /* 40-78 */
    2352, 2352, 2352, 2352, 2352, 2352, 2352, 2352,  /* 80-B8 */
    2352, 2352, 2352, 2352, 2352, 2352, 2352, 2352   /* C0-F8 */
  },
  {    0,    0, 2048, 2336,    4, -296, 2052, 2344,  /* 00-38 (Mode 1) */
       8, -296, 2048, 2048,   12, -296, 2052, 2052,  /* 40-78 */
    -296, -296, -296, -296,   16, -296, 2064, 2344,  /* 80-B8 */
    -296, -296, 2048, 2048,   24, -296, 2064, 2352   /* C0-F8 */
  },
  {    0,    0, 2336, 2336,    4, -296, 2340, 2340,  /* 00-38 (Mode 2 non-XA) */
       8, -296, 2336, 2336,   12, -296, 2340, 2340,  /* 40-78 */
    -296, -296, -296, -296,   16, -296, 2352, 2340,  /* 80-B8 */
    -296, -296, 2336, 2336,   24, -296, 2352, 2352   /* C0-F8 */
  },
  {    0,    0, 2048, 2336,    4, -296, -296, -296,  /* 00-38 (Mode 2 Form 1) */
       8, -296, 2056, 2344,   12, -296, 2060, 2340,  /* 40-78 */
    -296, -296, -296, -296,   16, -296, -296, -296,  /* 80-B8 */
    -296, -296, -296, -296,   24, -296, 2072, 2352   /* C0-F8 */
  },
  {    0,    0, 2328, 2328,    4, -296, -296, -296,  /* 00-38 (Mode 2 Form 2) */
       8, -296, 2336, 2336,   12, -296, 2340, 2340,  /* 40-78 */
    -296, -296, -296, -296,   16, -296, -296, -296,  /* 80-B8 */
    -296, -296, -296, -296,   24, -296, 2352, 2352   /* C0-F8 */
  }
};


static int get_sector_data_type(cdrom_t *, uint8_t, uint8_t, uint8_t, uint8_t, int);


static void
illegal_mode(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    cdrom_sense_key = SENSE_ILLEGAL_REQUEST;
    cdrom_asc = ASC_ILLEGAL_MODE_FOR_THIS_TRACK;
    cdrom_ascq = 0;
}


static int
get_block_length(cdrom_t *dev, const UCHAR *cdb, int nblocks, int no_len_check)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    int sector_type = 0;
    int temp_len = 0;

    if (no_len_check) {
	switch (cdb[0]) {
		case 0x25:	/* READ CAPACITY */
		case 0x44:	/* READ HEADER */
			return 8;

		case 0x42:	/* READ SUBCHANNEL */
		case 0x43:	/* READ TOC */
		case 0x51:	/* READ DISC INFORMATION */
		case 0x52:	/* READ TRACK INFORMATION */
		case 0x5A:	/* MODE SENSE (10) */
			return (((uint32_t)cdb[7]) << 8) | ((uint32_t)cdb[8]);

		case 0xAD:	/* READ DVD STRUCTURE */
			return (((uint32_t)cdb[8]) << 8) | ((uint32_t)cdb[9]);

		default:
			return 65534;
	}
    }

    switch (cdb[0]) {
	case 0x25:	/* READ CAPACITY */
	case 0x44:	/* READ HEADER */
		return 8;

	case 0x42:	/* READ SUBCHANNEL */
	case 0x43:	/* READ TOC */
	case 0x51:	/* READ DISC INFORMATION */
	case 0x52:	/* READ TRACK INFORMATION */
	case 0x5A:	/* MODE SENSE (10) */
		return (((uint32_t)cdb[7]) << 8) | ((uint32_t)cdb[8]);

	case 0xAD:	/* READ DVD STRUCTURE */
		return (((uint32_t)cdb[8]) << 8) | ((uint32_t)cdb[9]);

	case 0x08:
	case 0x28:
	case 0xa8:
		/* READ (6), READ (10), READ (12) */
		return 2048 * nblocks;

	case 0xb9:
		sector_type = (cdb[1] >> 2) & 7;
		if (sector_type == 0) {
			sector_type = get_sector_data_type(dev, 0, cdb[3], cdb[4], cdb[5], 1);
			if (sector_type == 0) {
				illegal_mode(dev);
				return -1;
			}
		}
		goto common_handler;

	case 0xbe:
		/* READ CD MSF, READ CD */
		sector_type = (cdb[1] >> 2) & 7;
		if (sector_type == 0) {
			sector_type = get_sector_data_type(dev, cdb[2], cdb[3], cdb[4], cdb[5], 0);
			if (sector_type == 0) {
				illegal_mode(dev);
				return -1;
			}
		}

common_handler:
		temp_len = flags_to_size[sector_type - 1][cdb[9] >> 3];
		if ((cdb[9] & 6) == 2)
			temp_len += 294;
		else if ((cdb[9] & 6) == 4)
			temp_len += 296;
		if ((cdb[10] & 7) == 1)
			temp_len += 96;
		else if ((cdb[10] & 7) == 2)
			temp_len += 16;
		else if ((cdb[10] & 7) == 4)
			temp_len += 96;

		if (temp_len <= 0) {
			illegal_mode(dev);
			return -1;
		}
		return temp_len * hdev->requested_blocks;

	default:
		/* Other commands */
		return 65534;

    }
}


static int
SCSICommand(cdrom_t *dev, const UCHAR *cdb, UCHAR *buf, uint32_t *len, int no_length_check)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    struct sptd_with_sense sptd;
    DWORD size;
    int ret = 0;

#if 0
    SCSISense.SenseKey = 0;
    SCSISense.Asc = 0;
    SCSISense.Ascq = 0;
#endif

    *len = 0;
    memset(&sptd, 0, sizeof(sptd));
    sptd.s.Length = sizeof(SCSI_PASS_THROUGH);
    sptd.s.CdbLength = 12;
    sptd.s.DataIn = SCSI_IOCTL_DATA_IN;
    sptd.s.TimeOutValue = 80 * 60;
    sptd.s.DataTransferLength = get_block_length(dev, cdb, hdev->actual_requested_blocks, no_length_check);
    sptd.s.SenseInfoOffset = (uintptr_t)&sptd.sense - (uintptr_t)&sptd;
    sptd.s.SenseInfoLength = 32;
    sptd.s.DataBufferOffset = (uintptr_t)&sptd.data - (uintptr_t)&sptd;

    memcpy(sptd.s.Cdb, cdb, 12);
    ret = DeviceIoControl(hdev->hIOCTL, IOCTL_SCSI_PASS_THROUGH,
			  &sptd, sizeof(sptd),
			  &sptd, sizeof(sptd), &size, NULL);

    if (sptd.s.SenseInfoLength) {
	cdrom_sense_key = sptd.sense[2];
	cdrom_asc = sptd.sense[12];
	cdrom_ascq = sptd.sense[13];
    }

    DEBUG("Transferred length: %i (command: %02X)\n", sptd.s.DataTransferLength, cdb[0]);
    DEBUG("Sense length: %i (%02X %02X %02X %02X %02X)\n", sptd.s.SenseInfoLength, sptd.sense[0], sptd.sense[1], sptd.sense[2], sptd.sense[12], sptd.sense[13]);
    DEBUG("IOCTL bytes: %i; SCSI status: %i, status: %i, LastError: %08X\n", size, sptd.s.ScsiStatus, ret, GetLastError());
    DEBUG("DATA:  %02X %02X %02X %02X %02X %02X\n", sptd.data[0],
	sptd.data[1], sptd.data[2], sptd.data[3], sptd.data[4], sptd.data[5]);
    DEBUG("       %02X %02X %02X %02X %02X %02X\n",
	sptd.data[6], sptd.data[7], sptd.data[8], sptd.data[9],
	sptd.data[10], sptd.data[11]);
    DEBUG("       %02X %02X %02X %02X %02X %02X\n",
	sptd.data[12], sptd.data[13], sptd.data[14], sptd.data[15],
	sptd.data[16], sptd.data[17]);
    DEBUG("SENSE: %02X %02X %02X %02X %02X %02X\n",
	sptd.sense[0], sptd.sense[1], sptd.sense[2], sptd.sense[3],
	sptd.sense[4], sptd.sense[5]);
    DEBUG("       %02X %02X %02X %02X %02X %02X\n",
	sptd.sense[6], sptd.sense[7], sptd.sense[8], sptd.sense[9],
	sptd.sense[10], sptd.sense[11]);
    DEBUG("       %02X %02X %02X %02X %02X %02X\n",
	sptd.sense[12], sptd.sense[13], sptd.sense[14], sptd.sense[15],
	sptd.sense[16], sptd.sense[17]);

    *len = sptd.s.DataTransferLength;
    if (sptd.s.DataTransferLength != 0)
	memcpy(buf, sptd.data, sptd.s.DataTransferLength);

    return ret;
}


static void
read_capacity(cdrom_t *dev, uint8_t *b)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    const UCHAR cdb[] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    UCHAR buf[16];
    uint32_t len = 0;

    if (! hdev->capacity_read || (b == NULL)) {
	SCSICommand(dev, cdb, buf, &len, 1);
	
	memcpy(hdev->rcbuf, buf, len);
	hdev->capacity_read = 1;
    } else {
	memcpy(b, hdev->rcbuf, 16);
    }
}


static int
media_type_id(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    const UCHAR cdb[] = { 0x5A, 0x00, 0x2A, 0, 0, 0, 0, 0, 28, 0, 0, 0 };
    uint8_t old_sense[3] = { 0, 0, 0 };
    UCHAR msbuf[28];
    uint32_t len = 0;
    int sense = 0;

    old_sense[0] = cdrom_sense_key;
    old_sense[1] = cdrom_asc;
    old_sense[2] = cdrom_asc;
	
    SCSICommand(dev, cdb, msbuf, &len, 1);

    sense = cdrom_sense_key;
    cdrom_sense_key = old_sense[0];
    cdrom_asc = old_sense[1];
    cdrom_asc = old_sense[2];

    if (sense == 0)
	return msbuf[2];

    return 3;
}


static uint32_t
msf_to_lba32(int lba)
{
    int m = (lba >> 16) & 0xff;
    int s = (lba >> 8) & 0xff;
    int f = lba & 0xff;

    return (m * 60 * 75) + (s * 75) + f;
}


static int
get_type(cdrom_t *dev, UCHAR *cdb, UCHAR *buf)
{
    uint32_t len = 0;
    int i, ret = 0;

    for (i = 2; i <= 5; i++) {
	cdb[1] = i << 2;

	/*
	 * Bypass length check so we don't risk calling
	 * this again and getting stuck in an endless loop.
	 */
	ret = SCSICommand(dev, cdb, buf, &len, 1);
	if (ret)
		return i;
    }

    return 0;
}


static int
sector_data_type(cdrom_t *dev, int sector, int ismsf)
{
    UCHAR cdb_lba[] = { 0xBE, 0, 0, 0, 0, 0, 0, 0, 1, 0x10, 0, 0 };
    UCHAR cdb_msf[] = { 0xB9, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0 };
    UCHAR buf[2352];
    int ret = 0;

    cdb_lba[2] = (sector >> 24);
    cdb_lba[3] = ((sector >> 16) & 0xff);
    cdb_lba[4] = ((sector >> 8) & 0xff);
    cdb_lba[5] = (sector & 0xff);

    cdb_msf[3] = cdb_msf[6] = ((sector >> 16) & 0xff);
    cdb_msf[4] = cdb_msf[7] = ((sector >> 8) & 0xff);
    cdb_msf[5] = cdb_msf[8] = (sector & 0xff);

    if (is_track_audio(dev, sector, ismsf))
	return 1;

    if (ismsf)
	ret = get_type(dev, cdb_msf, buf);
    else
	ret = get_type(dev, cdb_lba, buf);

   if (ret)
	return ret;

    if (ismsf) {
	sector = msf_to_lba32(sector);
	if (sector < 150)
		return 0;
	sector -= 150;
	ret = get_type(dev, (UCHAR *)cdb_lba, buf);
    }

    return ret;
}


static int
get_sector_data_type(cdrom_t *dev, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, int ismsf)
{
    int sector = b3;

    sector |= ((uint32_t) b2) << 8;
    sector |= ((uint32_t) b1) << 16;
    sector |= ((uint32_t) b0) << 24;

    return sector_data_type(dev, sector, ismsf);
}


#if 0
static void
validate_toc(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    DWORD size;

    if (! dev->host_drive) return;

    dev->cd_state = CD_STOPPED;

    DEBUG("Validating TOC...\n");
    (void)DeviceIoControl(hdev->hIOCTL,
			  IOCTL_CDROM_READ_TOC,
			  NULL, 0,
			  &hdev->toc, sizeof(hdev->toc), &size, NULL);
    hdev->disc_changed = 0;
}
#endif


/* API: pass through a SCSI command to the host device. */
static int
pass_through(cdrom_t *dev, uint8_t *buffer, int sector, int is_msf,
	     int cdrom_sector_type, int cdrom_sector_flags, int *len)
{
#if 0
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;
    const UCHAR cdb[12];
    uint32_t temp_len = 0;
    int temp_block_length = 0;
    int i = 0, buffer_pos = 0;
#endif
    int ret = 0;

    DEBUG("IOCTL pass_through(%i) msf=%i type=%i flags=%04x\n",
	  sector, is_msf, cdrom_sector_type, cdrom_sector_flags);

#if 0
    if (in_cdb[0] == 0x43) {
	/*
	 * This is a read TOC, so we have to validate
	 * the TOC to make the rest of the emulator happy.
	 */
	validate_toc(dev);
    }

    memcpy((void *)cdb, in_cdb, 12);

    temp_len = 0;
    temp_block_length = get_block_length(dev, cdb, hdev->requested_blocks, 0);
    if (temp_block_length != -1) {
	hdev->actual_requested_blocks = 1;
	if ((cdb[0] == 0x08) || (cdb[0] == 0x28) || (cdb[0] == 0xA8) ||
	    (cdb[0] == 0xB9) || (cdb[0] == 0xBE)) {
		buffer_pos = 0;
		temp_len = 0;

		for (i = 0; i < hdev->requested_blocks; i++) {
			DEBUG("pass_through(): Transferring block...\n");
			scsi_cdrom_update_cdb((uint8_t *)cdb,
					 hdev->sector_pos + i, 1);
			ret = SCSICommand(dev, cdb, b + buffer_pos, &temp_len, 0);
			buffer_pos += temp_len;
		}

		*len = buffer_pos;
	} else {
		DEBUG("pass_through(): Expected transfer length %i is smaller than 65534, transferring all at once...\n", temp_block_length);
		ret = SCSICommand(dev, cdb, b, len, 0);
		DEBUG("pass_through(): Single transfer done\n");
	}
    }

    DEBUG("IOCTL DATA:  %02X %02X %02X %02X %02X %02X %02X %02X\n",
		b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
	
    DEBUG("IOCTL Returned value: %i\n", ret);
#endif
	
    return ret;
}


static const cdrom_ops_t cdrom_host_ops = {
    ioctl_ready,
    notify_change,
    media_lock,
    media_type_id,

    ioctl_size,
    ioctl_status,
    ioctl_stop,
    ioctl_close,

    ioctl_eject,
    ioctl_load,

    read_toc,
    read_toc_session,
    read_toc_raw,

    audio_play,
    audio_stop,
    audio_pause,
    audio_resume,
    audio_callback,

    get_current_subchannel,
    pass_through
};


static void
ioctl_reset(cdrom_t *dev)
{
    cdrom_host_t *hdev = (cdrom_host_t *)dev->local;

    DEBUG("IOCTL reset(%c) handle=%08lx\n", dev->host_drive, hdev->hIOCTL);

    if (! dev->host_drive) {
	hdev->disc_changed = 1;
	return;
    }

    /* Grab the current TOC. */
    if (! get_toc(dev, &hdev->toc)) return;

    hdev->disc_changed = 0;
}


/* API: open a host device and attach it. */
int
cdrom_host_open(cdrom_t *dev, char d)
{
    cdrom_host_t *hdev;
    DWORD size;

    /* Allocate our control block. */
    hdev = (cdrom_host_t *)mem_alloc(sizeof(cdrom_host_t));
    swprintf(hdev->path, sizeof_w(hdev->path), L"\\\\.\\%c:", d);
    dev->local = hdev;

    /* Create a handle to the device. */
    hdev->hIOCTL = CreateFile(hdev->path,
			      FILE_READ_ATTRIBUTES|GENERIC_READ|GENERIC_WRITE,
			      FILE_SHARE_READ|FILE_SHARE_WRITE,
			      NULL, OPEN_EXISTING, 0, NULL);
    DEBUG("IOCTL open(%ls) = %08lx\n", hdev->path, hdev->hIOCTL);

    /* See if we can lock this drive for exclusive access. */
    if (! DeviceIoControl(hdev->hIOCTL, FSCTL_LOCK_VOLUME,
			  NULL, 0, NULL, 0, &size, NULL)) {
	ERRLOG("HostCDROM: unable to lock volume %c: (ignored)\n", d);
    } else
	ERRLOG("HostCDROM: volume %c: locked\n", d);

    /* See if we have media in the drive. */
    hdev->is_ready = check_media(dev);
    hdev->disc_changed = 1;

    /*
     * See if we can unlock the media in the drive. If that
     * fails with an error, the drive does not support it.
     * Otherwise, it will be unlocked.
     */
    (void)media_lock(dev, 0);

    /* Looks good - attach to this cdrom instance. */
    dev->ops = &cdrom_host_ops;
    dev->reset = ioctl_reset;

    /* If we have media, read TOC and capacity. */
    if (hdev->is_ready)
	(void)dev->ops->ready(dev);

    /* All good! */
    return 0;
}


void
cdrom_host_init(void)
{
    WCHAR temp[16];
    int i = 0;

    cdrom_host_drive_available_num = 0;
    for (i = 'A'; i <= 'Z'; i++) {
	swprintf(temp, sizeof_w(temp), L"%c:\\", i);

	if (GetDriveType(temp) == DRIVE_CDROM) {
		cdrom_host_drive_available[i - 'A'] = 1;

		cdrom_host_drive_available_num++;
	} else
		cdrom_host_drive_available[i - 'A'] = 0;
    }
}
#endif	/*USE_HOST_CDROM*/
