/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the network module.
 *
 * Version:	@(#)network.h	1.0.9	2019/05/02
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef EMU_NETWORK_H
# define EMU_NETWORK_H
# include <stdint.h>


enum {
    NET_NONE = 0,
    NET_SLIRP,
    NET_PCAP
#ifdef USE_VNS
    ,NET_VNS
#endif
};

enum {
    NET_CARD_NONE = 0,
    NET_CARD_INTERNAL
};


typedef void (*NETRXCB)(void *, uint8_t *bufp, int);

/* Define a host interface entry for a network provider. */
typedef struct {
    char		device[128];
    char		description[128];
} netdev_t;

/* Define a network provider. */
typedef struct {
    const char		*name;

    int			(*init)(netdev_t *);
    void		(*close)(void);
    int			(*reset)(uint8_t *);
    int			(*available)(void);
    void		(*send)(uint8_t *, int);
} network_t;


#ifdef __cplusplus
extern "C" {
#endif

/* Global variables. */
extern int      network_host_ndev;
extern netdev_t network_host_devs[32];

extern const network_t	network_slirp;
extern const network_t	network_pcap;
#ifdef USE_VNS
extern const network_t	network_vns;
#endif


/* Function prototypes. */
extern int		network_get_from_internal_name(const char *s);
extern const char	*network_get_internal_name(int net);
extern const char	*network_getname(int net);
extern int		network_available(int net);

extern void		network_log(int level, const char *fmt, ...);

extern void		network_init(void);
extern void		network_close(void);
extern void		network_reset(void);
extern int		network_attach(void *, uint8_t *, NETRXCB);
extern void		network_tx(uint8_t *, int);
extern void		network_rx(uint8_t *, int);

extern void		network_wait(int8_t do_wait);
extern void		network_poll(void);
extern void		network_busy(int8_t set);
extern void		network_end(void);

extern void		network_card_log(int level, const char *fmt, ...);
extern int		network_card_to_id(const char *);
extern int		network_card_available(int);
extern const char	*network_card_getname(int);
extern int		network_card_has_config(int);
extern const char	*network_card_get_internal_name(int);
extern int		network_card_get_from_internal_name(const char *);
extern const device_t	*network_card_getdevice(int);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_NETWORK_H*/
