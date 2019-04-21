/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement I/O ports and their operations.
 *
 * Version:	@(#)io.c	1.0.4	2019/04/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
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
#include "emu.h"
#include "io.h"
#include "cpu/cpu.h"


#define NPORTS		65536		/* PC/AT supports 64K ports */


typedef struct _io_ {
    uint8_t  (*inb)(uint16_t addr, void *priv);
    uint16_t (*inw)(uint16_t addr, void *priv);
    uint32_t (*inl)(uint16_t addr, void *priv);

    void     (*outb)(uint16_t addr, uint8_t  val, void *priv);
    void     (*outw)(uint16_t addr, uint16_t val, void *priv);
    void     (*outl)(uint16_t addr, uint32_t val, void *priv);

    void	*priv;

    struct _io_ *prev, *next;
} io_t;


static int	initialized = 0;
static io_t	*io[NPORTS],
		*io_last[NPORTS];
  

#ifdef IO_CATCH
static uint8_t null_inb(uint16_t addr, void *priv) { DEBUG("IO: read(%04x)\n"); return(0xff); }
static uint16_t null_inw(uint16_t addr, void *priv) { DEBUG("IO: readw(%04x)\n"); return(0xffff); }
static uint32_t null_inl(uint16_t addr, void *priv) { DEBUG("IO: readl(%04x)\n"); return(0xffffffff); }
static void null_outb(uint16_t addr, uint8_t val, void *priv) { DEBUG("IO: write(%04x, %02x)\n", val); }
static void null_outw(uint16_t addr, uint16_t val, void *priv) { DEBUG("IO: writew(%04x, %04x)\n", val); }
static void null_outl(uint16_t addr, uint32_t val, void *priv) { DEBUG("IO: writel(%04x, %08lx)\n", val); }
#endif


void
io_reset(void)
{
    io_t *p, *q;
    int c;

    INFO("IO: initializing\n");
    if (! initialized) {
	for (c = 0; c < NPORTS; c++)
		io[c] = io_last[c] = NULL;
	initialized = 1;
    }

    for (c = 0; c < NPORTS; c++) {
        if (io_last[c] != NULL) {
		/* Port has at least one handler. */
		p = io_last[c];
		while (p != NULL) {
			q = p->prev;
			free(p);
			p = q;
		}
		p = NULL;
	}

#ifdef IO_CATCH
	/* Set up a default (catch) handler. */
	p = (io_t *)mem_alloc(sizeof(io_t));
	memset(p, 0x00, sizeof(io_t));
	io[c] = io_last[c] = p;
	p->next = NULL;
	p->prev = NULL;
	p->inb  = null_inb;
	p->outb = null_outb;
	p->inw  = null_inw;
	p->outw = null_outw;
	p->inl  = null_inl;
	p->outl = null_outl;
	p->priv = NULL;
#else
	io[c] = io_last[c] = NULL;
#endif
    }
}


void
io_sethandler(uint16_t base, int size, 
	      uint8_t (*f_inb)(uint16_t addr, void *priv),
	      uint16_t (*f_inw)(uint16_t addr, void *priv),
	      uint32_t (*f_inl)(uint16_t addr, void *priv),
	      void (*f_outb)(uint16_t addr, uint8_t val, void *priv),
	      void (*f_outw)(uint16_t addr, uint16_t val, void *priv),
	      void (*f_outl)(uint16_t addr, uint32_t val, void *priv),
	      void *priv)
{
    io_t *p, *q = NULL;
    int c;

    for (c = 0; c < size; c++) {
	p = io_last[base + c];
	q = (io_t *)mem_alloc(sizeof(io_t));
	memset(q, 0x00, sizeof(io_t));
	if (p != NULL) {
		p->next = q;
		q->prev = p;
	} else {
		io[base + c] = q;
		q->prev = NULL;
	}

	q->inb = f_inb; q->inw = f_inw; q->inl = f_inl;

	q->outb = f_outb; q->outw = f_outw; q->outl = f_outl;

	q->priv = priv;
	q->next = NULL;

	io_last[base + c] = q;
    }
}


void
io_removehandler(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, void *priv),
	uint16_t (*f_inw)(uint16_t addr, void *priv),
	uint32_t (*f_inl)(uint16_t addr, void *priv),
	void (*f_outb)(uint16_t addr, uint8_t val, void *priv),
	void (*f_outw)(uint16_t addr, uint16_t val, void *priv),
	void (*f_outl)(uint16_t addr, uint32_t val, void *priv),
	void *priv)
{
    io_t *p;
    int c;

    for (c = 0; c < size; c++) {
	p = io[base + c];
	if (p == NULL)
		continue;
	while (p != NULL) {
		if ((p->inb == f_inb) && (p->inw == f_inw) &&
		    (p->inl == f_inl) && (p->outb == f_outb) &&
		    (p->outw == f_outw) && (p->outl == f_outl) &&
		    (p->priv == priv)) {
			if (p->prev != NULL)
				p->prev->next = p->next;
			  else
				io[base + c] = p->next;
			if (p->next != NULL)
				p->next->prev = p->prev;
			  else
				io_last[base + c] = p->prev;
			free(p);
			p = NULL;
			break;
		}
		p = p->next;
	}
    }
}


