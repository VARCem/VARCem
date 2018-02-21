/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SiS 85C496 chipset.
 *
 * Version:	@(#)m_at_sis_85c496.c	1.0.1	2018/02/14
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
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../pci.h"
#include "../device.h"
#include "../mem.h"
#include "../memregs.h"
#include "../sio.h"
#include "../disk/hdc.h"
#include "machine.h"


typedef struct sis_85c496_t
{
        uint8_t pci_conf[256];
} sis_85c496_t;


sis_85c496_t sis496;


static void sis_85c496_recalcmapping(void)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                uint32_t base = 0xc0000 + (c << 15);
                if (sis496.pci_conf[0x44] & (1 << c))
                {
                        switch (sis496.pci_conf[0x45] & 3)
                        {
                                case 0:
                                mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                                break;
                                case 1:
                                mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                                break;
                                case 2:
                                mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                                break;
                                case 3:
                                mem_set_mem_state(base, 0x8000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                                break;
                        }
                }
                else
                        mem_set_mem_state(base, 0x8000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }

        flushmmucache();
        shadowbios = (sis496.pci_conf[0x44] & 0xf0);
}


static void sis_85c496_write(int func, int addr, uint8_t val, void *p)
{
        switch (addr)
        {
                case 0x44: /*Shadow configure*/
                if ((sis496.pci_conf[0x44] & val) ^ 0xf0)
                {
                        sis496.pci_conf[0x44] = val;
                        sis_85c496_recalcmapping();
                }
                break;
                case 0x45: /*Shadow configure*/
                if ((sis496.pci_conf[0x45] & val) ^ 0x01)
                {
                        sis496.pci_conf[0x45] = val;
                        sis_85c496_recalcmapping();
                }
                break;
                
                case 0xc0:
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTA, val & 0xf);
                else
                        pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
                break;
                case 0xc1:
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTB, val & 0xf);
                else
                        pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
                break;
                case 0xc2:
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTC, val & 0xf);
                else
                        pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
                break;
                case 0xc3://                pclog("IRQ routing %02x %02x\n", addr, val);
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTD, val & 0xf);
                else
                        pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
                break;
        }
  
        if ((addr >= 4 && addr < 8) || addr >= 0x40)
           sis496.pci_conf[addr] = val;
}


static uint8_t sis_85c496_read(int func, int addr, void *p)
{
        return sis496.pci_conf[addr];
}
 

static void sis_85c496_reset(void)
{
        memset(&sis496, 0, sizeof(sis_85c496_t));
        
        sis496.pci_conf[0x00] = 0x39; /*SiS*/
        sis496.pci_conf[0x01] = 0x10; 
        sis496.pci_conf[0x02] = 0x96; /*496/497*/
        sis496.pci_conf[0x03] = 0x04; 

        sis496.pci_conf[0x04] = 7;
        sis496.pci_conf[0x05] = 0;

        sis496.pci_conf[0x06] = 0x80;
        sis496.pci_conf[0x07] = 0x02;
        
        sis496.pci_conf[0x08] = 2; /*Device revision*/

        sis496.pci_conf[0x09] = 0x00; /*Device class (PCI bridge)*/
        sis496.pci_conf[0x0a] = 0x00;
        sis496.pci_conf[0x0b] = 0x06;

        sis496.pci_conf[0x0e] = 0x00; /*Single function device*/
}


static void sis_85c496_pci_reset(void)
{
	uint8_t val = 0;

	val = sis_85c496_read(0, 0x44, NULL);		/* Read current value of 0x44. */
	sis_85c496_write(0, 0x44, val & 0xf, NULL);		/* Turn off shadow BIOS but keep the lower 4 bits. */
}


static void sis_85c496_init(void)
{
        pci_add_card(5, sis_85c496_read, sis_85c496_write, NULL);

	sis_85c496_reset();

	pci_reset_handler.pci_master_reset = sis_85c496_pci_reset;
}


static void
machine_at_sis_85c496_common_init(machine_t *model)
{
	machine_at_ps2_init(model);
	device_add(&ide_pci_device);

	memregs_init();
        pci_init(PCI_CONFIG_TYPE_1);
	pci_register_slot(0x05, PCI_CARD_SPECIAL, 0, 0, 0, 0);
	pci_register_slot(0x0B, PCI_CARD_NORMAL, 1, 2, 3, 4);
	pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
	pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);
	pci_register_slot(0x07, PCI_CARD_NORMAL, 4, 1, 2, 3);

	sis_85c496_init();
}


void
machine_at_r418_init(machine_t *model)
{
	machine_at_sis_85c496_common_init(model);

        fdc37c665_init();
}
