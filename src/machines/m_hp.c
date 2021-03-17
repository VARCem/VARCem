/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various HP machines.
 *
 * Version:	@(#)m_hp.c	1.0.1	2021/03/16
 *
 * Author:	Altheos, <altheos@varcem.com>
 *
 *		Copyright 2021 Altheos.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/vl82c480.h"
#include "../devices/system/memregs.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/sio/sio.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, UNUSED(void *arg))
{
    /* Allocate machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&vl82c480_device);

    m_at_common_init(); 

    device_add(&ide_vlb_device); /* This machine has only one IDE port (VLB ?) : no LBA & CD drive support */
    device_add(&fdc37c932fr_device);
    
    device_add(&keyboard_ps2_device); /* VLSI 82c113A SCAMP IO*/

    if (config.mouse_type == MOUSE_INTERNAL)
	device_add(&mouse_ps2_device);
    
    if (config.video_card == VID_INTERNAL)
	device_add(&gd5426_onboard_vlb_device);	/* 5426-80QC-B */

    return(arg);
}


static const machine_t hpv486_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE,
    0,
    1, 32, 1, 128, -1,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}} /* 5V only */
};

const device_t m_hpv486 = {
    "HP Vectra 486VL series",
    DEVICE_ROOT,
    0,
    L"hp/vectra486",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &hpv486_info,
    NULL
};
