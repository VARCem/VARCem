/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Windows raw keyboard input handler.
 *
 * Version:	@(#)win_keyboard.c	1.0.9	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define  _WIN32_WINNT 0x0501
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../emu.h"
#include "../config.h"
#include "../device.h"
#include "../plat.h"
#include "../devices/input/keyboard.h"
#include "win.h"


static uint16_t	scancode_map[768];


/*
 * This is so we can disambiguate scan codes that would otherwise
 * conflict and get passed on incorrectly.
 */
static UINT16
convert_scan_code(UINT16 code)
{
    if ((code & 0xFF00) == 0xE000) {
	code &= 0x00FF;
	code |= 0x0100;
    } else if (code == 0xE11D)
	code = 0xE000;
    /* E0 00 is sent by some USB keyboards for their special keys, as it is an
       invalid scan code (it has no untranslated set 2 equivalent), we mark it
       appropriately so it does not get passed through. */
    else if ((code > 0x00FF) || (code == 0xE000)) {
	code = 0xFFFF;
    }

    return code;
}


void
keyboard_getkeymap(void)
{
    WCHAR *keyName = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
    WCHAR *valueName = L"Scancode Map";
    unsigned char buf[32768];
    DWORD bufSize;
    HKEY hKey;
    int j;
    UINT32 *bufEx2;
    int scMapCount;
    UINT16 *bufEx;
    int scancode_unmapped;
    int scancode_mapped;

    /* First, prepare the default scan code map list which is 1:1.
     * Remappings will be inserted directly into it.
     * 512 bytes so this takes less memory, bit 9 set means E0
     * prefix.
     */
    for (j = 0; j < 512; j++)
	scancode_map[j] = j;

    /* Get the scan code remappings from:
    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layout */
    bufSize = 32768;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, 1, &hKey) == ERROR_SUCCESS) {
	if (RegQueryValueEx(hKey, valueName, NULL, NULL, buf, &bufSize) == ERROR_SUCCESS) {
		bufEx2 = (UINT32 *) buf;
		scMapCount = bufEx2[2];
		if ((bufSize != 0) && (scMapCount != 0)) {
			bufEx = (UINT16 *) (buf + 12);
			for (j = 0; j < scMapCount*2; j += 2) {
 				/* Each scan code is 32-bit: 16 bits of remapped scan code,
 				   and 16 bits of original scan code. */
  				scancode_unmapped = bufEx[j + 1];
  				scancode_mapped = bufEx[j];

				scancode_unmapped = convert_scan_code(scancode_unmapped);
				scancode_mapped = convert_scan_code(scancode_mapped);
				/* DEBUG("Scan code map found: %04X -> %04X\n", scancode_unmapped, scancode_mapped); */

				/* Ignore source scan codes with prefixes other than E1
				   that are not E1 1D. */
				if (scancode_unmapped != 0xFFFF)
					scancode_map[scancode_unmapped] = scancode_mapped;
			}
		}
	}
	RegCloseKey(hKey);
    }
}


void
keyboard_handle(LPARAM lParam, int focus)
{
    static int recv_lalt = 0, recv_ralt = 0, recv_tab = 0;
    uint32_t ri_size;
    RAWKEYBOARD rawKB;
    UINT size = 0;
    RAWINPUT *raw;
    USHORT code;

    if (! focus) return;

    /* See how much data the RI has for us, and allocate a buffer. */
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL,
		    &size, sizeof(RAWINPUTHEADER));
    raw = (RAWINPUT *)mem_alloc(size);
    if (raw == NULL) {
	ERRLOG("KBD: out of memory for Raw Input buffer!\n");
	return;
    }

    /* Read the event buffer from RI. */
    ri_size = GetRawInputData((HRAWINPUT)(lParam), RID_INPUT,
			      raw, &size, sizeof(RAWINPUTHEADER));
    if (ri_size != size) {
	ERRLOG("KBD: bad event buffer %d/%d\n", size, ri_size);
	return;
    }

    /* We only process keyboard events. */
    if (raw->header.dwType != RIM_TYPEKEYBOARD) {
	free(raw);
	return;
    }

    rawKB = raw->data.keyboard;
    code = rawKB.MakeCode;

    /* If it's not a scan code that starts with 0xE1 */
    if (!(rawKB.Flags & RI_KEY_E1)) {
	if (rawKB.Flags & RI_KEY_E0)
		code |= (0xE0 << 8);

	/* Translate the scan code to 9-bit */
	code = convert_scan_code(code);

	/* Remap it according to the list from the Registry */
	DBGLOG(1, "Scan code: %04X (map: %04X)\n", code, scancode_map[code]);
	code = scancode_map[code];

	/*
	 * If it's not 0xFFFF, send it to the emulated keyboard.
	 * We use scan code 0xFFFF to mean a mapping that
	 * has a prefix other than E0 and that is not E1 1D,
	 * which is, for our purposes, invalid.
	 */
	if ((code == 0x00F) && !(rawKB.Flags & RI_KEY_BREAK) &&
	    (recv_lalt || recv_ralt) && !mouse_capture) {
		/* We received a TAB while ALT was pressed, while the mouse
		   is not captured, suppress the TAB and send an ALT key up. */
		if (recv_lalt) {
			keyboard_input(0, 0x038);
			/* Extra key press and release so the guest is not stuck in the
			   menu bar. */
			keyboard_input(1, 0x038);
			keyboard_input(0, 0x038);
			recv_lalt = 0;
		}
		if (recv_ralt) {
			keyboard_input(0, 0x138);
			/* Extra key press and release so the guest is not stuck in the
			   menu bar. */
			keyboard_input(1, 0x138);
			keyboard_input(0, 0x138);
			recv_ralt = 0;
		}
	} else if (((code == 0x038) || (code == 0x138)) &&
		   !(rawKB.Flags & RI_KEY_BREAK) && recv_tab && !mouse_capture) {
		/* We received an ALT while TAB was pressed, while the mouse
		   is not captured, suppress the ALT and send a TAB key up. */
		keyboard_input(0, 0x00F);
		recv_tab = 0;
	} else {
		switch(code) {
			case 0x00F:
				recv_tab = !(rawKB.Flags & RI_KEY_BREAK);
				break;

			case 0x038:
				recv_lalt = !(rawKB.Flags & RI_KEY_BREAK);
				break;

			case 0x138:
				recv_ralt = !(rawKB.Flags & RI_KEY_BREAK);
				break;
		}

		/* Translate right CTRL to left ALT if the user has so
		   chosen. */
		if ((code == 0x11D) && config.rctrl_is_lalt)
			code = 0x038;

		/* Normal scan code pass through, pass it through as is if
		   it's not an invalid scan code. */
		if (code != 0xFFFF)
			keyboard_input(!(rawKB.Flags & RI_KEY_BREAK), code);
	}
    } else {
	if (rawKB.MakeCode == 0x1D) {
		/* Translate E1 1D to 0x100 (which would
		   otherwise be E0 00 but that is invalid
		   anyway).
		   Also, take a potential mapping into
		   account. */
		code = scancode_map[0x100];
	} else
		code = 0xFFFF;

	if (code != 0xFFFF)
		keyboard_input(!(rawKB.Flags & RI_KEY_BREAK), code);
    }

    free(raw);
}
