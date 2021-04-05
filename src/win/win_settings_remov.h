/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the "Removable Devices" dialog.
 *
 * Version:	@(#)win_settings_remov.h	1.0.14	2019/05/03
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


/************************************************************************
 *									*
 *			  Removable Devices Dialog			*
 *									*
 ************************************************************************/


static int
combo_to_string(int id)
{
    if (id == 0)
	return(IDS_DISABLED);

    return(IDS_3580 + id - 1);
}


static void
id_to_combo(HWND h, int num)
{
    WCHAR temp[16];
    int i;

    for (i = 0; i < num; i++) {
	swprintf(temp, sizeof_w(temp), L"%i", i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}