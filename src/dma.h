/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Intel DMA controller.
 *
 * Version:	@(#)dma.h	1.0.1	2018/02/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#ifndef EMU_DMA_H
# define EMU_DMA_H


#define DMA_NODATA	-1
#define DMA_OVER	0x10000
#define DMA_VERIFY	0x20000


typedef struct DMA {
    uint32_t	ab[4],
		ac[4];
    uint16_t	cb[4];
    int		cc[4];
    int		wp;
    uint8_t	m,
		mode[4];
    uint8_t	page[4];
    uint8_t	stat,
		stat_rq;
    uint8_t	command;
    uint8_t	request;

    int		xfr_command,
		xfr_channel;
    int		byte_ptr;

    int		is_ps2;
    uint8_t	arb_level[4];
    uint8_t	ps2_mode[4];
} DMA;


extern DMA	dma, dma16;


extern void	dma_init(void);
extern void	dma16_init(void);
extern void	ps2_dma_init(void);
extern void	dma_reset(void);
extern int	dma_mode(int channel);

extern void	readdma0(void);
extern int	readdma1(void);
extern uint8_t	readdma2(void);
extern int	readdma3(void);

extern void	writedma2(uint8_t temp);

extern int	dma_channel_read(int channel);
extern int	dma_channel_write(int channel, uint16_t val);

extern void	dma_alias_set(void);
extern void	dma_alias_remove(void);
extern void	dma_alias_remove_piix(void);

extern void	DMAPageRead(uint32_t PhysAddress, uint8_t *DataRead,
			    uint32_t TotalSize);
extern void	DMAPageWrite(uint32_t PhysAddress, const uint8_t *DataWrite,
			     uint32_t TotalSize);


#endif	/*EMU_DMA_H*/
