/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the PCI handler module.
 *
 * Version:	@(#)pci.h	1.0.4	2019/05/13
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
#ifndef EMU_PCI_H
# define EMU_PCI_H


#define PCI_REG_COMMAND 0x04

#define PCI_COMMAND_IO  0x01
#define PCI_COMMAND_MEM 0x02

#define PCI_CONFIG_TYPE_1 1
#define PCI_CONFIG_TYPE_2 2

#define PCI_INTA 1
#define PCI_INTB 2
#define PCI_INTC 3
#define PCI_INTD 4

#define PCI_MIRQ0 0
#define PCI_MIRQ1 1

#define PCI_IRQ_DISABLED -1

enum {
    PCI_CARD_NORMAL = 0,
    PCI_CARD_ONBOARD,
    PCI_CARD_SPECIAL
};


#define PCI_ADD_NORMAL	0x80
#define PCI_ADD_VIDEO	0x81

typedef union {
    uint32_t addr;
    uint8_t addr_regs[4];
} bar_t;


extern void	pci_set_irq_routing(int pci_int, int irq);

extern void	pci_enable_mirq(int mirq);
extern void	pci_set_mirq_routing(int mirq, int irq);

extern uint8_t	pci_use_mirq(uint8_t mirq);

extern int	pci_irq_is_level(int irq);

extern void	pci_set_mirq(uint8_t mirq);
extern void	pci_set_irq(uint8_t card, uint8_t pci_int);
extern void	pci_clear_mirq(uint8_t mirq);
extern void	pci_clear_irq(uint8_t card, uint8_t pci_int);

extern void	pci_reset(void);
extern void	pci_init(int type);
extern void	pci_set_speed(uint32_t speed);
extern uint32_t	pci_get_speed(int burst);
extern void	pci_register_slot(int card, int type,
				  int inta, int intb, int intc, int intd);
extern void	pci_close(void);
extern uint8_t	pci_add_card(uint8_t add_type, uint8_t (*read)(int, int, priv_t), void (*write)(int, int, uint8_t, priv_t), priv_t priv);

extern void     trc_init(void);


#endif	/*EMU_PCI_H*/
