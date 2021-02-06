/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the IBM PS/1 models 2011, 2121 and 2133.
 *
 * Model 2011:	The initial model, using a 10MHz 80286.
 *
 * Model 2121:	This is similar to model 2011 but some of the functionality
 *		has moved to a chip at ports 0xe0 (index)/0xe1 (data). The
 *		only functions I have identified are enables for the first
 *		512K and next 128K of RAM, in bits 0 of registers 0 and 1
 *		respectively.
 *
 *		Port 0x105 has bit 7 forced high. Without this 128K of
 *		memory will be missed by the BIOS on cold boots.
 *
 *		The reserved 384K is remapped to the top of extended memory.
 *		If this is not done then you get an error on startup.
 *
 * Version:	@(#)m_ps1.c	1.0.31	2021/02/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../timer.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/chipsets/vl82c480.h"
#include "../devices/system/dma.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/nmi.h"
#include "../devices/system/memregs.h"
#include "../devices/ports/game.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/sio/sio.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_sn76489.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"
#include "m_ps1.h"


typedef struct {
    sn76489_t	sn76489;
    uint8_t	status, ctrl;
    tmrval_t	timer_latch,
		timer_count;
    tmrval_t	timer_enable;
    uint8_t	fifo[2048];
    int		fifo_read_idx,
		fifo_write_idx;
    int		fifo_threshold;
    uint8_t	dac_val;
    int16_t	buffer[SOUNDBUFLEN];
    int		pos;
} ps1snd_t;

typedef struct {
    int		model;

    rom_t	high_rom;
    mem_map_t	romext_mapping;
    uint8_t	romext[32768];

    uint8_t	reg_91,
		reg_92,
		reg_94,
		reg_102,
		reg_103,
		reg_104,
		reg_105,
		reg_190;
    int		e0_addr;
    uint8_t	e0_regs[256];
} ps1_t;


static void
snd_update_irq(ps1snd_t *snd)
{
    if (((snd->status & snd->ctrl) & 0x12) && (snd->ctrl & 0x01))
	picint(1 << 7);
      else
	picintc(1 << 7);
}


static uint8_t
snd_read(uint16_t port, priv_t priv)
{
    ps1snd_t *snd = (ps1snd_t *)priv;
    uint8_t ret = 0xff;

    switch (port & 7) {
	case 0:		/* ADC data */
		snd->status &= ~0x10;
		snd_update_irq(snd);
		ret = 0;
		break;

	case 2:		/* status */
		ret = snd->status;
		ret |= (snd->ctrl & 0x01);
		if ((snd->fifo_write_idx - snd->fifo_read_idx) >= 2048)
			ret |= 0x08; /* FIFO full */
		if (snd->fifo_read_idx == snd->fifo_write_idx)
			ret |= 0x04; /* FIFO empty */
		break;

	case 3:		/* FIFO timer */
		/*
		 * The PS/1 Technical Reference says this should return
		 * thecurrent value, but the PS/1 BIOS and Stunt Island
		 * expect it not to change.
		 */
		ret = (uint8_t)(snd->timer_latch & 0xff);
		break;

	case 4:
	case 5:
	case 6:
	case 7:
		ret = 0;
    }

    return(ret);
}


static void
snd_write(uint16_t port, uint8_t val, priv_t priv)
{
    ps1snd_t *snd = (ps1snd_t *)priv;

    switch (port & 7) {
	case 0:		/* DAC output */
		if ((snd->fifo_write_idx - snd->fifo_read_idx) < 2048) {
			snd->fifo[snd->fifo_write_idx & 2047] = val;
			snd->fifo_write_idx++;
		}
		break;

	case 2:		/* control */
		snd->ctrl = val;
		if (! (val & 0x02))
			snd->status &= ~0x02;
		snd_update_irq(snd);
		break;

	case 3:		/* timer reload value */
		snd->timer_latch = val;
		snd->timer_count = (tmrval_t) ((0xff-val) * TIMER_USEC);
		snd->timer_enable = (val != 0);
		break;

	case 4:		/* almost empty */
		snd->fifo_threshold = val * 4;
		break;
    }
}


static void
snd_update(ps1snd_t *snd)
{
    for (; snd->pos < sound_pos_global; snd->pos++)        
	snd->buffer[snd->pos] = (int8_t)(snd->dac_val ^ 0x80) * 0x20;
}


static void
snd_callback(priv_t priv)
{
    ps1snd_t *snd = (ps1snd_t *)priv;

    snd_update(snd);

    if (snd->fifo_read_idx != snd->fifo_write_idx) {
	snd->dac_val = snd->fifo[snd->fifo_read_idx & 2047];
	snd->fifo_read_idx++;
    }

    if ((snd->fifo_write_idx - snd->fifo_read_idx) == snd->fifo_threshold)
	snd->status |= 0x02; /*FIFO almost empty*/

    snd->status |= 0x10; /*ADC data ready*/
    snd_update_irq(snd);

    snd->timer_count += snd->timer_latch * TIMER_USEC;
}


static void
snd_get_buffer(int32_t *bufp, int len, priv_t priv)
{
    ps1snd_t *snd = (ps1snd_t *)priv;
    int c;

    snd_update(snd);

    for (c = 0; c < len * 2; c++)
	bufp[c] += snd->buffer[c >> 1];

    snd->pos = 0;
}


static void
snd_close(priv_t priv)
{
    ps1snd_t *snd = (ps1snd_t *)priv;

    free(snd);
}


static priv_t
snd_init(const device_t *info, UNUSED(void *parent))
{
    ps1snd_t *snd;

    snd = (ps1snd_t *)mem_alloc(sizeof(ps1snd_t));
    memset(snd, 0x00, sizeof(ps1snd_t));

    sn76489_init(&snd->sn76489, 0x0205, 0x0001, SN76496, 4000000);

    io_sethandler(0x0200, 1,
		  snd_read,NULL,NULL, snd_write,NULL,NULL, snd);
    io_sethandler(0x0202, 6,
		  snd_read,NULL,NULL, snd_write,NULL,NULL, snd);

    timer_add(snd_callback, snd, &snd->timer_count, &snd->timer_enable);

    sound_add_handler(snd_get_buffer, snd);

    return((priv_t)snd);
}


static const device_t snd_device = {
    "PS/1 Audio",
    0, 0, NULL,
    snd_init, snd_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static uint8_t
read_romext(uint32_t addr, priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;

    return dev->romext[addr & 0x7fff];
}


static uint16_t
read_romextw(uint32_t addr, priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;
    uint16_t *p = (uint16_t *)&dev->romext[addr & 0x7fff];

    return *p;
}


static uint32_t
read_romextl(uint32_t addr, priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;
    uint32_t *p = (uint32_t *)&dev->romext[addr & 0x7fff];

    return *p;
}


static void
recalc_memory(ps1_t *dev)
{
    /* Enable first 512K */
    mem_set_mem_state(0x00000, 0x80000,
		      (dev->e0_regs[0] & 0x01) ?
			(MEM_READ_INTERNAL | MEM_WRITE_INTERNAL) :
			(MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL));

    /* Enable 512-640K */
    mem_set_mem_state(0x80000, 0x20000,
		      (dev->e0_regs[1] & 0x01) ?
			(MEM_READ_INTERNAL | MEM_WRITE_INTERNAL) :
			(MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL));
}


static void
ps1_write(uint16_t port, uint8_t val, priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;

    switch (port) {
	case 0x0092:
		if (dev->model != 2011) {
			if (val & 1) {
				cpu_reset(0);
				cpu_set_edx();
			}
			dev->reg_92 = val & ~1;
		} else {
			dev->reg_92 = val;    
		}
		mem_a20_alt = val & 2;
		mem_a20_recalc();
		break;

	case 0x0094:
		dev->reg_94 = val;
		break;

	case 0x00e0:
		if (dev->model != 2011)
			dev->e0_addr = val;
		break;

	case 0x00e1:
		if (dev->model != 2011) {
			dev->e0_regs[dev->e0_addr] = val;
			recalc_memory(dev);
		}
		break;

	case 0x0102:
		if (val & 0x04)
			serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
#if 0
		  else
			serial_remove(0);
#endif
		if (val & 0x10) {
			switch ((val >> 5) & 3) {
				case 0:
					parallel_setup(0, 0x03bc);
					break;

				case 1:
					parallel_setup(0, 0x0378);
					break;

				case 2:
					parallel_setup(0, 0x0278);
					break;
			}
		}
		dev->reg_102 = val;
		break;

	case 0x0103:
		dev->reg_103 = val;
		break;

	case 0x0104:
		dev->reg_104 = val;
		break;

	case 0x0105:
		dev->reg_105 = val;
		break;

	case 0x0190:
		dev->reg_190 = val;
		break;
    }
}


static uint8_t
ps1_read(uint16_t port, priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0091:		/* Card Select Feedback register */
		ret = dev->reg_91;
		dev->reg_91 = 0;
		break;

	case 0x0092:
		ret = dev->reg_92;
		break;

	case 0x0094:
		ret = dev->reg_94;
		break;

	case 0x00e1:
		if (dev->model != 2011)
			ret = dev->e0_regs[dev->e0_addr];
		break;

	case 0x0102:
		if (dev->model == 2011)
			ret = dev->reg_102 | 0x08;
		  else
			ret = dev->reg_102;
		break;

	case 0x0103:
		ret = dev->reg_103;
		break;

	case 0x0104:
		ret = dev->reg_104;
		break;

	case 0x0105:
		if (dev->model == 2011)
			ret = dev->reg_105;
		  else
			ret = dev->reg_105 | 0x80;
		break;

	case 0x0190:
		ret = dev->reg_190;
		break;

	default:
		break;
    }

    return(ret);
}


static void
ps1_close(priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;

    free(dev);
}


static priv_t
ps1_init(const device_t *info, void *arg)
{
//    romdef_t *bios = (romdef_t *)arg;
    ps1_t *dev;
    int i;

    dev = (ps1_t *)mem_alloc(sizeof(ps1_t));
    memset(dev, 0x00, sizeof(ps1_t));
    dev->model = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    if (dev->model == 2011) {
	/* Force some configuration settings. */
	config.video_card = VID_INTERNAL;
	config.mouse_type = MOUSE_PS2;

	mem_map_add(&dev->romext_mapping, 0xc8000, 0x08000,
		    read_romext,read_romextw,read_romextl,
		    NULL,NULL, NULL, dev->romext, 0, dev);

	if (machine_get_config_int("rom_shell")) {
		if (! rom_init(&dev->high_rom,
			       L"machines/ibm/ps1_2011/fc0000_105775_us.bin",
			       0xfc0000, 0x20000, 0x01ffff, 0, MEM_MAPPING_EXTERNAL)) {
			ERRLOG("PS1: unable to load ROM Shell !\n");
		}
	}

	/* Set up the parallel port. */
    parallel_setup(0, 0x03bc);

	/* Enable the PS/1 VGA controller. */
	device_add(&vga_ps1_device);

	/* Enable the builtin sound chip. */
	device_add(&snd_device);

 	/* Enable the builtin FDC. */
	device_add(&fdc_at_actlow_device);

 	/* Enable the builtin HDC. */
	if (config.hdc_type == HDC_INTERNAL)
		device_add(&ps1_hdc_device);
    }

    if (dev->model == 2121) {
	/* Force some configuration settings. */
	config.video_card = VID_INTERNAL;
	config.mouse_type = MOUSE_PS2;

	/* Set up the parallel port. */
    parallel_setup(0, 0x03bc);

	io_sethandler(0x00e0, 2, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);

	if (machine_get_config_int("rom_shell")) {
		DEBUG("PS1: loading ROM Shell..\n");
		if (! rom_init(&dev->high_rom,
			       L"machines/ibm/ps1_2121/fc0000_92f9674.bin",
			       0xfc0000, 0x20000, 0x1ffff, 0, MEM_MAPPING_EXTERNAL)) {
			ERRLOG("PS1: unable to load ROM Shell !\n");
		}
	}

	/* Initialize the video controller. */
	device_add(&ti_ps1_device);

	/* Enable the builtin sound chip. */
	device_add(&snd_device);

 	/* Enable the builtin FDC. */
	device_add(&fdc_at_ps1_device);

	/* Enable the builtin IDE port. */
	device_add(&ide_isa_device);
    }

    if (dev->model == 2133) {
	/* Force some configuration settings. */
	config.hdc_type = HDC_INTERNAL;
	config.mouse_type = MOUSE_PS2;

	/* Enable the builtin IDE port. */
	device_add(&ide_isa_device);

	/* Enable the chipset & SIO */
	device_add(&vl82c480_device);
	device_add(&pc87332_ps1_device);

	if (config.video_card == VID_INTERNAL)
		device_add(&gd5426_onboard_vlb_device);

	device_add(&memregs_ed_device);

	nmi_mask = 0x80;
    }

    io_sethandler(0x0091, 1, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);
    io_sethandler(0x0092, 1, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);
    io_sethandler(0x0094, 1, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);
    io_sethandler(0x0102, 4, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);
    io_sethandler(0x0190, 1, ps1_read,NULL,NULL, ps1_write,NULL,NULL, dev);

    /* Hack to prevent Game from being initialized there. */
    i = config.game_enabled;
    config.game_enabled = 0;
    machine_common_init();
    config.game_enabled = i;

    mem_remap_top(384);

    /* Set up our DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_at_refresh_timer);

    dma16_init();
    pic2_init();

    device_add(&ps_nvr_device);

    device_add(&keyboard_ps2_ps1_device);
	
	device_add(&mouse_ps2_device);

    /* Audio uses ports 200h,202-207h, so only initialize gameport on 201h. */
    if (config.game_enabled)
		device_add(&game_201_device);

    return((priv_t)dev);
}


static const device_config_t ps1_config[] = {
    {
	"rom_shell", "ROM Shell", CONFIG_SELECTION, "", 1,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled", 1
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


static const CPU cpus_ps1_m2011[] = {
    { "286/10", CPU_286, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1 },
    { NULL }
};

static const machine_t m2011_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,
    MACHINE_VIDEO,
    512, 6144, 512, 64, -1,
    {{"",cpus_ps1_m2011}}
};

const device_t m_ps1_2011 = {
    "IBM PS/1 2011",
    DEVICE_ROOT,
    2011,
    L"ibm/ps1_2011",
    ps1_init, ps1_close, NULL,
    NULL, NULL, NULL,
    &m2011_info,
    ps1_config
};


static const machine_t m2121_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_FDC_PS2 | MACHINE_VIDEO,
    MACHINE_VIDEO,
    1, 6, 1, 64, -1,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_ps1_2121 = {
    "IBM PS/1 2121",
    DEVICE_ROOT,
    2121,
    L"ibm/ps1_2121",
    ps1_init, ps1_close, NULL,
    NULL, NULL, NULL,
    &m2121_info,
    ps1_config
};


static const machine_t m2133_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO | MACHINE_NONMI,
    0,
    1, 64, 1, 128, -1,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_ps1_2133 = {
    "IBM PS/1 2133",
    DEVICE_ROOT,
    2133,
    L"ibm/ps1_2133",
    ps1_init, ps1_close, NULL,
    NULL, NULL, NULL,
    &m2133_info,
    ps1_config
};


/* Set the Card Selected Flag */
void
ps1_set_feedback(priv_t priv)
{
    ps1_t *dev = (ps1_t *)priv;

    dev->reg_91 |= 0x01;
}
