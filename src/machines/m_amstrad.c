/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Amstrad series of PC's: PC1512, PPC512,
 *		PC1640 and PC200, including their keyboard/mouse devices,
 *		as well as the PC2086, PC3086 and PC5086 systems.
 *
 * **NOTE**	The PC200 still has the weirdness that if Television mode
 *		is selected, it will ignore that, and select CGA mode in
 *		80 columns. To be fixed...
 *		Also, the DDM bits stuff needs to be verified.
 *
 * Version:	@(#)m_amstrad.c	1.0.32	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *              John Elliott, <jce@seasip.info>
 *
 *              Copyright 2017-2021 Fred N. van Kempen.
 *              Copyright 2016-2018 Miran Grca.
 *              Copyright 2017-2018 John Elliott.
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
#define dbglog kbd_log
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../timer.h"
#include "../device.h"
#include "../machines/machine.h"
#include "../nvr.h"
#include "../devices/chipsets/cs82c100.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/ppi.h"
#include "../devices/ports/parallel.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/sio/sio.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_speaker.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"
#include "m_amstrad.h"


/* Values for LK1-3 jumpers (BIOS language.) */
#define LK13_EN		0		/* English */
#define LK13_DE		1		/* German */
#define LK13_FR		2		/* French */
#define LK13_ES		3		/* Spanish */
#define LK13_DK		4		/* Danish */
#define LK13_SE		5		/* Swedish */
#define LK13_IT		6		/* Italian */
#define LK13_DIAG	7		/* Diagnostic Mode */

#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01


typedef struct {
    /* Machine stuff. */
    int8_t	type;
    uint8_t	dead;
    uint8_t	stat1,
		stat2;

    /* Configuration options. */
    int		flopswap;
    int		codepage;
    int		language;
    int		vidtype;
    int		disptype;

    /* Keyboard stuff. */
    int8_t	wantirq;
    uint8_t	key_waiting;
    uint8_t	pa,
		pb;

    /* Video stuff. */
    priv_t	vid;

    /* Mouse stuff. */
    uint8_t	mousex,
		mousey;
    int		oldb;
} amstrad_t;


static const video_timings_t pvga1a_timing = {VID_ISA,6,8,16,6,8,16};
static const video_timings_t wd90c11_timing = {VID_ISA,3,3,6,5,5,10};

static uint8_t	key_queue[16];
static int	key_queue_start = 0,
		key_queue_end = 0;


static void
mse_write(uint16_t port, UNUSED(uint8_t val), priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    if (port == 0x0078)
	dev->mousex = 0;
      else
	dev->mousey = 0;
}


static uint8_t
mse_read(uint16_t port, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;
    uint8_t ret;

    if (port == 0x0078) {
	ret = dev->mousex;
	dev->mousex = 0;
    } else {
	ret = dev->mousey;
	dev->mousey = 0;
    }

    return(ret);
}


static int
mse_poll(int x, int y, UNUSED(int z), int b, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    dev->mousex += x;
    dev->mousey -= y;

    if ((b & 1) && !(dev->oldb & 1))
	keyboard_send(0x7e);
    if (!(b & 1) && (dev->oldb & 1))
	keyboard_send(0xfe);

    if ((b & 2) && !(dev->oldb & 2))
	keyboard_send(0x7d);
    if (!(b & 2) && (dev->oldb & 2))
	keyboard_send(0xfd);

    dev->oldb = b;

    return(1);
}


static void
kbd_adddata(uint16_t val)
{
    key_queue[key_queue_end] = (uint8_t)(val & 0xff);
    key_queue_end = (key_queue_end + 1) & 0x0f;
}


static void
kbd_adddata_ex(uint16_t val)
{
    keyboard_adddata(val, kbd_adddata);
}


static void
kbd_write(uint16_t port, uint8_t val, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    DBGLOG(2, "AMSkb: write(%04x,%02x) pb=%02x\n", port, val, dev->pb);

    switch (port) {
	case 0x61:
		/*
		 * PortB - System Control.
		 *
		 *  7	Enable Status-1/Disable Keyboard Code on Port A.
		 *  6	Enable incoming Keyboard Clock.
		 *  5	Prevent external parity errors from causing NMI.
		 *  4	Disable parity checking of on-board system Ram.
		 *  3	Undefined (Not Connected).
		 *  2	Enable Port C LSB / Disable MSB. (See 1.8.3)
		 *  1	Speaker Drive.
		 *  0	8253 GATE 2 (Speaker Modulate).
		 *
		 * This register is controlled by BIOS and/or ROS.
		 */
		DBGLOG(1, "AMSkb: write PB=%02x (%02x)\n", val, dev->pb);
		if (!(dev->pb & 0x40) && (val & 0x40)) { /*Reset keyboard*/
			DEBUG("AMSkb: reset keyboard\n");
			kbd_adddata(0xaa);
		}
		dev->pb = val;
		ppi.pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();
		speaker_gated = val & 0x01;
		speaker_enable = val & 0x02;
		if (speaker_enable) 
			speaker_was_enable = 1;
		pit_set_gate(&pit, 2, val & 0x01);

		if (val & 0x80) {
			/* Keyboard disabled, so enable PA reading. */
			dev->pa = 0x00;
		}
		break;

	case 0x63:
		break;

	case 0x64:
		/*
		 * ROS writes to this port the bits that can be
		 * read back at the KBC controller, port C, the
		 * the DDM bits at 7:6.
		 */
		dev->stat1 = val;
		break;

	case 0x65:
		/*
		 * ROS writes to this register the bits that are
		 * normally read out at the printer status port,
		 * indicating memory size, DDM and so on.
		 */
		dev->stat2 = val;
		break;

	case 0x66:
		pc_reset(1);
		break;

	default:
		ERRLOG("AMSkb: bad keyboard write %04X %02X\n", port, val);
    }
}


static uint8_t
kbd_read(uint16_t port, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x60:
		if (dev->pb & 0x80) {
			/*
			 * PortA - System Status 1
			 *
			 *  7	Always 0			    (KBD7)
			 *  6	Second Floppy disk drive installed  (KBD6)
			 *  5	DDM1 - Default Display Mode bit 1   (KBD5)
			 *  4	DDM0 - Default Display Mode bit 0   (KBD4)
			 *  3	Always 1			    (KBD3)
			 *  2	Always 1			    (KBD2)
			 *  1	8087 NDP installed		    (KBD1)
			 *  0	Always 1			    (KBD0)
			 *
			 * DDM10
			 *    00 External controller
			 *    01 Color,alpha,40x25, bright white on black.
			 *    10 Color,alpha,80x25, bright white on black.
			 *    11 Monochrome,80x25.
			 *
			 * Following a reset, the hardware selects VDU mode
			 * 2. The ROS then sets the initial VDU state based
			 * on the DDM value.
			 */
			ret = (0x0d | dev->stat1) & 0x7f;
		} else {
			ret = dev->pa;
			if (key_queue_start == key_queue_end) {
				dev->wantirq = 0;
			} else {
				dev->key_waiting = key_queue[key_queue_start];
				key_queue_start = (key_queue_start + 1) & 0xf;
				dev->wantirq = 1;	
			}
		}	
		break;

	case 0x61:
		ret = dev->pb;
		break;

	case 0x62:
		/*
		 * PortC - System Status 2.
		 *
		 *  7	On-board system RAM parity error.
		 *  6	External parity error (I/OCHCK from expansion bus).
		 *  5	8253 PIT OUT2 output.
		 *  4 	Undefined (Not Connected).
		 *-------------------------------------------
		 *	LSB 	MSB (depends on PB2)
		 *-------------------------------------------
		 *  3	RAM3	Undefined
		 *  2	RAM2	Undefined
		 *  1	RAM1	Undefined
		 *  0	RAM0	RAM4
		 *
		 * PC7 is forced to 0 when on-board system RAM parity
		 * checking is disabled by PB4.
		 *
		 * RAM4:0
		 * 01110	512K bytes on-board.
		 * 01111	544K bytes (32K external).
		 * 10000	576K bytes (64K external).
		 * 10001	608K bytes (96K external).
		 * 10010	640K bytes (128K external or fitted on-board).
		 */
		if (dev->pb & 0x04)
			ret = (dev->stat2 & 0x0f);
		  else
			ret = (dev->stat2 >> 4);
		ret |= (ppispeakon ? 0x20 : 0);
		if (nmi)
			ret |= 0x40;
		break;

	default:
		ERRLOG("AMSkb: bad keyboard read %04X\n", port);
    }

    return(ret);
}