#ifdef PC98
void
io_sethandler_interleaved(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, void *priv),
	uint16_t (*f_inw)(uint16_t addr, void *priv),
	uint32_t (*f_inl)(uint16_t addr, void *priv),
	void (*f_outb)(uint16_t addr, uint8_t val, void *priv),
	void (*f_outw)(uint16_t addr, uint16_t val, void *priv),
	void (*f_outl)(uint16_t addr, uint32_t val, void *priv),
	void *priv)
{
    io_t *p, *q;
    int c;

    size <<= 2;
    for (c = 0; c < size; c += 2) {
	p = last_handler(base + c);
	q = (io_t *)mem_alloc(sizeof(io_t));
	memset(q, 0x00, sizeof(io_t));
	if (p != NULL) {
		p->next = q;
		q->prev = p;
	} else {
		io[base + c] = q;
		q->prev = NULL;
	}

	q->inb = f_inb; q->inw = f_inw; q->inl = f_inl;

	q->outb = f_outb; q->outw = f_outw; q->outl = f_outl;

	q->priv = priv;
    }
}


void
io_removehandler_interleaved(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, void *priv),
	uint16_t (*f_inw)(uint16_t addr, void *priv),
	uint32_t (*f_inl)(uint16_t addr, void *priv),
	void (*f_outb)(uint16_t addr, uint8_t val, void *priv),
	void (*f_outw)(uint16_t addr, uint16_t val, void *priv),
	void (*f_outl)(uint16_t addr, uint32_t val, void *priv),
	void *priv)
{
    io_t *p;
    int c;

    size <<= 2;
    for (c = 0; c < size; c += 2) {
	p = io[base + c];
	if (p == NULL)
		return;
	while (p != NULL) {
		if ((p->inb == f_inb) && (p->inw == f_inw) &&
		    (p->inl == f_inl) && (p->outb == f_outb) &&
		    (p->outw == f_outw) && (p->outl == f_outl) &&
		    (p->priv == priv)) {
			if (p->prev != NULL)
				p->prev->next = p->next;
			if (p->next != NULL)
				p->next->prev = p->prev;
			free(p);
			break;
		}
		p = p->next;
	}
    }
}
#endif


uint8_t
inb(uint16_t port)
{
    uint8_t r = 0xff;
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inb != NULL)
		r &= p->inb(port, p->priv);
	p = p->next;
    }

#ifdef IO_TRACE
    if (CS == IO_TRACE)
	DEBUG("IOTRACE(%04X): inb(%04x)=%02x\n", IO_TRACE, port, r);
#endif

    return(r);
}


void
outb(uint16_t port, uint8_t val)
{
    io_t *p;

    if (io[port] != NULL) {
	p = io[port];
	while (p != NULL) {
		if (p->outb != NULL)
			p->outb(port, val, p->priv);
		p = p->next;
	}
    }

#ifdef IO_TRACE
    if (CS == IO_TRACE)
	DEBUG("IOTRACE(%04X): outb(%04x,%02x)\n", IO_TRACE, port, val);
#endif
}


uint16_t
inw(uint16_t port)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inw != NULL)
		return(p->inw(port, p->priv));
	p = p->next;
    }

    return(inb(port) | (inb(port + 1) << 8));
}


void
outw(uint16_t port, uint16_t val)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->outw != NULL) {
		p->outw(port, val, p->priv);
		return;
	}
	p = p->next;
    }

    outb(port,val & 0xff);
    outb(port+1,val>>8);
}


uint32_t
inl(uint16_t port)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inl != NULL)
		return(p->inl(port, p->priv));
	p = p->next;
    }

    return(inw(port) | (inw(port + 2) << 16));
}


void
outl(uint16_t port, uint32_t val)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->outl != NULL) {
		p->outl(port, val, p->priv);
		return;
	}
	p = p->next;
    }

    outw(port, val);
    outw(port + 2, val >> 16);
}
