#ifndef SLIRP_ARP_H
# define SLIRP_ARP_H


#define ETH_ALEN	6
#define ETH_HLEN	14

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/


#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/


struct ethhdr {
    unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
    unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
    unsigned short	h_proto;		/* packet type ID field	*/
};

struct arphdr {
    unsigned short	ar_hrd;		/* format of hardware address	*/
    unsigned short	ar_pro;		/* format of protocol address	*/
    unsigned char	ar_hln;		/* length of hardware address	*/
    unsigned char	ar_pln;		/* length of protocol address	*/
    unsigned short	ar_op;		/* ARP opcode (command)		*/

    /*
     *	 Ethernet looks like this : This bit is variable sized however...
     */
    unsigned char	ar_sha[ETH_ALEN];	/* sender hardware address */
    unsigned char	ar_sip[4];		/* sender IP address	   */
    unsigned char	ar_tha[ETH_ALEN];	/* target hardware address */
    unsigned char	ar_tip[4];		/* target IP address	   */
};


extern void	arp_input(const uint8_t *pkt, int pkt_len);


#endif	/*SLIRP_ARP_H*/