static void
kbd_poll(priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    keyboard_delay += (1000 * TIMER_USEC);

    if (dev->wantirq) {
	dev->wantirq = 0;
	dev->pa = dev->key_waiting;
	picint(1 << 1);
    }

    if (key_queue_start != key_queue_end && !dev->pa) {
	dev->key_waiting = key_queue[key_queue_start];
	key_queue_start = (key_queue_start + 1) & 0x0f;
	dev->wantirq = 1;
    }
}


static void
ams_write(uint16_t port, uint8_t val, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    switch (port) {
	case 0xdead:
		dev->dead = val;
		break;
    }
}


static uint8_t
ams_read(uint16_t port, priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;
    uint8_t ddm, ret = 0xff;
    int i;

    switch (port) {
	case 0x0379:	/* printer status, also set LK1-3.
			 *   0	English Language.
			 *   1	German Language.
			 *   2	French Language.
			 *   3	Spanish Language.
			 *   4	Danish Language.
			 *   5	Swedish Language.
			 *   6	Italian Language.
			 *   7	Diagnostic Mode.
			 */
		ret = dev->language;
		ret ^= 0x07;				/* bits inverted! */
		break;

	case 0x037a:	/* printer control */
		ret = 0x00;
		switch(dev->type) {
			case 0:		/* PC1512 */
				ret |= 0x20;
				break;

			case 2:		/* PC200 */
			case 3:		/* PPC */
				if (dev->vid != NULL) {
					/* IDA is enabled. */
					ddm = m_amstrad_ida_ddm(dev->vid);
#if 0
					ret |= ((ddm & 0x01) << 7);  /* DDM0 */
					ret |= ((ddm & 0x02) << 5);  /* DDM1 */
#else
					ret |= ((ddm & 0x03) << 6);  /* DDM */
#endif
				} else {
					/* External controller. */
					i = video_type();
					if (i == VID_TYPE_MDA)
               					ret |= 0xc0;	/* MDA/Herc */
				  	else
						ret |= 0x40;	/* CGA */
				}

#if 0
				if (dev->type == 3)
					ret ^= 0xc0;	/* bits inverted! */
#endif
				break;

			default:
				break;
		}
		break;

	case 0x03de:
		/*
		 * This port is only mapped in for the PC200, and
		 * then only if External Video controller is in use.
		 */
		ret = 0x20;
		break;

	case 0xdead:
		ret = dev->dead;
		break;
    }

    return(ret);
}


