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
 * Version:	@(#)vid_ati_eeprom.c	1.0.2	2018/03/31
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../emu.h"
#include "../mem.h"
#include "../nvr.h"
#include "../plat.h"
#include "vid_ati_eeprom.h"


enum
{
        EEPROM_IDLE,
        EEPROM_WAIT,
        EEPROM_OPCODE,
        EEPROM_INPUT,
        EEPROM_OUTPUT
};

enum
{
        EEPROM_OP_EW    = 4,
        EEPROM_OP_WRITE = 5,
        EEPROM_OP_READ  = 6,
        EEPROM_OP_ERASE = 7,
        
        EEPROM_OP_WRALMAIN = -1
};

enum
{
        EEPROM_OP_EWDS = 0,
        EEPROM_OP_WRAL = 1,
        EEPROM_OP_ERAL = 2,
        EEPROM_OP_EWEN = 3
};

void ati_eeprom_load(ati_eeprom_t *eeprom, wchar_t *fn, int type)
{
        FILE *f;

        eeprom->type = type;
        memset(eeprom->data, 0, eeprom->type ? 512 : 128);

        wcscpy(eeprom->fn, fn);
        f = plat_fopen(nvr_path(eeprom->fn), L"rb");
        if (f != NULL)
        {
        	(void)fread(eeprom->data, 1, eeprom->type ? 512 : 128, f);
        	fclose(f);
        }
}

void ati_eeprom_save(ati_eeprom_t *eeprom)
{
        FILE *f = plat_fopen(nvr_path(eeprom->fn), L"wb");

        if (f != NULL) {
        	(void)fwrite(eeprom->data, 1, eeprom->type ? 512 : 128, f);
        	fclose(f);
	}
}

void ati_eeprom_write(ati_eeprom_t *eeprom, int ena, int clk, int dat)
{
        int c;
        if (!ena)
        {
                eeprom->out = 1;
        }
        if (clk && !eeprom->oldclk)
        {
                if (ena && !eeprom->oldena)
                {
                        eeprom->state = EEPROM_WAIT;
                        eeprom->opcode = 0;
                        eeprom->count = 3;
                        eeprom->out = 1;
                }
                else if (ena)
                {
                        switch (eeprom->state)
                        {
                                case EEPROM_WAIT:
                                if (!dat)
                                        break;
                                eeprom->state = EEPROM_OPCODE;
                                /* fall through */
                                case EEPROM_OPCODE:
                                eeprom->opcode = (eeprom->opcode << 1) | (dat ? 1 : 0);
                                eeprom->count--;
                                if (!eeprom->count)
                                {
                                        switch (eeprom->opcode)
                                        {
                                                case EEPROM_OP_WRITE:
                                                eeprom->count = eeprom->type ? 24 : 22;
                                                eeprom->state = EEPROM_INPUT;
                                                eeprom->dat = 0;
                                                break;
                                                case EEPROM_OP_READ:
                                                eeprom->count = eeprom->type ? 8 : 6;
                                                eeprom->state = EEPROM_INPUT;
                                                eeprom->dat = 0;
                                                break;
                                                case EEPROM_OP_EW:
                                                eeprom->count = 2;
                                                eeprom->state = EEPROM_INPUT;
                                                eeprom->dat = 0;
                                                break;
                                                case EEPROM_OP_ERASE:
                                                eeprom->count = eeprom->type ? 8 : 6;
                                                eeprom->state = EEPROM_INPUT;
                                                eeprom->dat = 0;
                                                break;
                                        }
                                }
                                break;
                                
                                case EEPROM_INPUT:
                                eeprom->dat = (eeprom->dat << 1) | (dat ? 1 : 0);
                                eeprom->count--;
                                if (!eeprom->count)
                                {
                                        switch (eeprom->opcode)
                                        {
                                                case EEPROM_OP_WRITE:
                                                if (!eeprom->wp)
                                                {
                                                        eeprom->data[(eeprom->dat >> 16) & (eeprom->type ? 255 : 63)] = eeprom->dat & 0xffff;
                                                        ati_eeprom_save(eeprom);
                                                }
                                                eeprom->state = EEPROM_IDLE;
                                                eeprom->out = 1;
                                                break;

                                                case EEPROM_OP_READ:
                                                eeprom->count = 17;
                                                eeprom->state = EEPROM_OUTPUT;
                                                eeprom->dat = eeprom->data[eeprom->dat];
                                                break;
                                                case EEPROM_OP_EW:
                                                switch (eeprom->dat)
                                                {
                                                        case EEPROM_OP_EWDS:
                                                        eeprom->wp = 1;
                                                        break;
                                                        case EEPROM_OP_WRAL:
                                                        eeprom->opcode = EEPROM_OP_WRALMAIN;
                                                        eeprom->count = 20;
                                                        break;
                                                        case EEPROM_OP_ERAL:
                                                        if (!eeprom->wp)
                                                        {
                                                                memset(eeprom->data, 0xff, 128);
                                                                ati_eeprom_save(eeprom);
                                                        }
                                                        break;
                                                        case EEPROM_OP_EWEN:
                                                        eeprom->wp = 0;
                                                        break;
                                                }
                                                eeprom->state = EEPROM_IDLE;
                                                eeprom->out = 1;
                                                break;

                                                case EEPROM_OP_ERASE:
                                                if (!eeprom->wp)
                                                {
                                                        eeprom->data[eeprom->dat] = 0xffff;
                                                        ati_eeprom_save(eeprom);
                                                }
                                                eeprom->state = EEPROM_IDLE;
                                                eeprom->out = 1;
                                                break;

                                                case EEPROM_OP_WRALMAIN:
                                                if (!eeprom->wp)
                                                {
                                                        for (c = 0; c < 256; c++)
                                                                eeprom->data[c] = eeprom->dat;
                                                        ati_eeprom_save(eeprom);
                                                }
                                                eeprom->state = EEPROM_IDLE;
                                                eeprom->out = 1;
                                                break;
                                        }
                                }
                                break;
                        }
                }                
                eeprom->oldena = ena;
        }
        else if (!clk && eeprom->oldclk)
        {
                if (ena)
                {
                        switch (eeprom->state)
                        {
                                case EEPROM_OUTPUT:
                                eeprom->out = (eeprom->dat & 0x10000) ? 1 : 0;
                                eeprom->dat <<= 1;
                                eeprom->count--;
                                if (!eeprom->count)
                                {
                                        eeprom->state = EEPROM_IDLE;
                                }
                                break;
                        }
                }
        }
        eeprom->oldclk = clk;
}

int ati_eeprom_read(ati_eeprom_t *eeprom)
{
        return eeprom->out;
}

