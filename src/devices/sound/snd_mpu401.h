/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Roland MPU-401 emulation.
 *
 * Version:	@(#)snd_mpu401.h	1.0.5	2018/09/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		DOSBox Team,
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *		Copyright 2008-2017 DOSBox Team.
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
#ifndef SOUND_MPU401_H
# define SOUND_MPU401_H


#define MPU401_VERSION		0x15
#define MPU401_REVISION		0x01
#define MPU401_QUEUE		32
#define MPU401_TIMECONSTANT	(60000000/1000.0f)
#define MPU401_RESETBUSY	27.0f

typedef enum {
    M_UART,
    M_INTELLIGENT
} MpuMode;
#define M_MCA	0x10

typedef enum {
    T_OVERFLOW,
    T_MARK,
    T_MIDI_SYS,
    T_MIDI_NORM,
    T_COMMAND
} MpuDataType;


/* Messages sent to MPU-401 from host */
#define MSG_EOX			0xf7
#define MSG_OVERFLOW		0xf8
#define MSG_MARK		0xfc

/* Messages sent to host from MPU-401 */
#define MSG_MPU_OVERFLOW	0xf8
#define MSG_MPU_COMMAND_REQ	0xf9
#define MSG_MPU_END		0xfc
#define MSG_MPU_CLOCK		0xfd
#define MSG_MPU_ACK		0xfe


typedef struct {
    int		uart_mode, intelligent,
		irq,
		queue_pos, queue_used;

    uint8_t	rx_data, is_mca,
	    	status,
	    	queue[MPU401_QUEUE], pos_regs[8];
    		MpuMode mode;

    struct track {
	int counter;
	uint8_t value[8], sys_val,
		vlength,length;
	MpuDataType type;
    }		playbuf[8], condbuf;

    struct {
	int	conductor, cond_req,
	    	cond_set, block_ack,
	    	playing, reset,
	    	wsd, wsm, wsd_start,
	    	run_irq, irq_pending,
	    	send_now, eoi_scheduled,
	    	data_onoff;
		uint8_t tmask, cmask,
		amask,
		channel, old_chan;
		uint16_t midi_mask, req_mask;
		uint32_t command_byte, cmd_pending;
    }		state;

    struct {
	uint8_t timebase, old_timebase,
		tempo, old_tempo,
		tempo_rel, old_tempo_rel,
		tempo_grad,
		cth_rate, cth_counter;
		int clock_to_host,cth_active;
    }		clock;
} mpu_t;


extern const device_t	mpu401_device;
extern const device_t	mpu401_mca_device;


extern uint8_t	MPU401_ReadData(mpu_t *mpu);
extern void	mpu401_init(mpu_t *mpu, uint16_t addr, int irq, int mode);
extern void	mpu401_device_add(void);
extern void	mpu401_uart_init(mpu_t *mpu, uint16_t addr);


#endif	/*SOUND_MPU401_H*/
