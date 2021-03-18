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
 * Version:	@(#)i2c_eeprom.c	1.0.2	2021/03/18
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define dbglog i2c_log
#include "../../emu.h"
#include "../../plat.h"
#include "i2c.h"


typedef struct {
    void	*i2c;
    int8_t	writable;
    uint8_t	addr, *data;

    uint32_t	addr_mask, addr_register;
    uint8_t	addr_len, addr_pos;
} i2c_eeprom_t;


static uint8_t
i2c_eeprom_start(UNUSED(void *bus), uint8_t addr, int8_t read, priv_t priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *)priv;

    DEBUG("I2C: EEPROM %s %02X: start()\n",
	i2c_getbusname(dev->i2c), dev->addr);

    if (! read) {
	dev->addr_pos = 0;
	dev->addr_register = (addr << dev->addr_len) & dev->addr_mask;
    }

    return(1);
}


static uint8_t
i2c_eeprom_read(UNUSED(void *bus), UNUSED(uint8_t addr), priv_t priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *)priv;
    uint8_t ret = dev->data[dev->addr_register];

    DEBUG("I2C: EEPROM %s %02X: read(%06X) = %02X\n",
	i2c_getbusname(dev->i2c), dev->addr, dev->addr_register, ret);
    dev->addr_register++;
    dev->addr_register &= dev->addr_mask; /* roll-over */

    return(ret);
}


static uint8_t
i2c_eeprom_write(UNUSED(void *bus), uint8_t addr, uint8_t data, priv_t priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *)priv;

    if (dev->addr_pos < dev->addr_len) {
	dev->addr_register <<= 8;
	dev->addr_register |= data;
	dev->addr_register &= (1 << dev->addr_len) - 1;
	dev->addr_register |= addr << dev->addr_len;
	dev->addr_register &= dev->addr_mask;
	DEBUG("I2C: EEPROM %s %02X: write(address, %06X)\n",
		i2c_getbusname(dev->i2c), dev->addr, dev->addr_register);
	dev->addr_pos += 8;
    } else {
	DEBUG("I2C: EEPROM %s %02X: write(%06X, %02X) = %d\n",
		i2c_getbusname(dev->i2c), dev->addr, dev->addr_register,
		data, !!dev->writable);
	if (dev->writable)
		dev->data[dev->addr_register] = data;
	dev->addr_register++;
	dev->addr_register &= dev->addr_mask; /* roll-over */
	return(dev->writable);
    }

    return(1);
}


static void
i2c_eeprom_stop(UNUSED(void *bus), UNUSED(uint8_t addr), priv_t priv)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *)priv;

    DEBUG("I2C: EEPROM %s %02X: stop()\n",
	i2c_getbusname(dev->i2c), dev->addr);

    dev->addr_pos = 0;
}


static inline uint8_t
log2i(uint32_t i)
{
    uint8_t ret = 0;

    while ((i >>= 1))
	ret++;

    return ret;
}


void
i2c_eeprom_close(void *dev_handle)
{
    i2c_eeprom_t *dev = (i2c_eeprom_t *)dev_handle;

    DEBUG("I2C: EEPROM %s %02X: close()\n",
	i2c_getbusname(dev->i2c), dev->addr);

    i2c_removehandler(dev->i2c, dev->addr & ~(dev->addr_mask >> dev->addr_len), (dev->addr_mask >> dev->addr_len) + 1, i2c_eeprom_start, i2c_eeprom_read, i2c_eeprom_write, i2c_eeprom_stop, dev);

    free(dev);
}


void *
i2c_eeprom_init(void *i2c, uint8_t addr, uint8_t *data, uint32_t size, int8_t writable)
{
    i2c_eeprom_t *dev;

    dev = (i2c_eeprom_t *)mem_alloc(sizeof(i2c_eeprom_t));
    memset(dev, 0, sizeof(i2c_eeprom_t));

    /* Round size up to the next power of 2. */
    uint32_t pow_size = 1 << log2i(size);
    if (pow_size < size)
	size = pow_size << 1;
    size &= 0x7fffff; /* address space limit of 8 MB = 7 bits from I2C address + 16 bits */

    DEBUG("I2C: EEPROM %s %02X: init(%i, %i)\n",
	i2c_getbusname(i2c), addr, size, writable);

    dev->i2c = i2c;
    dev->addr = addr;
    dev->data = data;
    dev->writable = writable;

    dev->addr_len = (size >= 4096) ? 16 : 8; /* use 16-bit addresses on 24C32 and above */
    dev->addr_mask = size - 1;

    i2c_sethandler(dev->i2c, dev->addr & ~(dev->addr_mask >> dev->addr_len), (dev->addr_mask >> dev->addr_len) + 1, i2c_eeprom_start, i2c_eeprom_read, i2c_eeprom_write, i2c_eeprom_stop, dev);

    return(dev);
}
