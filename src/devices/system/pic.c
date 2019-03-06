/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Intel 8259 interrupt controller.
 *
 * Version:	@(#)pic.c	1.0.6	2019/03/04
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include "../../machines/machine.h"
#include "../../io.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"


PIC		pic,
		pic2;
int		pic_pending;


static uint16_t	pic_current;


static int
is_level(uint16_t irq)
{
    if (PCI)
	return pci_irq_is_level(irq);
    else {
	if (irq < 8)
		return (pic.icw1 & 0x08) ? 1 : 0;
	else
		return (pic2.icw1 & 0x08) ? 1 : 0;
    }
}


static void
update_mask(uint8_t *mask, uint8_t ins)
{
    int c;

    *mask = 0x00;
    for (c = 0; c < 8; c++) {
	if (ins & (1 << c)) {
		*mask = 0xff << c;
		return;
	}
    }
}


static void
update_pending(void)
{
    uint16_t temp = 0;

    if (AT) {
	if ((pic2.pend & ~pic2.mask) & ~pic2.mask2)
		pic.pend |= pic.icw3;
	else
		pic.pend &= ~pic.icw3;
    }

    pic_pending = (pic.pend & ~pic.mask) & ~pic.mask2;

    if (AT) {
	if (! ((pic.mask | pic.mask2) & pic.icw3)) {
		temp = ((pic2.pend & ~pic2.mask) & ~pic2.mask2);
		temp <<= 8;
		pic_pending |= temp;
	}
    }

#if 0
    DBGLOG(2, "pic_intpending = %i  %02X %02X %02X %02X\n",
	   pic_pending, pic.ins, pic.pend, pic.mask, pic.mask2);
    DBGLOG(2, "                    %02X %02X %02X %02X %i %i\n",
	   pic2.ins, pic2.pend, pic2.mask, pic2.mask2,
	   ((pic.mask | pic.mask2) & (1 << 2)),
	   ((pic2.pend & ~pic2.mask) & ~pic2.mask2));
#endif
}


static void
pic_write(uint16_t addr, uint8_t val, void *priv)
{
    PIC *dev = (PIC *)priv;
    int c;

    if (addr & 1) {
	switch (dev->icw) {
		case 0:		/* OCW1 */
			dev->mask = val;
			update_pending();
			break;

		case 1:		/* ICW2 */
			dev->vector = val & 0xf8;
			DBGLOG(1, "PIC: vector now: %02X\n", dev->vector);
			if (dev->icw1 & 0x02)
				dev->icw = 3;
			else
				dev->icw = 2;
			break;

		case 2:		/* ICW3 */
			dev->icw3 = val;
			DBGLOG(1, "PIC: ICW3 now %02x\n", val);
			if (dev->icw1 & 0x01)
				dev->icw = 3;
			else
				dev->icw = 0;
			break;

		case 3:		/* ICW4 */
			dev->icw4 = val;
			dev->icw = 0;
			break;
	}

	return;
    }

    if (val & 0x10) {
	/* ICW1 */
	dev->mask = 0x00;
	dev->mask2 = 0x00;
	dev->icw = 1;
	dev->icw1 = val;
	dev->ins = 0;
	update_pending();

	return;
    }

    if (! (val & 0x08)) {
	/* OCW2 */
	if ((val & 0xe0) == 0x60) {
		dev->ins &= ~(1 << (val & 0x07));
		update_mask(&dev->mask2, dev->ins);

		if (AT) {
			if (((val & 0x07) == pic2.icw3) &&
			    (pic2.pend & ~pic2.mask) & ~pic2.mask2)
				dev->pend |= dev->icw3;
		}

		if ((pic_current & (1 << (val & 0x07))) && is_level(val & 0x07)) {
			if ((((1 << (val & 0x07)) != dev->icw3) || !AT))
				dev->pend |= 1 << (val & 0x07);
		}

		update_pending();
	} else {
		for (c = 0; c < 8; c++) {
			if (dev->ins & (1 << c)) {
				dev->ins &= ~(1 << c);
				update_mask(&dev->mask2, dev->ins);

				if (AT) {
					if (((1 << c) == dev->icw3) &&
					    (pic2.pend & ~pic2.mask) & ~pic2.mask2)
						dev->pend |= dev->icw3;
				}

				if ((pic_current & (1 << c)) &&
				    is_level(c)) {
					if ((((1 << c) != dev->icw3) || !AT))
						dev->pend |= 1 << c;
				}

				update_pending();

				return;
			}
		}
	}

	return;
    }

    /* OCW3 */
    if (val & 0x02)
	dev->read = (val & 0x01);
}


static uint8_t
pic_read(uint16_t addr, void *priv)
{
    PIC *dev = (PIC *)priv;

    if (addr & 1) {
	DBGLOG(1, "PIC1: read mask %02X\n", dev->mask);
	return(pic.mask);
    }

    if (dev->read) {
	DBGLOG(1, "PIC1: read ins %02X\n", dev->ins);
	if (AT)
		return(dev->ins | (pic2.ins ? 4 : 0));
	else
		return(dev->ins);
    }

    return(dev->pend);
}


static void
pic_autoeoi(void)
{
    PIC *dev = &pic;
    int c;

    for (c = 0; c < 8; c++) {
	if (dev->ins & ( 1 << c)) {
		dev->ins &= ~(1 << c);
		update_mask(&dev->mask2, dev->ins);

		if (AT) {
			if (((1 << c) == dev->icw3) &&
			    (pic2.pend & ~pic2.mask) & ~pic2.mask2)
				dev->pend |= dev->icw3;
		}

		if ((pic_current & (1 << c)) && is_level(c)) {
			if (((1 << c) != dev->icw3) || !AT)
				dev->pend |= (1 << c);
		}

		update_pending();
		return;
	}
    }
}


static void
pic2_write(uint16_t addr, uint8_t val, void *priv)
{
    PIC *dev = (PIC *)priv;
    int c;

    if (addr & 1) {
	switch (dev->icw) {
		case 0:		/* OCW1 */
			dev->mask = val;
			update_pending();
			break;

		case 1:		/* ICW2 */
			dev->vector = val & 0xf8;
			DBGLOG(1, "PIC2: vector now: %02X\n", dev->vector);
			if (dev->icw1 & 0x02)
				dev->icw = 3;
			else
				dev->icw = 2;
			break;

		case 2:		/* ICW3 */
			dev->icw3 = val;
			DBGLOG(1, "PIC2: ICW3 now %02X\n", val);
			if (dev->icw1 & 1)
				dev->icw = 3;
			else
				dev->icw = 0;
			break;

		case 3:		/* ICW4 */
			dev->icw4 = val;
			dev->icw = 0;
			break;
	}

	return;
    }

    if (val & 0x10) {
	/* ICW1 */
	dev->mask = 0x00;
	dev->mask2 = 0x00;
	dev->icw = 1;
	dev->icw1 = val;
	dev->ins = 0;
	update_pending();

	return;
    }

    if (! (val & 0x08)) {
	/* OCW2 */
	if ((val & 0xe0) == 0x60) {
		dev->ins &= ~(1 << (val & 0x07));
		update_mask(&dev->mask2, dev->ins);

		if (pic_current & (0x100 << (val & 0x07)) &&
		    is_level((val & 0x07) + 8)) {
			dev->pend |= (1 << (val & 0x07));
			pic.pend |= (1 << dev->icw3);
		}

		update_pending();
	} else {
		for (c = 0; c < 8; c++) {
			if (dev->ins & (1 << c)) {
				dev->ins &= ~(1 << c);
				update_mask(&dev->mask2, dev->ins);

				if (pic_current & (0x100 << c) && is_level(c + 8)) {
					dev->pend |= (1 << c);
					pic.pend |= (1 << dev->icw3);
				}

				update_pending();
				return;
			}
		}
	}

	return;
    }

    /* OCW3 */
    if (val & 0x02)
	dev->read = (val & 0x01);
}


static uint8_t
pic2_read(uint16_t addr, void *priv)
{
    PIC *dev = (PIC *)priv;

    if (addr & 1) {
	DBGLOG(1, "PIC2: read mask %02x\n", dev->mask);
	return(dev->mask);
    }

    if (dev->read) {
	DBGLOG(1, "PIC2: read ins %02x\n", dev->ins);
	return(dev->ins);
    }

    DBGLOG(1, "PIC2: read pend %02x\n", dev->pend);
    return(dev->pend);
}


static void
pic2_autoeoi(void)
{
    PIC *dev = &pic2;
    int c;

    for (c = 0; c < 8; c++) {
	if (dev->ins & (1 << c)) {
		dev->ins &= ~(1 << c);
		update_mask(&dev->mask2, dev->ins);

		if (pic_current & (0x100 << c) && is_level(c + 8)) {
			dev->pend |= (1 << c);
			pic.pend |= (1 << dev->icw3);
		}

		update_pending();
		return;
	}
    }
}


static uint8_t
pic_process(PIC *dev, int c)
{
    uint8_t pending = dev->pend & ~dev->mask;
    int pic_int = c & 7;
    int pic_int_num = 1 << pic_int;
    int in_service = 0;

    in_service = (dev->ins & (pic_int_num - 1));

    if (AT && (c >= 8))
	in_service |= (pic.ins & 0x03);

    if ((pending & pic_int_num) && !in_service) {
	dev->pend &= ~pic_int_num;
	dev->ins |= pic_int_num;
	update_mask(&dev->mask2, dev->ins);

	if (AT && (c >= 8)) {
		pic.ins |= (1 << pic2.icw3); /*Cascade IRQ*/
		update_mask(&pic.mask2, pic.ins);
	}

	update_pending();

	if (dev->icw4 & 0x02)
		(AT && (c >= 8)) ? pic2_autoeoi() : pic_autoeoi();

	if (! c)
		pit_set_gate(&pit2, 0, 0);

	return(pic_int + dev->vector);
    }

    return(0xff);
}


/* Try to raise an interrupt. */
static void
picint_common(uint16_t num, int level)
{
    int c = 0;

    if (num == 0) {
	ERRLOG("PIC: attempting to raise IRQ0 !\n");
	return;
    }

    if (AT && (num == pic.icw3) && (pic.icw3 == 4))
	num = 1 << 9;

    while (!(num & (1 << c)))
	c++;

    if (AT && (num == pic.icw3) && (pic.icw3 != 4)) {
	ERRLOG("PIC: attempting to raise cascaded IRQ%i !\n", c);
	return;
    }

    if (!(pic_current & num) || !level) {
	DBGLOG(2, "PIC: raising IRQ%i\n", c);

	if (level)
                pic_current |= num;

        if (num > 0xFF) {
		if (! AT)
			return;

		pic2.pend |= (num >> 8);
		if ((pic2.pend & ~pic2.mask) & ~pic2.mask2)
			pic.pend |= (1 << pic2.icw3);
        } else
                pic.pend |= num;

        update_pending();
    }
}


void
pic_init(void)
{
    memset(&pic, 0x00, sizeof(PIC));

    io_sethandler(0x0020, 2,
		  pic_read,NULL,NULL, pic_write,NULL,NULL, &pic);
}


void
pic2_init(void)
{
    memset(&pic2, 0x00, sizeof(PIC));

    io_sethandler(0x00a0, 2,
		  pic2_read,NULL,NULL, pic2_write,NULL,NULL, &pic2);
}


void
pic_reset(void)
{
    pic.icw = 0;
    pic.mask = 0xff;
    pic.mask2 = 0x00;
    pic.pend = pic.ins = 0;
    pic.vector = 8;
    pic.read = 1;

    pic2.icw = 0;
    pic2.mask = 0xff;
    pic.mask2 = 0x00;
    pic2.pend = pic2.ins = 0;

    pic_pending = 0;
}


#if 0	/*NOT USED */
void
pic_clear(void)
{
    pic.pend = pic.ins = 0;
    pic_current = 0;

    update_pending();
}
#endif


void
picintc(uint16_t num)
{
    int c = 0;

    if (num == 0) {
	ERRLOG("PIC: attempting to lower IRQ0 !\n");
	return;
    }

    if (AT && (num == pic.icw3) && (pic.icw3 == 4))
	num = 1 << 9;

    while (! (num & (1 << c)))
	c++;

    if (AT && (num == pic.icw3) && (pic.icw3 != 4)) {
	ERRLOG("PIC: attempting to lower cascaded IRQ%i !\n", c);
	return;
    }

    if (pic_current & num)
        pic_current &= ~num;

    DBGLOG(2, "PIC: lowering IRQ%i\n", c);

    if (num > 0xff) {
	if (! AT)
		return;

	pic2.pend &= ~(num >> 8);
	if (! ((pic2.pend & ~pic2.mask) & ~pic2.mask2))
		pic.pend &= ~(1 << pic2.icw3);
    } else
	pic.pend &= ~num;

    update_pending();
}


void
picint(uint16_t num)
{
    picint_common(num, 0);
}


void
picintlevel(uint16_t num)
{
    picint_common(num, 1);
}


uint8_t
pic_interrupt(void)
{
    uint8_t ret = 0xff;
    int c, d;

    if (! pic_pending) return(ret);

    for (c = 0; c <= 7; c++) {
	if (AT && ((1 << c) == pic.icw3)) {
		for (d = 8; d <= 15; d++) {
			ret = pic_process(&pic2, d);
			if (ret != 0xff)
				return(ret);
		}
	} else {
		ret = pic_process(&pic, c);
		if (ret != 0xff)
			return(ret);
	}
    }

    return(ret);
}


void
pic_dump(void)
{
    DEBUG("PIC1 : MASK %02X PEND %02X INS %02X LEVEL %02X VECTOR %02X CASCADE %02X\n", pic.mask, pic.pend, pic.ins, (pic.icw1 & 8) ? 1 : 0, pic.vector, pic.icw3);
    if (AT)
	DEBUG("PIC2 : MASK %02X PEND %02X INS %02X LEVEL %02X VECTOR %02X CASCADE %02X\n", pic2.mask, pic2.pend, pic2.ins, (pic2.icw1 & 8) ? 1 : 0, pic2.vector, pic2.icw3);
}