static void
amstrad_close(priv_t priv)
{
    amstrad_t *dev = (amstrad_t *)priv;

    free(dev);
}


static priv_t
amstrad_init(const device_t *info, void *arg)
{
    romdef_t *roms = (romdef_t *)arg;
    amstrad_t *dev;
    priv_t lpt;

    dev = (amstrad_t *)mem_alloc(sizeof(amstrad_t));
    memset(dev, 0x00, sizeof(amstrad_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    /* Force the LPT1 port disabled and add our own. */
    config.parallel_enabled[0] = 0;
    machine_common_init();
    lpt = device_add(&parallel_1_device);

    nmi_init();

    device_add(&amstrad_nvr_device);

    switch (dev->type) {
	case 0:		/* PC1512 */
		dev->codepage = device_get_config_int("codepage");
		dev->language = device_get_config_int("language");

		/* Initialize the internal CGA controller. */
		dev->vid = m_amstrad_1512_vid_init(roms->fontfn,
						   roms->fontnum,
						   dev->codepage);
		device_add(&fdc_xt_device);
		parallel_set_func(lpt, ams_read, (priv_t)dev);
		break;

	case 1:		/* PC1640 */
		dev->language = device_get_config_int("language");

		/* Initialize the internal CGA/EGA controller. */
		dev->vid = m_amstrad_1640_vid_init(roms->vidfn, roms->vidsz);

		device_add(&fdc_xt_device);
		parallel_set_func(lpt, ams_read, (priv_t)dev);
		break;

	case 2:		/* PC200 */
		dev->codepage = device_get_config_int("codepage");
		dev->language = device_get_config_int("language");
		dev->flopswap = device_get_config_int("floppy_swap");
		dev->vidtype = device_get_config_int("video_emulation");
		dev->disptype = device_get_config_int("display_type");

		if (config.video_card == VID_INTERNAL) {
			/* Initialize the internal video controller. */
			dev->vid = m_amstrad_ida_init(dev->type, roms->fontfn,
						      dev->codepage,
						      dev->vidtype,
						      dev->disptype);
		} else {
			io_sethandler(0x03de, 1,
				      ams_read,NULL,NULL, NULL,NULL,NULL, dev);
		}

		device_add(&fdc_xt_device);
		parallel_set_func(lpt, ams_read, (priv_t)dev);
		break;

	case 3:		/* PPC */
		dev->codepage = device_get_config_int("codepage");
		dev->language = device_get_config_int("language");
		dev->vidtype = device_get_config_int("video_emulation");
		dev->disptype = device_get_config_int("display_type");

		if (config.video_card == VID_INTERNAL) {
			/* Initialize the internal video controller. */
			dev->vid = m_amstrad_ida_init(1, roms->fontfn,
						      dev->codepage,
						      dev->vidtype,
						      dev->disptype);
		}

		device_add(&fdc_xt_device);
		parallel_set_func(lpt, ams_read, (priv_t)dev);
		break;

	case 4:		/* PC2086 */
		if (config.video_card == VID_INTERNAL) {
			device_add(&paradise_pvga1a_pc2086_device);
			video_inform(VID_TYPE_SPEC, &pvga1a_timing);
		}
		device_add(&fdc_at_actlow_device);
		break;

	case 5:		/* PC3086 */
		if (config.video_card == VID_INTERNAL) {
			device_add(&paradise_pvga1a_pc3086_device);
			video_inform(VID_TYPE_SPEC, &pvga1a_timing);
		}
		device_add(&fdc_at_actlow_device);
		break;

	case 6:		/* MEGAPC */
		if (config.video_card == VID_INTERNAL) {
			device_add(&paradise_wd90c11_megapc_device);
			video_inform(VID_TYPE_SPEC, &wd90c11_timing);
		}
		device_add(&fdc_at_actlow_device);
		break;

	case 7:		/* PC 5086 */
#if 0
		if (config.video_card == VID_INTERNAL) {
			device_add(&ct82c451_pc5086_device);
			video_inform(VID_TYPE_SPEC, &ct82c451_timing);
		}
#endif

		/* Enable the builtin FDC. */
#if 0
		device_add(&fdc_at_actlow_device);
#endif

		/*
		 * Enable the builtin HDC.
		 * Fix Me : Check if it should be tied to chipset
		 */
		if (config.hdc_type == HDC_INTERNAL)
			device_add(&xta_pc5086_device);
		device_add(&cs82c100_device);
		device_add(&f82c710_device);		
		break;
    }

    config.parallel_enabled[0] = 1;

    io_sethandler(0xdead, 1, ams_read,NULL,NULL, ams_write,NULL,NULL, dev);

    /* Initialize the (custom) keyboard interface. */
    dev->wantirq = 0;
    io_sethandler(0x0060, 7, kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    timer_add(kbd_poll, dev, &keyboard_delay, TIMER_ALWAYS_ENABLED);
    keyboard_set_table(scancode_xt);
    keyboard_send = kbd_adddata_ex;
    keyboard_scan = 1;

    /* Initialize the (custom) mouse interface if needed. */
    if (config.mouse_type == MOUSE_INTERNAL) {
	io_sethandler(0x0078, 1, mse_read,NULL,NULL, mse_write,NULL,NULL, dev);
	io_sethandler(0x007a, 1, mse_read,NULL,NULL, mse_write,NULL,NULL, dev);

	mouse_reset();

	mouse_set_poll(mse_poll, (priv_t)dev);
    }

    return((priv_t)dev);
}


static const device_config_t pc1512_config[] = {
    {
	"codepage", "Hardware font", CONFIG_SELECTION, "", 3,
	{
		{
			"US English", 3
		},
		{
			"Danish", 1
		},
		{
			"Greek", 0
		},
		{
			NULL
		}
	}
    },
    {
	"language", "ROS Language", CONFIG_SELECTION, "", 0,
	{
		{
			"English", 0
		},
		{
			"German", 1
		},
		{
			"French", 2
		},
		{
			"Spanish", 3
		},
		{
			"Danish", 4
		},
		{
			"Swedish", 5
		},
		{
			"Italian", 6
		},
		{
			"DIAG Mode (English)", 7
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

static const device_config_t pc1640_config[] = {
    {
	"language", "ROS Language", CONFIG_SELECTION, "", 0,
	{
		{
			"English", 0
		},
		{
			"German", 1
		},
		{
			"French", 2
		},
		{
			"Spanish", 3
		},
		{
			"Danish", 4
		},
		{
			"Swedish", 5
		},
		{
			"Italian", 6
		},
		{
			"DIAG Mode (English)", 7
		},
		{
			NULL
		}
	}
    },
#if 1
    {
	"iga", "Internal Graphics Adapter", CONFIG_SELECTION, "", 1,
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
	"iga_mode", "Inititial Video Mode", CONFIG_SELECTION, "", 2,
	{
		{
			"Monochrome", 0
		},
		{
			"CO40", 1
		},
		{
			"CO80", 2
		},
		{
			"ECD200", 3
		},
		{
			"ECD350", 4
		},
		{
			NULL
		}
	}
    },
#endif
    {
	NULL
    }
};

static const device_config_t pc200_config[] = {
    {
	"codepage", "Hardware font", CONFIG_SELECTION, "", 3,
	{
		{
			"US English", 3
		},
		{
			"Portugese", 2
		},
		{
			"Norwegian", 1
		},
		{
			"Greek", 0
		},
		{
			NULL
		}
	}
    },
    {
	"language", "ROS Language", CONFIG_SELECTION, "", 0,
	{
		{
			"English", 0
		},
		{
			"German", 1
		},
		{
			"French", 2
		},
		{
			"Spanish", 3
		},
		{
			"Danish", 4
		},
		{
			"Swedish", 5
		},
		{
			"Italian", 6
		},
		{
			"DIAG Mode (English)", 7
		},
		{
			NULL
		}
	}
    },
    {
	"video_emulation", "Display type", CONFIG_SELECTION, "", IDA_CGA,
	{
		{
			"CGA monitor", IDA_CGA
		},
		{
			"MDA monitor", IDA_MDA
		},
		{
			"Television", IDA_TV
		},
		{
			NULL
		}
	}
    },
    {
	"floppy_swap", "Swap Diskette Drives", CONFIG_SELECTION, "", 0,
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
#if 0
    {
	"display_type", "Monitor type", CONFIG_SELECTION, "", DISPLAY_RGB,
	{
		{
			"RGB", DISPLAY_RGB
		},
		{
			"RGB (no brown)", DISPLAY_RGB_NO_BROWN
		},
		{
			"Monochrome (green)", DISPLAY_GREEN
		},
		{
			"Monochrome (amber)", DISPLAY_AMBER
		},
		{
			"Monochrome (white)", DISPLAY_WHITE
		},
		{
			NULL
		}
	}
    },
#endif
    {
	NULL
    }
};

static const device_config_t ppc_config[] = {
    {
	"codepage", "Hardware font", CONFIG_SELECTION, "", 3,
	{
		{
			"US English", 3
		},
		{
			"Portugese", 2
		},
		{
			"Norwegian", 1
		},
		{
			"Greek", 0
		},
		{
			NULL
		}
	}
    },
    {
	"language", "ROS Language", CONFIG_SELECTION, "", 0,
	{
		{
			"English", 0
		},
		{
			"German", 1
		},
		{
			"French", 2
		},
		{
			"Spanish", 3
		},
		{
			"Danish", 4
		},
		{
			"Swedish", 5
		},
		{
			"Italian", 6
		},
		{
			"DIAG Mode (English)", 7
		},
		{
			NULL
		}
	}
    },
    {
	"video_emulation", "Display type", CONFIG_SELECTION, "", IDA_LCD_C,
	{
		{
			"External monitor (CGA)", IDA_CGA,
		},
		{
			"External monitor (MDA)", IDA_MDA,
		},
		{
			"LCD (CGA mode)", IDA_LCD_C,
		},
		{
			"LCD (MDA mode)", IDA_LCD_M,
		},
		{
 			NULL
		}
	}
    },
#if 0
    {
	"display_type", "External monitor", CONFIG_SELECTION, "", DISPLAY_RGB,
	{
		{
			"RGB", DISPLAY_RGB
		},
		{
			"RGB (no brown)", DISPLAY_RGB_NO_BROWN
		},
		{
			"Monochrome (green)", DISPLAY_GREEN
		},
		{
			"Monochrome (amber)", DISPLAY_AMBER
		},
		{
			"Monochrome (white)", DISPLAY_WHITE
		},
		{
			NULL
		}
	}
    },
#endif
    {
	NULL
    }
};


static const CPU cpus_pc1512[] = {
    { "8086/8", CPU_8086, 8000000, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1 },
    { NULL }
};

static const machine_t pc1512_info = {
    MACHINE_ISA | MACHINE_NONMI | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    512, 640, 32, 64, -1,
    {{"Intel",cpus_pc1512}}
};

const device_t m_amstrad_1512 = {
    "Amstrad PC1512",
    DEVICE_ROOT,
    0,
    L"amstrad/pc1512",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc1512_info,
    pc1512_config
};


static const machine_t pc1640_info = {
    MACHINE_ISA | MACHINE_NONMI | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    640, 640, 0, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_1640 = {
    "Amstrad PC1640",
    DEVICE_ROOT,
    1,
    L"amstrad/pc1640",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc1640_info,
    pc1640_config
};


static const machine_t pc200_info = {
    MACHINE_ISA | MACHINE_NONMI | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    512, 640, 32, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_200 = {
    "Amstrad PC200",
    DEVICE_ROOT,
    2,
    L"amstrad/pc200",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc200_info,
    pc200_config
};


static const machine_t ppc_info = {
    MACHINE_ISA | MACHINE_NONMI | MACHINE_VIDEO,
    0,
    512, 640, 32, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_ppc = {
    "Amstrad PPC512/640",
    DEVICE_ROOT,
    3,
    L"amstrad/ppc512",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &ppc_info,
    ppc_config
};


static const machine_t pc2086_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    640, 640, 0, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_2086 = {
    "Amstrad 2086",
    DEVICE_ROOT,
    4,
    L"amstrad/pc2086",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc2086_info,
    NULL
};


static const machine_t pc3086_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_MOUSE,
    MACHINE_VIDEO,
    640, 640, 0, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_3086 = {
    "Amstrad 3086",
    DEVICE_ROOT,
    5,
    L"amstrad/pc3086",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc3086_info,
    NULL
};


static const machine_t pc5086_info = {
    MACHINE_ISA | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,
    0,
    640, 640, 0, 64, -1,
    {{"Intel",cpus_8086}}
};

const device_t m_amstrad_5086 = {
    "Amstrad 5086",
    DEVICE_ROOT,
    7,
    L"amstrad/pc5086",
    amstrad_init, amstrad_close, NULL,
    NULL, NULL, NULL,
    &pc5086_info,
    NULL
};
