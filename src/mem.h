/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the memory interface.
 *
 * Version:	@(#)mem.h	1.0.19	2021/02/19
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef EMU_MEM_H
# define EMU_MEM_H


#define MEM_MAPPING_EXTERNAL	1	/* on external bus (ISA/PCI) */
#define MEM_MAPPING_INTERNAL	2	/* on internal bus (RAM) */
#define MEM_MAPPING_ROM		4	/* Executing from ROM may involve
					 * additional wait states. */

#define MEM_MAP_TO_SHADOW_RAM_MASK 1
#define MEM_MAP_TO_RAM_ADDR_MASK   2

#define MEM_READ_ANY		0x00
#define MEM_READ_INTERNAL	0x10
#define MEM_READ_EXTERNAL	0x20
#define MEM_READ_MASK		0xf0

#define MEM_WRITE_ANY		0x00
#define MEM_WRITE_INTERNAL	0x01
#define MEM_WRITE_EXTERNAL	0x02
#define MEM_WRITE_DISABLED	0x03
#define MEM_WRITE_MASK		0x0f

/*
 * Macros for memory granularity, currently 16K.
 * This may change in the future - 4K works, less
 * does not because of internal 4K pages.
 */
#if defined(DEFAULT_GRANULARITY) || !defined(MEM_GRANULARITY_BITS)
# define MEM_GRANULARITY_BITS	14
# define MEM_GRANULARITY_SIZE	(1 << MEM_GRANULARITY_BITS)
# define MEM_GRANULARITY_HBOUND	(MEM_GRANULARITY_SIZE - 2)
# define MEM_GRANULARITY_QBOUND	(MEM_GRANULARITY_SIZE - 4)
# define MEM_GRANULARITY_MASK	(MEM_GRANULARITY_SIZE - 1)
# define MEM_GRANULARITY_HMASK	((1 << (MEM_GRANULARITY_BITS - 1)) - 1)
# define MEM_GRANULARITY_QMASK	((1 << (MEM_GRANULARITY_BITS - 2)) - 1)
# define MEM_GRANULARITY_PMASK	((1 << (MEM_GRANULARITY_BITS - 3)) - 1)
# define MEM_MAPPINGS_NO	((0x100000 >> MEM_GRANULARITY_BITS) << 12)
# define MEM_GRANULARITY_PAGE	(MEM_GRANULARITY_MASK & ~0xfff)
#else
# define MEM_GRANULARITY_BITS	12
# define MEM_GRANULARITY_SIZE	(1 << MEM_GRANULARITY_BITS)
# define MEM_GRANULARITY_HBOUND	(MEM_GRANULARITY_SIZE - 2)
# define MEM_GRANULARITY_QBOUND	(MEM_GRANULARITY_SIZE - 4)
# define MEM_GRANULARITY_MASK	(MEM_GRANULARITY_SIZE - 1)
# define MEM_GRANULARITY_HMASK	((1 << (MEM_GRANULARITY_BITS - 1)) - 1)
# define MEM_GRANULARITY_QMASK	((1 << (MEM_GRANULARITY_BITS - 2)) - 1)
# define MEM_GRANULARITY_PMASK	((1 << (MEM_GRANULARITY_BITS - 3)) - 1)
# define MEM_MAPPINGS_NO	((0x100000 >> MEM_GRANULARITY_BITS) << 12)
# define MEM_GRANULARITY_PAGE	(MEM_GRANULARITY_MASK & ~0xfff)
#endif


typedef struct _memmap_ {
    struct _memmap_ *prev, *next;

    int		enable;

    uint32_t	base;
    uint32_t	size;

    uint8_t	(*read_b)(uint32_t addr, void *priv);
    uint16_t	(*read_w)(uint32_t addr, void *priv);
    uint32_t	(*read_l)(uint32_t addr, void *priv);
    void	(*write_b)(uint32_t addr, uint8_t  val, void *priv);
    void	(*write_w)(uint32_t addr, uint16_t val, void *priv);
    void	(*write_l)(uint32_t addr, uint32_t val, void *priv);

    uint8_t	*exec;

    uint32_t	flags;

    void	*p;		/* backpointer to mapping or device */
    void	*p2;		/* FIXME: temporary hack for Headland --FvK */
    void	*dev;		/* backpointer to memory device */
} mem_map_t;

typedef struct _page_ {
    void	(*write_b)(uint32_t addr, uint8_t val, struct _page_ *p);
    void	(*write_w)(uint32_t addr, uint16_t val, struct _page_ *p);
    void	(*write_l)(uint32_t addr, uint32_t val, struct _page_ *p);

    uint8_t	*mem;

    uint64_t	code_present_mask[4],
		dirty_mask[4];

    struct codeblock_t *block[4], *block_2[4];

    /*Head of codeblock tree associated with this page*/
    struct codeblock_t *head;
} page_t;


extern uint8_t		*ram;
extern uint32_t		rammask;

extern int		readlookup[256],
			readlookupp[256];
extern uintptr_t	*readlookup2;
extern int		readlnext;
extern int		writelookup[256],
			writelookupp[256];
extern uintptr_t	*writelookup2;
extern int		writelnext;
extern uint32_t		ram_mapped_addr[64];

extern mem_map_t	base_mapping,
			ram_low_mapping,
			ram_mid_mapping,
			ram_remapped_mapping,
			ram_high_mapping;

extern uint32_t		mem_logical_addr;

extern page_t		*pages,
			**page_lookup;

extern uint32_t		get_phys_virt,get_phys_phys;

extern uint32_t		pccache;
extern uint8_t		*pccache2;

extern int		memspeed[11];

extern int		mmu_perm;

extern int		mem_a20_state,
			mem_a20_alt,
			mem_a20_key;


#define readmemb(a) ((readlookup2[(a)>>12]== (uintptr_t)-1) \
		? readmembl(a) \
		: *(uint8_t *)(readlookup2[(a) >> 12] + (a)))

