/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the C&T 82c710 Super I/O Chip.
 *
 * Version:	@(#)sio_f82c710.c	1.0.0	2020/11/16
 *
 * Authors:	    Eluan Costa Miranda <eluancm@gmail.com>
 *              Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *      Copyright 2020 Eluan Costa Miranda.
 *		Copyright 2020 Fred N. van Kempen.
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
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../device.h"
#include "../input/keyboard.h"
#include "../ports/parallel.h"
#include "../ports/serial.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../../plat.h"
#include "sio.h"


typedef struct upc_t
{
	int configuration_state; /* state of algorithm to enter configuration mode */
	int configuration_mode;
    uint8_t next_value;     // next expected value of configuration algorithm
	uint16_t cri_addr; /* cri = configuration index register, addr is even */
	uint16_t cap_addr; /* cap = configuration access port, addr is odd and is cri_addr + 1 */
	uint8_t cri; /* currently indexed register */

    /* these regs are not affected by reset */
    uint8_t regs[15]; /* there are 16 indexes, but there is no need to store the last one which is: R = cri_addr / 4, W = exit config mode */
	
    uint16_t mdata_addr;    // Address of PS/2 data register
    uint16_t mstat_addr;    // Address of PS/2 status register
    uint8_t mouse_status;   // Mouse interface status register
    uint8_t mouse_data;     // Mouse interface data register
    void (*mouse_write)(uint8_t val, void *p);

    fdc_t *fdc;

} upc_t;


#define UPC_MOUSE_DEV_IDLE     0x01      /* bit 0, Device Idle */
#define UPC_MOUSE_RX_FULL      0x02      /* bit 1, Device Char received */
#define UPC_MOUSE_TX_IDLE      0x04      /* bit 2, Device XMIT Idle */
#define UPC_MOUSE_RESET        0x08      /* bit 3, Device Reset */
#define UPC_MOUSE_INTS_ON      0x10      /* bit 4, Device Interrupt On */
#define UPC_MOUSE_ERROR_FLAG   0x20      /* bit 5, Device Error */
#define UPC_MOUSE_CLEAR        0x40      /* bit 6, Device Clear */
#define UPC_MOUSE_ENABLE       0x80      /* bit 7, Device Enable */



static void 
upc_update_ports(priv_t priv)
{
	upc_t *dev = (upc_t *)priv;
    uint16_t com_addr = 0;
	uint16_t lpt_addr = 0;
	
        
        
    switch (dev->cri) {
        case 0x0:
            if (dev->regs[0] & 0x4) {
                com_addr = dev->regs[4] * 4;
		        if (com_addr == SERIAL1_ADDR) {
			        serial_setup(0, com_addr, 4);
		        } else if (com_addr == SERIAL2_ADDR) {
			        serial_setup(1, com_addr, 3);
		        }
            }
              
            if (dev->regs[0] & 8) {
		        lpt_addr = dev->regs[6] * 4;
                parallel_setup(0, lpt_addr);
            }
            break;
        case 0x1: case 0x2:
        case 0x9: case 0xe:
            break;

        case 0xc :
            if (! (dev->regs[0xc] & 0x80)) {
                if (dev->regs[0xc] & 0x40) 
                    ide_pri_disable();
            }

            if (dev->regs[0xc] & 0x80) {
                if (dev->regs[0xc] & 0x40)
                    ide_pri_enable();
            }

            if (dev->regs[0xc] & 0x20)
                fdc_set_base(dev->fdc, 0x03f0);
            break;
        
        case 0x0d :
            if (dev->regs[0xd] != 0) {
                dev->mdata_addr = dev->regs[0xc] * 4;
                dev->mstat_addr = dev->mdata_addr + 1;
                //upc_mouse_enable(dev);
            } else
            {
                //upc_mouse_disabled(dev);
            }
            break;
            
    }
}

static uint8_t 
upc_read(uint16_t addr, priv_t priv)
{
    upc_t *dev = (upc_t *)priv; 
    uint8_t ret = 0xff;

    if (dev->configuration_mode) {
		if (addr == dev->cri_addr) {
			ret = dev->cri;
        } else if (addr == dev->cap_addr) {
			if (dev->cri == 0xf)
				ret = dev->cri_addr / 4;
			else
				ret = dev->regs[dev->cri];
        }
    }

    return(ret);
}

static void 
upc_write(uint16_t addr, uint8_t val, priv_t priv)
{
    upc_t *dev = (upc_t *)priv; 
    int configuration_state_event;
    uint8_t addr_verify;

    switch(addr) {
        case 0x2fa:
			if (dev->configuration_state == 0) {
				configuration_state_event = 1;
                 /* next value should be the 1's complement of the current one */
                dev->next_value = 0xff - val;
            }
            else if (dev->configuration_state == 4) {
                addr_verify = dev->cri_addr / 4;
				addr_verify += val;
				if (addr_verify == 0xff) {
                    dev->configuration_mode = 1;
                    /* TODO: is the value of cri reset here or when exiting configuration mode? */
                    io_sethandler(dev->cri_addr, 0x0002, upc_read, NULL, NULL, upc_write, NULL, NULL, dev);
                } else {
					dev->configuration_mode = 0;
				}
			}
			break;
        case 0x3fa:
			/* go to configuration step 2 if value is the expected one */
            if (dev->configuration_state == 1 && val == dev->next_value)
				configuration_state_event = 1;
            else if (dev->configuration_state == 2 && val == 0x36)
				configuration_state_event = 1;
            else if (dev->configuration_state == 3) {
				dev->cri_addr = val * 4;
				dev->cap_addr = dev->cri_addr + 1;
				configuration_state_event = 1;
			}
			break;

        default:
            break;
    }
        
	if (dev->configuration_mode) {
        if (addr == dev->cri_addr) {
            dev->cri = val & 0xf;
        } else if (addr == dev->cap_addr) {
            if (dev->cri == 0xf) {
				dev->configuration_mode = 0;
				io_removehandler(dev->cri_addr, 0x0002, upc_read, NULL, NULL, upc_write, NULL, NULL, dev);
			} else {
                dev->regs[dev->cri] = val;
                /* configuration should be updated at each register write, otherwise PC5086 do not detect ports correctly */
                upc_update_ports(dev);
            }
        }
    }

    /* TODO: is the state only reset when accessing 0x2fa and 0x3fa wrongly? */
	if ((addr == 0x2fa || addr == 0x3fa) && configuration_state_event)
		dev->configuration_state++;
	else
		dev->configuration_state = 0;
}

#if 0 /* Should be in mouse_ps2 */
uint8_t upc_mouse_read(uint16_t port, void *priv)
{
        upc_t *dev = (upc_t *)priv; 
        uint8_t ret = 0xff;

        if (port == dev->mstat_addr)
                ret = dev->mouse_status;

        if (port == dev->mdata_addr && (dev->mouse_status & UPC_MOUSE_RX_FULL)) {
                ret = dev->mouse_data;
                dev->mouse_data = 0xff;
                dev->mouse_status &= ~UPC_MOUSE_RX_FULL;
                dev->mouse_status |= UPC_MOUSE_DEV_IDLE;
        }

        return ret;
}

void upc_mouse_write(uint16_t port, uint8_t val, void *priv)
{

        upc_t *dev = (upc_t *)priv;

        if (port == dev->mstat_addr) {
            /* write status bits
            * DEV_IDLE, TX_IDLE, RX_FULL and ERROR_FLAG bits are unchanged
            */
            dev->mouse_status = (val & 0xD8) | (dev->mouse_status & 0x27);
            if (dev->mouse_status & UPC_MOUSE_ENABLE)
                mouse_scan = 1;
            else
                mouse_scan = 0;
            if (dev->mouse_status & (UPC_MOUSE_CLEAR | UPC_MOUSE_RESET)) {
                /* if CLEAR or RESET bit is set, clear mouse queue */
                mouse_queue_start = mouse_queue_end;
                dev->mouse_status &= ~UPC_MOUSE_RX_FULL;
                dev->mouse_status |= UPC_MOUSE_DEV_IDLE | UPC_MOUSE_TX_IDLE;
                mouse_scan = 0;
            }
        }

        if (port == dev->mdata_addr) {
            if ((dev->mouse_status & UPC_MOUSE_TX_IDLE) && (dev->mouse_status & UPC_MOUSE_ENABLE)) {
                dev->mouse_data = val;
                if (dev->mouse_write)
                    dev->mouse_write(val, dev->mouse_p);
                }
        }
}

void upc_mouse_disable(priv_t priv)
{
        upc_t *dev = (upc_t *)priv;

        io_removehandler(dev->mdata_addr, 0x0002, upc_mouse_read, NULL, NULL, upc_mouse_write, NULL, NULL, dev);
}

void upc_mouse_enable(priv_t priv)
{
        upc_t *dev = (upc_t *)priv;

        io_sethandler(dev->mdata_addr, 0x0002, upc_mouse_read, NULL, NULL, upc_mouse_write, NULL, NULL, dev);
}

void upc_set_mouse(void (*mouse_write)(uint8_t val, void *p), void *p)
{
        upc_t *dev = (upc_t *)priv;

        dev->mouse_write = mouse_write;
        dev->mouse_p = p;
}

void upc_mouse_poll(void *priv)
{
    upc_t *dev = (upc_t *)priv;
        
    //timer_advance_u64(&upc->mouse_delay_timer, (1000 * TIMER_USEC));

    /* check if there is something in the mouse queue */
    if (mouse_queue_start != mouse_queue_end) {
	    if ((dev->mouse_status & UPC_MOUSE_ENABLE) && !(upc->mouse_status & UPC_MOUSE_RX_FULL)) {
	        dev->mouse_data = mouse_queue[mouse_queue_start];
            mouse_queue_start = (mouse_queue_start + 1) & 0xf;
            /* update mouse status */
            dev->mouse_status |= UPC_MOUSE_RX_FULL;
            dev->mouse_status &= ~(UPC_MOUSE_DEV_IDLE);
            /* raise IRQ if enabled */
            if (upc->mouse_status & UPC_MOUSE_INTS_ON) {
                picint(1 << dev->mouse_irq);
            }
        }
    }
}
#endif

static void
upc_reset(upc_t *dev)
{
    memset(dev->regs, 0x00, sizeof(dev->regs));

    dev->regs[0] = 0x0c;
    dev->regs[1] = 0x00;
    dev->regs[2] = 0x00;
    dev->regs[3] = 0x00;
    dev->regs[4] = 0xfe;
    dev->regs[5] = 0x00;
    dev->regs[6] = 0x9e;
    dev->regs[7] = 0x00;
    dev->regs[8] = 0x00;
    dev->regs[9] = 0xb0;
    dev->regs[0xa] = 0x00;
    dev->regs[0xb] = 0x00;
    dev->regs[0xc] = 0xa0;
    dev->regs[0xd] = 0x00;
    dev->regs[0xe] = 0x00;

    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);

    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

    parallel_setup(0, 0x0378);

    fdc_reset(dev->fdc);
}


