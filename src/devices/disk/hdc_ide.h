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
 * Version:	@(#)hdc_ide.h	1.0.11	2018/10/14
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


enum {
    IDE_NONE = 0,
    IDE_HDD,
    IDE_ATAPI
};

/* Type:
 *  0 = PIO,
 *  1 = SDMA,
 *  2 = MDMA,
 *  3 = UDMA
 * Return:
 *  -1 = Not supported,
 *  Anything else = maximum mode
 *
 * This will eventually be hookable.
*/
enum {
    TYPE_PIO = 0,
    TYPE_SDMA,
    TYPE_MDMA,
    TYPE_UDMA
};

/* Return:
 *  0 = Not supported,
 *  Anything else = timings
 *
 * This will eventually be hookable.
 */
enum {
    TIMINGS_DMA = 0,
    TIMINGS_PIO,
    TIMINGS_PIO_FC
};


typedef struct {
    uint8_t atastat, error,
	    command, fdisk;

    int type, board,
	irqstat, service,
	blocksize, blockcount,
	hdd_num, channel,
	pos, sector_pos,
	lba, skip512,
	reset, mdma_mode,
	do_initial_read;

    uint32_t secount, sector,
	     cylinder, head,
	     drive, cylprecomp,
	     cfg_spt, cfg_hpc,
	     lba_addr, tracks,
	     spt, hpc;

    uint16_t *buffer;

    uint8_t *sector_buffer;

    /* Stuff mostly used by ATAPI */
    void	*p;
    int		interrupt_drq;

    int		(*get_max)(int ide_has_dma, int type);
    int		(*get_timings)(int ide_has_dma, int type);
    void	(*identify)(void *p, int ide_has_dma);
    void	(*set_signature)(void *p);
    void	(*packet_write)(void *p, uint32_t val, int length);
    uint32_t	(*packet_read)(void *p, int length);
    void	(*stop)(void *p);
    void	(*packet_callback)(void *p);
    void	(*device_reset)(void *p);
} ide_t;


extern int	ideboard;
extern int	ide_ter_enabled, ide_qua_enabled;
extern ide_t	*ide_drives[IDE_NUM+XTIDE_NUM];
extern int64_t	idecallback[5];


extern void	ide_irq_raise(ide_t *);
extern void	ide_irq_lower(ide_t *);

extern void	*ide_xtide_init(void);
extern void	ide_xtide_close(void);

extern void	ide_writew(uint16_t addr, uint16_t val, void *priv);
extern void	ide_write_devctl(uint16_t addr, uint8_t val, void *priv);
extern void	ide_writeb(uint16_t addr, uint8_t val, void *priv);
extern uint8_t	ide_readb(uint16_t addr, void *priv);
extern uint8_t	ide_read_alt_status(uint16_t addr, void *priv);
extern uint16_t	ide_readw(uint16_t addr, void *priv);
extern void	ide_set_bus_master(int (*read)(int channel, uint8_t *data, int transfer_length, void *priv),
				   int (*write)(int channel, uint8_t *data, int transfer_length, void *priv),
				   void (*set_irq)(int channel, void *priv),
				   void *priv0, void *priv1);


extern void	win_cdrom_eject(uint8_t id);
extern void	win_cdrom_reload(uint8_t id);

extern void	ide_set_base(int controller, uint16_t port);
extern void	ide_set_side(int controller, uint16_t port);

extern void	ide_pri_enable(void);
extern void	ide_pri_disable(void);
extern void	ide_sec_enable(void);
extern void	ide_sec_disable(void);

extern void	ide_set_callback(uint8_t channel, int64_t callback);
extern void	secondary_ide_check(void);


extern void	ide_padstr(char *str, const char *src, int len);
extern void	ide_padstr8(uint8_t *buf, int buf_size, const char *src);

extern int	(*ide_bus_master_read)(int channel, uint8_t *data, int transfer_length, void *priv);
extern int	(*ide_bus_master_write)(int channel, uint8_t *data, int transfer_length, void *priv);
extern void	(*ide_bus_master_set_irq)(int channel, void *priv);
extern void	*ide_bus_master_priv[2];

extern void	ide_enable_pio_override(void);
extern void	ide_allocate_buffer(ide_t *dev);
extern void	ide_atapi_attach(ide_t *dev);


#endif	/*EMU_IDE_H*/