#define readmemw(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]== (uintptr_t)-1 || \
		       (((s)+(a)) & 1)) ? readmemwl((s)+(a)) \
		: *(uint16_t *)(readlookup2[(uint32_t)((s)+(a))>>12]+(uint32_t)((s)+(a))))
#define readmeml(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]== (uintptr_t)-1 || \
		       (((s)+(a)) & 3)) ? readmemll((s)+(a)) \
		: *(uint32_t *)(readlookup2[(uint32_t)((s)+(a))>>12]+(uint32_t)((s)+(a))))


/* Memory access for the 808x CPUs. */
extern uint8_t	read_mem_b(uint32_t addr);
extern uint16_t	read_mem_w(uint32_t addr);
extern void	write_mem_b(uint32_t addr, uint8_t val);
extern void	write_mem_w(uint32_t addr, uint16_t val);

extern uint8_t	readmembl(uint32_t addr);
extern void	writemembl(uint32_t addr, uint8_t val);
extern uint16_t	readmemwl(uint32_t addr);
extern void	writememwl(uint32_t addr, uint16_t val);
extern uint32_t	readmemll(uint32_t addr);
extern void	writememll(uint32_t addr, uint32_t val);
extern uint64_t	readmemql(uint32_t addr);
extern void	writememql(uint32_t addr, uint64_t val);

extern uint8_t	*getpccache(uint32_t a);
extern uint32_t	mmutranslatereal(uint32_t addr, int rw);
extern void	addreadlookup(uint32_t virt, uint32_t phys);
extern void	addwritelookup(uint32_t virt, uint32_t phys);


extern void	mem_map_del(mem_map_t *);

extern void	mem_map_add(mem_map_t *,
                    uint32_t base, 
                    uint32_t size, 
                    uint8_t  (*read_b)(uint32_t addr, void *p),
                    uint16_t (*read_w)(uint32_t addr, void *p),
                    uint32_t (*read_l)(uint32_t addr, void *p),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
                    void (*write_w)(uint32_t addr, uint16_t val, void *p),
                    void (*write_l)(uint32_t addr, uint32_t val, void *p),
                    uint8_t *exec,
                    uint32_t flags,
                    void *p);

