/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of NS8250-series UART devices.
 *
 *		The original IBM-PC design did not have any serial ports of
 *		any kind. Rather, these were offered as add-on devices, most
 *		likely because a) most people did not need one at the time,
 *		and, b) this way, IBM could make more money off them.
 *
 *		So, for the PC, the offerings were for an IBM Asynchronous
 *		Communications Adapter, and, later, a model for synchronous
 *		communications. The "Async Adapter" was based on the NS8250
 *		UART chip, and is what we now call the "com" port of the PC.
 *
 *		Of course, many system builders came up with similar boards,
 *		and even more boards were designed where several I/O functions
 *		were combined into a single "multi-I/O" board, as that saved
 *		space and bus slots. Early boards had discrete chips for most
 *		functions, but later on, many of these were integrated into a
 *		single "super-I/O" chip.
 *
 *		This file implements the standard NS8250, as well as the later
 *		16450 and 16550 series, which fixed bugs and added features
 *		like FIFO buffers, higher line speeds and DMA transfers.
 *
 *		The lower half of the driver can interface to the host system
 *		serial ports, or other channels, for real-world access.
 *
 * Version:	@(#)serial.c	1.0.14	2019/04/14
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog serial_log
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../system/pic.h"
#include "../../plat.h"
#include "serial.h"


/* Interrupt reasons. */
#define SER_INT_LSR	0x01
#define SER_INT_RX	0x02
#define SER_INT_TX	0x04
#define SER_INT_MSR	0x08

/* IER register bits. */
#define IER_RDAIE	0x01
#define IER_THREIE	0x02
#define IER_RXLSIE	0x04
#define IER_MSIE	0x08
#define IER_SLEEP	0x10		/* NS16750 */
#define IER_LOWPOWER	0x20		/* NS16750 */
#define IER_MASK	0x0f		/* not including SLEEP|LOWP */

/* IIR register bits. */
#define IIR_IP		0x01
#define IIR_IID		0x0e
# define IID_IDMDM	0x00
# define IID_IDTX	0x02
# define IID_IDRX	0x04
# define IID_IDERR	0x06
# define IID_IDTMO	0x0c		/* 16550+ */
#define IIR_IIRFE	0xc0		/* 16550+ */
# define IIR_FIFO64	0x20
# define IIR_FIFOBAD	0x80
# define IIR_FIFOENB	0xc0

/* FCR register bits. */
#define FCR_FCRFE	0x01
#define FCR_RFR		0x02
#define FCR_TFR		0x04
#define FCR_SELDMA1	0x08
#define FCR_FENB64	0x20		/* 16750 */
#define FCR_RTLS	0xc0
# define FCR_RTLS1	0x00
# define FCR_RTLS4	0x40
# define FCR_RTLS8	0x80
# define FCR_RTLS14	0xc0

/* LCR register bits. */
#define LCR_WLS		0x03
# define WLS_BITS5	0x00
# define WLS_BITS6	0x01
# define WLS_BITS7	0x02
# define WLS_BITS8	0x03
#define LCR_SBS		0x04
#define LCR_PE		0x08
#define LCR_EP		0x10
#define LCR_PS		0x20
# define PAR_NONE	0x00
# define PAR_EVEN	(LCR_PE | LCR_EP)
# define PAR_ODD	LCR_PE
# define PAR_MARK	(LCR_PE | LCR_PS)
# define PAR_SPACE	(LCR_PE | LCR_PS | LCR_EP)
#define LCR_BC		0x40
#define LCR_DLAB	0x80

/* MCR register bits. */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_OUT1	0x04		/* 8250 */
#define MCR_OUT2	0x08		/* 8250, INTEN on IBM-PC */
#define MCR_LMS		0x10
#define MCR_AUTOFLOW	0x20		/* 16750 */

/* LSR register bits. */
#define	LSR_DR		0x01
#define	LSR_OE		0x02
#define	LSR_PE		0x04
#define	LSR_FE		0x08
#define	LSR_BI		0x10
#define	LSR_THRE	0x20
#define	LSR_TEMT	0x40
#define	LSR_RXFE	0x80
#define LSR_MASK	(LSR_BI|LSR_FE|LSR_PE|LSR_OE)

/* MSR register bits. */
#define MSR_DCTS	0x01
#define MSR_DDSR	0x02
#define MSR_TERI	0x04
#define MSR_DDCD	0x08
#define MSR_CTS		0x10
#define MSR_DSR		0x20
#define MSR_RI		0x40
#define MSR_DCD		0x80
#define MSR_MASK	(MSR_DDCD|MSR_TERI|MSR_DDSR|MSR_DCTS)


typedef struct serial {
    int8_t	port;			/* port number (0,1,..) */
    int8_t	irq;			/* IRQ channel used */
    uint16_t	base;			/* I/O address used */

    int8_t	is_pcjr;		/* PCjr UART (fixed OUT2) */
    int8_t	type;			/* UART type */
    uint8_t	int_status;

    uint8_t	lsr, thr, mcr, rcr,	/* UART registers */
		iir, ier, lcr, msr;
    uint8_t	dlab1, dlab2;
    uint8_t	dat,
		hold;
    uint8_t	scratch;
    uint8_t	fcr;

    /* Callback data. */
    serial_ops_t *ops;
    void	*ops_arg;

    int64_t	delay;

    void	*bh;			/* BottomHalf handler */

    int		fifo_read,
		fifo_write;
    uint8_t	fifo[64];
} serial_t;


#ifdef ENABLE_SERIAL_LOG
int		serial_do_log = ENABLE_SERIAL_LOG;
#endif


static serial_t	ports[SERIAL_MAX];	/* the ports */


#ifdef _LOGGING
static void
serial_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SERIAL_LOG
    va_list ap;

    if (serial_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


static void
update_ints(serial_t *dev)
{
    int stat = 0;

    /* None yet. */
    dev->iir = IIR_IP;

    if ((dev->ier & IER_RXLSIE) && (dev->int_status & SER_INT_LSR)) {
	/* Line Status interrupt. */
	stat = 1;
	dev->iir = IID_IDERR;
    } else if ((dev->ier & IER_RDAIE) && (dev->int_status & SER_INT_RX)) {
	/* Received Data available. */
	stat = 1;
	dev->iir = IID_IDRX;
    } else if ((dev->ier & IER_THREIE) && (dev->int_status & SER_INT_TX)) {
	/* Transmit Data empty. */
	stat = 1;
	dev->iir = IID_IDTX;
    } else if ((dev->ier & IER_MSIE) && (dev->int_status & SER_INT_MSR)) {
	/* Modem Status interrupt. */
	stat = 1;
	dev->iir = IID_IDMDM;
    }

    DEBUG("Serial%d: intr, IIR=%02X, type=%d, mcr=%02X\n",
		dev->port, dev->iir, dev->type, dev->mcr);

    /* Are hardware interrupts enabled? */
    if (!(dev->mcr & MCR_OUT2) && !dev->is_pcjr) return;

    if (stat) {
	/* Raise an interrupt. */
	if (dev->type < UART_TYPE_16450) {
		/* Edge-triggered. */
		picint(1 << dev->irq);               
	} else {
		/* Level-triggered. */
		picintlevel(1 << dev->irq);
	}
    } else {
	/* Clear an interrupt. */
	picintc(1 << dev->irq);
    }
}


static void
clear_fifo(serial_t *dev)
{
    memset(dev->fifo, 0x00, sizeof(dev->fifo));

    dev->fifo_read = dev->fifo_write = 0;
}


static void
write_fifo(serial_t *dev, uint8_t *ptr, uint8_t len)
{
    while (len-- > 0) {
	dev->fifo[dev->fifo_write++] = *ptr++;
	if (dev->fifo_write == sizeof(dev->fifo))
		dev->fifo_write = 0;
	/*OVERFLOW NOT DETECTED*/
    }

    if (! (dev->lsr & LSR_DR)) {
	dev->lsr |= LSR_DR;
	dev->int_status |= SER_INT_RX;

	update_ints(dev);
    }
}


static uint8_t
read_fifo(serial_t *dev)
{
    if (dev->fifo_read != dev->fifo_write) {
	dev->dat = dev->fifo[dev->fifo_read++];
	if (dev->fifo_read == sizeof(dev->fifo))
		dev->fifo_read = 0;
    }

#if 0
    /* If we have more, generate (new) int. */
    if (sp->fifo_read != sp->fifo_write) {
#if 1
	sp->delay = 1000*TIMER_USEC;
#else
	if (sp->bh != NULL) {
		sp->int_status |= SERINT_RECEIVE;
		sp->lsr |= LSR_DR;

		/* Update interrupt state. */
		update_ints(sp);
	} else {
		sp->delay = 1000*TIMER_USEC;
	}
#endif
	}
    }
#endif

    return(dev->dat);
}


static void
receive_callback(void *priv)
{
    serial_t *dev = (serial_t *)priv;

    dev->delay = 0;

    if (dev->fifo_read != dev->fifo_write) {
	dev->lsr |= LSR_DR;
	dev->int_status |= SER_INT_RX;

	update_ints(dev);
    }
}


static void
reset_port(serial_t *dev)
{
    dev->iir = dev->ier = dev->lcr = 0;
    dev->fifo_read = dev->fifo_write = 0;

    dev->int_status = 0x00;
}


static void
read_timer(void *priv)
{
    serial_t *dev = (serial_t *)priv;

    dev->delay = 0;

    if (dev->fifo_read != dev->fifo_write) {
	dev->lsr |= LSR_DR;
	dev->int_status |= SER_INT_RX;
	update_ints(dev);
    }
}


#ifdef USE_HOST_SERIAL
/* Platform module has data, so read it! */
static void
read_done(void *arg, int num)
{
    serial_t *dev = (serial_t *)arg;

    /* We can do at least 'num' bytes.. */
    while (num-- > 0) {
	/* Get a byte from them. */
	if (plat_serial_read(dev->bh, &dev->hold, 1) < 0) break;

	/* Stuff it into the FIFO and set intr. */
	write_fifo(dev, &dev->hold, 1);
    }

    /* We have data waiting for us.. delay a little, and then read it. */
//    timer_add(ser_timer, &dev->delay, &dev->delay, dev);
}
#endif


static void
ser_write(uint16_t addr, uint8_t val, void *priv)
{
    serial_t *dev = (serial_t *)priv;
#if defined(_LOGGING) || defined(USE_HOST_SERIAL)
    uint32_t speed;
    uint8_t wl, sb, pa;
#endif
    uint32_t baud;
    uint8_t msr;


    DEBUG("Serial%i: write(%i, %02x)\n", dev->port, (addr & 0x0007), val);

    switch (addr & 0x0007) {
	case 0:		/* DATA,DLAB1 */
                if (dev->lcr & LCR_DLAB) {
			/* DLAB set, set DLAB low byte. */
                        dev->dlab1 = val;
                        return;
                }

		/* DLAB clear, regular data write. */
                dev->thr = val;

#ifdef USE_HOST_SERIAL
		if (dev->bh != NULL) {
			/* We are linked, so send to BH layer. */
			plat_serial_write(dev->bh, dev->thr);
		} else
#endif

		if (dev->ops && dev->ops->write)
			dev->ops->write(dev, dev->ops_arg, val);

		/* WRITE completed, we are ready for more. */
		dev->lsr |= LSR_THRE;
                dev->int_status |= SER_INT_TX;
                update_ints(dev);

		/* Loopback echo data to RX needed? */
                if (dev->mcr & MCR_LMS) {
                        write_fifo(dev, &val, 1);

			dev->int_status |= SER_INT_TX;
			update_ints(dev);
		}
                break;

	case 1:		/* IER,DLAB2 */
                if (dev->lcr & LCR_DLAB) {
			/* DLAB set, set DLAB high byte. */
                        dev->dlab2 = val;
                        return;
                }

		/* DLAB clear, set IER register bits. */
		dev->ier = (val & IER_MASK);
                update_ints(dev);
                break;

	case 2:		/* FCR */
		if (dev->type >= UART_TYPE_16550) {
pclog(0,"Serial%i: tried to enable FIFO (%02x), type %d!\n", dev->port, val, dev->type);
			dev->fcr = val;
		}
                break;

	case 3:		/* LCR */
		if ((dev->lcr & LCR_DLAB) && !(val & LCR_DLAB)) {
			/* We dropped DLAB, so handle baudrate. */
			baud = ((dev->dlab2 << 8) | dev->dlab1);
			if (baud > 0) {
#if defined(_LOGGING) || defined(USE_HOST_SERIAL)
				speed = 115200UL / baud;
#endif
				DEBUG("Serial%i: divisor %u, baudrate %i\n",
						dev->port, baud, speed);
#ifdef USE_HOST_SERIAL
				if (dev->bh != NULL)
					plat_serial_speed(dev->bh, speed);
#endif
			} else {
				DEBUG("Serial%i: divisor %u invalid!\n",
							dev->port, baud);
			}
		}

#if defined(_LOGGING) || defined(USE_HOST_SERIAL)
		wl = (val & LCR_WLS) + 5;		/* databits */
		sb = (val & LCR_SBS) ? 2 : 1;		/* stopbits */
		pa = (val & (LCR_PE|LCR_EP|LCR_PS)) >> 3;
#endif
		DEBUG("Serial%i: WL=%i SB=%i PA=%i\n", dev->port, wl, sb, pa);
#ifdef USE_HOST_SERIAL
		if (dev->bh != NULL)
			plat_serial_params(dev->bh, wl, pa, sb);
#endif

		dev->lcr = val;
		break;

	case 4:		/* MCR */
		if (dev->bh == NULL) {
			/* Not linked, force LOOPBACK mode. */
			val |= MCR_LMS;
		}

		if ((val & MCR_RTS) && !(dev->mcr & MCR_RTS)) {
			/*
			 * This is old code for use by the Serial Mouse
			 * driver. If the user toggles RTS, serial mice
			 * are expected to send an ID, to inform any
			 * enumerator there 'is' something.
			 */
			if (dev->ops && dev->ops->mcr) {
				dev->ops->mcr(dev, dev->ops_arg);
				DEBUG("Serial%i: RTS raised\n", dev->port);
			}
		}

		if ((val & MCR_OUT2) && !(dev->mcr & MCR_OUT2)) {
#ifdef USE_HOST_SERIAL
			if (dev->bh != NULL) {
				/* Linked, start host port. */
				(void)plat_serial_active(dev->bh, 1);
			} else {
#endif
				/* Not linked, start RX timer. */
				timer_add(read_timer,
					  &dev->delay, &dev->delay, dev);

				/* Fake CTS, DSR and DCD (for now.) */
				dev->msr = (MSR_CTS | MSR_DCTS |
					    MSR_DSR | MSR_DDSR |
					    MSR_DCD | MSR_DDCD);
				dev->int_status |= SER_INT_MSR;
				update_ints(dev);
#ifdef USE_HOST_SERIAL
			}
#endif
		}

                dev->mcr = val;

		if (val & MCR_LMS) {	/* loopback mode */
			msr = (val & 0x0c) << 4;
			msr |= (val & MCR_RTS) ? MCR_LMS : 0;
			msr |= (val & MCR_DTR) ? MCR_AUTOFLOW : 0;

			if ((dev->msr ^ msr) & MSR_CTS)
				msr |= MSR_DCTS;
			if ((dev->msr ^ msr) & MSR_DSR)
				msr |= MSR_DDSR;
			if ((dev->msr ^ msr) & MSR_DCD)
				msr |= MSR_DDCD;
			if ((dev->msr & MSR_TERI) && !(msr & MSR_RI))
				msr |= MSR_TERI;

			dev->msr = msr;
		}
		break;

	case 5:		/* LSR */
                if (val & LSR_MASK)
                        dev->int_status |= SER_INT_LSR;
                if (val & LSR_DR)
                        dev->int_status |= SER_INT_RX;
                if (val & LSR_THRE)
                        dev->int_status |= SER_INT_TX;
                dev->lsr = val;
                update_ints(dev);
                break;

	case 6:		/* MSR */
                dev->msr = val;
		if (dev->msr & MSR_MASK)
                        dev->int_status |= SER_INT_MSR;
                update_ints(dev);
                break;

	case 7:		/* SCRATCH */
		if (dev->type > UART_TYPE_8250) {
			dev->scratch = val;
		}
                break;
    }
}


static uint8_t
ser_read(uint16_t addr, void *priv)
{
    serial_t *dev = (serial_t *)priv;
    uint8_t ret = 0x00;

    switch (addr & 0x0007) {
	case 0:		/* DATA / DLAB1 */
                if (dev->lcr & LCR_DLAB) {
			/* DLAB set, read DLAB low byte. */
                        ret = dev->dlab1;
                        break;
                }

		/*
		 * DLAB clear, regular data read.
		 * First, clear the RXDATA interrupt.
		 */
                dev->lsr &= ~LSR_DR;
                dev->int_status &= ~SER_INT_RX;
                update_ints(dev);

		/* If there is data in the RX FIFO, grab it. */
                ret = read_fifo(dev);

                if (dev->fifo_read != dev->fifo_write) {
                        dev->delay = 1000LL * TIMER_USEC;
		}
                break;

	case 1:		/* LCR / DLAB2 */
		if (dev->lcr & LCR_DLAB) {
			/* DLAB set, read DLAB high byte. */
			ret = dev->dlab2;
		} else {
			/* DLAB clear, read IER register bits. */
			ret = dev->ier;
		}
		break;

	case 2: 	/* IIR */
                ret = dev->iir;
		if ((ret & IIR_IID) == IID_IDTX) {
			/* Transmit is done. */
                        dev->int_status &= ~SER_INT_TX;
                        update_ints(dev);
                }

		if (dev->type >= UART_TYPE_16550) {
			/* If FIFO enabled.. */
                	if (dev->fcr & FCR_FCRFE)
				/* Report FIFO active. */
                        	ret |= FCR_RTLS14;
		}
                break;

	case 3:		/* LCR */
                ret = dev->lcr;
                break;

	case 4:		/* MCR */
                ret = dev->mcr;
                break;

	case 5:		/* LSR */
                if (dev->lsr & LSR_THRE)
                        dev->lsr |= LSR_TEMT;
                dev->lsr |= LSR_THRE;
                ret = dev->lsr;
                if (dev->lsr & 0x1f)
                        dev->lsr &= ~0x1e;

                dev->int_status &= ~SER_INT_LSR;
                update_ints(dev);
                break;

	case 6:		/* MSR */
		/* Grab current modem status and reset delta bits. */
                ret = dev->msr;
                dev->msr &= ~0x0f;

		/* Clear MSR interrupt status. */
                dev->int_status &= ~SER_INT_MSR;
                update_ints(dev);
                break;

	case 7:		/* SCRATCH */
		if (dev->type > UART_TYPE_8250) {
			ret = dev->scratch;
		}
                break;
    }

    return(ret);
}


static void *
ser_init(const device_t *info, UNUSED(void *parent))
{
    serial_t *dev;

    /* Get the correct device. */
    dev = &ports[(info->local & 127) - 1];

    /* Set up local/weird stuff. */
    if (info->local & 128)
	dev->is_pcjr = 1;

    /* Clear port. */
    reset_port(dev);

    /* Enable the I/O handler for this port. */
    io_sethandler(dev->base, 8, ser_read,NULL,NULL, ser_write,NULL,NULL, dev);

    timer_add(receive_callback, &dev->delay, &dev->delay, dev);

    INFO("SERIAL: COM%i (I/O=%04X, IRQ=%i)\n",
	 info->local & 127, dev->base, dev->irq);

    return(dev);
}


static void
ser_close(void *priv)
{
    serial_t *dev = (serial_t *)priv;

#ifdef USE_HOST_SERIAL
    /* Close the host device. */
    if (dev->bh != NULL)
	(void)serial_link(dev->port, NULL);
#endif

    /* Remove the I/O handler. */
    io_removehandler(dev->base, 8,
		     ser_read,NULL,NULL, ser_write,NULL,NULL, dev);

    /* Clear port. */
    reset_port(dev);
}


const device_t serial_1_device = {
    "COM1",
    0, 1, NULL,
    ser_init, ser_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t serial_2_device = {
    "COM2",
    0, 2, NULL,
    ser_init, ser_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL,
};

const device_t serial_1_pcjr_device = {
    "COM1 (PCjr)",
    0,
    1+128,
    NULL,
    ser_init, ser_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


/* API: (re-)initialize all serial ports. */
void
serial_reset(void)
{
    serial_t *dev;
    int i;

    DEBUG("SERIAL: reset ([%i] [%i])\n",
	serial_enabled[0], serial_enabled[1]);

    for (i = 0; i < SERIAL_MAX; i++) {
	/* Get the correct device and clear it. */
	dev = &ports[i];
	memset(dev, 0x00, sizeof(serial_t));
	dev->port = i;
	dev->type = UART_TYPE_8250;

	/* Set up default port and IRQ for the device. */
	switch(i) {
		case 0:		/* standard first port, "COM1" */
			dev->base = SERIAL1_ADDR;
			dev->irq = 4;
			break;

		case 1:		/* standard second port, "COM2" */
			dev->base = SERIAL2_ADDR;
			dev->irq = 3;
			break;

		case 2:		/* "COM3" */
			dev->base = 0x03e8;
			dev->irq = 4;
			break;

		case 3:		/* "COM4" */
			dev->base = 0x02e8;
			dev->irq = 3;
			break;

		default:
			break;
	}

	/* Clear port. */
	reset_port(dev);
    }
}


/* API: set up (the address/IRQ of) one of the serial ports. */
void
serial_setup(int id, uint16_t port, int8_t irq)
{
    serial_t *dev = &ports[id];

    INFO("SERIAL: setting up COM%i as %04X [enabled=%i]\n",
			id+1, port, serial_enabled[id]);

    if (! serial_enabled[id]) return;

    dev->base = port;
    dev->irq = irq;
}


/* API: attach another device to a serial port. */
void *
serial_attach(int port, serial_ops_t *ops, void *arg)
{
    serial_t *dev;

    /* No can do if port not enabled. */
    if (! serial_enabled[port]) return(NULL);

    /* Grab the desired port block. */
    dev = &ports[port];

    /* Set up callback info. */
    dev->ops = ops;
    dev->ops_arg = arg;

    return(dev);
}


#ifdef USE_HOST_SERIAL
/* Link a serial port to a host (serial) port. */
int
serial_link(int port, const char *arg)
{
    serial_t *dev;

    /* No can do if port not enabled. */
    if (! serial_enabled[port]) return(-1);

    /* Grab the desired port block. */
    dev = &ports[port];

    if (arg != NULL) {
	/* Make sure we're not already linked. */
	if (dev->bh != NULL) {
		ERRLOG("Serial%i already linked !\n", port);
		return(-1);
	}

	/* Request a port from the host system. */
	dev->bh = plat_serial_open(arg, 0);
	if (dev->bh == NULL) {
		ERRLOG("Serial%i unable to link to '%s' !\n", port, arg);
		return(-1);
	}

	/* Set up bottom-half I/O callback info. */
#if 0
	bh->rd_done = read_done;
	bh->rd_arg = dev;
#endif
    } else {
	/* If we are linked, unlink it. */
	if (dev->bh != NULL) {
		plat_serial_close(dev->bh);
		dev->bh = NULL;
	}

    }

    return(0);
}
#endif


/* API: clear the FIFO buffers of a serial port. */
void
serial_clear(void *arg)
{
    serial_t *dev = (serial_t *)arg;

    clear_fifo(dev);
}


/* API: write data to a serial port. */
void
serial_write(void *arg, uint8_t *ptr, uint8_t len)
{
    serial_t *dev = (serial_t *)arg;

    write_fifo(dev, ptr, len);
}