static void
upc_close(priv_t priv)
{
    upc_t *dev = (upc_t *)priv;

    free(dev);
}


static priv_t
upc_init(const device_t *info, UNUSED(void *parent))
{
    upc_t *dev;

    dev = (upc_t *)mem_alloc(sizeof(upc_t));
    memset(dev, 0x00, sizeof(upc_t));

    for (dev->cri = 0; dev->cri < 15; dev->cri++)
        upc_update_ports(dev);
    dev->cri = 0;
    
    //dev->mouse_irq = 12;
    dev->mdata_addr = dev->regs[0xc] * 4;
    dev->mstat_addr = dev->mdata_addr + 1;
    dev->mouse_status = UPC_MOUSE_DEV_IDLE | UPC_MOUSE_TX_IDLE;
    dev->mouse_data = 0xff;

    dev->fdc = device_add(&fdc_at_actlow_device); /* Fix me */

    io_sethandler(0x02fa, 1,
		  upc_read,NULL,NULL, upc_write,NULL,NULL, dev);
    io_sethandler(0x03fa, 1,
		  upc_read,NULL,NULL, upc_write,NULL,NULL, dev);

    upc_reset(dev);

    return((priv_t)dev);
}


const device_t f82c710_device = {
    "C&T 82c710 Super I/O",
    0, 0, NULL,
    upc_init, upc_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