extern void	mem_map_set_handler(mem_map_t *,
                    uint8_t  (*read_b)(uint32_t addr, void *p),
                    uint16_t (*read_w)(uint32_t addr, void *p),
                    uint32_t (*read_l)(uint32_t addr, void *p),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
                    void (*write_w)(uint32_t addr, uint16_t val, void *p),
                    void (*write_l)(uint32_t addr, uint32_t val, void *p));

extern void	mem_map_set_p(mem_map_t *, void *p);
extern void	mem_map_set_p2(mem_map_t *, void *p);

extern void	mem_map_set_dev(mem_map_t *, void *dev);

extern void	mem_map_set_addr(mem_map_t *, uint32_t base, uint32_t size);
extern void	mem_map_set_exec(mem_map_t *, uint8_t *exec);
extern void	mem_map_disable(mem_map_t *);
extern void	mem_map_enable(mem_map_t *);

extern void	mem_set_mem_state(uint32_t base, uint32_t size, int state);

extern uint8_t	mem_readb_phys(uint32_t addr);
extern uint16_t	mem_readw_phys(uint32_t addr);
extern void	mem_writeb_phys(uint32_t addr, uint8_t val);

extern uint8_t	mem_read_ram(uint32_t addr, void *priv);
extern uint16_t	mem_read_ramw(uint32_t addr, void *priv);
extern uint32_t	mem_read_raml(uint32_t addr, void *priv);
extern void	mem_write_ram(uint32_t addr, uint8_t val, void *priv);
extern void	mem_write_ramw(uint32_t addr, uint16_t val, void *priv);
extern void	mem_write_raml(uint32_t addr, uint32_t val, void *priv);

extern void	mem_write_null(uint32_t addr, uint8_t val, void *p);
extern void	mem_write_nullw(uint32_t addr, uint16_t val, void *p);
extern void	mem_write_nulll(uint32_t addr, uint32_t val, void *p);

extern uint32_t	mmutranslate_noabrt(uint32_t addr, int rw);

extern void	mem_invalidate_range(uint32_t start_addr, uint32_t end_addr);

extern void	mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *p);
extern void	mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *p);
extern void	mem_write_raml_page(uint32_t addr, uint32_t val, page_t *p);
extern void	mem_flush_write_page(uint32_t addr, uint32_t virt);

extern void	mem_reset_page_blocks(void);

extern int	mem_addr_is_ram(uint32_t addr);

extern void     flushmmucache(void);
extern void     flushmmucache_cr3(void);
extern void	flushmmucache_nopc(void);
extern void     mmu_invalidate(uint32_t addr);

extern void	mem_a20_recalc(void);

extern void	mem_init(void);
extern void	mem_reset(void);
extern void	mem_remap_top(int kb);


#ifdef EMU_CPU_H
static __inline uint32_t
get_phys(uint32_t addr)
{
    if (! ((addr ^ get_phys_virt) & ~0xfff))
	return get_phys_phys | (addr & 0xfff);

    get_phys_virt = addr;

    if (! (cr0 >> 31)) {
	get_phys_phys = (addr & rammask) & ~0xfff;

	return addr & rammask;
    }

    if (readlookup2[addr >> 12] != -1)
	get_phys_phys = ((uintptr_t)readlookup2[addr >> 12] + (addr & ~0xfff)) - (uintptr_t)ram;
    else {
	get_phys_phys = (mmutranslatereal(addr, 0) & rammask) & ~0xfff;

	if (!cpu_state.abrt && mem_addr_is_ram(get_phys_phys))
		addreadlookup(get_phys_virt, get_phys_phys);
    }

    return get_phys_phys | (addr & 0xfff);
}

static __inline uint32_t 
get_phys_noabrt(uint32_t addr)
{
    uint32_t phys_addr;

    if (!(cr0 >> 31))
	return addr & rammask;

    if (readlookup2[addr >> 12] != -1)
	return ((uintptr_t)readlookup2[addr >> 12] + addr) - (uintptr_t)ram;

    phys_addr = mmutranslate_noabrt(addr, 0) & rammask;
    if (phys_addr != 0xffffffff && mem_addr_is_ram(phys_addr))
	addreadlookup(addr, phys_addr);

    return phys_addr;
}
#endif


#endif	/*EMU_MEM_H*/
