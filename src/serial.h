/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the SERIAL card.
 *
 * Version:	@(#)serial.h	1.0.2	2018/04/05
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
#ifndef EMU_SERIAL_H
# define EMU_SERIAL_H


#define SERIAL_MAX		2	/* two ports supported */

/* Default settings for the standard ports. */
#define SERIAL1_ADDR		0x03f8
#define SERIAL1_IRQ		4
#define SERIAL2_ADDR		0x02f8
#define SERIAL2_IRQ		3


#ifdef WALTJE_SERIAL

/* Supported UART types. */
#define UART_TYPE_8250		0	/* standard NS8250 */
#define UART_TYPE_8250A		1	/* updated NS8250(A) */
#define UART_TYPE_16450		2	/* 16450 */
#define UART_TYPE_16550		3	/* 16550 (broken fifo) */
#define UART_TYPE_16550A	4	/* 16550a (working fifo) */
#define UART_TYPE_16670		5	/* 16670 (64b fifo) */


typedef struct _serial_ {
    int8_t	port;			/* port number (1,2,..) */
    int8_t	irq;			/* IRQ channel used */
    uint16_t	addr;			/* I/O address used */
    int8_t	type;			/* UART type */
    uint8_t	int_status;

    uint8_t	lsr, thr, mctrl, rcr,	/* UART registers */
		iir, ier, lcr, msr;
    uint8_t	dlab1, dlab2;
    uint8_t	dat,
		hold;
    uint8_t	scratch;
    uint8_t	fcr;

    /* Data for the RTS-toggle callback. */
    void	(*rts_callback)(void *);
    void	*rts_callback_p;

    uint8_t	fifo[256];
    int		fifo_read, fifo_write;

    int64_t	receive_delay;

    void	*bh;			/* BottomHalf handler */
} SERIAL;


/* Functions. */
extern void	serial_init(void);
extern void	serial_reset(void);
extern void	serial_setup(int port, uint16_t addr, int irq);
extern void	serial_remove(int port);
extern SERIAL	*serial_attach(int, void *, void *);
extern int	serial_link(int, char *);

extern void	serial_clear_fifo(SERIAL *);
extern void	serial_write_fifo(SERIAL *, uint8_t, int);


#else


void serial_remove(int port);
void serial_setup(int port, uint16_t addr, int irq);
void serial_init(void);
void serial_reset();

struct SERIAL;

typedef struct
{
        uint8_t lsr,thr,mctrl,rcr,iir,ier,lcr,msr;
        uint8_t dlab1,dlab2;
        uint8_t dat;
        uint8_t int_status;
        uint8_t scratch;
        uint8_t fcr;
        
        int irq;

        void (*rcr_callback)(struct SERIAL *serial, void *p);
        void *rcr_callback_p;
        uint8_t fifo[256];
        int fifo_read, fifo_write;
        
        int64_t recieve_delay;
} SERIAL;

void serial_clear_fifo(SERIAL *);
void serial_write_fifo(SERIAL *serial, uint8_t dat);

extern SERIAL serial1, serial2;
#endif


#endif	/*EMU_SERIAL_H*/
