/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the EEPROM on select ATI cards.
 *
 * Version:	@(#)vid_ati_eeprom.c	1.0.5	2019/03/03
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
#include <wchar.h>
#include "../../emu.h"
#include "../../mem.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "vid_ati.h"


enum {
    EEPROM_IDLE,
    EEPROM_WAIT,
    EEPROM_OPCODE,
    EEPROM_INPUT,
    EEPROM_OUTPUT
};

enum {
    EEPROM_OP_EW    = 4,
    EEPROM_OP_WRITE = 5,
    EEPROM_OP_READ  = 6,
    EEPROM_OP_ERASE = 7,
        
    EEPROM_OP_WRALMAIN = -1
};

enum {
    EEPROM_OP_EWDS = 0,
    EEPROM_OP_WRAL = 1,
    EEPROM_OP_ERAL = 2,
    EEPROM_OP_EWEN = 3
};


void
ati_eeprom_load(ati_eeprom_t *dev, wchar_t *fn, int type)
{
    FILE *fp;

    dev->type = type;
    memset(dev->data, 0, dev->type ? 512 : 128);

    wcscpy(dev->fn, fn);
    fp = plat_fopen(nvr_path(dev->fn), L"rb");
    if (fp != NULL) {
	(void)fread(dev->data, 1, dev->type ? 512 : 128, fp);
	fclose(fp);
    }
}


void
ati_eeprom_save(ati_eeprom_t *dev)
{
    FILE *fp = plat_fopen(nvr_path(dev->fn), L"wb");

    if (fp != NULL) {
	(void)fwrite(dev->data, 1, dev->type ? 512 : 128, fp);
	fclose(fp);
    }
}


void
ati_eeprom_write(ati_eeprom_t *dev, int ena, int clk, int dat)
{
    int c;

    if (! ena)
	dev->out = 1;

    if (clk && !dev->oldclk) {
	if (ena && !dev->oldena) {
		dev->state = EEPROM_WAIT;
		dev->opcode = 0;
		dev->count = 3;
		dev->out = 1;
	} else if (ena) {
		switch (dev->state) {
			case EEPROM_WAIT:
				if (! dat)
					break;
				dev->state = EEPROM_OPCODE;
				/*FALLTHROUGH*/

			case EEPROM_OPCODE:
				dev->opcode = (dev->opcode << 1) | (dat ? 1 : 0);
				dev->count--;
				if (! dev->count) {
					switch (dev->opcode) {
						case EEPROM_OP_WRITE:
							dev->count = dev->type ? 24 : 22;
							dev->state = EEPROM_INPUT;
							dev->dat = 0;
							break;

						case EEPROM_OP_READ:
							dev->count = dev->type ? 8 : 6;
							dev->state = EEPROM_INPUT;
							dev->dat = 0;
							break;

						case EEPROM_OP_EW:
							dev->count = 2;
							dev->state = EEPROM_INPUT;
							dev->dat = 0;
							break;

						case EEPROM_OP_ERASE:
							dev->count = dev->type ? 8 : 6;
							dev->state = EEPROM_INPUT;
							dev->dat = 0;
							break;
					}
                                }
				break;

			case EEPROM_INPUT:
				dev->dat = (dev->dat << 1) | (dat ? 1 : 0);
				dev->count--;
				if (! dev->count) {
					switch (dev->opcode) {
						case EEPROM_OP_WRITE:
							if (! dev->wp) {
								dev->data[(dev->dat >> 16) & (dev->type ? 255 : 63)] = dev->dat & 0xffff;
								ati_eeprom_save(dev);
							}
							dev->state = EEPROM_IDLE;
							dev->out = 1;
							break;

						case EEPROM_OP_READ:
							dev->count = 17;
							dev->state = EEPROM_OUTPUT;
							dev->dat = dev->data[dev->dat];
							break;

						case EEPROM_OP_EW:
							switch (dev->dat) {
								case EEPROM_OP_EWDS:
									dev->wp = 1;
									break;

								case EEPROM_OP_WRAL:
									dev->opcode = EEPROM_OP_WRALMAIN;
									dev->count = 20;
									break;

								case EEPROM_OP_ERAL:
									if (! dev->wp) {
										memset(dev->data, 0xff, 128);
										ati_eeprom_save(dev);
									}
									break;

								case EEPROM_OP_EWEN:
									dev->wp = 0;
									break;
							}
							dev->state = EEPROM_IDLE;
							dev->out = 1;
							break;

						case EEPROM_OP_ERASE:
							if (! dev->wp) {
								dev->data[dev->dat] = 0xffff;
								ati_eeprom_save(dev);
							}
							dev->state = EEPROM_IDLE;
							dev->out = 1;
							break;

						case EEPROM_OP_WRALMAIN:
							if (! dev->wp) {
								for (c = 0; c < 256; c++)
									dev->data[c] = dev->dat;
								ati_eeprom_save(dev);
							}
							dev->state = EEPROM_IDLE;
							dev->out = 1;
							break;
					}
				}
				break;
		}
	}                
	dev->oldena = ena;
    } else if (!clk && dev->oldclk) {
	if (ena) {
		switch (dev->state) {
			case EEPROM_OUTPUT:
				dev->out = (dev->dat & 0x10000) ? 1 : 0;
				dev->dat <<= 1;
				dev->count--;
				if (! dev->count) {
					dev->state = EEPROM_IDLE;
				}
				break;
		}
	}
    }

    dev->oldclk = clk;
}


int
ati_eeprom_read(ati_eeprom_t *dev)
{
    return dev->out;
}
