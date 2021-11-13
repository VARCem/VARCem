/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		DDC monitor emulation definitions.
 *
 * Version:	@(#)pnp.h	1.0.1	2021/11/11
 *
 * Author:	RichardG, <richardg867@gmail.com>
 *
 *		Copyright 2020,2021 RichardG.
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
#ifndef EMU_PNP_H
# define EMU_PNP_H

# include <stdint.h>


#define ISAPNP_MEM_DISABLED	0
#define ISAPNP_IO_DISABLED	0
#define ISAPNP_IRQ_DISABLED	0
#define ISAPNP_DMA_DISABLED	4


enum {
    ISAPNP_CARD_DISABLE = 0,
    ISAPNP_CARD_ENABLE = 1,
    ISAPNP_CARD_FORCE_CONFIG /* cheat code for UMC UM8669F */
};


typedef struct {
    uint8_t	activate;
    struct {
    	uint32_t base: 24, size: 24;
    } mem[4];
    struct {
    	uint32_t base, size;
    } mem32[4];
    struct {
    	uint16_t base;
    } io[8];
    struct {
    	uint8_t irq: 4, level: 1, type: 1;
    } irq[2];
    struct {
    	uint8_t dma: 3;
    } dma[2];
} isapnp_device_config_t;


priv_t	isapnp_add_card(uint8_t *rom, uint16_t rom_size,
			 void (*config_changed)(uint8_t ld, isapnp_device_config_t *config, priv_t priv),
			 void (*csn_changed)(uint8_t csn, priv_t priv),
			 uint8_t (*read_vendor_reg)(uint8_t ld, uint8_t reg, priv_t priv),
			 void (*write_vendor_reg)(uint8_t ld, uint8_t reg, uint8_t val, priv_t priv),
			 priv_t priv);
void	isapnp_update_card_rom(priv_t priv, uint8_t *rom, uint16_t rom_size);
void	isapnp_enable_card(priv_t priv, uint8_t enable);
void	isapnp_set_csn(priv_t priv, uint8_t csn);
void	isapnp_set_device_defaults(priv_t priv, uint8_t ldn, const isapnp_device_config_t *config);
void	isapnp_reset_card(priv_t priv);
void	isapnp_reset_device(priv_t priv, uint8_t ld);


#endif	/*EMU_PNP_H*/
