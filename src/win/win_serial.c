/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of host serial port services for Win32.
 *
 *		This code is based on a universal serial port driver for
 *		Windows and UNIX systems, with support for FTDI and Prolific
 *		USB ports. Support for these has been removed.
 *
 * Version:	@(#)win_serial.c	1.0.5	2018/11/22
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../emu.h"
#include "../plat.h"
#include "../devices/ports/serial.h"


typedef struct {
    char	name[80];		/* name of open port */
    void	(*rd_done)(void *, int);
    void	*rd_arg;
    HANDLE	handle;
    OVERLAPPED	rov,			/* READ and WRITE events */
		wov;
    int		tmo;			/* current timeout value */
    DCB		dcb,			/* terminal settings */
		odcb;
    thread_t	*tid;			/* pointer to receiver thread */
    char	buff[1024];
    int		icnt, ihead, itail;
} serial_t;


/* Handle the receiving of data from the host port. */
static void
reader_thread(void *arg)
{
    serial_t *dev = (serial_t *)arg;
    uint8_t b;
    DWORD n;

    INFO("%s: thread started\n", dev->name);

    /* As long as the channel is open.. */
    while (dev->tid != NULL) {
	/* Post a READ on the device. */
	n = 0;
	if (ReadFile(dev->handle, &b, (DWORD)1, &n, &dev->rov) == FALSE) {
		n = GetLastError();
		if (n != ERROR_IO_PENDING) {
			/* Not good, we got an error. */
			ERRLOG("%s: I/O error %i in read!\n", dev->name, n);
			break;
		}

		/* The read is pending, wait for it.. */
		if (GetOverlappedResult(dev->handle, &dev->rov, &n, TRUE) == FALSE) {
			n = GetLastError();
			ERRLOG("%s: I/O error %i in read!\n", dev->name, n);
			break;
		}
	}

pclog(0,"%s: got %i bytes of data\n", dev->name, n);
	if (n == 1) {
		/* We got data, update stuff. */
		if (dev->icnt < sizeof(dev->buff)) {
pclog(0,"%s: queued byte %02x (%i)\n", dev->name, b, dev->icnt+1);
			dev->buff[dev->ihead++] = b;
			dev->ihead &= (sizeof(dev->buff)-1);
			dev->icnt++;

			/* Do a callback to let them know. */
			if (dev->rd_done != NULL)
				dev->rd_done(dev->rd_arg, n);
		} else {
			ERRLOG("%s: RX buffer overrun!\n", dev->name);
		}
	}
    }

    /* Error or done, clean up. */
    dev->tid = NULL;
    INFO("%s: thread stopped.\n", dev->name);
}


/* Set the state of a port. */
static int
set_state(serial_t *dev, void *arg)
{
    if (SetCommState(dev->handle, (DCB *)arg) == FALSE) {
	/* Mark as error. */
	ERRLOG("%s: set state: %i\n", dev->name, GetLastError());
	return(-1);
    }

    return(0);
}


/* Fetch the state of a port. */
static int
get_state(serial_t *dev, void *arg)
{
    if (GetCommState(dev->handle, (DCB *)arg) == FALSE) {
	/* Mark as error. */
	ERRLOG("%s: get state: %i\n", dev->name, GetLastError());
	return(-1);
    }

    return(0);
}


/* Enable or disable RTS/CTS mode (hardware handshaking.) */
static int
set_crtscts(serial_t *dev, int8_t yes)
{
    /* Get the current mode. */
    if (get_state(dev, &dev->dcb) < 0) return(-1);

    switch (yes) {
	case 0:		/* disable CRTSCTS */
		dev->dcb.fOutxDsrFlow = 0;	/* disable DSR/DCD mode */
		dev->dcb.fDsrSensitivity = 0;

		dev->dcb.fOutxCtsFlow = 0;	/* disable RTS/CTS mode */

		dev->dcb.fTXContinueOnXoff = 0;	/* disable XON/XOFF mode */
		dev->dcb.fOutX = 0;
		dev->dcb.fInX = 0;
		break;

	case 1:		/* enable CRTSCTS */
		dev->dcb.fOutxDsrFlow = 0;	/* disable DSR/DCD mode */
		dev->dcb.fDsrSensitivity = 0;

		dev->dcb.fOutxCtsFlow = 1;	/* enable RTS/CTS mode */

		dev->dcb.fTXContinueOnXoff = 0;	/* disable XON/XOFF mode */
		dev->dcb.fOutX = 0;
		dev->dcb.fInX = 0;
		break;

	default:
		ERRLOG("%s: invalid parameter '%i'!\n", dev->name, yes);
		return(-1);
    }

    /* Set new mode. */
    if (set_state(dev, &dev->dcb) < 0) return(-1);

    return(0);
}


/* Set the port parameters. */
int
plat_serial_params(void *arg, char dbit, char par, char sbit)
{
    serial_t *dev = (serial_t *)arg;

    /* Get the current mode. */
    if (get_state(dev, &dev->dcb) < 0) return(-1);

    /* Set the desired word length. */
    switch ((int)dbit) {
	case -1:			/* no change */
		break;

	case 5:				/* FTDI doesnt like these */
	case 6:
	case 9:
		break;

	case 7:
	case 8:
		dev->dcb.ByteSize = dbit;
		break;

	default:
		ERRLOG("%s: invalid parameter '%i'!\n", dev->name, dbit);
		return(-1);
    }

    /* Set the type of parity encoding. */
    switch ((int)par) {
	case -1:			/* no change */
	case ' ':
		break;

	case 0:
	case 'N':
		dev->dcb.fParity = FALSE;
		dev->dcb.Parity = NOPARITY;
		break;

	case 1:
	case 'O':
		dev->dcb.fParity = TRUE;
		dev->dcb.Parity = ODDPARITY;
		break;

	case 2:
	case 'E':
		dev->dcb.fParity = TRUE;
		dev->dcb.Parity = EVENPARITY;
		break;

	case 3:
	case 'M':
	case 4:
	case 'S':
		break;

	default:
		ERRLOG("%s: invalid parameter '%c'!\n", dev->name, par);
		return(-1);
    }

    /* Set the number of stop bits. */
    switch ((int)sbit) {
	case -1:			/* no change */
		break;

	case 1:
		dev->dcb.StopBits = ONESTOPBIT;
		break;

	case 2:
		dev->dcb.StopBits = TWOSTOPBITS;
		break;

	default:
		ERRLOG("%s: invalid parameter '%i'!\n", dev->name, sbit);
		return(-1);
    }

    /* Set new mode. */
    if (set_state(dev, &dev->dcb) < 0) return(-1);

    return(0);
}


/* Put a port in transparent ("raw") state. */
void
plat_serial_raw(void *arg, void *data)
{
    serial_t *dev = (serial_t *)arg;
    DCB *dcb = (DCB *)data;

    /* Make sure we can do this. */
    if (dcb == NULL) {
	ERRLOG("%s: invalid parameter\n", dev->name);
	return;
    }

    /* Enable BINARY transparent mode. */
    dcb->fBinary = 1;
    dcb->fErrorChar = 0;			/* disable Error Replacement */
    dcb->fNull = 0;				/* disable NUL stripping */

    /* Disable the DTR and RTS lines. */
    dcb->fDtrControl = DTR_CONTROL_DISABLE;	/* DTR line */
    dcb->fRtsControl = RTS_CONTROL_DISABLE;	/* RTS line */

    /* Disable DSR/DCD handshaking. */
    dcb->fOutxDsrFlow = 0;			/* DSR handshaking */
    dcb->fDsrSensitivity = 0;			/* DSR Sensitivity */

    /* Disable RTS/CTS handshaking. */
    dcb->fOutxCtsFlow = 0;			/* CTS handshaking */

    /* Disable XON/XOFF handshaking. */
    dcb->fTXContinueOnXoff = 0;			/* continue TX after Xoff */
    dcb->fOutX = 0;				/* enable output X-ON/X-OFF */
    dcb->fInX = 0;				/* enable input X-ON/X-OFF */
    dcb->XonChar = 0x11;			/* ASCII XON */
    dcb->XoffChar = 0x13;			/* ASCII XOFF */
    dcb->XonLim = 100;
    dcb->XoffLim = 100;

    dcb->fParity = FALSE;
    dcb->Parity = NOPARITY;
    dcb->StopBits = ONESTOPBIT;
    dcb->BaudRate = CBR_1200;
}


/* Set the port speed. */
int
plat_serial_speed(void *arg, long speed)
{
    serial_t *dev = (serial_t *)arg;

    /* Get the current mode and speed. */
    if (get_state(dev, &dev->dcb) < 0) return(-1);

    /*
     * Set speed.
     *
     * This is not entirely correct, we should use a table
     * with DCB_xxx speed values here, but we removed that
     * and just hardcode the speed value into DCB.  --FvK
     */
    dev->dcb.BaudRate = speed;

    /* Set new speed. */
    if (set_state(dev, &dev->dcb) < 0) return(-1);

    return(0);
}


/* Clean up and flush. */
int
plat_serial_flush(void *arg)
{
    serial_t *dev = (serial_t *)arg;
    DWORD dwErrs;
    COMSTAT cst;

    /* First, clear any errors. */
    (void)ClearCommError(dev->handle, &dwErrs, &cst);

    /* Now flush all buffers. */
    if (PurgeComm(dev->handle,
		  (PURGE_RXABORT | PURGE_TXABORT | \
		   PURGE_RXCLEAR | PURGE_TXCLEAR)) == FALSE) {
	ERRLOG("%s: flush: %i\n", dev->name, GetLastError());
	return(-1);
    }

    /* Re-clear any errors. */
    if (ClearCommError(dev->handle, &dwErrs, &cst) == FALSE) {
	ERRLOG("%s: clear errors: %i\n", dev->name, GetLastError());
	return(-1);
    }

    return(0);
}


/* API: close an open serial port. */
void
plat_serial_close(void *arg)
{
    serial_t *dev = (serial_t *)arg;

    /* If the polling thread is running, stop it. */
    plat_serial_active(arg, 0);

    /* Close the event handles. */
    if (dev->rov.hEvent != INVALID_HANDLE_VALUE)
	CloseHandle(dev->rov.hEvent);
    if (dev->wov.hEvent != INVALID_HANDLE_VALUE)
	CloseHandle(dev->wov.hEvent);

    if (dev->handle != INVALID_HANDLE_VALUE) {
	INFO("%s: closing host port\n", dev->name);

	/* Restore the previous port state, if any. */
	set_state(dev, &dev->odcb);

	/* Close the port. */
	CloseHandle(dev->handle);
    }

    /* Release the control block. */
    free(dev);
}


/* API: open a host serial port for I/O. */
void *
plat_serial_open(const char *port, int tmo)
{
    char temp[84];
    COMMTIMEOUTS to;
    COMMCONFIG conf;
    serial_t *dev;
    DWORD d;

    /* First things first... create a control block. */
    if ((dev = (serial_t *)mem_alloc(sizeof(serial_t))) == NULL) {
	ERRLOG("%s: out of memory!\n", port);
	return(NULL);
    }
    memset(dev, 0x00, sizeof(serial_t));
    strncpy(dev->name, port, sizeof(dev->name)-1);

    /* Try a regular Win32 serial port. */
    sprintf(temp, "\\\\.\\%s", dev->name);
    if ((dev->handle = CreateFile(temp,
				  (GENERIC_READ|GENERIC_WRITE),
				  0,
				  NULL,
				  OPEN_EXISTING,
				  FILE_FLAG_OVERLAPPED,
				  0)) == INVALID_HANDLE_VALUE) {
	ERRLOG("%s: open port: %i\n", dev->name, GetLastError());
	free(dev);
	return(NULL);
    }

    /* Create event handles. */
    dev->rov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    dev->wov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* Set up buffer size of the port. */
    if (SetupComm(dev->handle, 32768L, 32768L) == FALSE) {
	/* This fails on FTDI-based devices, so, not fatal. */
	ERRLOG("%s: set buffers: %i\n", dev->name, GetLastError());
    }

    /* Grab default config for the driver and set it. */
    d = sizeof(COMMCONFIG);
    memset(&conf, 0x00, d);
    conf.dwSize = d;
    if (GetDefaultCommConfig(temp, &conf, &d) == TRUE) {
	/* Change config here... */

	/* Set new configuration. */
	if (SetCommConfig(dev->handle, &conf, d) == FALSE) {
		/* This fails on FTDI-based devices, so, not fatal. */
		ERRLOG("%s: set config: %i\n", dev->name, GetLastError());
	}
    }
    ERRLOG("%s: host port '%s' open\n", dev->name, temp);

    /*
     * We now have an open port. To allow for clean exit
     * of the application, we first retrieve the port's
     * current settings, and save these for later.
     */
    if (get_state(dev, &dev->odcb) < 0) {
	plat_serial_close(dev);
	return(NULL);
    }
    memcpy(&dev->dcb, &dev->odcb, sizeof(DCB));

    /* Force the port to BINARY mode. */
    plat_serial_raw(dev, &dev->dcb);

    /* Set new state of this port. */
    if (set_state(dev, &dev->dcb) < 0) {
	plat_serial_close(dev);
	return(NULL);
    }

    /* Just to make sure.. disable RTS/CTS mode. */
    set_crtscts(dev, 0);

    /* Set new timeout values. */
    if (GetCommTimeouts(dev->handle, &to) == FALSE) {
	ERRLOG("%s: error %i while getting current TO\n",
				dev->name, GetLastError());
	plat_serial_close(dev);
	return(NULL);
    }

    if (tmo < 0) {
	/* No timeout, immediate return. */
	to.ReadIntervalTimeout = MAXDWORD;
	to.ReadTotalTimeoutMultiplier = 0;
	to.ReadTotalTimeoutConstant = 0;
    } else if (tmo == 0) {
	/* No timeout, wait for data. */
	memset(&to, 0x00, sizeof(to));
    } else {
	/* Timeout specified. */
	to.ReadIntervalTimeout = MAXDWORD;
	to.ReadTotalTimeoutMultiplier = MAXDWORD;
	to.ReadTotalTimeoutConstant = tmo;
    }
    if (SetCommTimeouts(dev->handle, &to) == FALSE) {
	ERRLOG("%s: error %i while setting TO\n", dev->name, GetLastError());
	plat_serial_close(dev);
	return(NULL);
    }

    /* Clear all errors and flush all buffers. */
    if (plat_serial_flush(dev) < 0) {
	plat_serial_close(dev);
	return(NULL);
    }

    return(dev);
}


/* API: activate the I/O for this port. */
int
plat_serial_active(void *arg, int flg)
{
    serial_t *dev = (serial_t *)arg;

    if (flg) {
	INFO("%s: starting thread..\n", dev->name);
	dev->tid = thread_create(reader_thread, dev);
    } else {
	if (dev->tid != NULL) {
		INFO("%s: stopping thread..\n", dev->name);
		thread_kill(dev->tid);
		dev->tid = NULL;
	}
    }

    return(0);
}


/* API: try to write data to an open port. */
int
plat_serial_write(void *arg, unsigned char val)
{
    serial_t *dev = (serial_t *)arg;
    DWORD n = 0;

pclog(0,"%s: writing byte %02x\n", dev->name, val);
    if (WriteFile(dev->handle, &val, 1, &n, &dev->wov) == FALSE) {
	n = GetLastError();
	if (n != ERROR_IO_PENDING) {
		/* Not good, we got an error. */
		ERRLOG("%s: I/O error %i in write!\n", dev->name, n);
		return(-1);
	}

	/* The write is pending, wait for it.. */
	if (GetOverlappedResult(dev->handle, &dev->wov, &n, TRUE) == FALSE) {
		n = GetLastError();
		ERRLOG("%s: I/O error %i in write!\n", dev->name, n);
		return(-1);
	}
    }

    return((int)n);
}


/*
 * API: try to read data from an open port.
 *
 * For now, we will use one byte per call.  Eventually,
 * we should go back to loading a buffer full of data,
 * just to speed things up a bit.  --FvK
 */
int
plat_serial_read(void *arg, unsigned char *bufp, int max)
{
    serial_t *dev = (serial_t *)arg;

    if (dev->icnt == 0) return(0);

    while (max-- > 0) {
	*bufp++ = dev->buff[dev->itail++];
pclog(0,"%s: dequeued byte %02x (%i)\n", dev->name, *(bufp-1), dev->icnt);
	dev->itail &= (sizeof(dev->buff)-1);
	if (--dev->icnt == 0) break;
    }

    return(max);
}
