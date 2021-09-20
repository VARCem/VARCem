/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Header for the 3DFX Voodoo Graphics Texture
 *
 * Version:	@(#)vid_voodoo_texture.h	1.0.1	2021/09/14
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

static const uint32_t texture_offset[LOD_MAX+3] =
{
        0,
        256*256,
        256*256 + 128*128,
        256*256 + 128*128 + 64*64,
        256*256 + 128*128 + 64*64 + 32*32,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2 + 1*1,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2 + 1*1 + 1
};

void voodoo_recalc_tex(voodoo_t *voodoo, int tmu);
void use_texture(voodoo_t *voodoo, voodoo_params_t *params, int tmu);
void voodoo_tex_writel(uint32_t addr, uint32_t val, void *priv);
