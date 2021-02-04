/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,-*
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the 24Cxx series of I2C EEPROMs.
 *
 * Version:	@(#)i2c_eeprom.c	1.0.0	2021/02/02
 *
 * Authors:	RichardG, <richardg867@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
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
#define HAVE_STDARG_H
#include <wchar.h>
#include "../../emu.h"
#include "i2c.h"


typedef struct {
    void	*i2c;
    uint8_t	addr, *data, writable;

    uint32_t	addr_mask, addr_register;
    uint8_t	addr_len, addr_pos;
} i2c_eeprom_t;


#ifdef ENABLE_I2C_EEPROM_LOG
int i2c_eeprom_do_log = ENABLE_I2C_EEPROM_LOG;


static void
i2c_eeprom_log(const char *fmt, ...)
{
    va_list ap;

    if (i2c_eeprom_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
}
#else
#define i2c_eeprom_log(fmt, ...)
#endif


uint8_t
i2c_eeprom_start(void *bus, uint8_t addr, uint8_t read, priv_t priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *) priv;

    i2c_eeprom_log("I2C EEPROM %s %02X: start()\n", i2c_getbusname(dev->i2c), dev->addr);

    dev->addr_pos = 0;
    dev->addr_register = (addr << dev->addr_len) & dev->addr_mask;

    return 1;
}


uint8_t
i2c_eeprom_read(void *bus, uint8_t addr, void *priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *) priv;
    uint8_t ret = dev->data[dev->addr_register];

    i2c_eeprom_log("I2C EEPROM %s %02X: read(%06X) = %02X\n", i2c_getbusname(dev->i2c), dev->addr, dev->addr_register, ret);
    if (++dev->addr_register > dev->addr_mask) /* roll-over */
	dev->addr_register = 0;

    return ret;
}


uint8_t
i2c_eeprom_write(void *bus, uint8_t addr, uint8_t data, void *priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *) priv;

    if (dev->addr_pos < dev->addr_len) {
	dev->addr_register <<= 8;
	dev->addr_register |= data;
	dev->addr_register &= (1 << dev->addr_len) - 1;
	dev->addr_register |= addr << dev->addr_len;
	dev->addr_register &= dev->addr_mask;
	i2c_eeprom_log("I2C EEPROM %s %02X: write(address, %04X)\n", i2c_getbusname(dev->i2c), dev->addr, dev->addr_register);
	dev->addr_pos += 8;
    } else {
	i2c_eeprom_log("I2C EEPROM %s %02X: write(%06X, %02X) = %d\n", i2c_getbusname(dev->i2c), dev->addr, dev->addr_register, data, !!dev->writable);
	if (dev->writable)
		dev->data[dev->addr_register] = data;
	if (++dev->addr_register > dev->addr_mask) /* roll-over */
		dev->addr_register = 0;
	return dev->writable;
    }

    return 1;
}


void *
i2c_eeprom_init(void *i2c, uint8_t addr, uint8_t *data, uint32_t size, uint8_t writable)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *) malloc(sizeof(i2c_eeprom_t));
    memset(dev, 0, sizeof(i2c_eeprom_t));

    size &= 0x7fffff; /* address space limit of 8 MB = 7 bits from I2C address + 16 bits */

    i2c_eeprom_log("I2C EEPROM %s %02X: init(%d, %d)\n", i2c_getbusname(i2c), addr, size, writable);

    dev->i2c = i2c;
    dev->addr = addr;
    dev->data = data;
    dev->writable = writable;

    dev->addr_len = (size >= 4096) ? 16 : 8; /* use 16-bit addresses on 24C32 and above */
    dev->addr_mask = size - 1;

    i2c_sethandler(dev->i2c, dev->addr & ~(dev->addr_mask >> dev->addr_len), (dev->addr_mask >> dev->addr_len) + 1, i2c_eeprom_start, i2c_eeprom_read, i2c_eeprom_write, NULL, dev);

    return dev;
}


void
i2c_eeprom_close(void *dev_handle)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *) dev_handle;

    i2c_eeprom_log("I2C EEPROM %s %02X: close()\n", i2c_getbusname(dev->i2c), dev->addr);

    i2c_removehandler(dev->i2c, dev->addr & ~(dev->addr_mask >> dev->addr_len), (dev->addr_mask >> dev->addr_len) + 1, i2c_eeprom_start, i2c_eeprom_read, i2c_eeprom_write, NULL, dev);

    free(dev);
}
