/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define all known video cards.
 *
 *		To add a video card to the system, implement it, add an
 *		"extern" reference to its device into the video.h file,
 *		and add an entry for it into the table here.
 *
 * Version:	@(#)video_dev.c	1.0.22	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../../machines/machine.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"


static struct {
    const char		*internal_name;
    const device_t	*device;
} video_cards[] = {
    { "none",			NULL				},
    { "internal",		NULL				},

    /* Standard video controllers. */
    { "mda",			&mda_device			},
    { "cga",			&cga_device			},
    { "ega",			&ega_device			},
    { "vga",			&vga_device			},
    { "hercules",		&hercules_device		},

    { "mach64gx_isa",		&mach64gx_isa_device		},
    { "ati28800k",		&ati28800k_device		},
    { "ati18800v",		&ati18800_vga88_device		},
    { "ati28800",		&ati28800_device		},
    { "ati18800",		&ati18800_device		},
#if defined(DEV_BRANCH) && defined(USE_WONDER)
    { "ati18800w",		&ati18800_wonder_device		},
#endif
#if defined(DEV_BRANCH) && defined(USE_XL24)
    { "ati28800w",		&ati28800_wonderxl24_device	},
#endif
    { "superega",		&sega_device			},
    { "cl_gd5428_isa",		&gd5428_isa_device		},
    { "cl_gd5429_isa",		&gd5429_isa_device		},
    { "cl_gd5434_isa",		&gd5434_isa_device		},
    { "compaq_ati28800",	&ati28800_compaq_device		},
    { "compaq_cga",		&cga_compaq_device		},
    { "compaq_cga_2",		&cga_compaq2_device		},
    { "compaq_ega",		&ega_compaq_device		},
    { "hercules_plus",		&herculesplus_device		},
    { "incolor",		&incolor_device			},
    { "genius",			&genius_device			},
    { "oti037c",		&oti037c_device			},
    { "oti067",			&oti067_device			},
    { "oti077",			&oti077_device			},
    { "pvga1a",			&paradise_pvga1a_device		},
    { "wd90c11",		&paradise_wd90c11_device	},
    { "wd90c30",		&paradise_wd90c30_device	},
    { "plantronics",		&colorplus_device		},
#if defined(DEV_BRANCH) && defined(USE_TI)
    { "ti_cf62011",		&ti_cf62011_device		},
#endif
    { "tvga8900d",		&tvga8900d_device		},
    { "et4000ax",		&et4000_isa_device		},
    { "tgkorvga",		&et4000k_isa_device		},
    { "wy700",			&wy700_device			},

    { "et4000ax_mca",		&et4000_mca_device		},

    { "mach64gx_pci",		&mach64gx_pci_device		},
    { "mach64vt2",		&mach64vt2_device		},
    { "et4000w32p_pci",		&et4000w32p_cardex_pci_device	},
    { "cl_gd5430_pci",		&gd5430_pci_device		},
    { "cl_gd5434_pci",		&gd5434_pci_device		},
    { "cl_gd5436_pci",		&gd5436_pci_device		},
    { "cl_gd5440_pci",		&gd5440_pci_device		},
    { "cl_gd5446_pci",		&gd5446_pci_device		},
    { "cl_gd5480_pci",		&gd5480_pci_device		},
    { "stealth32_pci",		&et4000w32p_pci_device		},
    { "stealth3d_2000_pci",	&s3_virge_pci_device		},
    { "stealth3d_3000_pci",	&s3_virge_988_pci_device	},
    { "stealth64d_pci",		&s3_diamond_stealth64_pci_device},
    { "stealth64v_pci",		&s3_diamond_stealth64_964_pci_device},
    { "n9_9fx_pci",		&s3_9fx_pci_device		},
    { "bahamas64_pci",		&s3_bahamas64_pci_device	},
    { "px_vision864_pci",	&s3_phoenix_vision864_pci_device},
    { "px_trio32_pci",		&s3_phoenix_trio32_pci_device	},
    { "px_trio64_pci",		&s3_phoenix_trio64_pci_device	},
    { "virge375_pci",		&s3_virge_375_pci_device	},
    { "virge375_vbe20_pci",	&s3_virge_375_4_pci_device	},
    { "cl_gd5446_stb_pci",	&gd5446_stb_pci_device		},
    { "tgui9440_pci",		&tgui9440_pci_device		},

    { "mach64gx_vlb",		&mach64gx_vlb_device		},
    { "et4000w32p_vlb",		&et4000w32p_cardex_vlb_device	},
    { "cl_gd5429_vlb",		&gd5429_vlb_device		},
    { "cl_gd5434_vlb",		&gd5434_vlb_device		},
    { "stealth32_vlb",		&et4000w32p_vlb_device		},
    { "cl_gd5426_vlb",		&gd5426_vlb_device		},
    { "cl_gd5430_vlb",		&gd5430_vlb_device		},
    { "stealth3d_2000_vlb",	&s3_virge_vlb_device		},
    { "stealth3d_3000_vlb",	&s3_virge_988_vlb_device	},
    { "stealth64d_vlb",		&s3_diamond_stealth64_vlb_device},
    { "stealth64v_vlb",		&s3_diamond_stealth64_964_vlb_device},
    { "n9_9fx_vlb",		&s3_9fx_vlb_device		},
    { "bahamas64_vlb",		&s3_bahamas64_vlb_device	},
    { "px_vision864_vlb",	&s3_phoenix_vision864_vlb_device},
    { "px_trio32_vlb",		&s3_phoenix_trio32_vlb_device	},
    { "px_trio64_vlb",		&s3_phoenix_trio64_vlb_device	},
    { "virge375_vlb",		&s3_virge_375_vlb_device	},
    { "virge375_vbe20_vlb",	&s3_virge_375_4_vlb_device	},
    { "tgui9400cxi_vlb",	&tgui9400cxi_device		},
    { "tgui9440_vlb",		&tgui9440_vlb_device		},

    { NULL,			NULL				}
};


void
video_reset(void)
{
    INFO("VIDEO: reset (video_card=%d, internal=%d)\n",
       	 video_card, (machines[machine].flags & MACHINE_VIDEO) ? 1 : 0);

    /* Reset the CGA palette. */
    cga_palette = 0;
    cgapal_rebuild();

    /*
     * Clear (deallocate) any video font memory.
     *
     * We have to do this before the machine is reset, as
     * that *might* allocate it if that uses an internal
     * controller using extended font memory..
     */
    video_reset_font();

    /* Initialize the video font tables. */
    video_load_font(L"video/ibm/mda/mda.rom", 0);

    /* Do not initialize internal cards here. */
    if ((video_card == VID_NONE) || \
	(video_card == VID_INTERNAL) || \
	machines[machine].fixed_vidcard) return;

    /* Configure default timing parameters for the card. */
    video_inform(VID_TYPE_SPEC, NULL);

    device_add(video_cards[video_card].device);

    /* Enable the Voodoo if configured. */
    if (voodoo_enabled)
       	device_add(&voodoo_device);
}


int
video_card_available(int card)
{
    if (video_cards[card].device != NULL)
	return(device_available(video_cards[card].device));

    return(1);
}


const char *
video_card_getname(int card)
{
    if (video_cards[card].device != NULL)
	return(video_cards[card].device->name);

    return(NULL);
}


const device_t *
video_card_getdevice(int card)
{
    return(video_cards[card].device);
}


int
video_card_has_config(int card)
{
    if (video_cards[card].device != NULL)
	return(video_cards[card].device->config ? 1 : 0);

    return(0);
}


const char *
video_get_internal_name(int card)
{
    return(video_cards[card].internal_name);
}


int
video_get_video_from_internal_name(const char *s)
{
    int c = 0;

    while (video_cards[c].internal_name != NULL) {
	if (! strcmp(video_cards[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(-1);
}
