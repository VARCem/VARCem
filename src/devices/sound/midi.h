/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the MIDI module.
 *
 * Version:	@(#)midi.h	1.0.6	2018/09/15
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
#ifndef EMU_SOUND_MIDI_H
# define EMU_SOUND_MIDI_H


#ifdef _WIN32
# define SYSTEM_MIDI_NAME	"Windows MIDI"
# define SYSTEM_MIDI_INT	"system_midi"
#else
# define SYSTEM_MIDI_NAME	"System MIDI"
# define SYSTEM_MIDI_INT	"system_midi"
#endif


typedef struct {
    void	(*play_sysex)(uint8_t *sysex, unsigned int len);
    void	(*play_msg)(uint8_t *msg);
    void	(*poll)(void);
    int		(*write)(uint8_t val);
} midi_device_t;


extern int	midi_device_available(int card);
extern const char *midi_device_getname(int card);
#ifdef EMU_DEVICE_H
extern const device_t *midi_device_getdevice(int card);
#endif
extern int	midi_device_has_config(int card);
extern const char *midi_device_get_internal_name(int card);
extern int	midi_device_get_from_internal_name(const char *s);
extern void	midi_device_init(void);

extern void	sound_midi_log(int level, const char *fmt, ...);
extern void	midi_init(const midi_device_t *device);
extern void	midi_close(void);
extern void	midi_write(uint8_t val);
extern void	midi_poll(void);


#endif	/*EMU_SOUND_MIDI_H*/
