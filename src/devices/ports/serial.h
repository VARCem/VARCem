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
 * Version:	@(#)serial.h	1.0.11	2019/05/13
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
#ifndef EMU_SERIAL_H
# define EMU_SERIAL_H


#define SERIAL_FIFO_MAX		64	/* maximum FIFO size */

/* Default settings for the standard ports. */
#define SERIAL1_ADDR		0x03f8
#define SERIAL1_IRQ		4
#define SERIAL2_ADDR		0x02f8
#define SERIAL2_IRQ		3

/* Supported UART types. */
#define UART_TYPE_8250		0	/* standard NS8250 */
#define UART_TYPE_8250A		1	/* updated NS8250(A) */
#define UART_TYPE_16450		2	/* 8250B / 16450 */
#define UART_TYPE_16550		3	/* 16550 (broken fifo) */
#define UART_TYPE_16550A	4	/* 16550A (working fifo) */
#define UART_TYPE_16670		5	/* 16670 (64b fifo) */


typedef struct {
    void	(*mcr)(void *dev, void *arg);
    uint8_t	(*read)(void *dev, void *arg);
    void	(*write)(void *dev, void *arg, uint8_t val);
} serial_ops_t;


/* Global variables. */
#ifdef EMU_DEVICE_H
extern const device_t	serial_1_device;
extern const device_t	serial_2_device;

extern const device_t	serial_1_pcjr_device;
#endif


/* Functions. */
extern void	serial_reset(void);

extern void	serial_setup(int port, uint16_t addr, int8_t irq);
extern void	*serial_attach(int port, serial_ops_t *ops, priv_t priv);
extern int	serial_link(int port, const char *name);

extern void	serial_clear(priv_t arg);
extern void	serial_write(priv_t arg, uint8_t *ptr, uint8_t len);

/* Platform serial driver support. */
extern priv_t	plat_serial_open(const char *port, int tmo);
extern void	plat_serial_close(priv_t);
extern int	plat_serial_active(priv_t, int flg);
extern int	plat_serial_params(priv_t, char dbit, char par, char sbit);
extern int	plat_serial_flush(priv_t);
extern int	plat_serial_speed(priv_t, long speed);
extern int	plat_serial_write(priv_t, uint8_t val);
extern int	plat_serial_read(priv_t, uint8_t *bufp, int max);


#endif	/*EMU_SERIAL_H*/
