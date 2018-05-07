/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the IDE module.
 *
 * Version:	@(#)hdc_ide.h	1.0.8	2018/04/23
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
#ifndef EMU_IDE_H
# define EMU_IDE_H


typedef struct {
    int8_t	type;
    int8_t	board;
    uint8_t	atastat;
    uint8_t	error;
    int		secount,
		sector,
		cylinder,
		head,
		drive,
		cylprecomp;
    uint8_t	command;
    uint8_t	fdisk;
    int		pos;
    int		packlen;
    uint8_t	spt,
		hpc;
    int		t_spt,
		t_hpc;
    int		tracks;
    int		packetstatus;
    uint8_t	asc;
    int		reset;
    uint16_t	*buffer;
    int		irqstat;
    int		service;
    int		lba;
    int		channel;
    uint32_t	lba_addr;
    int		skip512;
    int		blocksize,
		blockcount;
    uint16_t	dma_identify_data[3];
    int		hdi,
		base;
    int		hdd_num;
    uint8_t	specify_success;
    int		mdma_mode;
    uint8_t	*sector_buffer;
    int		do_initial_read;
    int		sector_pos;
} IDE;


extern int	ideboard;
extern int	ide_enable[5];
extern int	ide_irq[5];
extern IDE	ide_drives[IDE_NUM];
extern int64_t	idecallback[5];


extern void	ide_irq_raise(IDE *ide);
extern void	ide_irq_lower(IDE *ide);

extern void	writeide(int ide_board, uint16_t addr, uint8_t val);
extern void	writeidew(int ide_board, uint16_t val);
extern uint8_t	readide(int ide_board, uint16_t addr);
extern uint16_t	readidew(int ide_board);
extern void	callbackide(int ide_board);

extern void	ide_set_bus_master(int (*read)(int channel, uint8_t *data, int transfer_length), int (*write)(int channel, uint8_t *data, int transfer_length), void (*set_irq)(int channel));

extern void	win_cdrom_eject(uint8_t id);
extern void	win_cdrom_reload(uint8_t id);

extern void	ide_set_base(int controller, uint16_t port);
extern void	ide_set_side(int controller, uint16_t port);

extern void	ide_init_first(void);

extern void	ide_reset(void);
extern void	ide_reset_hard(void);

extern void	ide_set_all_signatures(void);

extern void	ide_xtide_init(void);

extern void	ide_pri_enable(void);
extern void	ide_pri_enable_ex(void);
extern void	ide_pri_disable(void);
extern void	ide_sec_enable(void);
extern void	ide_sec_disable(void);
extern void	ide_ter_enable(void);
extern void	ide_ter_disable(void);
extern void	ide_ter_init(void);
extern void	ide_qua_enable(void);
extern void	ide_qua_disable(void);
extern void	ide_qua_init(void);

extern void	ide_set_callback(uint8_t channel, int64_t callback);
extern void	secondary_ide_check(void);

extern void	ide_padstr8(uint8_t *buf, int buf_size, const char *src);
extern void	ide_destroy_buffers(void);

extern int	(*ide_bus_master_read)(int channel, uint8_t *data, int transfer_length);
extern int	(*ide_bus_master_write)(int channel, uint8_t *data, int transfer_length);
extern void	(*ide_bus_master_set_irq)(int channel);


#endif	/*EMU_IDE_H*/
