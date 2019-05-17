/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of C&T CS8121 ("NEAT") based machines.
 *
 * Version:	@(#)m_neat.c	1.0.6	2019/05/13
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 * 		or  without modification, are permitted  provided that the
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
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/chipsets/neat.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&neat_device);

    switch(info->local) {
	case 1:		/* Generic AT with AMI BIOS and KBC */
		m_at_common_init();
		device_add(&keyboard_at_ami_device);
		break;

	case 2:		/* DTK-386 using NEAT */
		m_at_init();
		break;
    }

    device_add(&fdc_at_device);

    return((priv_t)arg);
}


static const machine_t ami_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 8192, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_neat_ami = {
    "AMI 286 (NEAT)",
    DEVICE_ROOT,
    1,
    L"generic/at/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &ami_info,
    NULL
};


static const machine_t dtk_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    512, 8192, 128, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_neat_dtk = {
    "DTK-386 (NEAT)",
    DEVICE_ROOT,
    2,
    L"dtk/386",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &dtk_info,
    NULL
};
