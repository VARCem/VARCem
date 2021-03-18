/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the I2C handler.
 *
 * Version:	@(#)i2c.h	1.0.1	2021/03/18
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
#ifndef EMU_I2C_H
# define EMU_I2C_H


/* i2c.c */
extern void	*i2c_smbus;


/* i2c.c */
extern void	i2c_log(int level, const char *fmt, ...);
extern void	*i2c_addbus(const char *name);
extern void	i2c_removebus(void *bus_handle);
extern const char *i2c_getbusname(void *bus_handle);

extern void	i2c_sethandler(void *bus_handle, uint8_t base, int size,
			uint8_t (*start)(void *, uint8_t, int8_t, priv_t),
			uint8_t (*read)(void *, uint8_t, priv_t),
			uint8_t (*write)(void *, uint8_t, uint8_t, priv_t),
			void (*stop)(void *, uint8_t, priv_t),
			priv_t priv);

extern void	i2c_removehandler(void *bus_handle, uint8_t base, int size,
			uint8_t (*start)(void *, uint8_t, int8_t, priv_t),
			uint8_t (*read)(void *, uint8_t, priv_t),
			uint8_t (*write)(void *, uint8_t, uint8_t, priv_t),
			void (*stop)(void *, uint8_t, priv_t),
			priv_t priv);

extern void	i2c_handler(int set, void *bus_handle, uint8_t base, int size,
			uint8_t (*start)(void *, uint8_t, int8_t, priv_t),
			uint8_t (*read)(void *, uint8_t, priv_t),
			uint8_t (*write)(void *, uint8_t, uint8_t, priv_t),
			void (*stop)(void *, uint8_t, priv_t),
			priv_t priv);

extern uint8_t	i2c_has_device(void *bus_handle, uint8_t addr);
extern uint8_t	i2c_start(void *bus_handle, uint8_t addr, int8_t read);
extern uint8_t	i2c_read(void *bus_handle, uint8_t addr);
extern uint8_t	i2c_write(void *bus_handle, uint8_t addr, uint8_t data);
extern void	i2c_stop(void *bus_handle, uint8_t addr);

/* i2c_eeprom.c */
extern void	*i2c_eeprom_init(void *i2c, uint8_t addr, uint8_t *data,
				 uint32_t size, int8_t writable);
extern void	i2c_eeprom_close(void *dev_handle);

/* i2c_gpio.c */
extern void	*i2c_gpio_init(char *bus_name);
extern void	i2c_gpio_close(void *dev_handle);
extern void	i2c_gpio_set(void *dev_handle, uint8_t scl, uint8_t sda);
extern uint8_t	i2c_gpio_get_scl(void *dev_handle);
extern uint8_t	i2c_gpio_get_sda(void *dev_handle);
extern void	*i2c_gpio_get_bus();


#endif	/*EMU_I2C_H*/
