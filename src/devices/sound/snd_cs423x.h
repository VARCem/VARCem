/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Crystal CS423x driver.
 *
 * Version:	@(#)snd_cs423x.h	1.0.1	2019/01/13
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018,2019 Altheos.
 *		Copyright 2018,2019 Fred N. van Kempen.
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
#ifndef SND_CS423X_H
# define SND_CS423X_H


typedef struct {
    int		indx;
    uint8_t	regs[32];
    uint8_t	status;

    int		trd;
    int		mce;
    int		ia4;
    int		mode2;
    int		initb;

    int		count;

    int16_t	out_l, out_r;

    int64_t	enable;

    int		irq, dma;

    int64_t	freq;

    int64_t	timer_count,
		timer_latch;

    int16_t	buffer[SOUNDBUFLEN * 2];
    int		pos;
} cs423x_t;


extern void	cs423x_setirq(cs423x_t *, int irq);
extern void	cs423x_setdma(cs423x_t *, int dma);

extern uint8_t	cs423x_read(uint16_t addr, void *priv);
extern void	cs423x_write(uint16_t addr, uint8_t val, void *priv);

extern void	cs423x_update(cs423x_t *);
extern void	cs423x_speed_changed(cs423x_t *);

extern void	cs423x_init(cs423x_t *);


#endif	/*SND_CS423X_H*/
