/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the mouse driver.
 *
 * Version:	@(#)mouse.h	1.0.8	2018/09/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#ifndef EMU_MOUSE_H
# define EMU_MOUSE_H


#define MOUSE_NONE		0	/* no mouse configured */
#define MOUSE_INTERNAL		1	/* machine has internal mouse */
#define MOUSE_LOGIBUS		2	/* Logitech/ATI Bus Mouse */
#define MOUSE_INPORT		3	/* Microsoft InPort Mouse */
#if 0
# define MOUSE_GENIBUS		4	/* Genius Bus Mouse */
# define MOUSE_NEXT		5
#else
# define MOUSE_NEXT		4
#endif
#define MOUSE_MSYSTEMS		MOUSE_NEXT+0	/* Mouse Systems mouse */
#define MOUSE_MICROSOFT		MOUSE_NEXT+1	/* Microsoft Serial Mouse */
#define MOUSE_LOGITECH		MOUSE_NEXT+2	/* Logitech Serial Mouse */
#define MOUSE_MSWHEEL		MOUSE_NEXT+3	/* Serial Wheel Mouse */
#define MOUSE_PS2		MOUSE_NEXT+4	/* PS/2 series Bus Mouse */


#ifdef __cplusplus
extern "C" {
#endif

extern int	mouse_x, mouse_y, mouse_z;
extern int	mouse_buttons;


#ifdef EMU_DEVICE_H
extern const device_t	*mouse_get_device(int mouse);
extern void		*mouse_ps2_init(const device_t *);

extern const device_t	mouse_logibus_device;
extern const device_t	mouse_logibus_internal_device;
extern const device_t	mouse_msinport_device;
#if 0
extern const device_t	mouse_genibus_device;
#endif
extern const device_t	mouse_mssystems_device;
extern const device_t	mouse_msserial_device;
extern const device_t	mouse_logiserial_device;
extern const device_t	mouse_mswhserial_device;
extern const device_t	mouse_ps2_device;
#endif

extern void	mouse_log(int level, const char *fmt, ...);
extern void	mouse_init(void);
extern void	mouse_close(void);
extern void	mouse_reset(void);
extern void	mouse_set_buttons(int buttons);
extern void	mouse_process(void);
extern void	mouse_set_poll(int (*f)(int,int,int,int,void *), void *);
extern void	mouse_poll(void);

extern const char *mouse_get_name(int mouse);
extern const char *mouse_get_internal_name(int mouse);
extern int	mouse_get_from_internal_name(const char *s);
extern int	mouse_has_config(int mouse);
extern int	mouse_get_type(int mouse);
extern int	mouse_get_ndev(void);
extern int	mouse_get_buttons(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_MOUSE_H*/
