/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the keyboard interface.
 *
 * Version:	@(#)keyboard.h	1.0.5	2018/04/01
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
#ifndef EMU_KEYBOARD_H
# define EMU_KEYBOARD_H


typedef struct {
    int	mk[9];
    int	brk[9];
} scancode;


#define STATE_SHIFT_MASK	0x22
#define STATE_RSHIFT		0x20
#define STATE_LSHIFT		0x02

#define FAKE_LSHIFT_ON		0x100
#define FAKE_LSHIFT_OFF		0x101
#define LSHIFT_ON		0x102
#define LSHIFT_OFF		0x103
#define RSHIFT_ON		0x104
#define RSHIFT_OFF		0x105


#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t	keyboard_mode;
extern int	keyboard_scan;
extern int64_t	keyboard_delay;

extern void	(*keyboard_send)(uint16_t val);
extern void	kbd_adddata_process(uint16_t val, void (*adddata)(uint16_t val));

extern const scancode scancode_xt[512];

extern uint8_t	keyboard_set3_flags[512];
extern uint8_t	keyboard_set3_all_repeat;
extern uint8_t	keyboard_set3_all_break;
extern int	mouse_queue_start, mouse_queue_end;
extern int	mouse_scan;

#ifdef EMU_DEVICE_H
extern const device_t	keyboard_xt_device;
extern const device_t	keyboard_tandy_device;
extern const device_t	keyboard_at_device;
extern const device_t	keyboard_at_ami_device;
extern const device_t	keyboard_at_toshiba_device;
extern const device_t	keyboard_ps2_device;
extern const device_t	keyboard_ps2_ami_device;
extern const device_t	keyboard_ps2_mca_device;
extern const device_t	keyboard_ps2_mca_2_device;
extern const device_t	keyboard_ps2_quadtel_device;
#endif

extern void	kbd_log(const char *fmt, ...);
extern void	keyboard_init(void);
extern void	keyboard_close(void);
extern void	keyboard_set_table(const scancode *ptr);
extern void	keyboard_poll_host(void);
extern void	keyboard_process(void);
extern uint16_t	keyboard_convert(int ch);
extern void	keyboard_input(int down, uint16_t scan);
extern void	keyboard_update_states(uint8_t cl, uint8_t nl, uint8_t sl);
extern uint8_t	keyboard_get_shift(void);
extern void	keyboard_get_states(uint8_t *cl, uint8_t *nl, uint8_t *sl);
extern void	keyboard_set_states(uint8_t cl, uint8_t nl, uint8_t sl);
extern int	keyboard_recv(uint16_t key);
extern void	keyboard_send_scan(uint8_t val);
extern void	keyboard_send_cad(void);
extern void	keyboard_send_cae(void);
extern void	keyboard_send_cab(void);
extern int	keyboard_isfsexit(void);
extern int	keyboard_ismsexit(void);

extern void	keyboard_at_reset(void);
extern void	keyboard_at_adddata_keyboard_raw(uint8_t val);
extern void	keyboard_at_adddata_mouse(uint8_t val);
extern void	keyboard_at_set_mouse(void (*mouse_write)(uint8_t val,void *), void *);
extern uint8_t	keyboard_at_get_mouse_scan(void);
extern void	keyboard_at_set_mouse_scan(uint8_t val);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_KEYBOARD_H*/
