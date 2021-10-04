/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Header for the 3DFX Voodoo Graphics FIFO
 *
 * Version:	@(#)vid_voodoo_fifo.h	1.0.1	2021/09/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2021 Fred N. van Kempen.
 *		Copyright 2020 Sarah Walker.
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
#ifndef VIDEO_VOODOO_FIFO_H
# define VIDEO_VOODOO_FIFO_H

void voodoo_wake_fifo_thread(voodoo_t *voodoo);
void voodoo_wake_fifo_thread_now(voodoo_t *voodoo);
void voodoo_wake_timer(void *priv);
void voodoo_queue_command(voodoo_t *voodoo, uint32_t addr_type, uint32_t val);
void voodoo_flush(voodoo_t *voodoo);
void voodoo_wake_fifo_threads(voodoo_set_t *set, voodoo_t *voodoo);
void voodoo_wait_for_swap_complete(voodoo_t *voodoo);
void voodoo_fifo_thread(void *param);

#endif