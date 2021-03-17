/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,-*
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the I2C bus and its operations.
 *
 * Version:	@(#)i2c.c	1.0.1	2021/03/16
 *
 * Author:	RichardG, <richardg867@gmail.com>
 *
 *		Copyright 2020-2021 RichardG.
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
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog i2c_log
#include "../../emu.h"
#include "i2c.h"


#define NADDRS		128		/* I2C supports 128 addresses */
#define MAX(a, b) ((a) > (b) ? (a) : (b))


typedef struct _i2c_ {
    uint8_t	(*start)(void *bus, uint8_t addr, uint8_t read, void *priv);
    uint8_t	(*read)(void *bus, uint8_t addr, void *priv);
    uint8_t	(*write)(void *bus, uint8_t addr, uint8_t data, void *priv);
    void	(*stop)(void *bus, uint8_t addr, void *priv);

    void	*priv;

    struct _i2c_ *prev, *next;
} i2c_t;

typedef struct {
    const char	*name;
    i2c_t	*devices[NADDRS],
		*last[NADDRS];
} i2c_bus_t;


void *i2c_smbus;


#ifdef ENABLE_I2C_LOG
int i2c_do_log = ENABLE_I2C_LOG;
#endif


#ifdef _LOGGING
static void
i2c_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_I2C_LOG
    va_list ap;

    if ((i2c_do_log + LOG_INFO) >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void *
i2c_addbus(const char *name)
{
    i2c_bus_t *bus = (i2c_bus_t *)mem_alloc(sizeof(i2c_bus_t));
    memset(bus, 0, sizeof(i2c_bus_t));

    bus->name = name;

    return bus;
}


void
i2c_removebus(void *bus_handle)
{
    int c;
    i2c_t *p, *q;
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;

    if (!bus_handle)
	return;

    for (c = 0; c < NADDRS; c++) {
	p = bus->devices[c];
	if (!p)
		continue;
	while (p) {
		q = p->next;
		free(p);
		p = q;
	}
    }

    free(bus);
}


const char *
i2c_getbusname(void *bus_handle)
{
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;

    if (!bus_handle)
	return(NULL);

    return(bus->name);
}


void
i2c_sethandler(void *bus_handle, uint8_t base, int size,
	uint8_t (*start)(void *bus, uint8_t addr, uint8_t read, priv_t priv),
	uint8_t (*read)(void *bus, uint8_t addr, priv_t priv),
	uint8_t (*write)(void *bus, uint8_t addr, uint8_t data, priv_t priv),
	void (*stop)(void *bus, uint8_t addr, priv_t priv),
	priv_t priv)
{
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;
    i2c_t *p, *q = NULL;
    int c;

    if (!bus_handle || ((base + size) > NADDRS))
	return;

    for (c = 0; c < size; c++) {
	p = bus->last[base + c];
	q = (i2c_t *) malloc(sizeof(i2c_t));
	memset(q, 0, sizeof(i2c_t));
	if (p) {
		p->next = q;
		q->prev = p;
	} else {
		bus->devices[base + c] = q;
		q->prev = NULL;
	}

	q->start = start;
	q->read = read;
	q->write = write;
	q->stop = stop;

	q->priv = priv;
	q->next = NULL;

	bus->last[base + c] = q;
    }
}


void
i2c_removehandler(void *bus_handle, uint8_t base, int size,
	  uint8_t (*start)(void *bus, uint8_t addr, uint8_t read, priv_t priv),
	  uint8_t (*read)(void *bus, uint8_t addr, priv_t priv),
	  uint8_t (*write)(void *bus, uint8_t addr, uint8_t data, priv_t priv),
	  void (*stop)(void *bus, uint8_t addr, priv_t priv),
	  priv_t priv)
{
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;
    i2c_t *p, *q;
    int c;

    if (!bus_handle || ((base + size) > NADDRS))
	return;

    for (c = 0; c < size; c++) {
	p = bus->devices[base + c];
	if (!p)
		continue;
	while (p) {
		q = p->next;
		if ((p->start == start) && (p->read == read) && (p->write == write) && (p->stop == stop) && (p->priv == priv)) {
			if (p->prev)
				p->prev->next = p->next;
			else
				bus->devices[base + c] = p->next;
			if (p->next)
				p->next->prev = p->prev;
			else
				bus->last[base + c] = p->prev;
			free(p);
			p = NULL;
			break;
		}
		p = q;
	}
    }
}


void
i2c_handler(int set, void *bus_handle, uint8_t base, int size,
    uint8_t (*start)(void *bus, uint8_t addr, uint8_t read, priv_t priv),
    uint8_t (*read)(void *bus, uint8_t addr, priv_t priv),
    uint8_t (*write)(void *bus, uint8_t addr, uint8_t data, priv_t priv),
    void (*stop)(void *bus, uint8_t addr, priv_t priv),
    priv_t priv)
{
    if (set)
	i2c_sethandler(bus_handle, base, size, start, read, write, stop, priv);
    else
	i2c_removehandler(bus_handle, base, size, start, read, write, stop, priv);
}


uint8_t
i2c_has_device(void *bus_handle, uint8_t addr)
{
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;

    if (!bus)
	return 0;

    DEBUG("I2C: has_device(%s, %02X) = %d\n",
	bus->name, addr, !!bus->devices[addr]);

    return(!!bus->devices[addr]);
}


uint8_t
i2c_start(void *bus_handle, uint8_t addr, uint8_t read)
{
    uint8_t ret = 0;
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;
    i2c_t *p;

    if (!bus)
	return(ret);

    p = bus->devices[addr];
    if (p) {
	while (p) {
		if (p->start) {
			ret |= p->start(bus_handle, addr, read, p->priv);
		}
		p = p->next;
	}
    }

    DEBUG("I2C: start(%s, %02X)\n", bus->name, addr);

    return(ret);
}


uint8_t
i2c_read(void *bus_handle, uint8_t addr)
{
    uint8_t ret = 0;
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;
    i2c_t *p;

    if (!bus)
	return(ret);

    p = bus->devices[addr];
    if (p) {
	while (p) {
		if (p->read) {
			ret = p->read(bus_handle, addr, p->priv);
			break;
		}
		p = p->next;
	}
    }

    DEBUG("I2C: read(%s, %02X) = %02X\n", bus->name, addr, ret);

    return(ret);
}


uint8_t
i2c_write(void *bus_handle, uint8_t addr, uint8_t data)
{
    uint8_t ret = 0;
    i2c_t *p;
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;

    if (!bus)
	return(ret);

    p = bus->devices[addr];
    if (p) {
	while (p) {
		if (p->write) {
			ret |= p->write(bus_handle, addr, data, p->priv);
		}
		p = p->next;
	}
    }

    DEBUG("I2C: write(%s, %02X, %02X) = %d\n", bus->name, addr, data, ret);

    return(ret);
}


void
i2c_stop(void *bus_handle, uint8_t addr)
{
    i2c_bus_t *bus = (i2c_bus_t *) bus_handle;
    i2c_t *p;

    if (!bus)
	return;

    p = bus->devices[addr];
    if (p) {
	while (p) {
		if (p->stop) {
			p->stop(bus_handle, addr, p->priv);
		}
		p = p->next;
	}
    }

    DEBUG("I2C: stop(%s, %02X)\n", bus->name, addr);
}
